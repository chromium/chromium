// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_KEYBOARD_UI_DISPLAY_UTIL_H_
#define ASH_KEYBOARD_UI_DISPLAY_UTIL_H_

#include "ui/aura/window.h"
#include "ui/display/display.h"

namespace keyboard {

// Helper class for querying information about the screen.
class DisplayUtil {
 public:
  DisplayUtil();

  display::Display GetNearestDisplayToWindow(aura::Window* window) const;
  display::Display FindAdjacentDisplayIfPointIsNearMargin(
      const display::Display& current_display,
      const gfx::Point& point) const;
};

}  // namespace keyboard

#endif  // ASH_KEYBOARD_UI_DISPLAY_UTIL_H_
