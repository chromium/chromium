// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_parenting_controller.h"

#include "ash/wm/container_finder.h"
#include "ui/aura/window.h"

namespace ash {

WindowParentingController::WindowParentingController() = default;

WindowParentingController::~WindowParentingController() = default;

aura::Window* WindowParentingController::GetDefaultParent(
    aura::Window* window,
    const gfx::Rect& bounds) {
  return GetDefaultParentForWindow(window, bounds);
}

}  // namespace ash
