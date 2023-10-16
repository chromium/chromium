// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/pip/pip_positioner.h"

#include <algorithm>

#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/wm/collision_detection/collision_detection_utils.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/work_area_insets.h"
#include "base/numerics/safe_conversions.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {
const float kPipDismissMovementProportion = 1.5f;
}  // namespace

gfx::Rect PipPositioner::GetBoundsForDrag(const display::Display& display,
                                          const gfx::Rect& bounds_in_screen,
                                          const gfx::Transform& transform) {
  gfx::Rect drag_bounds(bounds_in_screen.origin(),
                        transform.MapRect(bounds_in_screen).size());
  drag_bounds.AdjustToFit(CollisionDetectionUtils::GetMovementArea(display));
  drag_bounds = CollisionDetectionUtils::AvoidObstacles(
      display, drag_bounds,
      CollisionDetectionUtils::RelativePriority::kPictureInPicture);
  drag_bounds.set_size(bounds_in_screen.size());
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
  bounds_movement_area.Inset(gfx::Insets::VH(
      -bounds_in_screen.height() * kPipDismissMovementProportion,
      -bounds_in_screen.width() * kPipDismissMovementProportion));
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
  float* snap_fraction =
      window_state->window()->GetProperty(ash::kPipSnapFractionKey);
  if (snap_fraction)
    bounds_in_screen = GetSnapFractionAppliedBounds(window_state);
  return CollisionDetectionUtils::GetRestingPosition(
      window_state->GetDisplay(), bounds_in_screen,
      CollisionDetectionUtils::RelativePriority::kPictureInPicture);
}

gfx::Rect PipPositioner::GetSnapFractionAppliedBounds(
    WindowState* window_state) {
  gfx::Rect bounds = window_state->window()->GetBoundsInScreen();
  gfx::Rect movement_area =
      CollisionDetectionUtils::GetMovementArea(window_state->GetDisplay());
  if (!HasSnapFraction(window_state))
    return bounds;
  float snap_fraction =
      *(window_state->window()->GetProperty(ash::kPipSnapFractionKey));

  if (snap_fraction < 1.) {
    int offset = movement_area.x() +
                 base::ClampRound(snap_fraction *
                                  (movement_area.width() - bounds.width()));
    return gfx::Rect(offset, movement_area.y(), bounds.width(),
                     bounds.height());
  } else if (snap_fraction < 2.) {
    snap_fraction -= 1.;
    int offset = movement_area.y() +
                 base::ClampRound(snap_fraction *
                                  (movement_area.height() - bounds.height()));
    return gfx::Rect(movement_area.right() - bounds.width(), offset,
                     bounds.width(), bounds.height());
  } else if (snap_fraction < 3.) {
    snap_fraction -= 2.;
    int offset = movement_area.x() +
                 base::ClampRound((1. - snap_fraction) *
                                  (movement_area.width() - bounds.width()));
    return gfx::Rect(offset, movement_area.bottom() - bounds.height(),
                     bounds.width(), bounds.height());
  } else {
    snap_fraction -= 3.;
    int offset = movement_area.y() +
                 base::ClampRound((1. - snap_fraction) *
                                  (movement_area.height() - bounds.height()));
    return gfx::Rect(movement_area.x(), offset, bounds.width(),
                     bounds.height());
  }
}

void PipPositioner::ClearSnapFraction(WindowState* window_state) {
  return window_state->window()->ClearProperty(ash::kPipSnapFractionKey);
}

bool PipPositioner::HasSnapFraction(WindowState* window_state) {
  return window_state->window()->GetProperty(ash::kPipSnapFractionKey) !=
         nullptr;
}

void PipPositioner::SaveSnapFraction(WindowState* window_state,
                                     const gfx::Rect& bounds) {
  // Ensure that |bounds| is along one of the edges of the movement area.
  // If the PIP window is drag-moved onto some system UI, it's possible that
  // the PIP window is detached from any of them.
  gfx::Rect snapped_bounds =
      ash::CollisionDetectionUtils::AdjustToFitMovementAreaByGravity(
          window_state->GetDisplay(), bounds);
  gfx::Rect movement_area =
      CollisionDetectionUtils::GetMovementArea(window_state->GetDisplay());
  float width_fraction = (float)(snapped_bounds.x() - movement_area.x()) /
                         (movement_area.width() - snapped_bounds.width());
  float height_fraction = (float)(snapped_bounds.y() - movement_area.y()) /
                          (movement_area.height() - snapped_bounds.height());
  float snap_fraction;
  if (snapped_bounds.y() == movement_area.y()) {
    snap_fraction = width_fraction;
  } else if (snapped_bounds.right() == movement_area.right()) {
    snap_fraction = 1. + height_fraction;
  } else if (snapped_bounds.bottom() == movement_area.bottom()) {
    snap_fraction = 2. + (1. - width_fraction);
  } else {
    snap_fraction = 3. + (1. - height_fraction);
  }
  window_state->window()->SetProperty(ash::kPipSnapFractionKey,
                                      new float(snap_fraction));
}

}  // namespace ash
