// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/pip/pip_positioner.h"

#include <algorithm>

#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/wm/collision_detection/collision_detection_utils.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/work_area_insets.h"
#include "base/logging.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {
const float kPipDismissMovementProportion = 1.5f;
}  // namespace

gfx::Rect PipPositioner::GetBoundsForDrag(const display::Display& display,
                                          const gfx::Rect& bounds_in_screen) {
  gfx::Rect drag_bounds = bounds_in_screen;
  drag_bounds.AdjustToFit(CollisionDetectionUtils::GetMovementArea(display));
  drag_bounds = CollisionDetectionUtils::AvoidObstacles(
      display, drag_bounds,
      CollisionDetectionUtils::RelativePriority::kPictureInPicture);
  return drag_bounds;
}

gfx::Rect PipPositioner::GetDismissedPosition(
    const display::Display& display,
    const gfx::Rect& bounds_in_screen) {
  gfx::Rect work_area = CollisionDetectionUtils::GetMovementArea(display);
  const CollisionDetectionUtils::Gravity gravity =
      CollisionDetectionUtils::GetGravityToClosestEdge(bounds_in_screen,
                                                       work_area);
  // Allow the bounds to move at most |kPipDismissMovementProportion| of the
  // length of the bounds in the direction of movement.
  gfx::Rect bounds_movement_area = bounds_in_screen;
  bounds_movement_area.Inset(
      -bounds_in_screen.width() * kPipDismissMovementProportion,
      -bounds_in_screen.height() * kPipDismissMovementProportion);
  gfx::Rect dismissed_bounds =
      CollisionDetectionUtils::GetAdjustedBoundsByGravity(
          bounds_in_screen, bounds_movement_area, gravity);

  // If the PIP window isn't close enough to the edge of the screen, don't slide
  // it out.
  return work_area.Intersects(dismissed_bounds) ? bounds_in_screen
                                                : dismissed_bounds;
}

gfx::Rect PipPositioner::GetPositionAfterMovementAreaChange(
    WindowState* window_state) {
  // Restore to previous bounds if we have them. This lets us move the PIP
  // window back to its original bounds after transient movement area changes,
  // like the keyboard popping up and pushing the PIP window up.
  gfx::Rect bounds_in_screen = window_state->window()->GetBoundsInScreen();
  // If the client changes the window size, don't try to resize it back for
  // restore.
  if (window_state->HasRestoreBounds()) {
    bounds_in_screen.set_origin(
        window_state->GetRestoreBoundsInScreen().origin());
  }
  return CollisionDetectionUtils::GetRestingPosition(
      window_state->GetDisplay(), bounds_in_screen,
      CollisionDetectionUtils::RelativePriority::kPictureInPicture);
}

}  // namespace ash
