// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_MOUSE_WARP_CONTROLLER_H_
#define ASH_DISPLAY_MOUSE_WARP_CONTROLLER_H_

#include "ash/ash_export.h"

namespace ui {
class MouseEvent;
}

namespace ash {

// MouseWarpController implements the mouse warp behavior for
// different display modes and platforms.
class ASH_EXPORT MouseWarpController {
 public:
  virtual ~MouseWarpController() {}

  // An implementaion may warp the mouse cursor to another display
  // when necessary. Returns true if the mouse cursor has been
  // moved to another display, or false otherwise.
  virtual bool WarpMouseCursor(ui::MouseEvent* event) = 0;

  // Enables/Disables mouse warping.
  virtual void SetEnabled(bool enable) = 0;
};

}  // namespace ash

#endif  // ASH_DISPLAY_MOUSE_WARP_CONTROLLER_H_
