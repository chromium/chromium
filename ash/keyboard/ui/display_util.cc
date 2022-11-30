// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/ui/display_util.h"

#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"

namespace {

constexpr int kWindowMargin = 10;

}  // namespace

namespace keyboard {

DisplayUtil::DisplayUtil() = default;

display::Display DisplayUtil::GetNearestDisplayToWindow(
    aura::Window* window) const {
  return display::Screen::GetScreen()->GetDisplayNearestWindow(window);
}

display::Display DisplayUtil::FindAdjacentDisplayIfPointIsNearMargin(
    const display::Display& current_display,
    const gfx::Point& point_in_local) const {
  const gfx::Rect current_bounds = current_display.bounds();

  const gfx::Point point =
      point_in_local + current_display.bounds().origin().OffsetFromOrigin();

  int representative_x = point.x();
  int representative_y = point.y();

  int current_left = current_bounds.x();
  int current_right = current_left + current_bounds.width();
  int current_top = current_bounds.y();
  int current_bottom = current_top + current_bounds.height();

  // If the point is close to
  if (point.x() - current_left <= kWindowMargin) {
    representative_x = current_left - kWindowMargin;
  } else if (current_right - point.x() <= kWindowMargin) {
    representative_x = current_right + kWindowMargin;
  } else if (point.y() - current_top <= kWindowMargin) {
    representative_y = current_top - kWindowMargin;
  } else if (current_bottom - point.y() <= kWindowMargin) {
    representative_y = current_bottom + kWindowMargin;
  } else {
    return current_display;
  }

  for (const display::Display& display :
       display::Screen::GetScreen()->GetAllDisplays()) {
    const gfx::Rect& new_bounds = display.work_area();
    if (display.touch_support() == display::Display::TouchSupport::AVAILABLE &&
        display.id() != current_display.id() &&
        new_bounds.Contains(representative_x, representative_y)) {
      return display;
    }
  }
  return current_display;
}

}  // namespace keyboard
