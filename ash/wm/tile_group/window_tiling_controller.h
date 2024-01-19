// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TILE_GROUP_WINDOW_TILING_CONTROLLER_H_
#define ASH_WM_TILE_GROUP_WINDOW_TILING_CONTROLLER_H_

#include "ash/ash_export.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

class ASH_EXPORT WindowTilingController {
 public:
  WindowTilingController() = default;
  WindowTilingController(const WindowTilingController&) = delete;
  WindowTilingController& operator=(const WindowTilingController&) = delete;
  ~WindowTilingController() = default;

  // Checks whether the window can be tiling resized.
  bool CanTilingResize(aura::Window* window) const;

  // Tiling resizes the window in a direction.
  // They assume the window can be resized.
  void OnTilingResizeLeft(aura::Window* window);
  void OnTilingResizeUp(aura::Window* window);
  void OnTilingResizeRight(aura::Window* window);
  void OnTilingResizeDown(aura::Window* window);
};

}  // namespace ash

#endif  // ASH_WM_TILE_GROUP_WINDOW_TILING_CONTROLLER_H_
