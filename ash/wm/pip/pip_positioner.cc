// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/pip/pip_positioner.h"

#include <algorithm>

#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ui/aura/window.h"
#include "ui/keyboard/keyboard_controller.h"

namespace ash {

namespace {
const int kPipWorkAreaInsetsDp = 8;
const float kPipDismissMovementProportion = 1.5f;

enum { GRAVITY_LEFT, GRAVITY_RIGHT, GRAVITY_TOP, GRAVITY_BOTTOM };

// Returns the result of adjusting |bounds| according to |gravity| inside
// |region|.
gfx::Rect GetAdjustedBoundsByGravity(const gfx::Rect& bounds,
                                     const gfx::Rect& region,
                                     int gravity) {
  switch (gravity) {
    case GRAVITY_LEFT:
      return gfx::Rect(region.x(), bounds.y(), bounds.width(), bounds.height());
    case GRAVITY_RIGHT:
      return gfx::Rect(region.right() - bounds.width(), bounds.y(),
                       bounds.width(), bounds.height());
    case GRAVITY_TOP:
      return gfx::Rect(bounds.x(), region.y(), bounds.width(), bounds.height());
    case GRAVITY_BOTTOM:
      return gfx::Rect(bounds.x(), region.bottom() - bounds.height(),
                       bounds.width(), bounds.height());
    default:
      NOTREACHED();
  }
  return bounds;
}

// Returns the gravity that would make |bounds| fall to the closest edge of
// |region|. If |bounds| is outside of |region| then it will return the gravity
// as if |bounds| had fallen outside of |region|. See the below diagram for what
// the gravity regions look like for a point.
//  \  TOP /
//   \____/ R
// L |\  /| I
// E | \/ | G
// F | /\ | H
// T |/__\| T
//   /    \
//  /BOTTOM
int GetGravityToClosestEdge(const gfx::Rect& bounds, const gfx::Rect& region) {
  const int left_edge_dist = bounds.x() - region.x();
  const int right_edge_dist = region.right() - bounds.right();
  const int top_edge_dist = bounds.y() - region.y();
  const int bottom_edge_dist = region.bottom() - bounds.bottom();
  int minimum_edge_dist = std::min(left_edge_dist, right_edge_dist);
  minimum_edge_dist = std::min(minimum_edge_dist, top_edge_dist);
  minimum_edge_dist = std::min(minimum_edge_dist, bottom_edge_dist);

  if (left_edge_dist == minimum_edge_dist) {
    return GRAVITY_LEFT;
  } else if (right_edge_dist == minimum_edge_dist) {
    return GRAVITY_RIGHT;
  } else if (top_edge_dist == minimum_edge_dist) {
    return GRAVITY_TOP;
  } else {
    return GRAVITY_BOTTOM;
  }
}

}  // namespace

gfx::Rect PipPositioner::GetMovementArea(const display::Display& display) {
  gfx::Rect work_area = display.work_area();
  auto* keyboard_controller = keyboard::KeyboardController::Get();

  // Include keyboard if it's not floating.
  if (keyboard_controller->IsEnabled() &&
      keyboard_controller->GetActiveContainerType() !=
          keyboard::ContainerType::FLOATING) {
    gfx::Rect keyboard_bounds = keyboard_controller->visual_bounds_in_screen();
    work_area.Subtract(keyboard_bounds);
  }

  work_area.Inset(kPipWorkAreaInsetsDp, kPipWorkAreaInsetsDp);
  return work_area;
}

gfx::Rect PipPositioner::GetBoundsForDrag(const display::Display& display,
                                          const gfx::Rect& bounds) {
  gfx::Rect drag_bounds = bounds;
  drag_bounds.AdjustToFit(GetMovementArea(display));
  return drag_bounds;
}

gfx::Rect PipPositioner::GetRestingPosition(const display::Display& display,
                                            const gfx::Rect& bounds) {
  gfx::Rect resting_bounds = bounds;
  gfx::Rect area = GetMovementArea(display);
  resting_bounds.AdjustToFit(area);

  const int gravity = GetGravityToClosestEdge(resting_bounds, area);
  return GetAdjustedBoundsByGravity(resting_bounds, area, gravity);
}

gfx::Rect PipPositioner::GetDismissedPosition(const display::Display& display,
                                              const gfx::Rect& bounds) {
  gfx::Rect work_area = GetMovementArea(display);
  const int gravity = GetGravityToClosestEdge(bounds, work_area);
  // Allow the bounds to move at most |kPipDismissMovementProportion| of the
  // length of the bounds in the direction of movement.
  gfx::Rect bounds_movement_area = bounds;
  bounds_movement_area.Inset(-bounds.width() * kPipDismissMovementProportion,
                             -bounds.height() * kPipDismissMovementProportion);
  gfx::Rect dismissed_bounds =
      GetAdjustedBoundsByGravity(bounds, bounds_movement_area, gravity);

  // If the PIP window isn't close enough to the edge of the screen, don't slide
  // it out.
  return work_area.Intersects(dismissed_bounds) ? bounds : dismissed_bounds;
}

gfx::Rect PipPositioner::GetPositionAfterMovementAreaChange(
    wm::WindowState* window_state) {
  // Restore to previous bounds if we have them. This lets us move the PIP
  // window back to its original bounds after transient movement area changes,
  // like the keyboard popping up and pushing the PIP window up.
  const gfx::Rect bounds = window_state->HasRestoreBounds()
                               ? window_state->GetRestoreBoundsInScreen()
                               : window_state->window()->GetBoundsInScreen();
  return GetRestingPosition(window_state->GetDisplay(), bounds);
}

}  // namespace ash
