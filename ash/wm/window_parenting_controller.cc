// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_parenting_controller.h"

#include "ash/shell.h"
#include "ash/wm/container_finder.h"

namespace ash {

WindowParentingController::WindowParentingController(aura::Window* root_window)
    : root_window_(root_window) {
  aura::client::SetWindowParentingClient(root_window, this);
}

WindowParentingController::~WindowParentingController() {
  aura::client::SetWindowParentingClient(root_window_, nullptr);
}

aura::Window* WindowParentingController::GetDefaultParent(
    aura::Window* window,
    const gfx::Rect& bounds,
    int64_t display_id) {
  auto* target_root = Shell::GetRootWindowForDisplayId(display_id);
  if (target_root == nullptr) {
    target_root = root_window_;
  }
  return GetDefaultParentForWindow(window, target_root, bounds);
}

}  // namespace ash
