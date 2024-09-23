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

// Controller for tile-like window resizing.
//
// Resizing can be done in one of the 4 directions: left, right, up, and down.
// If the window is not currently aligned to an edge in that direction or its
// opposing direction, the window will first resize to half of the work area on
// that side of the screen.  Subsequent resizes in the same direction will
// shrink the window toward that direction to 1/3 then 1/4.  If you resize
// toward the opposite direction, it will expand that opposite side to the next
// ratio, e.g. 2/3 and 3/4, and eventually up to the full width or height of
// the work area.  At this point, if you keep resizing in that direction, it
// will start shrinking the opposite side of the window toward that direction.
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
  void OnTilingResizeRight(aura::Window* window);
  void OnTilingResizeUp(aura::Window* window);
  void OnTilingResizeDown(aura::Window* window);
};

}  // namespace ash

#endif  // ASH_WM_TILE_GROUP_WINDOW_TILING_CONTROLLER_H_
