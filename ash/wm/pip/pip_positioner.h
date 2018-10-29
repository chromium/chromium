// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_PIP_PIP_POSITIONER_H_
#define ASH_WM_PIP_PIP_POSITIONER_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

namespace wm {
class WindowState;
}  // namespace wm

class PipPositionerTest;

class ASH_EXPORT PipPositioner {
 public:
  static const int kPipDismissTimeMs = 300;

  PipPositioner() = delete;
  ~PipPositioner() = delete;

  // Returns the area that the PIP window can be positioned inside for a given
  // display |display|.
  static gfx::Rect GetMovementArea(const display::Display& display);

  // Returns the position the PIP window should come to rest at. For example,
  // this will be at a screen edge, not in the middle of the screen.
  // TODO(edcourtney): This should consider drag velocity for fling as well.
  static gfx::Rect GetRestingPosition(const display::Display& display,
                                      const gfx::Rect& bounds);

  // Adjusts bounds during a drag of a PIP window. For example, this will
  // ensure that the PIP window cannot leave the PIP movement area.
  // |bounds| is in screen coordinates.
  static gfx::Rect GetBoundsForDrag(const display::Display& display,
                                    const gfx::Rect& bounds);

  // Based on the current PIP window position, finds a final location of where
  // the PIP window should be animated to to show a dismissal off the side
  // of the screen. Note that this may return somewhere not off-screen if
  // animating the PIP window off-screen would travel too far.
  static gfx::Rect GetDismissedPosition(const display::Display& display,
                                        const gfx::Rect& bounds);

  // Gets the position the PIP window should be moved to after a movement area
  // change. For example, if the shelf is changed from auto-hidden to always
  // shown, the PIP window should move up to not intersect it.
  static gfx::Rect GetPositionAfterMovementAreaChange(
      wm::WindowState* window_state);

 private:
  friend class PipPositionerTest;

  DISALLOW_COPY_AND_ASSIGN(PipPositioner);
};

}  // namespace ash

#endif  // ASH_WM_PIP_PIP_POSITIONER_H_
