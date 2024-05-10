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
    const gfx::Rect& restore_bounds_in_parent)
    : is_landscape_(is_landscape_before_rotation) {
  const auto& display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window);
  window_bounds_in_screen_ = window->GetBoundsInScreen();
  display_id_ = display.id();
  display_offset_from_origin_in_screen_ = display.bounds().OffsetFromOrigin();
  display_size_in_pixel_ = display.GetSizeInPixel();

  WindowState* window_state = WindowState::Get(window);
  DCHECK(window_state);

  if (!restore_bounds_in_parent.IsEmpty()) {
    restore_bounds_in_parent_ = restore_bounds_in_parent;
  }
}

PersistentWindowInfo::~PersistentWindowInfo() = default;

}  // namespace ash
