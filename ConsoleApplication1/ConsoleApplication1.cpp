// ============================================================
//  PORT SCENE — OpenGL / GLUT  (C++)
//  Computer Graphics Project — Chapters 3 & 4
//
//  Features:
//    • Water with animated sine waves
//    • Two boats that bob gently on the water
//    • Lighthouse with rotating beacon light
//    • Birds flying across the sky
//    • Sun, sky gradient, clouds
//
//  Concepts used from notes:
//    • Translation  P' = P + T             (Ch4a §1)
//    • Rotation     x'=x cosθ − y sinθ     (Ch4a §2 / Trig §4)
//    • Polygon fill  GL_POLYGON, GL_TRIANGLE_FAN, GL_QUADS  (Ch3)
//    • GL_LINE_STRIP for waves              (Ch3)
//    • Display Lists for repeated objects   (Ch3 §5)
//    • glutReshapeFunc                      (Ch3 §6)
//    • Parameterised drawing functions      (Ch3 §7)
//    • Angle addition: sin(2θ)=2sinθcosθ   (Trig §6)
// ============================================================

#ifdef _WIN32
#  include <windows.h>
#endif

#include <GL/glut.h>
#include <cmath>
#include <cstdlib>

// ── Window ──────────────────────────────────────────────────
const int WIN_W = 900;
const int WIN_H = 600;
const float PI = 3.14159265f;

// ── Animation state ─────────────────────────────────────────
float gTime = 0.0f;   // global timer (seconds, incremented per frame)
float gLightAngle = 0.0f;   // lighthouse beam rotation angle (degrees)

// Birds: 4 birds, each stores (x, y, phase)
struct Bird { float x, y, phase; };
Bird gBirds[4] = {
    { -50.0f, 420.0f, 0.0f   },
    { -120.0f,400.0f, 1.2f   },
    {  -20.0f,435.0f, 0.6f   },
    { -90.0f, 415.0f, 1.8f   }
};

// Display list IDs
GLuint dlBoat = 0;
GLuint dlCloud = 0;

// ── Colour helpers ───────────────────────────────────────────
void setColor(float r, float g, float b) { glColor3f(r, g, b); }

// ── Sky gradient (two large quads blended) ───────────────────
void drawSky()
{
    // Sky: horizon is light blue, top is deeper blue
    glBegin(GL_QUADS);
    // horizon colour
    glColor3f(0.60f, 0.82f, 0.98f);
    glVertex2f(0.0f, 0.0f);
    glVertex2f((float)WIN_W, 0.0f);
    // top colour
    glColor3f(0.20f, 0.48f, 0.80f);
    glVertex2f((float)WIN_W, (float)WIN_H);
    glVertex2f(0.0f, (float)WIN_H);
    glEnd();
}

// ── Sun ──────────────────────────────────────────────────────
void drawSun(float cx, float cy, float r)
{
    setColor(1.0f, 0.95f, 0.30f);
    int segs = 40;
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int i = 0; i <= segs; i++) {
        float a = 2.0f * PI * i / segs;
        glVertex2f(cx + r * cosf(a), cy + r * sinf(a));
    }
    glEnd();

    // soft glow ring
    setColor(1.0f, 0.98f, 0.60f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int i = 0; i <= segs; i++) {
        float a = 2.0f * PI * i / segs;
        glVertex2f(cx + (r + 12) * cosf(a), cy + (r + 12) * sinf(a));
    }
    glEnd();
}

// ── Cloud (stored in a display list) ─────────────────────────
//   Drawn centred at (0,0); caller translates.
void buildCloudList()
{
    dlCloud = glGenLists(1);
    glNewList(dlCloud, GL_COMPILE);
    setColor(1.0f, 1.0f, 1.0f);
    // three overlapping circles make a cloud puff
    float cx[] = { 0.0f, 28.0f, -28.0f, 14.0f, -14.0f };
    float cy[] = { 0.0f, -8.0f,  -8.0f, 10.0f,  10.0f };
    float cr[] = { 20.0f, 18.0f,  18.0f, 16.0f,  16.0f };
    for (int c = 0; c < 5; c++) {
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(cx[c], cy[c]);
        for (int i = 0; i <= 30; i++) {
            float a = 2.0f * PI * i / 30;
            glVertex2f(cx[c] + cr[c] * cosf(a),
                cy[c] + cr[c] * sinf(a));
        }
        glEnd();
    }
    glEndList();
}

void drawCloud(float tx, float ty, float scale)
{
    glPushMatrix();
    glTranslatef(tx, ty, 0.0f);
    glScalef(scale, scale, 1.0f);
    glCallList(dlCloud);
    glPopMatrix();
}

// ── Water ─────────────────────────────────────────────────────
//   Horizon line at y = WATER_Y; water fills downward.
const float WATER_Y = 300.0f;

void drawWater()
{
    // solid sea base
    setColor(0.05f, 0.25f, 0.55f);
    glBegin(GL_QUADS);
    glVertex2f(0.0f, 0.0f);
    glVertex2f((float)WIN_W, 0.0f);
    glVertex2f((float)WIN_W, WATER_Y);
    glVertex2f(0.0f, WATER_Y);
    glEnd();

    // animated wave strips (3 layers, different amplitude/speed/phase)
    struct WaveLayer { float amp, freq, speed, yBase, alpha[3]; };
    WaveLayer layers[] = {
        { 6.0f,  0.025f, 1.2f, WATER_Y,       {0.10f,0.55f,0.90f} },
        { 4.0f,  0.040f, 1.8f, WATER_Y - 25.0f, {0.05f,0.35f,0.70f} },
        { 3.0f,  0.060f, 2.5f, WATER_Y - 55.0f, {0.08f,0.45f,0.80f} }
    };

    for (int L = 0; L < 3; L++) {
        WaveLayer& wl = layers[L];
        glColor3f(wl.alpha[0], wl.alpha[1], wl.alpha[2]);
        glLineWidth(2.0f);
        glBegin(GL_LINE_STRIP);
        for (int px = 0; px <= WIN_W; px += 3) {
            float y = wl.yBase +
                wl.amp * sinf(wl.freq * px + wl.speed * gTime);
            glVertex2f((float)px, y);
        }
        glEnd();
    }
    glLineWidth(1.0f);
}

// ── Boat (display list, centred at origin, hull bottom at y=0) ──
//   Width ~80, height ~50
void buildBoatList()
{
    dlBoat = glGenLists(1);
    glNewList(dlBoat, GL_COMPILE);

    // Hull — dark reddish-brown
    setColor(0.45f, 0.22f, 0.08f);
    glBegin(GL_POLYGON);
    glVertex2f(-40.0f, 0.0f);    // bow left
    glVertex2f(40.0f, 0.0f);    // bow right
    glVertex2f(50.0f, -20.0f);   // stern right
    glVertex2f(-50.0f, -20.0f);   // stern left
    glEnd();

    // Deck — lighter wood
    setColor(0.70f, 0.45f, 0.18f);
    glBegin(GL_QUADS);
    glVertex2f(-38.0f, 0.0f);
    glVertex2f(38.0f, 0.0f);
    glVertex2f(38.0f, 6.0f);
    glVertex2f(-38.0f, 6.0f);
    glEnd();

    // Cabin
    setColor(0.90f, 0.88f, 0.80f);
    glBegin(GL_QUADS);
    glVertex2f(-15.0f, 6.0f);
    glVertex2f(15.0f, 6.0f);
    glVertex2f(15.0f, 26.0f);
    glVertex2f(-15.0f, 26.0f);
    glEnd();
    // Cabin roof
    setColor(0.80f, 0.20f, 0.10f);
    glBegin(GL_TRIANGLES);
    glVertex2f(-18.0f, 26.0f);
    glVertex2f(18.0f, 26.0f);
    glVertex2f(0.0f, 36.0f);
    glEnd();

    // Mast
    setColor(0.55f, 0.40f, 0.20f);
    glLineWidth(3.0f);
    glBegin(GL_LINES);
    glVertex2f(0.0f, 6.0f);
    glVertex2f(0.0f, 65.0f);
    glEnd();

    // Sail
    setColor(0.97f, 0.97f, 0.92f);
    glBegin(GL_TRIANGLES);
    glVertex2f(0.0f, 60.0f);
    glVertex2f(0.0f, 14.0f);
    glVertex2f(35.0f, 30.0f);
    glEnd();

    // Windows
    setColor(0.60f, 0.85f, 1.00f);
    glBegin(GL_QUADS);
    glVertex2f(-12.0f, 12.0f); glVertex2f(-4.0f, 12.0f);
    glVertex2f(-4.0f, 22.0f); glVertex2f(-12.0f, 22.0f);

    glVertex2f(4.0f, 12.0f); glVertex2f(12.0f, 12.0f);
    glVertex2f(12.0f, 22.0f); glVertex2f(4.0f, 22.0f);
    glEnd();

    glLineWidth(1.0f);
    glEndList();
}

// Draw a boat: tx=x centre, ty=waterline y
void drawBoat(float tx, float ty, float bob, float tilt)
{
    glPushMatrix();
    // translate to position, lift by bob offset
    glTranslatef(tx, ty + bob, 0.0f);
    // slight tilt (rotation about origin)
    glRotatef(tilt, 0.0f, 0.0f, 1.0f);
    glCallList(dlBoat);
    glPopMatrix();
}

// ── Lighthouse ───────────────────────────────────────────────
const float LH_X = 750.0f;
const float LH_BASE_Y = WATER_Y;

void drawLighthouse()
{
    // Base rock
    setColor(0.45f, 0.42f, 0.38f);
    glBegin(GL_POLYGON);
    for (int i = 0; i <= 30; i++) {
        float a = PI * i / 30.0f; // semicircle
        glVertex2f(LH_X + 45 * cosf(a), LH_BASE_Y - 10 + 18 * sinf(a));
    }
    glEnd();

    // Tower body — trapezoid (wider at base)
    setColor(0.95f, 0.92f, 0.88f);
    glBegin(GL_POLYGON);
    glVertex2f(LH_X - 28, LH_BASE_Y);
    glVertex2f(LH_X + 28, LH_BASE_Y);
    glVertex2f(LH_X + 16, LH_BASE_Y + 140);
    glVertex2f(LH_X - 16, LH_BASE_Y + 140);
    glEnd();

    // Red stripes (use GL_QUADS for each stripe)
    setColor(0.85f, 0.15f, 0.10f);
    // 3 stripes
    float stripeY[] = { LH_BASE_Y + 30, LH_BASE_Y + 75, LH_BASE_Y + 115 };
    for (int s = 0; s < 3; s++) {
        float sy = stripeY[s];
        float t = (sy - LH_BASE_Y) / 140.0f; // interpolate width
        float hw = 28 - (28 - 16) * t;
        glBegin(GL_QUADS);
        glVertex2f(LH_X - hw, sy);
        glVertex2f(LH_X + hw, sy);
        glVertex2f(LH_X + hw, sy + 12);
        glVertex2f(LH_X - hw, sy + 12);
        glEnd();
    }

    // Balcony
    setColor(0.35f, 0.32f, 0.28f);
    glBegin(GL_QUADS);
    glVertex2f(LH_X - 20, LH_BASE_Y + 137);
    glVertex2f(LH_X + 20, LH_BASE_Y + 137);
    glVertex2f(LH_X + 20, LH_BASE_Y + 143);
    glVertex2f(LH_X - 20, LH_BASE_Y + 143);
    glEnd();

    // Lamp room
    setColor(0.22f, 0.20f, 0.18f);
    glBegin(GL_QUADS);
    glVertex2f(LH_X - 15, LH_BASE_Y + 143);
    glVertex2f(LH_X + 15, LH_BASE_Y + 143);
    glVertex2f(LH_X + 15, LH_BASE_Y + 170);
    glVertex2f(LH_X - 15, LH_BASE_Y + 170);
    glEnd();

    // Cap
    setColor(0.60f, 0.20f, 0.10f);
    glBegin(GL_TRIANGLES);
    glVertex2f(LH_X - 18, LH_BASE_Y + 170);
    glVertex2f(LH_X + 18, LH_BASE_Y + 170);
    glVertex2f(LH_X, LH_BASE_Y + 185);
    glEnd();

    // Door
    setColor(0.40f, 0.22f, 0.08f);
    glBegin(GL_QUADS);
    glVertex2f(LH_X - 7, LH_BASE_Y);
    glVertex2f(LH_X + 7, LH_BASE_Y);
    glVertex2f(LH_X + 7, LH_BASE_Y + 24);
    glVertex2f(LH_X - 7, LH_BASE_Y + 24);
    glEnd();
    // Arched door top
    setColor(0.40f, 0.22f, 0.08f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(LH_X, LH_BASE_Y + 24);
    for (int i = 0; i <= 12; i++) {
        float a = PI * i / 12.0f;
        glVertex2f(LH_X + 7 * cosf(a), LH_BASE_Y + 24 + 7 * sinf(a));
    }
    glEnd();
}

// ── Lighthouse rotating beam ─────────────────────────────────
//   Drawn using the rotation formula from Ch4a §2 / Trig §4:
//   x' = x cosθ − y sinθ
//   y' = x sinθ + y cosθ
void drawBeam(float angleDeg)
{
    float rad = angleDeg * PI / 180.0f;
    float cx = LH_X;
    float cy = LH_BASE_Y + 156.0f; // centre of lamp room

    float beamLen = 320.0f;

    // Two beam rays (± 8 degrees) — triangle fan
    float a1 = rad - 0.14f;
    float a2 = rad + 0.14f;

    // Yellow semi-transparent beam
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1.0f, 0.95f, 0.30f, 0.35f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    glVertex2f(cx + beamLen * cosf(a1), cy + beamLen * sinf(a1));
    glVertex2f(cx + beamLen * cosf(a2), cy + beamLen * sinf(a2));
    glEnd();
    glDisable(GL_BLEND);

    // Bright core line
    setColor(1.0f, 1.0f, 0.60f);
    glLineWidth(2.5f);
    glBegin(GL_LINES);
    glVertex2f(cx, cy);
    glVertex2f(cx + beamLen * cosf(rad), cy + beamLen * sinf(rad));
    glEnd();
    glLineWidth(1.0f);

    // Bright lamp centre circle
    setColor(1.0f, 0.98f, 0.70f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int i = 0; i <= 20; i++) {
        float a = 2.0f * PI * i / 20;
        glVertex2f(cx + 6 * cosf(a), cy + 6 * sinf(a));
    }
    glEnd();
}

// ── Birds ─────────────────────────────────────────────────────
//   Each bird is two V-shaped wing arcs drawn with GL_LINE_STRIP.
//   Wing flap uses sin(time + phase).
void drawBird(float bx, float by, float phase)
{
    float flap = 8.0f * sinf(gTime * 4.0f + phase);  // wing tip offset
    float wspan = 18.0f;

    setColor(0.10f, 0.10f, 0.12f);
    glLineWidth(2.0f);

    // Left wing: 3 points — tip, body, mid
    glBegin(GL_LINE_STRIP);
    glVertex2f(bx - wspan, by + flap);
    glVertex2f(bx - wspan / 2, by);
    glVertex2f(bx, by + 3.0f);
    glEnd();

    // Right wing
    glBegin(GL_LINE_STRIP);
    glVertex2f(bx, by + 3.0f);
    glVertex2f(bx + wspan / 2, by);
    glVertex2f(bx + wspan, by + flap);
    glEnd();

    glLineWidth(1.0f);
}

// ── Dock ──────────────────────────────────────────────────────
void drawDock()
{
    // Dock platform
    setColor(0.50f, 0.35f, 0.18f);
    glBegin(GL_QUADS);
    glVertex2f(80.0f, WATER_Y - 5);
    glVertex2f(260.0f, WATER_Y - 5);
    glVertex2f(260.0f, WATER_Y + 10);
    glVertex2f(80.0f, WATER_Y + 10);
    glEnd();

    // Dock planks (vertical lines)
    setColor(0.40f, 0.27f, 0.12f);
    glLineWidth(2.0f);
    for (int px = 90; px <= 255; px += 18) {
        glBegin(GL_LINES);
        glVertex2f((float)px, WATER_Y - 5);
        glVertex2f((float)px, WATER_Y + 10);
        glEnd();
    }

    // Piles
    setColor(0.35f, 0.22f, 0.10f);
    float pileX[] = { 100, 170, 240 };
    for (int p = 0; p < 3; p++) {
        glBegin(GL_QUADS);
        glVertex2f(pileX[p] - 5, WATER_Y + 10);
        glVertex2f(pileX[p] + 5, WATER_Y + 10);
        glVertex2f(pileX[p] + 5, WATER_Y - 55);
        glVertex2f(pileX[p] - 5, WATER_Y - 55);
        glEnd();
    }
    glLineWidth(1.0f);
}

// ── Hills / land on the horizon ──────────────────────────────
void drawLand()
{
    setColor(0.25f, 0.50f, 0.22f);
    glBegin(GL_POLYGON);
    glVertex2f(0.0f, WATER_Y);
    glVertex2f(0.0f, WATER_Y + 40);
    glVertex2f(80.0f, WATER_Y + 70);
    glVertex2f(200.0f, WATER_Y + 50);
    glVertex2f(340.0f, WATER_Y + 35);
    glVertex2f(500.0f, WATER_Y + 55);
    glVertex2f(680.0f, WATER_Y + 80);
    glVertex2f(800.0f, WATER_Y + 60);
    glVertex2f((float)WIN_W, WATER_Y + 50);
    glVertex2f((float)WIN_W, WATER_Y);
    glEnd();

    // Lighter grass stripe on top of hills
    setColor(0.35f, 0.62f, 0.28f);
    glLineWidth(3.0f);
    glBegin(GL_LINE_STRIP);
    glVertex2f(0.0f, WATER_Y + 40);
    glVertex2f(80.0f, WATER_Y + 70);
    glVertex2f(200.0f, WATER_Y + 50);
    glVertex2f(340.0f, WATER_Y + 35);
    glVertex2f(500.0f, WATER_Y + 55);
    glVertex2f(680.0f, WATER_Y + 80);
    glVertex2f(800.0f, WATER_Y + 60);
    glVertex2f((float)WIN_W, WATER_Y + 50);
    glEnd();
    glLineWidth(1.0f);
}

// ── Water reflections ────────────────────────────────────────
void drawReflections()
{
    // Simple horizontal shimmer lines
    setColor(0.25f, 0.60f, 0.90f);
    for (int i = 0; i < 8; i++) {
        float y = WATER_Y - 30.0f - i * 28.0f;
        float offset = 5.0f * sinf(gTime * 1.5f + i * 0.7f);
        float len = 60.0f + 20.0f * sinf(gTime + i);
        glBegin(GL_LINES);
        glVertex2f(350.0f + offset, y);
        glVertex2f(350.0f + len + offset, y);
        glEnd();
        glBegin(GL_LINES);
        glVertex2f(500.0f - offset, y + 8.0f);
        glVertex2f(500.0f + len * 0.6f - offset, y + 8.0f);
        glEnd();
    }
}

// ── Display callback ─────────────────────────────────────────
void display()
{
    glClear(GL_COLOR_BUFFER_BIT);

    // ── Background ──
    drawSky();
    drawSun(130.0f, 530.0f, 45.0f);

    // ── Clouds (animated slow drift) ──
    float cloudOff = fmodf(gTime * 8.0f, (float)WIN_W + 200.0f);
    drawCloud(200.0f - cloudOff * 0.15f, 530.0f, 1.0f);
    drawCloud(480.0f + cloudOff * 0.10f, 510.0f, 0.8f);
    drawCloud(700.0f - cloudOff * 0.08f, 545.0f, 1.2f);
    drawCloud(900.0f + cloudOff * 0.12f, 525.0f, 0.9f);

    // ── Land behind water ──
    drawLand();

    // ── Water ──
    drawWater();
    drawReflections();

    // ── Dock ──
    drawDock();

    // ── Boats ──
    // Boat 1: near dock, gentle bob + tilt
    float bob1 = 4.0f * sinf(gTime * 1.1f);
    float tilt1 = 2.5f * sinf(gTime * 0.9f);
    drawBoat(185.0f, WATER_Y - 20.0f, bob1, tilt1);

    // Boat 2: further out, slightly different rhythm
    float bob2 = 5.0f * sinf(gTime * 1.3f + 1.0f);
    float tilt2 = 3.0f * sinf(gTime * 1.0f + 0.5f);
    drawBoat(420.0f, WATER_Y - 20.0f, bob2, tilt2);

    // ── Lighthouse ──
    drawLighthouse();
    drawBeam(gLightAngle);

    // ── Birds ──
    for (int i = 0; i < 4; i++)
        drawBird(gBirds[i].x, gBirds[i].y, gBirds[i].phase);

    glutSwapBuffers();
}

// ── Timer / animation ────────────────────────────────────────
void timer(int /*value*/)
{
    const float dt = 1.0f / 60.0f;  // 60 fps target
    gTime += dt;

    // Rotate lighthouse beam — 1 full revolution every 3 seconds
    gLightAngle += 2.0f;            // degrees per frame at 60fps ≈ 120°/s
    if (gLightAngle >= 360.0f) gLightAngle -= 360.0f;

    // Move birds left to right; wrap around
    for (int i = 0; i < 4; i++) {
        gBirds[i].x += 1.4f + i * 0.2f;
        if (gBirds[i].x > WIN_W + 80.0f)
            gBirds[i].x = -80.0f;
    }

    glutPostRedisplay();
    glutTimerFunc(16, timer, 0);    // ~60 fps
}

// ── Reshape ──────────────────────────────────────────────────
void reshape(int w, int h)
{
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0.0, (GLdouble)WIN_W, 0.0, (GLdouble)WIN_H);
    glMatrixMode(GL_MODELVIEW);
}

// ── Init ─────────────────────────────────────────────────────
void init()
{
    glClearColor(0.20f, 0.48f, 0.80f, 1.0f);  // deep sky blue fallback
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0.0, (GLdouble)WIN_W, 0.0, (GLdouble)WIN_H);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    // Build display lists
    buildBoatList();
    buildCloudList();
}

// ── Main ─────────────────────────────────────────────────────
int main(int argc, char** argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(WIN_W, WIN_H);
    glutInitWindowPosition(100, 80);
    glutCreateWindow("Port Scene — OpenGL CG Project");

    init();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutTimerFunc(16, timer, 0);

    glutMainLoop();
    return 0;
}