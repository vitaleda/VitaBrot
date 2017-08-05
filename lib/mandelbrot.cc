/*
  VitaBrot, Mandelbrot explorer for the Playstation Vita.
  Copyright (C) 2017 Ian Tester

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "mandelbrot.hh"
#include <stdio.h>
#include "display.hh"

Mandelbrot::Mandelbrot() :
  _centre(-0.5, 0.0),
  _window_size(3), _pixel_size(_window_size / SCREEN_WIDTH),
  _next_x(0), _next_y(0),
  _running(false),
  _iteration_limit(0),
  _palette(nullptr),
  _coords_mutex(sceKernelCreateMutex("coords_mutex", 0, 0, nullptr))
{}

Mandelbrot::~Mandelbrot() {
  sceKernelDeleteMutex(_coords_mutex);
}

void Mandelbrot::move(float c_re, float c_im, float size) {
  _centre = complex(c_re, c_im);
  _window_size = size;
  _pixel_size = size / SCREEN_WIDTH;
}

void Mandelbrot::reset(void) {
  _next_x = _next_y = 0;
  _running = true;
}

void Mandelbrot::set_limit(uint32_t limit) {
  _iteration_limit = limit;

  if (_palette != nullptr)
    delete [] _palette;

  _palette = new SDL_Color[limit];

  for (uint32_t i = 0; i < limit; i++) {
    _palette[i].r = (i * 71) & 0xff;
    _palette[i].g = (i * 81) & 0xff;
    _palette[i].b = (i * 91) & 0xff;
    _palette[i].a = 255;
  }
}

void Mandelbrot::_get_coords(uint32_t& x, uint32_t& y) {
  sceKernelLockMutex(_coords_mutex, 1, nullptr);

  x = _next_x;
  y = _next_y;

  _next_y++;
  if (_next_y >= SCREEN_HEIGHT) {
    _next_x++;
    _next_y -= SCREEN_HEIGHT;

    if (_next_x >= SCREEN_WIDTH)
      _running = false;
  }

  sceKernelUnlockMutex(_coords_mutex, 1);
}

int Mandelbrot_thread(SceSize argsize, void* argp);

void Mandelbrot::start_threads(void) {
  for (uint8_t i = 0; i < 4; i++) {
    char name[12];
    snprintf(name, 11, "Mandelbrot%d", i+1);
    _threads[i] = sceKernelCreateThread(name, Mandelbrot_thread, 0x11, 262144, 0, 0x00, nullptr);
    if (_threads[i] >= 0)
      sceKernelStartThread(_threads[i], sizeof(Mandelbrot), this);
  }
}

void Mandelbrot::stop_threads(void) {
  for (uint8_t i = 0; i < 4; i++)
    if (_threads[i] >= 0)
      sceKernelDeleteThread(_threads[i]);
}

int Mandelbrot_thread(SceSize argsize, void* argp) {
  Mandelbrot *m = (Mandelbrot*)argp;

  while (1) {
    while (!m->_running)
      sceKernelDelayThread(1);

    uint32_t x, y;
    m->_get_coords(x, y);

    complex pos(x - (SCREEN_WIDTH * 0.5), (SCREEN_HEIGHT / SCREEN_HEIGHT) * (y - (SCREEN_HEIGHT * 0.5)));
    complex c = m->_centre + (pos * m->_pixel_size);
    uint32_t iteration = 0;

    complex z(0, 0);
    do {
      z = sqr(z) + c;
      iteration++;
    } while ((iteration < m->_iteration_limit) && (norm(z) < sqr(2)));

    SDL_SetRenderDrawBlendMode(gRenderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(gRenderer,
			   m->_palette[iteration].r,
			   m->_palette[iteration].g,
			   m->_palette[iteration].b,
			   m->_palette[iteration].a);
    SDL_RenderDrawPoint(gRenderer, x, y);
  }
}