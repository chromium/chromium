// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/multi_display/persistent_window_info.h"

#include "ash/wm/window_state.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace ash {

PersistentWindowInfo::PersistentWindowInfo(
    aura::Window* window,
    bool is_landscape_before_rotation,
    const gfx::Rect& given_restore_bounds_in_parent)
    : is_landscape(is_landscape_before_rotation) {
  const auto& display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window);
  window_bounds_in_screen = window->GetBoundsInScreen();
  display_id = display.id();
  display_bounds_in_screen = display.bounds();

  WindowState* window_state = WindowState::Get(window);
  DCHECK(window_state);

  if (!given_restore_bounds_in_parent.IsEmpty()) {
    restore_bounds_in_parent = given_restore_bounds_in_parent;
  }
}

PersistentWindowInfo::PersistentWindowInfo(const PersistentWindowInfo& other) =
    default;

PersistentWindowInfo::~PersistentWindowInfo() = default;

}  // namespace ash
