#include <GL/gl.h>
#include <GL/glut.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <SDL2/SDL_mixer.h>

#define WINDOW_W 500
#define WINDOW_H 500

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

//variables para escalar la dificultad
float gameTime        = 0.0f; //tiempo total de la partida
float speedMultiplier = 1.0f; //velocidad base
int   difficultyLevel = 0;

#define SPEED_INTERVAL   10.0f  //cada cuántos segundos sube
#define SPEED_STEP       0.15f  //cuánto sube cada vez
#define MAX_MULTIPLIER   3.0f //tope para no volverse imposible

// Carga de audios
const char* BGM_FILES[8] = {
    "assets/audio_065.ogg",   // 0.65×
    "assets/audio_075.ogg",   // 0.75×
    "assets/audio_085.ogg",   // 0.85×
    "assets/audio_095.ogg",   // 0.95×
    "assets/audio_100.ogg",   // original
    "assets/audio_110.ogg",   // 1.10×
    "assets/audio_115.ogg",   // 1.15×
    "assets/audio_120.ogg"    // 1.20×
};

Mix_Music* bgm[8] = { nullptr };
int currentBgmIndex = -1;

void initAudio() {
    // 1. Cargar decoders (OGG, etc.)
    int initted = Mix_Init(MIX_INIT_OGG);
    if ((initted & MIX_INIT_OGG) != MIX_INIT_OGG) {
        fprintf(stderr, "OGG no soportado: %s\n", Mix_GetError());
    }

    // 2. Abrir el dispositivo de audio
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        fprintf(stderr, "Error audio: %s\n", Mix_GetError());
        return;
    }

    Mix_VolumeMusic(64);

    for (int i = 0; i < 8; i++) {
        bgm[i] = Mix_LoadMUS(BGM_FILES[i]);
        if (!bgm[i]) {
            fprintf(stderr, "No se pudo cargar %s: %s\n",
                    BGM_FILES[i], Mix_GetError());
        }
    }
    int cargados = 0;
    for (int i = 0; i < 8; i++) if (bgm[i]) cargados++;
    printf("BGM cargados: %d/8\n", cargados);
    if (cargados < 8) {
        printf("Faltan archivos");
    }
}

void playBgmIndex(int idx) {
    if (idx < 0 || idx >= 8) return;
    if (idx == currentBgmIndex) return;
    if (!bgm[idx]) return;   // por si falló la carga

    currentBgmIndex = idx;
    Mix_HaltMusic();
    Mix_FadeInMusic(bgm[idx], -1, 300);  // -1 = loop, fade in 300 ms
}

int levelToBgmIndex(int nivel) {
    if (nivel < 2)  return 0;   // 0.65×  niveles 1-2
    if (nivel < 4)  return 1;   // 0.75×  niveles 3-4
    if (nivel < 6)  return 2;   // 0.85×  niveles 5-6
    if (nivel < 8)  return 3;   // 0.95×  niveles 7-8
    if (nivel < 10) return 4;   // 1.00×  niveles 9-10
    if (nivel < 13) return 5;   // 1.10×  niveles 11-13
    if (nivel < 16) return 6;   // 1.15×  niveles 14-16
    return 7;  // 1.20×  nivel 17+
}

//------------------------------------------------------------------------------
//Caja de batalla
float boxX, boxY, boxW, boxH;

typedef struct {
    float x, y;
    float size;
    float speed;
    int hp;
} Player;

typedef struct {
    float x, y;
    float vx, vy;
    float size;
    int active;
} Bullet;

#define MAX_BULLETS 12800
#define BULLETS_HELP 3
Bullet bullets[MAX_BULLETS];
Bullet bullets_help [BULLETS_HELP];
Player player;

int keyState[256]        = {0};
int specialKeyState[256] = {0};
int lastTime;
float spawnTimer = 0;
int gameOver = 0;

//detener la musica
void triggerGameOver() {
    if (gameOver) return; // evitar doble llamada
    gameOver = 1;

    //fade out de 800 ms
    Mix_FadeOutMusic(800);

    // Silenciar SFX que estén sonando (impactos, etc.)
    Mix_HaltChannel(-1);

    // Opcional: efecto de sonido de "muerte"
    // Mix_PlayChannel(-1, sndGameOver, 0);
}

// inicializacion
void initGame() {
    boxW = 300; boxH = 300;
    boxX = (WINDOW_W - boxW) / 2.0f;
    boxY = (WINDOW_H - boxH) / 2.0f;

    player.x = WINDOW_W / 2.0f;
    player.y = WINDOW_H / 2.0f;
    player.size = 14;
    player.speed = 180;
    player.hp = 20;

    gameTime         = 0.0f;
    speedMultiplier  = 1.0f;
    difficultyLevel  = 0;

    for (int i = 0; i < MAX_BULLETS; i++) bullets[i].active = 0;

    srand(time(NULL));
    lastTime = glutGet(GLUT_ELAPSED_TIME);
}

//dibujo del corazón
void drawHeart(float cx, float cy, float size) {
    glColor3f(1.0f, 0.0f, 0.0f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    float scale = size / 32.0f;
    for (int i = 0; i <= 24; i++) {
        float t = i / 24.0f * 2.0f * (float)M_PI;
        float x = 16 * powf(sinf(t), 3);
        float y = 13*cosf(t) - 5*cosf(2*t) - 2*cosf(3*t) - cosf(4*t);
        glVertex2f(cx + x*scale, cy + y*scale);
    }
    glEnd();
}

//proyectiles
void spawnBullet() {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].active) {
            bullets[i].active = 1;
            bullets[i].size = 10;
            //velocidad base + aleatorio por el multiplicador
            float speed = (100.0f + (rand() % 100)) * speedMultiplier;
            int side = rand() % 4;
            switch (side) {
                case 0:
                    bullets[i].x = boxX + (rand() % (int)boxW);
                    bullets[i].y = boxY + boxH + 10;
                    bullets[i].vx = 0;  bullets[i].vy = -speed; break;
                case 1:
                    bullets[i].x = boxX + (rand() % (int)boxW);
                    bullets[i].y = boxY - 10;
                    bullets[i].vx = 0;  bullets[i].vy = speed;  break;
                case 2:
                    bullets[i].x = boxX - 10;
                    bullets[i].y = boxY + (rand() % (int)boxH);
                    bullets[i].vx = speed;  bullets[i].vy = 0;   break;
                default:
                    bullets[i].x = boxX + boxW + 10;
                    bullets[i].y = boxY + (rand() % (int)boxH);
                    bullets[i].vx = -speed; bullets[i].vy = 0;   break;
            }
            return;
        }
    }
}

void spawnBulletHelp() {
    for (int i = 0; i < BULLETS_HELP; i++) {
        if (!bullets_help[i].active) {
            bullets_help[i].active = 1;
            bullets_help[i].size = 10;
            //velocidad base + aleatorio por el multiplicador
            float speed = (100.0f + (rand() % 100)) * speedMultiplier;
            int side = rand() % 4;
            switch (side) {
                case 0:
                    bullets_help[i].x = boxX + (rand() % (int)boxW);
                    bullets_help[i].y = boxY + boxH + 10;
                    bullets_help[i].vx = 0;  bullets_help[i].vy = -speed; break;
                case 1:
                    bullets_help[i].x = boxX + (rand() % (int)boxW);
                    bullets_help[i].y = boxY - 10;
                    bullets_help[i].vx = 0;  bullets_help[i].vy = speed;  break;
                case 2:
                    bullets_help[i].x = boxX - 10;
                    bullets_help[i].y = boxY + (rand() % (int)boxH);
                    bullets_help[i].vx = speed;  bullets_help[i].vy = 0;   break;
                default:
                    bullets_help[i].x = boxX + boxW + 10;
                    bullets_help[i].y = boxY + (rand() % (int)boxH);
                    bullets_help[i].vx = -speed; bullets_help[i].vy = 0;   break;
            }
            return;
        }
    }
}

int checkCollision(float ax, float ay, float as, float bx, float by, float bs) {
    float halfA = as * 0.4f;
    float halfB = bs * 0.4f;
    return fabsf(ax - bx) < (halfA + halfB) &&
           fabsf(ay - by) < (halfA + halfB);
}

//reiniciar
void resetGame() {
    // Resetear estado
    player.x = WINDOW_W / 2.0f;
    player.y = WINDOW_H / 2.0f;
    player.hp = 20;
    gameOver = 0;
    gameTime = 0.0f;
    speedMultiplier = 1.0f;
    difficultyLevel = 0;
    spawnTimer = 0.0f;

    // Limpiar balas
    for (int i = 0; i < MAX_BULLETS; i++) {
        bullets[i].active = 0;
    }

    // Reiniciar música desde el tier más lento
    Mix_HaltMusic();
    playBgmIndex(0);
    Mix_FadeInMusic(bgm[0], -1, 400);   // por si acaso el fadeout lo dejo a medias
}

int textWidth(const char *text, void *font) {
    int w = 0;
    for (const char *c = text; *c; c++) {
        w += glutBitmapWidth(font, *c);
    }
    return w;
}

void drawTextCentered(float cx, float cy, const char *text, void *font) {
    int w = textWidth(text, font);
    // cy es el centro vertical; ajustamos para que el texto quede centrado
    // (la altura de GLUT_BITMAP_9_BY_15 es 15 px, mitad = 7)
    int h = 15;
    glRasterPos2f(cx - w / 2.0f, cy - h / 2.0f);
    for (const char *c = text; *c; c++) {
        glutBitmapCharacter(font, *c);
    }
}


//render
void display_callback() {
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    //borde de la caja
    glColor3f(1, 1, 1);
    glLineWidth(3);
    glBegin(GL_LINE_LOOP);
    glVertex2f(boxX, boxY);
    glVertex2f(boxX + boxW, boxY);
    glVertex2f(boxX + boxW, boxY + boxH);
    glVertex2f(boxX, boxY + boxH);
    glEnd();

    //proyectiles hostiles
    glColor3f(1, 1, 1);
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].active) continue;
        float s = bullets[i].size * 0.5f;
        glBegin(GL_QUADS);
        glVertex2f(bullets[i].x - s, bullets[i].y - s);
        glVertex2f(bullets[i].x + s, bullets[i].y - s);
        glVertex2f(bullets[i].x + s, bullets[i].y + s);
        glVertex2f(bullets[i].x - s, bullets[i].y + s);
        glEnd();
    }

    //proyectiles de mejora
    glColor3f(0, 1, 0);
    for (int i = 0; i < BULLETS_HELP; i++) {
        if (!bullets_help[i].active) continue;
        float s = bullets_help[i].size * 0.5f;
        glBegin(GL_QUADS);
        glVertex2f(bullets_help[i].x - s, bullets_help[i].y - s);
        glVertex2f(bullets_help[i].x + s, bullets_help[i].y - s);
        glVertex2f(bullets_help[i].x + s, bullets_help[i].y + s);
        glVertex2f(bullets_help[i].x - s, bullets_help[i].y + s);
        glEnd();
    }


    // jugador
    if (!gameOver)
    {drawHeart(player.x, player.y, player.size);}


    //HP
    char buf[32];
    sprintf(buf, "HP: %d   NIVEL: %d", player.hp, difficultyLevel + 1);
    glColor3f(1, 1, 1);
    glRasterPos2f(10, WINDOW_H - 20);
    for (char *c = buf; *c; c++) glutBitmapCharacter(GLUT_BITMAP_9_BY_15, *c);

    if (gameOver) {
        glColor3f(1, 0, 0);
        drawTextCentered(WINDOW_W / 2.0f, WINDOW_H / 2.0f,
            "GAME OVER", GLUT_BITMAP_9_BY_15);

        glColor3f(1, 1, 1);
        drawTextCentered(WINDOW_W / 2.0f, WINDOW_H / 2.0f - 30,
            "Pulsa R para reiniciar", GLUT_BITMAP_9_BY_15);
    }

    glutSwapBuffers();
}

//loop
void update(int value) {
    int now = glutGet(GLUT_ELAPSED_TIME);
    float dt = (now - lastTime) / 1000.0f;
    lastTime = now;
    if (dt > 0.05f) dt = 0.05f;
    if (dt < 0)     dt = 0;

    if (!gameOver) {
        //escalado de dificultad
        gameTime += dt;
        int newLevel = (int)(gameTime / SPEED_INTERVAL);
        if (newLevel > difficultyLevel) {
            difficultyLevel = newLevel;
            speedMultiplier += SPEED_STEP;
            if (speedMultiplier > MAX_MULTIPLIER) speedMultiplier = MAX_MULTIPLIER;

            playBgmIndex(levelToBgmIndex(difficultyLevel));
        }

        //movimiento del jugador
        float dx = 0, dy = 0;
        if (specialKeyState[GLUT_KEY_LEFT]  || keyState['a'] || keyState['A']) dx -= 1;
        if (specialKeyState[GLUT_KEY_RIGHT] || keyState['d'] || keyState['D']) dx += 1;
        if (specialKeyState[GLUT_KEY_UP]    || keyState['w'] || keyState['W']) dy += 1;
        if (specialKeyState[GLUT_KEY_DOWN]  || keyState['s'] || keyState['S']) dy -= 1;

        float len = sqrtf(dx*dx + dy*dy);
        if (len > 0) {
            dx /= len; dy /= len;
            player.x += dx * player.speed * dt;
            player.y += dy * player.speed * dt;
        }

        // clamp a la caja
        float half = player.size * 0.5f;
        if (player.x - half < boxX)        player.x = boxX + half;
        if (player.x + half > boxX + boxW) player.x = boxX + boxW - half;
        if (player.y - half < boxY)        player.y = boxY + half;
        if (player.y + half > boxY + boxH) player.y = boxY + boxH - half;

        spawnTimer -= dt;

        if (spawnTimer <= 0) {
            spawnBullet();
            if (difficultyLevel >= 3) spawnBullet();

            if (difficultyLevel >= 2) spawnBulletHelp();

            // intervalo base aleatorio
            float baseInterval = 0.25f + (rand() % 100) / 300.0f;

            //se acorta con el nivel
            float spawnInterval = baseInterval / speedMultiplier;
            if (spawnInterval < 0.08f) spawnInterval = 0.08f;  // tope: ~12 balas/seg

            spawnTimer = spawnInterval;
        }
        // Balas malas
        for (int i = 0; i < MAX_BULLETS; i++) {
            if (!bullets[i].active) continue;
            bullets[i].x += bullets[i].vx * dt;
            bullets[i].y += bullets[i].vy * dt;

            if (bullets[i].x < boxX - 30 || bullets[i].x > boxX + boxW + 30 ||
                bullets[i].y < boxY - 30 || bullets[i].y > boxY + boxH + 30) {
                bullets[i].active = 0;
                continue;
            }

            if (checkCollision(player.x, player.y, player.size,
                               bullets[i].x, bullets[i].y, bullets[i].size)) {
                bullets[i].active = 0;
                player.hp--;
                if (player.hp <= 0) {
                    player.hp = 0;
                    triggerGameOver();   // 👈
                }
            }
        }
        // Balas buenas
        for (int i = 0; i < BULLETS_HELP; i++) {
            if (!bullets_help[i].active) continue;
            bullets_help[i].x += bullets_help[i].vx * dt;
            bullets_help[i].y += bullets_help[i].vy * dt;

            if (bullets_help[i].x < boxX - 30 || bullets_help[i].x > boxX + boxW + 30 ||
                bullets_help[i].y < boxY - 30 || bullets_help[i].y > boxY + boxH + 30) {
                bullets_help[i].active = 0;
                continue;
            }

            if (checkCollision(player.x, player.y, player.size,
                               bullets_help[i].x, bullets_help[i].y, bullets_help[i].size)) {
                bullets_help[i].active = 0;
                player.hp++;
                if (player.hp > 20) { player.hp = 20; gameOver = 0; }
            }
        }
    }

    glutPostRedisplay();
    glutTimerFunc(16, update, 0);
}

void shutdownAudio() {
    // Detener cualquier música en curso
    Mix_HaltMusic();

    // Liberar cada pista cargada
    for (int i = 0; i < 8; i++) {
        if (bgm[i]) {
            Mix_FreeMusic(bgm[i]);
            bgm[i] = nullptr;
        }
    }

    // Cerrar el dispositivo de audio
    Mix_CloseAudio();

    // Liberar los códecs inicializados con Mix_Init
    Mix_Quit();
}

// entrada
void keyboard(unsigned char key, int x, int y) {
    keyState[key] = 1;
    if (key == 27) {   // ESC
        shutdownAudio();
        exit(0);
    }
    if (key == 'r' || key == 'R') {
        if (gameOver) resetGame(); //solo reinicia si se muere
    }
    keyState[key] = 1;


}
void keyboardUp(unsigned char key, int x, int y) { keyState[key] = 0; }
void special(int key, int x, int y)              { specialKeyState[key] = 1; }
void specialUp(int key, int x, int y)            { specialKeyState[key] = 0; }

//setup de proyección
void reshape(int w, int h) {
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, WINDOW_W, 0, WINDOW_H, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

int main(int argc, char **argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
    glutInitWindowSize(WINDOW_W, WINDOW_H);
    glutCreateWindow("Bullet Hell");
    initGame();
    initAudio();
    playBgmIndex(0);

    glutDisplayFunc(display_callback);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutKeyboardUpFunc(keyboardUp);
    glutSpecialFunc(special);
    glutSpecialUpFunc(specialUp);
    glutTimerFunc(16, update, 0);
    glutMainLoop();
    return 0;
}
