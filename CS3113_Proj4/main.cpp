/**
* Author: Garvin Huang
* Assignment: Rise of the AI
* Date due: 2024-07-27, 11:59pm  
* I pledge that I have completed this assignment without
* collaborating with anyone else, in conformance with the
* NYU School of Engineering Policies and Procedures on
* Academic Misconduct.
**/

#define GL_SILENCE_DEPRECATION
#define STB_IMAGE_IMPLEMENTATION
#define LOG(argument) std::cout << argument << '\n'
#define GL_GLEXT_PROTOTYPES 1
#define FIXED_TIMESTEP 0.0166666f
#define LEVEL1_WIDTH 14
#define LEVEL1_HEIGHT 5

#ifdef _WINDOWS
#include <GL/glew.h>
#endif

#include <SDL_mixer.h>
#include <SDL.h>
#include <SDL_opengl.h>
#include "glm/mat4x4.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "ShaderProgram.h"
#include "stb_image.h"
#include "cmath"
#include <ctime>
#include <vector>
#include "Entity.h"
#include "Map.h"

// ————— GAME STATE ————— //
struct GameState
{
    Entity* player;
    Entity* enemies;
    Map* map;

    Mix_Music* bgm;
    Mix_Chunk* jump_sfx;
};

enum AppStatus { RUNNING, TERMINATED };


// ————— CONSTANTS ————— //
constexpr int WINDOW_WIDTH = 640 * 2,
WINDOW_HEIGHT = 480 * 2;

constexpr float BG_RED = 0.1922f,
BG_BLUE = 0.549f,
BG_GREEN = 0.9059f,
BG_OPACITY = 1.0f;

constexpr int VIEWPORT_X = 0,
VIEWPORT_Y = 0,
VIEWPORT_WIDTH = WINDOW_WIDTH,
VIEWPORT_HEIGHT = WINDOW_HEIGHT;

constexpr char GAME_WINDOW_NAME[] = "Hello, Maps!";

constexpr char V_SHADER_PATH[] = "shaders/vertex_textured.glsl",
F_SHADER_PATH[] = "shaders/fragment_textured.glsl";

constexpr float MILLISECONDS_IN_SECOND = 1000.0;

constexpr char PLAYER_SPRITESHEET_FILEPATH[] = "assets/player.png",
ENEMY_FILEPATH[] = "assets/goomba.png",
BULLET_FILEPATH[] = "assets/circle.png",
MAP_TILESET_FILEPATH[] = "assets/tileset2.png",
BGM_FILEPATH[] = "assets/Who Likes to Party.mp3",
FONT_FILEPATH[] = "assets/font1.png",
JUMP_SFX_FILEPATH[] = "assets/click.wav";

constexpr int NUMBER_OF_TEXTURES = 1;
constexpr GLint LEVEL_OF_DETAIL = 0;
constexpr GLint TEXTURE_BORDER = 0;

constexpr int FONTBANK_SIZE = 16;

constexpr int ENEMY_COUNT = 4;
constexpr int BULLET_COUNT = 1;

unsigned int LEVEL_1_DATA[] =
{
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    2, 1, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 1, 2,
    2, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2,
    2, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 2,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
};

// ————— VARIABLES ————— //
GameState g_game_state;

SDL_Window* g_display_window;
AppStatus g_app_status = RUNNING;
ShaderProgram g_shader_program;
glm::mat4 g_view_matrix, g_projection_matrix;

float g_previous_ticks = 0.0f,
g_accumulator = 0.0f;

int g_enemies_killed = 0;
bool g_game_over = false;

GLuint g_font_texture_id;
GLuint g_bullet_texture_id;

void initialise();
void process_input();
void update();
void render();
void shutdown();
void draw_text(ShaderProgram* program, GLuint font_texture_id, std::string text,
    float font_size, float spacing, glm::vec3 position);


void draw_text(ShaderProgram* program, GLuint font_texture_id, std::string text,
    float font_size, float spacing, glm::vec3 position)
{
    // Scale the size of the fontbank in the UV-plane
    // We will use this for spacing and positioning
    float width = 1.0f / FONTBANK_SIZE;
    float height = 1.0f / FONTBANK_SIZE;

    // Instead of having a single pair of arrays, we'll have a series of pairs—one for
    // each character. Don't forget to include <vector>!
    std::vector<float> vertices;
    std::vector<float> texture_coordinates;

    // For every character...
    for (int i = 0; i < text.size(); i++) {
        // 1. Get their index in the spritesheet, as well as their offset (i.e. their
        //    position relative to the whole sentence)
        int spritesheet_index = (int)text[i];  // ascii value of character
        float offset = (font_size + spacing) * i;

        // 2. Using the spritesheet index, we can calculate our U- and V-coordinates
        float u_coordinate = (float)(spritesheet_index % FONTBANK_SIZE) / FONTBANK_SIZE;
        float v_coordinate = (float)(spritesheet_index / FONTBANK_SIZE) / FONTBANK_SIZE;

        // 3. Inset the current pair in both vectors
        vertices.insert(vertices.end(), {
            offset + (-0.5f * font_size), 0.5f * font_size,
            offset + (-0.5f * font_size), -0.5f * font_size,
            offset + (0.5f * font_size), 0.5f * font_size,
            offset + (0.5f * font_size), -0.5f * font_size,
            offset + (0.5f * font_size), 0.5f * font_size,
            offset + (-0.5f * font_size), -0.5f * font_size,
            });

        texture_coordinates.insert(texture_coordinates.end(), {
            u_coordinate, v_coordinate,
            u_coordinate, v_coordinate + height,
            u_coordinate + width, v_coordinate,
            u_coordinate + width, v_coordinate + height,
            u_coordinate + width, v_coordinate,
            u_coordinate, v_coordinate + height,
            });
    }

    // 4. And render all of them using the pairs
    glm::mat4 model_matrix = glm::mat4(1.0f);
    model_matrix = glm::translate(model_matrix, position);

    program->set_model_matrix(model_matrix);
    glUseProgram(program->get_program_id());

    glVertexAttribPointer(program->get_position_attribute(), 2, GL_FLOAT, false, 0,
        vertices.data());
    glEnableVertexAttribArray(program->get_position_attribute());
    glVertexAttribPointer(program->get_tex_coordinate_attribute(), 2, GL_FLOAT, false, 0,
        texture_coordinates.data());
    glEnableVertexAttribArray(program->get_tex_coordinate_attribute());

    glBindTexture(GL_TEXTURE_2D, font_texture_id);
    glDrawArrays(GL_TRIANGLES, 0, (int)(text.size() * 6));

    glDisableVertexAttribArray(program->get_position_attribute());
    glDisableVertexAttribArray(program->get_tex_coordinate_attribute());
}



Entity init_enemy(GLuint texture_id);

// ————— GENERAL FUNCTIONS ————— //
GLuint load_texture(const char* filepath)
{
    int width, height, number_of_components;
    unsigned char* image = stbi_load(filepath, &width, &height, &number_of_components, STBI_rgb_alpha);

    if (image == NULL)
    {
        LOG("Unable to load image. Make sure the path is correct.");
        assert(false);
    }

    GLuint texture_id;
    glGenTextures(NUMBER_OF_TEXTURES, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexImage2D(GL_TEXTURE_2D, LEVEL_OF_DETAIL, GL_RGBA, width, height, TEXTURE_BORDER, GL_RGBA, GL_UNSIGNED_BYTE, image);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    stbi_image_free(image);

    return texture_id;
}


Entity init_enemy(GLuint texture_id)
{
    return Entity(
        texture_id,         // texture id
        2.5f,
        0.75f,
        0.75f,
        ENEMY,
        GUARD,
        IDLE

    );
}


void initialise()
{
    // ————— GENERAL ————— //
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    g_display_window = SDL_CreateWindow(GAME_WINDOW_NAME,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_OPENGL);

    SDL_GLContext context = SDL_GL_CreateContext(g_display_window);
    SDL_GL_MakeCurrent(g_display_window, context);
    if (context == nullptr)
    {
        LOG("ERROR: Could not create OpenGL context.\n");
        shutdown();
    }

#ifdef _WINDOWS
    glewInit();
#endif

    // ————— VIDEO SETUP ————— //
    glViewport(VIEWPORT_X, VIEWPORT_Y, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);

    g_shader_program.load(V_SHADER_PATH, F_SHADER_PATH);

    g_view_matrix = glm::mat4(1.0f);
    g_projection_matrix = glm::ortho(-5.0f, 5.0f, -3.75f, 3.75f, -1.0f, 1.0f);

    g_shader_program.set_projection_matrix(g_projection_matrix);
    g_shader_program.set_view_matrix(g_view_matrix);

    glUseProgram(g_shader_program.get_program_id());

    glClearColor(BG_RED, BG_BLUE, BG_GREEN, BG_OPACITY);

    // ————— MAP SET-UP ————— //
    GLuint map_texture_id = load_texture(MAP_TILESET_FILEPATH);
    g_game_state.map = new Map(LEVEL1_WIDTH, LEVEL1_HEIGHT, LEVEL_1_DATA, map_texture_id, 1.0f, 6, 6);

    // ————— PlAYER SET-UP ————— //

    GLuint player_texture_id = load_texture(PLAYER_SPRITESHEET_FILEPATH);

    int player_walking_animation[4][4] =
    {

        { 8, 11, 13, 15 }, //left 
        { 0, 3, 5, 7 },  // right
        { 2, 6, 10, 14 },
        { 0, 4, 8, 12 }
    };

    glm::vec3 acceleration = glm::vec3(0.0f, -4.905f, 0.0f);

    g_game_state.player = new Entity(
        player_texture_id,         // texture id
        2.25f,                      // speed
        acceleration,              // acceleration
        3.5f,                      // jumping power
        player_walking_animation,  // animation index sets
        0.0f,                      // animation time
        4,                         // animation frame amount
        0,                         // current animation index
        8,                         // animation column amount
        4,                        // animation row amount
        0.35f,                      // width
        0.35f,                       // height
        PLAYER
    );
 
    g_game_state.player->set_position(glm::vec3(7.0f, 0.0f, 0.0f));

    // ----- ENEMY SETUP ----- //
    GLuint enemy_texture_id = load_texture(ENEMY_FILEPATH);
    g_bullet_texture_id = load_texture(BULLET_FILEPATH);

    g_game_state.enemies = new Entity[ENEMY_COUNT];
    for (int i = 0; i < ENEMY_COUNT; ++i) {
        g_game_state.enemies[i] = init_enemy(enemy_texture_id);
        g_game_state.enemies[i].set_position(glm::vec3(2.0f, 0.0f, 0.0f));
        g_game_state.enemies[i].set_acceleration(glm::vec3(0.0f, -4.91f, 0.0f));
        g_game_state.enemies[i].set_movement(glm::vec3(0.0f, 0.0f, 0.0f));
        g_game_state.enemies[i].set_velocity(glm::vec3(0.0f, 0.0f, 0.0f));
        //g_game_state.enemies[i].set_scale(glm::vec3(0.15f, 0.15f, 0.0f));
        g_game_state.enemies[i].set_ai_state(IDLE);

    }

    g_game_state.enemies[0].set_ai_type(BULLET);
    g_game_state.enemies[0].set_texture_id(g_bullet_texture_id);
    //g_game_state.enemies[0].set_position(glm::vec3(4.0f, 0.0f, 0.0f));
    g_game_state.enemies[0].set_init_pos(g_game_state.enemies[0].get_position());
    g_game_state.enemies[0].deactivate();
    g_game_state.enemies[0].set_scale(glm::vec3(0.55f, 0.55f, 0.0f));
    g_game_state.enemies[0].set_width(0.15f);
    g_game_state.enemies[0].set_height(0.15f);
    g_game_state.enemies[0].set_acceleration(glm::vec3(0.0f, 0.0f, 0.0f));
   

    g_game_state.enemies[1].set_ai_type(SHOOTER);
    g_game_state.enemies[1].set_position(glm::vec3(1.0f, 0.0f, 0.0f));
    g_game_state.enemies[1].set_bullet(&g_game_state.enemies[0]);  
    g_game_state.enemies[1].set_width(0.55f);
    g_game_state.enemies[1].set_height(0.55f);
    g_game_state.enemies[1].set_scale(glm::vec3(0.55f, 0.55f, 0.0f));
    g_game_state.enemies[1].set_acceleration(glm::vec3(0.0f, 0.0f, 0.0f));
    

    g_game_state.enemies[2].set_ai_type(STALKER);   
    g_game_state.enemies[2].set_jumping_power(1.0f);

    g_game_state.enemies[3].set_ai_type(GUARD);
    g_game_state.enemies[3].set_width(0.5f);
    g_game_state.enemies[3].set_height(0.5f);
    g_game_state.enemies[3].set_jumping_power(1.0f);


    // ----- SOUND ----- //


    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096);

    g_game_state.bgm = Mix_LoadMUS(BGM_FILEPATH);
    Mix_PlayMusic(g_game_state.bgm, -1);
    Mix_VolumeMusic(MIX_MAX_VOLUME / 16.0f);

    g_game_state.jump_sfx = Mix_LoadWAV(JUMP_SFX_FILEPATH);

    // ----- FONT ----- //
    g_font_texture_id = load_texture(FONT_FILEPATH);
    // ————— BLENDING ————— //
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void process_input()
{
    g_game_state.player->set_movement(glm::vec3(0.0f));

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type) {
        case SDL_QUIT:
        case SDL_WINDOWEVENT_CLOSE:
            g_app_status = TERMINATED;
            break;

        case SDL_KEYDOWN:
            switch (event.key.keysym.sym) {
            case SDLK_q:
                // Quit the game with a keystroke
                g_app_status = TERMINATED;
                break;

            case SDLK_SPACE:
                // Jump
                if (g_game_state.player->get_collided_bottom())
                {
                    g_game_state.player->jump();
                    Mix_PlayChannel(-1, g_game_state.jump_sfx, 0);
                }
                break;
                
            default:
                break;
            }

        default:
            break;
        }
    }

    const Uint8* key_state = SDL_GetKeyboardState(NULL);

    if (key_state[SDL_SCANCODE_LEFT])       g_game_state.player->move_left();
    else if (key_state[SDL_SCANCODE_RIGHT]) g_game_state.player->move_right();

    if (glm::length(g_game_state.player->get_movement()) > 1.0f)
        g_game_state.player->normalise_movement();
}

void update()
{
    float ticks = (float)SDL_GetTicks() / MILLISECONDS_IN_SECOND;
    float delta_time = ticks - g_previous_ticks;
    g_previous_ticks = ticks;

    delta_time += g_accumulator;
    g_enemies_killed = 0;
    if (delta_time < FIXED_TIMESTEP)
    {
        g_accumulator = delta_time;
        return;
    }

    while (delta_time >= FIXED_TIMESTEP)
    {
        g_game_state.player->update(FIXED_TIMESTEP, g_game_state.player, g_game_state.enemies, ENEMY_COUNT
            , g_game_state.map);
        
        for (int i = 0; i < ENEMY_COUNT; ++i) {

            g_game_state.enemies[i].update(FIXED_TIMESTEP, g_game_state.player, g_game_state.player, 1,
                g_game_state.map);
            
                
            if (!g_game_state.enemies[i].get_active() 
                && g_game_state.enemies[i].get_ai_type() != BULLET) ++g_enemies_killed;
            if (g_enemies_killed == ENEMY_COUNT - BULLET_COUNT ) { g_game_over = true; }
            
           
        }
        if (!g_game_state.player->get_active()) { g_game_over = true; }
        
        delta_time -= FIXED_TIMESTEP;
    }

    g_accumulator = delta_time;

    g_view_matrix = glm::mat4(1.0f);
    g_view_matrix = glm::translate(g_view_matrix, glm::vec3(-g_game_state.player->get_position().x, -g_game_state.player->get_position().y / 2, 0.0f));
}

void render()
{
    g_shader_program.set_view_matrix(g_view_matrix);

    glClear(GL_COLOR_BUFFER_BIT);

    g_game_state.player->render(&g_shader_program);
    for (int i = 0; i < ENEMY_COUNT; ++i) {
        g_game_state.enemies[i].render(&g_shader_program);
    }
    g_game_state.map->render(&g_shader_program);
    if (g_game_over) {
        if (g_enemies_killed == ENEMY_COUNT - BULLET_COUNT) {
            draw_text(&g_shader_program, g_font_texture_id,
                "YOU WIN!", 0.5f, 0.05f, glm::vec3(g_game_state.player->get_position().x - 2.0f,
                    g_game_state.player->get_position().y + 2.0f,
                    0.0f));
        }
        else if (g_game_state.player->get_active() == false) {
            draw_text(&g_shader_program, g_font_texture_id,
                "YOU LOSE", 0.5f, 0.05f, glm::vec3(g_game_state.player->get_position().x - 2.0f,
                    g_game_state.player->get_position().y + 2.0f,
                    0.0f));
        }
    }
    
    SDL_GL_SwapWindow(g_display_window);
}

void shutdown()
{
    SDL_Quit();

    delete[] g_game_state.enemies;
    delete    g_game_state.player;
    delete    g_game_state.map;
    Mix_FreeChunk(g_game_state.jump_sfx);
    Mix_FreeMusic(g_game_state.bgm);
}

// ————— GAME LOOP ————— //
int main(int argc, char* argv[])
{
    initialise();

    while (g_app_status == RUNNING)
    {
        process_input();
        if (!g_game_over) {
            update();
            render();
        }
            
        
            
  
       
    }

    shutdown();
    return 0;
}