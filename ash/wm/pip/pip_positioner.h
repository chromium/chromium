// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_PIP_PIP_POSITIONER_H_
#define ASH_WM_PIP_PIP_POSITIONER_H_

#include <vector>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

class WindowState;

// Computes resting and dragging positions for PIP windows. Note that this
// class uses only Screen coordinates.
class ASH_EXPORT PipPositioner {
 public:
  static const int kPipDismissTimeMs = 300;

  PipPositioner() = delete;
  ~PipPositioner() = delete;

  // Adjusts bounds during a drag of a PIP window. For example, this will
  // ensure that the PIP window cannot leave the PIP movement area.
  static gfx::Rect GetBoundsForDrag(const display::Display& display,
                                    const gfx::Rect& bounds_in_screen);

  // Based on the current PIP window position, finds a final location of where
  // the PIP window should be animated to to show a dismissal off the side
  // of the screen. Note that this may return somewhere not off-screen if
  // animating the PIP window off-screen would travel too far.
  static gfx::Rect GetDismissedPosition(const display::Display& display,
                                        const gfx::Rect& bounds_in_screen);

  // Gets the position the PIP window should be moved to after a movement area
  // change. For example, if the shelf is changed from auto-hidden to always
  // shown, the PIP window should move up to not intersect it.
  static gfx::Rect GetPositionAfterMovementAreaChange(
      WindowState* window_state);

 private:
  friend class PipPositionerDisplayTest;

  DISALLOW_COPY_AND_ASSIGN(PipPositioner);
};

}  // namespace ash

#endif  // ASH_WM_PIP_PIP_POSITIONER_H_
