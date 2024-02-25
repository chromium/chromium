// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_PIP_PIP_POSITIONER_H_
#define ASH_WM_PIP_PIP_POSITIONER_H_

#include "ash/ash_export.h"
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

  PipPositioner(const PipPositioner&) = delete;
  PipPositioner& operator=(const PipPositioner&) = delete;

  ~PipPositioner() = delete;

  // Adjusts bounds during a drag of a PIP window. For example, this will
  // ensure that the PIP window cannot leave the PIP movement area.
  // If the window is transformed with `gfx::Transform`, it returns
  // bounds with unscaled size but with origin that avoids obstacles
  // even when the scale is applied.
  static gfx::Rect GetBoundsForDrag(const display::Display& display,
                                    const gfx::Rect& bounds_in_screen,
                                    const gfx::Transform& transform);

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

  // Moves the PIP window along the movement area to the given snap fraction.
  // The fraction is defined in a clockwise fashion against the PIP movement
  // area.
  //
  //            0   1
  //          4 +---+ 1
  //            |   |
  //          3 +---+ 2
  //            3   2
  //
  static gfx::Rect GetSnapFractionAppliedBounds(WindowState* window_state);

  // Calculates the PIP snap fraction.
  static void ClearSnapFraction(WindowState* window_state);

  // Returns whether the PIP window has the snap fraction or not.
  static bool HasSnapFraction(WindowState* window_state);

  // Saves the current PIP snap fraction.
  static void SaveSnapFraction(WindowState* window_state,
                               const gfx::Rect& bounds);

 private:
  friend class PipPositionerDisplayTest;
};

}  // namespace ash

#endif  // ASH_WM_PIP_PIP_POSITIONER_H_
