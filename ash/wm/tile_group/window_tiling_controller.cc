// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tile_group/window_tiling_controller.h"

#include "ui/aura/window.h"

namespace ash {

bool WindowTilingController::CanTilingResize(aura::Window* window) const {
  return true;
}

void WindowTilingController::OnTilingResizeLeft(aura::Window* window) {}

void WindowTilingController::OnTilingResizeUp(aura::Window* window) {}

void WindowTilingController::OnTilingResizeRight(aura::Window* window) {}

void WindowTilingController::OnTilingResizeDown(aura::Window* window) {}

}  // namespace ash
