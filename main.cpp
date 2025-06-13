#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <vector>

using namespace std;

struct Target
{
    Vector3 position;
    float radius;
    bool active;
    float speed;
    int currentWaypoint;
    bool stopped;
    int pathIndex;
    float lifeTimer = 0.0f;
    float lifeTimeLimit = 3.0f;
};

struct Missile
{
    Vector3 position;
    Vector3 direction;
    bool active;
    float speed;
    float lifetime;
};

struct Tower
{
    Vector3 startPos;
    Vector3 endPos;
    bool active;
    float turretCooldown;
    float turretRange;
    int upgradeLevel;
};

struct Fence
{
    Vector3 startPos;
    Vector3 endPos;
    bool fenceActive;
    float fenceTimer;
    float fenceContactTimer;
    float fenceContactTimeLimit = 3.0f;
    bool fenceInContact;
};

struct Game
{
    Camera3D camera;
    vector<vector<Vector3>> allWaypoints;
    vector<Target> targets;
    vector<Missile> missiles;
    vector<Tower> towers;
    vector<Fence> fences;
    Texture2D moonSoilTexture;
    Material moonMaterial;
    Model plane;
    int waveNumber = 1;
    int baseEnemiesPerWave = 15;
    int maxEnemies = baseEnemiesPerWave;
    float spawnTimer = 0.0f;
    float spawnDelay = 1.0f;
    int enemiesSpawned = 0;
    float waveDelayTimer = 0.0f;
    bool waveActive = true;
    bool secondPathActive = false;
    int coins = 1000;
    float contactTimer = 0.0f;
    bool inContact = false;
    bool gameOver = false;
    bool pause = false;
    int towerCount = 0;
    int fenceCount = 0;
    bool canUpgrade = false;
    Target target;
};
Game game;

const int screenWidth = 1100;
const int screenHeight = 650;

void InitializeGame(Game &game);
void HandleInput(Game &game);
void UpdateWave(Game &game);
void SpawnEnemies(Game &game);
void UpdateTargets(Game &game);
void UpdateFences(Game &game);
void UpdateTowers(Game &game);
void UpdateMissiles(Game &game);
void UpdateGame(Game &game);
void RenderPath(const vector<Vector3> &waypoints, float pathWidth);
void RenderGame(const Game &game);
void ResetGame(Game &game);

int main()
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_TRANSPARENT);
    InitWindow(screenWidth, screenHeight, "Kingshot 3D");

    InitializeGame(game);

    while (!WindowShouldClose())
    {
        HandleInput(game);
        UpdateGame(game);
        RenderGame(game);

        if (game.gameOver && IsKeyPressed(KEY_R))
        {
            ResetGame(game);
        }
    }

    UnloadTexture(game.moonSoilTexture);
    UnloadMaterial(game.moonMaterial);
    UnloadModel(game.plane);
    CloseWindow();
    return 0;
}

void InitializeGame(Game &game)
{
    SetTargetFPS(60);

    game.camera.position = (Vector3){0.0f, 5.0f, -10.0f};
    game.camera.target = (Vector3){0.0f, 0.0f, 0.0f};
    game.camera.up = (Vector3){0.0f, 1.0f, 0.0f};
    game.camera.fovy = 60.0f;
    game.camera.projection = CAMERA_PERSPECTIVE;

    game.allWaypoints = {{{(Vector3){-15.0f, 0.1f, -10.0f}, (Vector3){-5.0f, 0.1f, 0.0f}, (Vector3){0.0f, 0.1f, 0.0f}}},
                         {{(Vector3){15.0f, 0.1f, -10.0f}, (Vector3){5.0f, 0.1f, 0.0f}, (Vector3){0.0f, 0.1f, 0.0f}}}};

    game.moonSoilTexture = LoadTexture("resources/moon_soil.png");
    game.moonMaterial = LoadMaterialDefault();
    game.moonMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = game.moonSoilTexture;

    game.plane = LoadModelFromMesh(GenMeshPlane(1.0f, 1.0f, 1, 1));
    game.plane.materials[0] = game.moonMaterial;

    DisableCursor();
}

void HandleInput(Game &game)
{
    const float towerCost = 50.0f;
    const float fenceCost = 20.0f;
    const int maxTowers = 4;
    const int maxFences = 4;
    const int maxUpgrades = 3;
    const float missileSpeed = 40.0f;
    const float missileLifetime = 2.0f;

    UpdateCamera(&game.camera, CAMERA_FIRST_PERSON);

    if (IsKeyPressed(KEY_P) && !game.gameOver)
    {
        game.pause = !game.pause;
    }

    if (game.pause || game.gameOver)
        return;

    if (IsKeyPressed(KEY_SPACE))
    {
        Missile missile;
        missile.position = game.camera.position;
        Vector3 forward = Vector3Subtract(game.camera.target, game.camera.position);
        missile.direction = Vector3Normalize(forward);
        missile.active = true;
        missile.speed = missileSpeed;
        missile.lifetime = missileLifetime;
        game.missiles.push_back(missile);
    }

    if (IsKeyPressed(KEY_T) && game.coins >= towerCost)
    {
        if (game.towerCount < maxTowers)
        {
            Tower tower;
            tower.active = true;
            tower.turretCooldown = 0.0f;
            tower.turretRange = 7.0f;
            tower.upgradeLevel = 0;
            Vector3 playerPos = (Vector3){0.0f, 0.1f, 0.0f};
            float baseDistance = 3.0f;
            float distanceIncrease = 1.5f * ((float)game.towerCount / 4);
            float distance = baseDistance + distanceIncrease;
            float towerLength = 4.0f;
            int positionIndex = game.towerCount % 4;
            if (positionIndex == 0)
            {
                tower.startPos = (Vector3){-distance, playerPos.y, -towerLength / 2};
                tower.endPos = (Vector3){-distance, playerPos.y, towerLength / 2};
            }
            else if (positionIndex == 1)
            {
                tower.startPos = (Vector3){distance, playerPos.y, -towerLength / 2};
                tower.endPos = (Vector3){distance, playerPos.y, towerLength / 2};
            }
            else if (positionIndex == 2)
            {
                tower.startPos = (Vector3){-towerLength / 2, playerPos.y, distance};
                tower.endPos = (Vector3){towerLength / 2, playerPos.y, distance};
            }
            else
            {
                tower.startPos = (Vector3){-towerLength / 2, playerPos.y, -distance};
                tower.endPos = (Vector3){towerLength / 2, playerPos.y, -distance};
            }
            game.towers.push_back(tower);
            game.coins -= towerCost;
            game.towerCount++;
        }
        else
        {
            game.canUpgrade = false;
            for (auto &tower : game.towers)
            {
                if (tower.upgradeLevel < maxUpgrades)
                {
                    game.canUpgrade = true;
                    break;
                }
            }
            if (game.canUpgrade)
            {
                for (auto &tower : game.towers)
                {
                    if (tower.upgradeLevel < maxUpgrades)
                    {
                        tower.turretCooldown = max(0.5f, tower.turretCooldown - 0.5f);
                        tower.turretRange += 2.0f;
                        tower.upgradeLevel++;
                        game.coins -= towerCost;
                        break;
                    }
                }
            }
        }
    }

    if (IsKeyPressed(KEY_F) && game.coins >= fenceCost && game.towerCount == maxTowers && game.fenceCount < maxFences)
    // if (IsKeyPressed(KEY_F) && game.coins >= fenceCost && game.fenceCount < maxFences)
    {
        Fence fence;
        fence.fenceActive = true;
        fence.fenceContactTimer = 0.0f;
        fence.fenceInContact = false;
        fence.fenceContactTimeLimit = 3.0f;

        float baseDistance = 3.0f;
        float distanceIncrease = 1.5f * ((float)game.towerCount / 4);
        float distance = baseDistance + distanceIncrease;
        float towerLength = 4.0f;
        int positionIndex = game.fenceCount % 4;

        if (positionIndex == 0)
        {
            fence.startPos = (Vector3){-distance / 1.5f, 0.1f, -towerLength / 2};
            fence.endPos = (Vector3){-distance / 1.5f, 0.1f, towerLength / 2};
        }
        else if (positionIndex == 1)
        {
            fence.startPos = (Vector3){distance / 1.3f, 0.1f, -towerLength / 2};
            fence.endPos = (Vector3){distance / 1.3f, 0.1f, towerLength / 2};
        }
        else if (positionIndex == 2)
        {
            fence.startPos = (Vector3){-towerLength / 2, 0.1f, distance - 0.8f};
            fence.endPos = (Vector3){towerLength / 2, 0.1f, distance - 0.8f};
        }
        else
        {
            fence.startPos = (Vector3){-towerLength / 2, 0.1f, -distance + 0.4f};
            fence.endPos = (Vector3){towerLength / 2, 0.1f, -distance + 0.4f};
        }

        game.fences.push_back(fence);
        game.coins -= fenceCost;
        game.fenceCount++;
    }
}

void UpdateWave(Game &game)
{
    const float waveDelay = 5.0f;

    if (game.waveNumber >= 10 && !game.secondPathActive)
    {
        game.secondPathActive = true;
        game.maxEnemies = game.baseEnemiesPerWave + (game.waveNumber - 1) * 5 + 10;
    }

    if (game.waveActive && game.targets.empty() && game.enemiesSpawned >= game.maxEnemies)
    {
        game.waveActive = false;
        game.waveDelayTimer = 0.0f;
    }

    if (!game.waveActive)
    {
        game.waveDelayTimer += GetFrameTime();
        if (game.waveDelayTimer >= waveDelay)
        {
            game.waveNumber++;
            game.spawnDelay -= 0.1f;
            if (game.spawnDelay <= 0.3f)
            {
                game.spawnDelay = 0.3f;
            }
            game.target.speed += 0.2f;
            if (game.target.speed >= 7.0f)
            {
                game.target.speed = 7.0f;
            }
            game.maxEnemies = game.baseEnemiesPerWave + (game.waveNumber - 1) * 5;
            if (game.secondPathActive)
            {
                game.maxEnemies += 10;
            }
            game.enemiesSpawned = 0;
            game.waveActive = true;
        }
    }
}

void SpawnEnemies(Game &game)
{
    if (!game.waveActive || game.enemiesSpawned >= game.maxEnemies)
    {
        return;
    }

    game.spawnTimer += GetFrameTime();
    if (game.spawnTimer >= game.spawnDelay)
    {
        game.target.radius = 0.5f;
        game.target.active = true;
        game.target.speed = 3.0f;
        game.target.currentWaypoint = 0;
        game.target.stopped = false;
        game.target.lifeTimer = 0.0f;

        if (game.secondPathActive)
        {
            game.target.pathIndex = (game.enemiesSpawned % 2);
            game.target.position = game.allWaypoints[game.target.pathIndex][0];
        }
        else
        {
            game.target.pathIndex = 0;
            game.target.position = game.allWaypoints[0][0];
        }

        game.targets.push_back(game.target);
        game.enemiesSpawned++;
        game.spawnTimer = 0.0f;
    }
}

void UpdateTargets(Game &game)
{
    const float contactTimeLimit = 2.0f;
    game.inContact = false;

    int maxFences = 4;

    for (auto &target : game.targets)
    {
        if (!target.active)
            continue;
        target.stopped = false;

        bool inContactWithFence = false;
        for (auto &fence : game.fences)
        {
            if (fence.fenceActive)
            {
                Vector3 fenceDir = Vector3Subtract(fence.endPos, fence.startPos);
                Vector3 toTarget = Vector3Subtract(target.position, fence.startPos);
                float t = Vector3DotProduct(toTarget, fenceDir) / Vector3DotProduct(fenceDir, fenceDir);
                t = max(0.0f, min(1.0f, t));
                Vector3 closestPoint = Vector3Add(fence.startPos, Vector3Scale(fenceDir, t));
                float fenceWidth = 0.2f;
                float distanceToFence = Vector3Distance(target.position, closestPoint);
                if (distanceToFence < (target.radius + fenceWidth / 2))
                {
                    fence.fenceInContact = true;
                    fence.fenceContactTimer += GetFrameTime();
                    target.stopped = true;
                    inContactWithFence = true;
                    target.lifeTimer += GetFrameTime();
                    if (target.lifeTimer >= target.lifeTimeLimit)
                    {
                        target.active = false;
                        game.coins += 1;
                    }
                    if (fence.fenceContactTimer >= fence.fenceContactTimeLimit)
                    {
                        fence.fenceActive = false;
                    }
                }
            }
        }

        if (!inContactWithFence)
        {
            target.lifeTimer = 0.0f;
            for (auto &fence : game.fences)
            {
                if (fence.fenceActive && !fence.fenceInContact)
                {
                    fence.fenceContactTimer = 0.0f;
                }
            }
        }

        if (target.currentWaypoint < game.allWaypoints[target.pathIndex].size() && !target.stopped)
        {
            Vector3 direction = Vector3Normalize(
                Vector3Subtract(game.allWaypoints[target.pathIndex][target.currentWaypoint], target.position));
            target.position = Vector3Add(target.position, Vector3Scale(direction, target.speed * GetFrameTime()));
            if (Vector3Distance(target.position, game.allWaypoints[target.pathIndex][target.currentWaypoint]) < 0.5f)
            {
                target.currentWaypoint++;
            }
        }

        if (target.currentWaypoint >= game.allWaypoints[target.pathIndex].size() &&
            game.contactTimer >= contactTimeLimit)
        {
            target.active = false;
            game.gameOver = true;
        }

        float distanceToPlayer = Vector3Distance(target.position, (Vector3){0.0f, 0.1f, 0.0f});
        if (distanceToPlayer < (target.radius + 0.25f))
        {
            game.inContact = true;
            game.contactTimer += GetFrameTime();
        }
    }

    if (game.inContact)
    {
        game.contactTimer += GetFrameTime();
        if (game.contactTimer >= contactTimeLimit)
        {
            game.gameOver = true;
            game.contactTimer = contactTimeLimit;
        }
    }
    else
    {
        game.contactTimer = 0.0f;
    }
}

void UpdateFences(Game &game)
{
    for (auto &fence : game.fences)
    {
        if (fence.fenceActive && fence.fenceContactTimer >= fence.fenceContactTimeLimit)
        {
            fence.fenceActive = false;
        }
    }
}

void UpdateTowers(Game &game)
{
    const float missileSpeed = 40.0f;
    const float missileLifetime = 2.0f;
    const float turretCooldownMax = 3.5f;

    for (auto &tower : game.towers)
    {
        if (!tower.active)
            continue;
        tower.turretCooldown -= GetFrameTime();
        if (tower.turretCooldown <= 0.0f)
        {
            for (int turret = 0; turret < 2; turret++)
            {
                Vector3 turretPos = (turret == 0) ? tower.startPos : tower.endPos;
                turretPos.y += 1.0f;
                Target *nearestTarget = nullptr;
                float nearestDist = tower.turretRange;
                for (auto &target : game.targets)
                {
                    if (!target.active)
                        continue;
                    float dist = Vector3Distance(turretPos, target.position);
                    if (dist < nearestDist)
                    {
                        nearestDist = dist;
                        nearestTarget = &target;
                    }
                }
                if (nearestTarget)
                {
                    Missile missile;
                    missile.position = turretPos;
                    missile.direction = Vector3Normalize(Vector3Subtract(nearestTarget->position, turretPos));
                    missile.active = true;
                    missile.speed = missileSpeed;
                    missile.lifetime = missileLifetime;
                    game.missiles.push_back(missile);
                }
            }
            tower.turretCooldown = turretCooldownMax - (tower.upgradeLevel * 0.5f);
        }
    }
}

void UpdateMissiles(Game &game)
{
    for (auto &missile : game.missiles)
    {
        if (!missile.active)
            continue;
        missile.position =
            Vector3Add(missile.position, Vector3Scale(missile.direction, missile.speed * GetFrameTime()));
        missile.lifetime -= GetFrameTime();
        for (auto &target : game.targets)
        {
            if (!target.active)
                continue;
            float distance = Vector3Distance(missile.position, target.position);
            if (distance < (target.radius + 0.1f))
            {
                target.active = false;
                missile.active = false;
                game.coins += 1;
                break;
            }
        }
        if (missile.lifetime <= 0)
        {
            missile.active = false;
        }
    }

    size_t oldFenceCount = game.fences.size();

    game.missiles.erase(
        remove_if(game.missiles.begin(), game.missiles.end(), [](const Missile &m) { return !m.active; }),
        game.missiles.end());
    game.targets.erase(remove_if(game.targets.begin(), game.targets.end(), [](const Target &t) { return !t.active; }),
                       game.targets.end());
    game.fences.erase(remove_if(game.fences.begin(), game.fences.end(), [](const Fence &f) { return !f.fenceActive; }),
                      game.fences.end());

    size_t newFenceCount = game.fences.size();
    game.fenceCount -= (oldFenceCount - newFenceCount);
}

void UpdateGame(Game &game)
{
    if (game.pause || game.gameOver)
        return;

    UpdateWave(game);
    SpawnEnemies(game);
    UpdateTargets(game);
    UpdateFences(game);
    UpdateTowers(game);
    UpdateMissiles(game);
}

void RenderPath(const vector<Vector3> &waypoints, float pathWidth)
{
    for (size_t i = 0; i < waypoints.size() - 1; i++)
    {
        Vector3 start = waypoints[i];
        Vector3 end = waypoints[i + 1];
        start.y = 0.05f;
        end.y = 0.05f;
        Vector3 direction = Vector3Normalize(Vector3Subtract(end, start));
        Vector3 right = Vector3CrossProduct(direction, (Vector3){0.0f, 1.0f, 0.0f});
        right = Vector3Scale(Vector3Normalize(right), pathWidth / 2.0f);
        Vector3 p1 = Vector3Add(start, right);
        Vector3 p2 = Vector3Subtract(start, right);
        Vector3 p3 = Vector3Add(end, right);
        Vector3 p4 = Vector3Subtract(end, right);
        DrawTriangle3D(p1, p3, p2, VIOLET);
        DrawTriangle3D(p2, p3, p4, VIOLET);
    }
}

void RenderGame(const Game &game)
{
    const float crosshairSize = 10.0f;
    const float waveDelay = 5.0f;
    const float contactTimeLimit = 2.0f;
    const int screenWidth = GetScreenWidth();
    const int screenHeight = GetScreenHeight();

    BeginDrawing();
    ClearBackground(Color{0, 0, 0, 0});

    BeginMode3D(game.camera);

    rlPushMatrix();
    rlScalef(50.0f, 1.0f, 50.0f);
    DrawModel(game.plane, (Vector3){0.0f, 0.0f, 0.0f}, 1.0f, WHITE);
    rlPopMatrix();

    float pathWidth = 2.0f;
    RenderPath(game.allWaypoints[0], pathWidth);
    if (game.secondPathActive)
    {
        RenderPath(game.allWaypoints[1], pathWidth);
    }

    for (const auto &target : game.targets)
    {
        if (target.active)
        {
            DrawSphere(target.position, target.radius, RED);
            DrawSphereWires(target.position, target.radius, 10, 10, BLACK);
        }
    }

    for (const auto &missile : game.missiles)
    {
        if (missile.active)
        {
            DrawSphere(missile.position, 0.1f, RED);
        }
    }

    for (const auto &tower : game.towers)
    {
        if (tower.active)
        {
            float towerWidth = 0.5f + (tower.upgradeLevel * 0.05f);
            float towerHeight = 2.0f + (tower.upgradeLevel * 0.1f);
            float towerLength = 0.5f + (tower.upgradeLevel * 0.05f);
            DrawCube(tower.startPos, towerWidth, towerHeight, towerLength, GRAY);
            DrawCube(tower.endPos, towerWidth, towerHeight, towerLength, GRAY);
            Vector3 turretStart = tower.startPos;
            Vector3 turretEnd = tower.endPos;
            turretStart.y += 1.0f;
            turretEnd.y += 1.0f;
            float turretSize = 0.3f + (tower.upgradeLevel * 0.05f);
            DrawSphere(turretStart, turretSize, ORANGE);
            DrawSphere(turretEnd, turretSize, ORANGE);
        }
    }

    for (const auto &fence : game.fences)
    {
        if (fence.fenceActive)
        {
            Vector3 center =
                Vector3Add(fence.startPos, Vector3Scale(Vector3Subtract(fence.endPos, fence.startPos), 0.5f));
            float length = Vector3Distance(fence.startPos, fence.endPos);
            Vector3 direction = Vector3Normalize(Vector3Subtract(fence.endPos, fence.startPos));
            float width = 0.2f;
            float height = 2.0f;
            float fenceLength = (fabs(direction.x) > fabs(direction.z)) ? length : width;
            float fenceWidth = (fabs(direction.x) > fabs(direction.z)) ? width : length;
            DrawCube(center, fenceLength, height, fenceWidth, GRAY);

            if (fence.fenceContactTimer > 0.0f)
            {
                Vector3 lifeSpherePos = {(fence.startPos.x + fence.endPos.x) / 2.0f, fence.startPos.y + 2.0f + 0.5f,
                                         (fence.startPos.z + fence.endPos.z) / 2.0f};
                float fenceLifePercentage = 0.1f - (fence.fenceContactTimer / fence.fenceContactTimeLimit) / 2;

                DrawSphere(lifeSpherePos, 0.3f - fenceLifePercentage, GREEN);
                DrawSphereWires(lifeSpherePos, 0.7f, 10, 10, BLACK);
            }
        }
    }

    DrawCube((Vector3){0.0f, 0.1f, 0.0f}, 0.5f, 0.5f, 0.5f, GREEN);

    EndMode3D();

    Vector2 lifeBarPos = GetWorldToScreen((Vector3){0.0f, 1.0f, 0.0f}, game.camera);
    float lifeBarWidth = 50.0f;
    float lifeBarHeight = 10.0f;
    float lifePercentage = 1.0f - (game.contactTimer / contactTimeLimit);
    DrawRectangle(lifeBarPos.x - lifeBarWidth / 2, lifeBarPos.y - lifeBarHeight / 2, lifeBarWidth * lifePercentage,
                  lifeBarHeight, GREEN);
    DrawRectangleLines(lifeBarPos.x - lifeBarWidth / 2, lifeBarPos.y - lifeBarHeight / 2, lifeBarWidth, lifeBarHeight,
                       BLACK);

    DrawRectangle((float)screenWidth / 2 - crosshairSize / 2, screenHeight / 2 - 2, crosshairSize, 4, BLACK);
    DrawRectangle(screenWidth / 2 - 2, (float)screenHeight / 2 - crosshairSize / 2, 4, crosshairSize, BLACK);
    // // if you want the crosshair outlined:
    // DrawRectangleLines((float)screenWidth / 2 - crosshairSize / 2, screenHeight / 2 - 2, crosshairSize, 4, SKYBLUE);
    // DrawRectangleLines(screenWidth / 2 - 2, (float)screenHeight / 2 - crosshairSize / 2, 4, crosshairSize, SKYBLUE);

    DrawText(TextFormat("Coins: %d", game.coins), 10, 10, 20, WHITE);
    DrawText("Press SPACE to shoot | P to Pause | T to Build Tower (50 coins)", 10, 40, 20, WHITE);
    DrawText("If you have four towers; T to Upgrade tower (50 coins) | F to Build Fence (20 coins)", 10, 70, 20, WHITE);
    DrawText(TextFormat("Enemies Left: %d", game.maxEnemies - game.enemiesSpawned), 10, 130, 20, WHITE);
    DrawText(TextFormat("Wave: %d", game.waveNumber), 10, 190, 20, WHITE);

    const char *moveText = "(You can move with W (forward) | A (left) | S (down) | D (right))";
    int textWidth = MeasureText(moveText, 20);
    int x = (GetScreenWidth() - textWidth) / 2;
    int y = 600;
    DrawText(moveText, x, y, 20, WHITE);

    if (!game.waveActive)
    {
        DrawText(TextFormat("Next Wave In: %.1f", waveDelay - game.waveDelayTimer), 10, 220, 20, WHITE);
    }

    if (game.pause)
    {
        DrawText("Paused", screenWidth / 2 - MeasureText("Paused", 40) / 2, screenHeight / 2 - 20, 40, BLUE);
        DrawText("Press P to Resume", screenWidth / 2 - MeasureText("Press P to Resume", 20) / 2, screenHeight / 2 + 20,
                 20, BLACK);
    }

    if (game.gameOver)
    {
        DrawText("Game Over!", screenWidth / 2 - MeasureText("Game Over!", 40) / 2, screenHeight / 2 - 20, 40, RED);
        DrawText("Press R to Restart", screenWidth / 2 - MeasureText("Press R to Restart", 20) / 2,
                 screenHeight / 2 + 20, 20, BLACK);
    }

    DrawFPS(screenWidth - 90, 10);

    EndDrawing();
}

void ResetGame(Game &game)
{
    game.coins = 1000;
    game.gameOver = false;
    game.pause = false;
    game.contactTimer = 0.0f;
    game.inContact = false;
    game.spawnTimer = 0.0f;
    game.enemiesSpawned = 0;
    game.waveNumber = 1;
    game.maxEnemies = game.baseEnemiesPerWave;
    game.waveActive = true;
    game.waveDelayTimer = 0.0f;
    game.secondPathActive = false;
    game.targets.clear();
    game.missiles.clear();
    game.towers.clear();
    game.fences.clear();
    game.towerCount = 0;
    game.fenceCount = 0;
    game.target.speed = 3.0f;
    game.spawnDelay = 1.0f;
}
