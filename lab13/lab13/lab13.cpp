#include <GL/glew.h>
#include <SFML/Window.hpp>
#include <SFML/OpenGL.hpp>
#include <SFML/Graphics/Image.hpp>

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void ShaderLog(GLuint shader)
{
    GLint infologLen = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infologLen);
    if (infologLen > 1)
    {
        std::vector<char> infoLog(infologLen);
        GLsizei charsWritten = 0;
        glGetShaderInfoLog(shader, infologLen, &charsWritten, infoLog.data());
        std::cout << "Shader log:\n" << infoLog.data() << std::endl;
    }
}

void ProgramLog(GLuint prog)
{
    GLint infologLen = 0;
    glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &infologLen);
    if (infologLen > 1)
    {
        std::vector<char> infoLog(infologLen);
        GLsizei charsWritten = 0;
        glGetProgramInfoLog(prog, infologLen, &charsWritten, infoLog.data());
        std::cout << "Program log:\n" << infoLog.data() << std::endl;
    }
}

GLuint CompileShader(GLenum type, const char* src)
{
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, nullptr);
    glCompileShader(sh);
    ShaderLog(sh);
    return sh;
}

GLuint LinkProgram(GLuint vert, GLuint frag)
{
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glLinkProgram(prog);
    GLint success = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if (!success)
        ProgramLog(prog);
    return prog;
}

struct Vec3
{
    float x = 0, y = 0, z = 0;
    Vec3() = default;
    Vec3(float X, float Y, float Z) : x(X), y(Y), z(Z) {}
};

Vec3 operator+(const Vec3& a, const Vec3& b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
Vec3 operator-(const Vec3& a, const Vec3& b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
Vec3 operator*(const Vec3& a, float s) { return { a.x * s, a.y * s, a.z * s }; }

float Dot(const Vec3& a, const Vec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 Cross(const Vec3& a, const Vec3& b)
{
    return Vec3(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    );
}

float Length(const Vec3& v)
{
    return std::sqrt(Dot(v, v));
}

Vec3 Normalize(const Vec3& v)
{
    float len = Length(v);
    if (len <= 1e-6f) return v;
    return v * (1.0f / len);
}

// 4x4 матрица в формате column-major,
// как ожидает OpenGL (m[col*4 + row])
struct Mat4
{
    float m[16] = { 0 };

    static Mat4 Identity()
    {
        Mat4 r;
        r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
        return r;
    }

    static Mat4 Translation(float x, float y, float z)
    {
        Mat4 r = Identity();
        r.m[12] = x;
        r.m[13] = y;
        r.m[14] = z;
        return r;
    }

    static Mat4 Scale(float x, float y, float z)
    {
        Mat4 r;
        r.m[0] = x;
        r.m[5] = y;
        r.m[10] = z;
        r.m[15] = 1.0f;
        return r;
    }

    static Mat4 RotationY(float angleRad)
    {
        Mat4 r = Identity();
        float c = std::cos(angleRad);
        float s = std::sin(angleRad);
        r.m[0] = c;
        r.m[2] = s;
        r.m[8] = -s;
        r.m[10] = c;
        return r;
    }

    static Mat4 Perspective(float fovyRad, float aspect, float zNear, float zFar)
    {
        Mat4 r;
        float tanHalfFovy = std::tan(fovyRad / 2.0f);

        r.m[0] = 1.0f / (aspect * tanHalfFovy);
        r.m[5] = 1.0f / tanHalfFovy;
        r.m[10] = -(zFar + zNear) / (zFar - zNear);
        r.m[11] = -1.0f;
        r.m[14] = -(2.0f * zFar * zNear) / (zFar - zNear);
        return r;
    }

    static Mat4 LookAt(const Vec3& eye, const Vec3& center, const Vec3& up)
    {
        Vec3 f = Normalize(center - eye);
        Vec3 s = Normalize(Cross(f, up));
        Vec3 u = Cross(s, f);

        Mat4 r = Identity();
        r.m[0] = s.x;
        r.m[4] = s.y;
        r.m[8] = s.z;

        r.m[1] = u.x;
        r.m[5] = u.y;
        r.m[9] = u.z;

        r.m[2] = -f.x;
        r.m[6] = -f.y;
        r.m[10] = -f.z;

        r.m[12] = -Dot(s, eye);
        r.m[13] = -Dot(u, eye);
        r.m[14] = Dot(f, eye);
        return r;
    }
};

Mat4 operator*(const Mat4& a, const Mat4& b)
{
    Mat4 r;
    for (int col = 0; col < 4; ++col)
    {
        for (int row = 0; row < 4; ++row)
        {
            r.m[col * 4 + row] =
                a.m[0 * 4 + row] * b.m[col * 4 + 0] +
                a.m[1 * 4 + row] * b.m[col * 4 + 1] +
                a.m[2 * 4 + row] * b.m[col * 4 + 2] +
                a.m[3 * 4 + row] * b.m[col * 4 + 3];
        }
    }
    return r;
}


GLuint LoadTextureFromFile(const std::string& filename)
{
    sf::Image img;
    if (!img.loadFromFile(filename))
    {
        std::cout << "Failed to load texture: " << filename << std::endl;
        return 0;
    }
    img.flipVertically();

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
        img.getSize().x, img.getSize().y,
        0, GL_RGBA, GL_UNSIGNED_BYTE, img.getPixelsPtr());

    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

bool LoadOBJ(const std::string& filename, std::vector<float>& outVertices)
{
    std::ifstream file(filename);
    if (!file.is_open())
    {
        std::cout << "Failed to open OBJ: " << filename << std::endl;
        return false;
    }

    std::vector<Vec3> positions;
    std::vector<sf::Vector2f> texcoords;

    std::string line;
    while (std::getline(file, line))
    {
        if (line.empty()) continue;
        std::istringstream iss(line);
        std::string prefix;
        iss >> prefix;

        if (prefix == "v")
        {
            float x, y, z;
            iss >> x >> y >> z;
            positions.emplace_back(x, y, z);
        }
        else if (prefix == "vt")
        {
            float u, v;
            iss >> u >> v;
            texcoords.emplace_back(u, v);
        }
        else if (prefix == "f")
        {
            // поддержка треугольников и квадов, индексы вида v/vt или v/vt/vn
            std::vector<std::string> tokens;
            std::string token;
            while (iss >> token)
                tokens.push_back(token);

            auto parseIndex = [&](const std::string& s, int& vi, int& ti)
                {
                    vi = 0; ti = 0;
                    size_t firstSlash = s.find('/');
                    if (firstSlash == std::string::npos)
                    {
                        vi = std::stoi(s);
                        return;
                    }
                    std::string vStr = s.substr(0, firstSlash);
                    if (!vStr.empty())
                        vi = std::stoi(vStr);

                    size_t secondSlash = s.find('/', firstSlash + 1);
                    std::string vtStr;
                    if (secondSlash == std::string::npos)
                        vtStr = s.substr(firstSlash + 1);
                    else
                        vtStr = s.substr(firstSlash + 1, secondSlash - firstSlash - 1);

                    if (!vtStr.empty())
                        ti = std::stoi(vtStr);
                };

            auto pushVertex = [&](int vi, int ti)
                {
                    if (vi <= 0 || vi > (int)positions.size())
                        return;
                    Vec3 p = positions[vi - 1];
                    sf::Vector2f t(0.f, 0.f);
                    if (ti > 0 && ti <= (int)texcoords.size())
                        t = texcoords[ti - 1];

                    outVertices.push_back(p.x);
                    outVertices.push_back(p.y);
                    outVertices.push_back(p.z);
                    outVertices.push_back(t.x);
                    outVertices.push_back(t.y);
                };

            if (tokens.size() < 3) continue;

            // triangulation: (0, i-1, i) для i = 2..n-1
            int v0i, v0t;
            parseIndex(tokens[0], v0i, v0t);
            for (size_t i = 1; i + 1 < tokens.size(); ++i)
            {
                int v1i, v1t, v2i, v2t;
                parseIndex(tokens[i], v1i, v1t);
                parseIndex(tokens[i + 1], v2i, v2t);

                pushVertex(v0i, v0t);
                pushVertex(v1i, v1t);
                pushVertex(v2i, v2t);
            }
        }
    }

    if (outVertices.empty())
    {
        std::cout << "OBJ has no vertices: " << filename << std::endl;
        return false;
    }

    std::cout << "OBJ loaded: " << filename
        << ", vertices: " << outVertices.size() / 5 << std::endl;
    return true;
}

struct Mesh
{
    GLuint VAO = 0;
    GLuint VBO = 0;
    GLsizei vertexCount = 0;
};

Mesh CreateMeshFromInterleaved(const std::vector<float>& data)
{
    Mesh m;
    m.vertexCount = static_cast<GLsizei>(data.size() / 5);

    glGenVertexArrays(1, &m.VAO);
    glGenBuffers(1, &m.VBO);

    glBindVertexArray(m.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m.VBO);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_STATIC_DRAW);

    // layout (location=0) vec3 position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // layout (location=1) vec2 texcoord
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return m;
}

const char* vertexShaderSrc = R"(
    #version 330 core
    layout(location = 0) in vec3 aPos;
    layout(location = 1) in vec2 aTex;

    uniform mat4 uModel;
    uniform mat4 uView;
    uniform mat4 uProj;

    out vec2 vTex;

    void main()
    {
        vTex = aTex;
        gl_Position = uProj * uView * uModel * vec4(aPos, 1.0);
    }
)";

const char* fragmentShaderSrc = R"(
    #version 330 core
    in vec2 vTex;
    out vec4 FragColor;

    uniform sampler2D uTexture;

    void main()
    {
        FragColor = texture(uTexture, vTex);
    }
)";

// =======================================================
// ПЛАНЕТЫ
// =======================================================



int main()
{
    setlocale(LC_ALL, "ru_RU.utf8");

    sf::Window window(
        sf::VideoMode({ 1200u, 900u }),
        "OpenGL Solar System (OBJ + camera)",
        sf::Style::Default
    );
    window.setFramerateLimit(60);
    window.setActive(true);

    GLenum err = glewInit();
    if (err != GLEW_OK)
    {
        std::cout << "glewInit failed: "
            << reinterpret_cast<const char*>(glewGetErrorString(err))
            << std::endl;
        return 1;
    }

    std::cout << "OpenGL: " << glGetString(GL_VERSION) << "\n";
    std::cout << "GLSL:   " << glGetString(GL_SHADING_LANGUAGE_VERSION) << "\n";

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // --- шейдерная программа ---
    GLuint vert = CompileShader(GL_VERTEX_SHADER, vertexShaderSrc);
    GLuint frag = CompileShader(GL_FRAGMENT_SHADER, fragmentShaderSrc);
    GLuint prog = LinkProgram(vert, frag);
    glDeleteShader(vert);
    glDeleteShader(frag);

    GLint uModelLoc = glGetUniformLocation(prog, "uModel");
    GLint uViewLoc = glGetUniformLocation(prog, "uView");
    GLint uProjLoc = glGetUniformLocation(prog, "uProj");
    GLint uTexLoc = glGetUniformLocation(prog, "uTexture");

    // --- загрузка OBJ ---
    std::vector<float> modelData;
    if (!LoadOBJ("model.obj", modelData))
        return 1;

    Mesh modelMesh = CreateMeshFromInterleaved(modelData);

    // --- текстура для всех объектов (можно потом добавить разные) ---
    GLuint tex = LoadTextureFromFile("model_diffuse.png");
    if (!tex) return 1;

    // --- камера ---
    Vec3 camPos(0.0f, 3.0f, 12.0f);
    Vec3 worldUp(0.0f, 1.0f, 0.0f);
    float yaw = -90.0f;       // в градусах
    float pitch = -15.0f;

    auto deg2rad = [](float d) { return d * (float)M_PI / 180.0f; };

    auto calcCameraFront = [&]()
        {
            float cy = std::cos(deg2rad(yaw));
            float sy = std::sin(deg2rad(yaw));
            float cp = std::cos(deg2rad(pitch));
            float sp = std::sin(deg2rad(pitch));
            Vec3 front;
            front.x = cy * cp;
            front.y = sp;
            front.z = sy * cp;
            return Normalize(front);
        };

    Vec3 camFront = calcCameraFront();

    // --- матрица проекции (обновится при ресайзе) ---
    auto makeProjection = [&](unsigned w, unsigned h)
        {
            float aspect = (h == 0) ? 1.0f : (float)w / (float)h;
            return Mat4::Perspective(deg2rad(60.0f), aspect, 0.1f, 1000.0f);
        };

    Mat4 proj = makeProjection(window.getSize().x, window.getSize().y);

    struct Planet
    {
        float orbitRadius;    // радиус орбиты
        float orbitSpeed;     // скорость по орбите (рад/сек)
        float selfSpeed;      // скорость вращения вокруг своей оси
        float scale;          // масштаб модели
        float orbitAngle = 0; // текущий угол на орбите
        float selfAngle = 0;  // текущий угол собственного вращения
    };

    // --- планеты (0-я — "Солнце") ---
    srand(static_cast<unsigned>(time(nullptr)));
    auto frand = [](float a, float b)
        {
            return a + (b - a) * (rand() / (float)RAND_MAX);
        };
    int planetCount = 100;
    std::vector<Planet> planets;
    planets.push_back({ 0.0f, 0.0f, 0.2f, 4.0f });   // Солнце — в центре, большое

    for (int i = 0; i < planetCount; i++) {
        float orbitRadius = i/2 + 4.0f;
        float orbitSpeed = frand(0.5f, 1.5f) / orbitRadius;
        float selfSpeed = frand(0.3f, 1.5f); 
        float scale = frand(0.4f, 1.5f);
        float orbitAngle = frand(0.0f, 360.0f);
        float selfAngle = frand(0.0f, 360.0f);

        planets.push_back({
            orbitRadius,
            orbitSpeed,
            selfSpeed,
            scale,
            orbitAngle,
            selfAngle
            });
    }
    
    /*planets.push_back({ 6.0f, 0.4f, 0.7f, 1.0f });
    planets.push_back({ 8.0f, 0.3f, 1.3f, 1.2f });
    planets.push_back({ 10.0f, 0.2f, 0.9f, 0.9f });
    planets.push_back({ 12.0f, 0.15f, 0.5f, 1.4f });*/

    // --- время ---
    sf::Clock clock;

    float cameraSpeed = 7.0f;       // юнитов/сек
    float rotationSpeed = 50.0f;    // град/сек для стрелок

    while (window.isOpen())
    {
        float dt = clock.restart().asSeconds();

        while (auto event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
                window.close();

            if (const auto* resized = event->getIf<sf::Event::Resized>())
            {
                glViewport(0, 0, resized->size.x, resized->size.y);
                proj = makeProjection(resized->size.x, resized->size.y);
            }
        }

        camFront = calcCameraFront();
        Vec3 camRight = Normalize(Cross(camFront, worldUp));

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W))
            camPos = camPos + camFront * (cameraSpeed * dt);
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S))
            camPos = camPos - camFront * (cameraSpeed * dt);
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A))
            camPos = camPos - camRight * (cameraSpeed * dt);
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D))
            camPos = camPos + camRight * (cameraSpeed * dt);
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space))
            camPos = camPos + worldUp * (cameraSpeed * dt);
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift))
            camPos = camPos - worldUp * (cameraSpeed * dt);

        // поворот (стрелочки)
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left))
            yaw -= rotationSpeed * dt;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right))
            yaw += rotationSpeed * dt;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up))
            pitch += rotationSpeed * dt * 0.5f;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down))
            pitch -= rotationSpeed * dt * 0.5f;

        if (pitch > 89.0f) pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;

        camFront = calcCameraFront();
        Mat4 view = Mat4::LookAt(camPos, camPos + camFront, worldUp);

        // =================== ОБНОВЛЕНИЕ ПЛАНЕТ ===================
        for (auto& p : planets)
        {
            p.orbitAngle += p.orbitSpeed * dt;
            p.selfAngle += p.selfSpeed * dt;
        }

        // =================== РЕНДЕР ===================
        glClearColor(0.02f, 0.02f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(prog);
        glUniform1i(uTexLoc, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);

        glUniformMatrix4fv(uViewLoc, 1, GL_FALSE, view.m);
        glUniformMatrix4fv(uProjLoc, 1, GL_FALSE, proj.m);

        glBindVertexArray(modelMesh.VAO);

        for (const auto& p : planets)
        {
            float x = 0.0f;
            float z = 0.0f;
            if (p.orbitRadius > 0.0f)
            {
                x = std::cos(p.orbitAngle) * p.orbitRadius;
                z = std::sin(p.orbitAngle) * p.orbitRadius;
            }

            Mat4 model = Mat4::Translation(x, 0.0f, z) *
                Mat4::RotationY(p.selfAngle) *
                Mat4::Scale(p.scale, p.scale, p.scale);

            glUniformMatrix4fv(uModelLoc, 1, GL_FALSE, model.m);
            glDrawArrays(GL_TRIANGLES, 0, modelMesh.vertexCount);
        }

        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glUseProgram(0);

        window.display();
    }

    glDeleteBuffers(1, &modelMesh.VBO);
    glDeleteVertexArrays(1, &modelMesh.VAO);
    glDeleteTextures(1, &tex);
    glDeleteProgram(prog);

    return 0;
}
