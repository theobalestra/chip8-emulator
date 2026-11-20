#include <SDL3/SDL.h>
#include <errno.h>
#include <iso646.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TIMER_HZ 60
#define CPU_HZ 700

#define MEMORY_FONT_START 0x50
#define MEMORY_PROGRAM_START 0x200

#define NUM_PIXELS_WIDTH 64
#define NUM_PIXELS_HEIGHT 32
#define NUM_TOTAL_PIXELS (NUM_PIXELS_WIDTH * NUM_PIXELS_HEIGHT)

#define KEY_0 0x0
#define KEY_1 0x1
#define KEY_2 0x2
#define KEY_3 0x3
#define KEY_4 0x4
#define KEY_5 0x5
#define KEY_6 0x6
#define KEY_7 0x7
#define KEY_8 0x8
#define KEY_9 0x9
#define KEY_A 0xA
#define KEY_B 0xB
#define KEY_C 0xC
#define KEY_D 0xD
#define KEY_E 0xE
#define KEY_F 0xF

#define COSMAC_MODE false

static int WINDOW_WIDTH = NUM_PIXELS_WIDTH * 10,
           WINDOW_HEIGHT = NUM_PIXELS_HEIGHT * 10;

typedef struct {
  uint8_t *data;
  uint16_t size;
} Memory;

Memory *create_memory(uint16_t size) {
  Memory *mem = malloc(sizeof(Memory));
  mem->data = malloc(size * sizeof(uint8_t));
  mem->size = size;

  return mem;
}

bool fits_in_bounds(Memory *mem, uint16_t pos, size_t size) {
  if (pos + size > mem->size) {
    return false;
  }

  return true;
}

bool load_into_memory(Memory *mem, uint16_t pos, size_t size,
                      const uint8_t *data) {
  if (pos + size > mem->size) {
    return false;
  }

  memcpy(mem->data + pos, data, size);
  return true;
}

bool read_memory(Memory *mem, uint16_t pos, uint8_t *out) {
  if (pos >= mem->size) {
    return false;
  }

  *out = mem->data[pos];
  return true;
}

uint8_t *memory_address_at(Memory *mem, uint16_t pos) {
  if (pos >= mem->size) {
    return NULL;
  }

  return &(mem->data[pos]);
}

void destroy_memory(Memory *mem) {
  free(mem->data);
  free(mem);
}

typedef struct {
  uint8_t data[256];
  uint8_t width;
  uint8_t height;
  bool dirty;
} Display;

void clear_display(Display *display) {
  display->dirty = true;
  for (int i = 0; i < 256; i++) {
    display->data[i] = 0x00;
  }
}

Display *create_display() {
  Display *display = malloc(sizeof(Display));
  display->width = NUM_PIXELS_WIDTH;
  display->height = NUM_PIXELS_HEIGHT;
  clear_display(display);
  return display;
}

/**
 * Returns whether or not the pixel end state is on or not
 */
bool flip_on_display(Display *display, uint8_t w, uint8_t h) {
  if (w < 0 || w >= display->width || h < 0 || h >= display->height)
    return false;

  int pixel_number = h * display->width + w;
  int data_block = pixel_number >> 3;
  uint8_t mask = 0b1 << (pixel_number & 0b00000111);
  int cur_value = display->data[data_block] & mask;

  display->dirty = true;

  if (cur_value > 0) {
    display->data[data_block] = display->data[data_block] & ~mask;
    return false;
  }

  display->data[data_block] = display->data[data_block] | mask;
  return true;
}

void set_on_display(Display *display, int w, int h, bool on) {
  if (w < 0 || w >= display->width || h < 0 || h >= display->height)
    return;

  int pixel_number = h * display->width + w;
  int data_block = pixel_number >> 3;
  uint8_t mask = 0b1 << (pixel_number & 0b00000111);
  int cur_value = display->data[data_block] & mask;
  if (!on && cur_value > 0) {
    display->data[data_block] = display->data[data_block] & ~mask;
    display->dirty = true;
    return;
  }

  if (on && cur_value == 0) {
    display->data[data_block] = display->data[data_block] | mask;
    display->dirty = true;
  }

  return;
}

bool get_on_display(Display *display, int w, int h) {
  if (w < 0 || w >= display->width || h < 0 || h >= display->height)
    return false;

  int pixel_number = h * display->width + w;
  int data_block = pixel_number >> 3;
  uint8_t mask = 0b1 << (pixel_number & 0b00000111);

  return display->data[data_block] & mask;
}

void destroy_display(Display *display) { free(display); }

typedef enum {
  ERR_OK = 0,
  ERR_STACK_OVERFLOW = 1,
  ERR_STACK_UNDERFLOW = 2,
} StackError;

typedef struct {
  uint16_t values[16];
  uint8_t top;
  uint8_t capacity;
} Stack;

Stack *create_stack() {
  Stack *stack = malloc(sizeof(Stack));
  stack->top = 0;
  stack->capacity = 16;

  return stack;
}

StackError push_stack(Stack *stack, uint16_t value) {
  if (stack->top == stack->capacity)
    return ERR_STACK_OVERFLOW;

  stack->values[stack->top] = value;
  stack->top++;
  return ERR_OK;
}

StackError pop_stack(Stack *stack, uint16_t *out) {
  if (stack->top == 0)
    return ERR_STACK_UNDERFLOW;

  stack->top--;
  *out = stack->values[stack->top];
  return ERR_OK;
}

void destroy_stack(Stack *stack) { free(stack); }

typedef struct {
  uint64_t last_time_ms;
  double accumulator_ms;
  double hz_period_ms;
} Clock;

Clock *create_clock(uint16_t hz, uint64_t start_time_ms) {
  Clock *clock = malloc(sizeof(Clock));
  clock->last_time_ms = start_time_ms;
  clock->accumulator_ms = 0.0;
  clock->hz_period_ms = (1.0 / hz) * 1000;

  return clock;
}

uint8_t get_clock_ticks(Clock *clock, uint64_t total_time) {
  clock->accumulator_ms += total_time - clock->last_time_ms;
  clock->last_time_ms = total_time;

  uint8_t ticks = 0;
  while (clock->accumulator_ms >= clock->hz_period_ms) {
    ticks++;
    clock->accumulator_ms -= clock->hz_period_ms;
  }

  return ticks;
}

void destroy_clock(Clock *clock) { free(clock); }

typedef bool KeyboardState[16];

typedef struct {
  Memory *memory;
  Display *display;
  Stack *stack;

  uint16_t index_register;
  uint8_t variable_register[16];

  uint16_t program_counter;
  uint8_t delay_timer;
  uint8_t sound_timer;

  KeyboardState key_states;
} Chip8State;

Chip8State *create_chip8_state(int memory_size) {
  Chip8State *state = malloc(sizeof(Chip8State));
  state->memory = create_memory(memory_size);
  state->display = create_display();
  state->stack = create_stack();
  state->program_counter = MEMORY_PROGRAM_START;
  state->delay_timer = 0;
  state->sound_timer = 0;

  return state;
}

void advance_chip8_timers(Chip8State *state, uint8_t ticks) {
  if (ticks == 0)
    return;

  if (ticks > state->delay_timer)
    state->delay_timer = 0;
  else
    state->delay_timer -= ticks;

  if (ticks > state->sound_timer)
    state->sound_timer = 0;
  else
    state->sound_timer -= ticks;
}

void destroy_chip8_state(Chip8State *state) {
  destroy_display(state->display);
  destroy_memory(state->memory);
  destroy_stack(state->stack);

  free(state);
}

typedef struct {
  bool running;
  Clock *timer_clock;
  Clock *cpu_clock;
} ApplicationState;

ApplicationState *create_application_state(uint64_t current_time_ms) {
  ApplicationState *state = malloc(sizeof(ApplicationState));
  state->running = true;
  state->timer_clock = create_clock(TIMER_HZ, current_time_ms);
  state->cpu_clock = create_clock(CPU_HZ, current_time_ms);

  return state;
}

void handle_key_event(ApplicationState *application_state,
                      Chip8State *chip8_state, SDL_KeyboardEvent *event) {
  if (event->down && event->scancode == SDL_SCANCODE_ESCAPE) {
    application_state->running = false;
  } else if (event->scancode == SDL_SCANCODE_1) {
    chip8_state->key_states[KEY_1] = event->down;
  } else if (event->scancode == SDL_SCANCODE_2) {
    chip8_state->key_states[KEY_2] = event->down;
  } else if (event->scancode == SDL_SCANCODE_3) {
    chip8_state->key_states[KEY_3] = event->down;
  } else if (event->scancode == SDL_SCANCODE_4) {
    chip8_state->key_states[KEY_C] = event->down;
  } else if (event->scancode == SDL_SCANCODE_Q) {
    chip8_state->key_states[KEY_4] = event->down;
  } else if (event->scancode == SDL_SCANCODE_W) {
    chip8_state->key_states[KEY_5] = event->down;
  } else if (event->scancode == SDL_SCANCODE_E) {
    chip8_state->key_states[KEY_6] = event->down;
  } else if (event->scancode == SDL_SCANCODE_R) {
    chip8_state->key_states[KEY_D] = event->down;
  } else if (event->scancode == SDL_SCANCODE_A) {
    chip8_state->key_states[KEY_7] = event->down;
  } else if (event->scancode == SDL_SCANCODE_S) {
    chip8_state->key_states[KEY_8] = event->down;
  } else if (event->scancode == SDL_SCANCODE_D) {
    chip8_state->key_states[KEY_9] = event->down;
  } else if (event->scancode == SDL_SCANCODE_F) {
    chip8_state->key_states[KEY_E] = event->down;
  } else if (event->scancode == SDL_SCANCODE_Z) {
    chip8_state->key_states[KEY_A] = event->down;
  } else if (event->scancode == SDL_SCANCODE_X) {
    chip8_state->key_states[KEY_0] = event->down;
  } else if (event->scancode == SDL_SCANCODE_C) {
    chip8_state->key_states[KEY_B] = event->down;
  } else if (event->scancode == SDL_SCANCODE_V) {
    chip8_state->key_states[KEY_F] = event->down;
  }
}

void handle_sdl_events(ApplicationState *application_state,
                       Chip8State *chip8_state) {
  SDL_Event event;

  while (SDL_PollEvent(&event)) {
    switch (event.type) {
    case SDL_EVENT_QUIT:
      application_state->running = false;
      break;
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
      handle_key_event(application_state, chip8_state, &(event.key));
      break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
      for (int i = 0; i < 16; i++) {
        chip8_state->key_states[i] = false;
      }
      break;
    }
  }
}

typedef enum {
  ERR_PROCESSED = 0,
  ERR_COULD_NOT_READ = 1,
  ERR_COULD_NOT_PROCESS = 2,
} ProcessInstructionError;

ProcessInstructionError process_instructions(Chip8State *chip8_state,
                                             uint8_t instruction_count) {

  for (uint8_t i = 0; i < instruction_count; i++) {
    uint8_t high;
    uint8_t low;
    bool high_success =
        read_memory(chip8_state->memory, chip8_state->program_counter, &high);
    bool low_success = read_memory(chip8_state->memory,
                                   chip8_state->program_counter + 1, &low);
    if (!high_success || !low_success) {
      return ERR_COULD_NOT_READ;
    }

    chip8_state->program_counter += 2;

    uint16_t opcode = (uint16_t)high << 8 | low;
    switch (opcode >> 12) {
    case 0x0:
      if (opcode == 0x00E0) {
        clear_display(chip8_state->display);
      }

      if (opcode == 0x00EE) {
        uint16_t new_program_counter;
        StackError pop_result =
            pop_stack(chip8_state->stack, &new_program_counter);
        if (pop_result != ERR_OK) {
          fprintf(stderr, "ERR: Stack Underflow\n");
          return ERR_COULD_NOT_PROCESS;
        }

        chip8_state->program_counter = new_program_counter;
      }
      break;
    case 0x1:
      chip8_state->program_counter = opcode & 0x0FFF;
      break;
    case 0x2:
      StackError push_result =
          push_stack(chip8_state->stack, chip8_state->program_counter);
      if (push_result != ERR_OK) {
        fprintf(stderr, "ERR: Stack Overflow\n");
        return ERR_COULD_NOT_PROCESS;
      }
      chip8_state->program_counter = opcode & 0x0FFF;
      break;
    case 0x3:
      if (chip8_state->variable_register[(opcode & 0x0F00) >> 8] ==
          (opcode & 0x00FF)) {
        chip8_state->program_counter += 2;
      }
      break;
    case 0x4:
      if (chip8_state->variable_register[(opcode & 0x0F00) >> 8] !=
          (opcode & 0x00FF)) {
        chip8_state->program_counter += 2;
      }
      break;
    case 0x5:
      if ((opcode & 0x000F) == 0 &&
          chip8_state->variable_register[(opcode & 0x0F00) >> 8] ==
              chip8_state->variable_register[(opcode & 0x00F0) >> 4]) {
        chip8_state->program_counter += 2;
      }
      break;
    case 0x6:
      chip8_state->variable_register[(opcode & 0x0F00) >> 8] = opcode & 0x00FF;
      break;
    case 0x7:
      chip8_state->variable_register[(opcode & 0x0F00) >> 8] += opcode & 0x00FF;
      break;
    case 0x8:
      switch (opcode & 0x000F) {
      case 0x0:
        chip8_state->variable_register[(opcode & 0x0F00) >> 8] =
            chip8_state->variable_register[(opcode & 0x00F0) >> 4];

        break;
      case 0x1:
        chip8_state->variable_register[(opcode & 0x0F00) >> 8] |=
            chip8_state->variable_register[(opcode & 0x00F0) >> 4];

        if (COSMAC_MODE) {
          chip8_state->variable_register[0xF] = 0;
        }
        break;
      case 0x2:
        chip8_state->variable_register[(opcode & 0x0F00) >> 8] &=
            chip8_state->variable_register[(opcode & 0x00F0) >> 4];

        if (COSMAC_MODE) {
          chip8_state->variable_register[0xF] = 0;
        }
        break;
      case 0x3:
        chip8_state->variable_register[(opcode & 0x0F00) >> 8] ^=
            chip8_state->variable_register[(opcode & 0x00F0) >> 4];

        if (COSMAC_MODE) {
          chip8_state->variable_register[0xF] = 0;
        }
        break;
      case 0x4:
        uint8_t add_variable_register = (opcode & 0x0F00) >> 8;
        uint8_t old_add_value =
            chip8_state->variable_register[add_variable_register];
        uint8_t new_add_value =
            chip8_state->variable_register[add_variable_register] +
            chip8_state->variable_register[(opcode & 0x00F0) >> 4];
        chip8_state->variable_register[add_variable_register] = new_add_value;

        if (old_add_value > new_add_value) {
          chip8_state->variable_register[0xF] = 1;
          break;
        }

        chip8_state->variable_register[0xF] = 0;
        break;
      case 0x5: {
        uint8_t sub_variable_register = (opcode & 0x0F00) >> 8;
        uint8_t old_sub_value =
            chip8_state->variable_register[sub_variable_register];

        chip8_state->variable_register[sub_variable_register] -=
            chip8_state->variable_register[(opcode & 0x00F0) >> 4];
        if (old_sub_value <
            chip8_state->variable_register[sub_variable_register]) {
          chip8_state->variable_register[0xF] = 0;
          break;
        }

        chip8_state->variable_register[0xF] = 1;
        break;
      }
      case 0x6:
        uint8_t shift_right_register = (opcode & 0x0F00) >> 8;

        if (COSMAC_MODE) {
          chip8_state->variable_register[shift_right_register] =
              chip8_state->variable_register[(opcode & 0x00F0) >> 4];
        }

        chip8_state->variable_register[0xF] =
            chip8_state->variable_register[shift_right_register] & 0b1;
        chip8_state->variable_register[shift_right_register] >>= 1;
        break;
      case 0x7: {
        uint8_t sub_variable_register = (opcode & 0x0F00) >> 8;
        uint8_t v_x = chip8_state->variable_register[sub_variable_register];
        uint8_t v_y = chip8_state->variable_register[(opcode & 0x00F0) >> 4];
        chip8_state->variable_register[sub_variable_register] = v_y - v_x;
        if (v_y < v_x) {
          chip8_state->variable_register[0xF] = 0;
        } else {
          chip8_state->variable_register[0xF] = 1;
        }
        break;
      }
      case 0xE:
        uint8_t shift_left_register = (opcode & 0x0F00) >> 8;

        if (COSMAC_MODE) {
          chip8_state->variable_register[shift_left_register] =
              chip8_state->variable_register[(opcode & 0x00F0) >> 4];
        }

        chip8_state->variable_register[0xF] =
            (chip8_state->variable_register[shift_left_register] &
             0b10000000) >>
            7;
        chip8_state->variable_register[shift_left_register] <<= 1;
        break;
      }
      break;
    case 0x9:
      if ((opcode & 0x000F) == 0 &&
          chip8_state->variable_register[(opcode & 0x0F00) >> 8] !=
              chip8_state->variable_register[(opcode & 0x00F0) >> 4]) {
        chip8_state->program_counter += 2;
      }
      break;
    case 0xA:
      chip8_state->index_register = opcode & 0x0FFF;
      break;
    case 0xB:
      if (COSMAC_MODE) {
        chip8_state->program_counter =
            (opcode & 0x0FFF) + chip8_state->variable_register[0x0];
        break;
      }

      chip8_state->program_counter =
          (opcode & 0x0FFF) +
          chip8_state->variable_register[(opcode & 0x0F00) >> 8];
      break;
    case 0xC:
      chip8_state->variable_register[(opcode & 0x0F00) >> 8] =
          rand() & 0x00FF & opcode;
      break;
    case 0xD:
      uint8_t x =
          chip8_state->variable_register[(opcode & 0x0F00) >> 8] & 0b00111111;

      uint8_t n = opcode & 0x000F;
      if (!fits_in_bounds(chip8_state->memory, chip8_state->index_register,
                          n)) {
        fprintf(stderr, "ERR: out of bounds to read %d bytes starting at %X\n",
                n, chip8_state->index_register);
        return ERR_COULD_NOT_PROCESS;
      }
      uint8_t *sprite_start =
          memory_address_at(chip8_state->memory, chip8_state->index_register);

      chip8_state->variable_register[0xF] = 0;

      uint8_t y_draw =
          chip8_state->variable_register[(opcode & 0x00F0) >> 4] & 0b00011111;

      for (uint8_t i = 0; i < n; i++) {
        uint8_t data = *(sprite_start + i);
        uint8_t mask = 0b10000000;
        uint8_t x_draw = x;

        for (uint8_t j = 0; j < 8; j++) {
          if ((data & mask) > 0) {
            if (!flip_on_display(chip8_state->display, x_draw, y_draw)) {
              chip8_state->variable_register[0xF] = 1;
            }
          }

          x_draw++;
          if (!COSMAC_MODE && x_draw >= NUM_PIXELS_WIDTH) {
            break;
          }

          mask >>= 1;
        }

        y_draw++;
        if (!COSMAC_MODE && y_draw >= NUM_PIXELS_HEIGHT) {
          break;
        }
      }

      break;
    case 0xE:
      switch (opcode & 0x00FF) {
      case 0x9E:
        if (chip8_state->key_states
                [chip8_state->variable_register[(opcode & 0x0F00) >> 8]]) {
          chip8_state->program_counter += 2;
        }
        break;
      case 0xA1:
        if (!chip8_state->key_states
                 [chip8_state->variable_register[(opcode & 0x0F00) >> 8]]) {
          chip8_state->program_counter += 2;
        }
        break;
      }
      break;
    case 0xF:
      uint8_t x_register = (opcode & 0x0F00) >> 8;

      switch (opcode & 0x00FF) {
      case 0x07:
        chip8_state->variable_register[x_register] = chip8_state->delay_timer;
        break;
      case 0x15:
        chip8_state->delay_timer = chip8_state->variable_register[x_register];
        break;
      case 0x18:
        chip8_state->sound_timer = chip8_state->variable_register[x_register];
        break;
      case 0x1E:
        bool below_threshold = chip8_state->index_register < 0x1000;
        chip8_state->index_register +=
            chip8_state->variable_register[x_register];

        if (!COSMAC_MODE) {
          if (below_threshold && chip8_state->index_register >= 0x1000)
            chip8_state->variable_register[0xF] = 1;
          else
            chip8_state->variable_register[0xF] = 0;
        }
        break;
      case 0x0A:
        bool key_pressed = false;
        for (uint8_t i = 0; i <= 0xF; i++) {
          if (chip8_state->key_states[i]) {
            key_pressed = true;
            chip8_state->variable_register[x_register] = i;
          }
        }

        if (!key_pressed) {
          chip8_state->program_counter -= 2;
        }
        break;
      case 0x29:
        chip8_state->index_register =
            MEMORY_FONT_START +
            (chip8_state->variable_register[x_register] & 0xF) * 5;
        break;
      case 0x33:
        uint16_t scratch = 0b0;
        uint8_t value = chip8_state->variable_register[x_register];
        uint16_t mask = 0b1111;

        for (int i = 0; i < 8; i++) {
          for (int space = 0; space < 12; space += 4) {
            uint8_t space_value = (scratch >> space) & mask;
            if (space_value >= 5) {
              space_value += 3;

              scratch &= ~(mask << space);
              scratch += space_value << space;
            }
          }

          scratch <<= 1;
          scratch += value >> 7;
          value <<= 1;
        }
        uint8_t decimal_data[3] = {
            (scratch >> 8),
            (scratch >> 4) & 0b1111,
            scratch & 0b1111,
        };
        bool loaded = load_into_memory(
            chip8_state->memory, chip8_state->index_register, 3, decimal_data);
        if (!loaded) {
          fprintf(stderr, "Could not load decimal conversion at pos:%X \n",
                  chip8_state->index_register);
          return ERR_COULD_NOT_PROCESS;
        }
        break;
      case 0x55: {
        if (!fits_in_bounds(chip8_state->memory, chip8_state->index_register,
                            x_register + 1)) {
          fprintf(
              stderr,
              "Could not load into memory. Out of bounds. pos:%X, size:%d \n",
              chip8_state->index_register, x_register + 1);
          return ERR_COULD_NOT_PROCESS;
        }

        memcpy(
            memory_address_at(chip8_state->memory, chip8_state->index_register),
            &chip8_state->variable_register, x_register + 1);

        if (COSMAC_MODE) {
          chip8_state->index_register += x_register + 1;
        }
        break;
      }
      case 0x65: {
        uint8_t load_data[x_register + 1];
        if (!fits_in_bounds(chip8_state->memory, chip8_state->index_register,
                            x_register + 1)) {
          fprintf(stderr,
                  "Could not load into registers. Out of bounds. pos:%X, "
                  "size:%d \n",
                  chip8_state->index_register, x_register + 1);
          return ERR_COULD_NOT_PROCESS;
        }

        memcpy(
            &load_data,
            memory_address_at(chip8_state->memory, chip8_state->index_register),
            x_register + 1);

        for (int i = 0; i < x_register + 1; i++)
          chip8_state->variable_register[i] = load_data[i];

        if (COSMAC_MODE) {
          chip8_state->index_register += x_register + 1;
        }
        break;
      }
      }
      break;
    }
  }

  return ERR_PROCESSED;
}

void destroy_application_state(ApplicationState *state) {
  destroy_clock(state->timer_clock);
  destroy_clock(state->cpu_clock);
  free(state);
}

typedef struct {
  SDL_Window *window;
  SDL_Renderer *renderer;
  SDL_FPoint points[NUM_TOTAL_PIXELS];
} Renderer;

Renderer *create_renderer() {
  Renderer *renderer = malloc(sizeof(Renderer));
  renderer->window = NULL;
  renderer->renderer = NULL;

  return renderer;
}

SDL_AppResult init_renderer(Renderer *renderer) {
  SDL_SetAppMetadata("Chip8 Emulator", "1.0", "com.theobalestra.chip8");

  if (!SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO)) {
    SDL_Log("Couldn't load SDL: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  if (!SDL_CreateWindowAndRenderer("Chip8 Emulator", WINDOW_WIDTH,
                                   WINDOW_HEIGHT, 0, &(renderer->window),
                                   &(renderer->renderer))) {
    SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  SDL_SetRenderLogicalPresentation(renderer->renderer, 64, 32,
                                   SDL_LOGICAL_PRESENTATION_LETTERBOX);

  return SDL_APP_CONTINUE;
}

void render_display(Renderer *renderer, Chip8State *state) {
  if (!state->display->dirty)
    return;

  SDL_SetRenderDrawColorFloat(renderer->renderer, 0.0, 0.0, 0.0, 1.0);

  int num_points = 0;

  SDL_RenderClear(renderer->renderer);

  for (int x = 0; x < NUM_PIXELS_WIDTH; x++) {
    for (int y = 0; y < NUM_PIXELS_HEIGHT; y++) {
      if (get_on_display(state->display, x, y)) {
        renderer->points[num_points].x = (float)x;
        renderer->points[num_points].y = (float)y;

        num_points++;
      }
    }
  }

  SDL_SetRenderDrawColorFloat(renderer->renderer, 1.0, 1.0, 1.0, 1.0);

  SDL_RenderPoints(renderer->renderer, renderer->points, num_points);
  SDL_RenderPresent(renderer->renderer);

  state->display->dirty = false;
}

void destroy_renderer(Renderer *renderer) {
  SDL_DestroyRenderer(renderer->renderer);
  SDL_DestroyWindow(renderer->window);
  free(renderer);
}

const uint8_t font_data[80] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

int load_file_into_memory(Chip8State *state, char const *file_path) {
  FILE *fd = fopen(file_path, "rb");
  if (fd == nullptr) {
    fprintf(stderr, "ERR: Could not load file '%s': %s\n", file_path,
            strerror(errno));
    return -1;
  }

  if (fseek(fd, 0, SEEK_END) != 0) {
    fclose(fd);

    fprintf(stderr, "ERR: Could not get file '%s' size\n", file_path);
    return -1;
  }

  long file_size = ftell(fd);
  if (file_size == -1) {
    fclose(fd);
    fprintf(stderr, "ERR: Could not get file '%s' size\n", file_path);
    return -1;
  }

  size_t size = (size_t)file_size;

  if (!fits_in_bounds(state->memory, MEMORY_PROGRAM_START, size)) {
    fclose(fd);
    fprintf(stderr, "ERR: file too large at %zu bytes\n", size);
    return -1;
  }

  if (fseek(fd, 0, SEEK_SET) != 0) {
    fclose(fd);
    fprintf(stderr, "ERR: could not seek to beginning of file '%s'\n",
            file_path);
    return -1;
  }

  if (fread(memory_address_at(state->memory, MEMORY_PROGRAM_START), 1, size,
            fd) != size) {
    if (ferror(fd))
      fprintf(stderr, "ERR: Could not load file '%s': %s\n", file_path,
              strerror(errno));
    else
      fprintf(stderr, "ERR: Could not load file '%s'\n", file_path);

    fclose(fd);
    return -1;
  }

  fclose(fd);
  return 0;
}

int main(int argc, char const *argv[]) {
  if (argc != 2) {
    fprintf(stderr,
            "ERR: Invalid invocation.\nExpected format: ./main {file_path}\n");
    return 1;
  }

  Renderer *renderer = create_renderer();
  SDL_AppResult renderer_init_result = init_renderer(renderer);

  if (renderer_init_result != SDL_APP_CONTINUE) {
    destroy_renderer(renderer);
    return 1;
  }

  Chip8State *chip8_state = create_chip8_state(4096);
  if (load_file_into_memory(chip8_state, argv[1]) == -1) {
    destroy_chip8_state(chip8_state);
    destroy_renderer(renderer);
    return 1;
  }

  ApplicationState *application_state =
      create_application_state(SDL_GetTicks());

  load_into_memory(chip8_state->memory, MEMORY_FONT_START, 80, font_data);

  while (application_state->running) {
    uint64_t total_time_ms = SDL_GetTicks();

    uint8_t elapsed_timer_ticks =
        get_clock_ticks(application_state->timer_clock, total_time_ms);
    uint8_t elapsed_cpu_ticks =
        get_clock_ticks(application_state->cpu_clock, total_time_ms);
    advance_chip8_timers(chip8_state, elapsed_timer_ticks);

    ProcessInstructionError result =
        process_instructions(chip8_state, elapsed_cpu_ticks);
    if (result != ERR_PROCESSED) {
      application_state->running = false;
      fprintf(stderr, "Could not process instructions\n");
    }

    handle_sdl_events(application_state, chip8_state);
    render_display(renderer, chip8_state);
  }

  destroy_chip8_state(chip8_state);
  destroy_application_state(application_state);
  destroy_renderer(renderer);

  SDL_Quit();

  return 0;
}