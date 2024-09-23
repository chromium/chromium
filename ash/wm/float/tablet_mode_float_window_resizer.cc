// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/float/tablet_mode_float_window_resizer.h"

#include "ash/shell.h"
#include "ash/wm/drag_details.h"
#include "ash/wm/float/float_controller.h"
#include "ash/wm/window_state.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"

namespace ash {

namespace {

// The minimum distance that will be considered as a drag event.
const float kMinimumDragDistance = 5.f;

// Minimum fling velocity required to tuck the window.
const int kFlingToTuckVelocityThresholdSquared = 800 * 800;

}  // namespace

TabletModeFloatWindowResizer::TabletModeFloatWindowResizer(
    WindowState* window_state)
    : WindowResizer(window_state),
      last_location_in_parent_(details().initial_location_in_parent) {
  window_state->OnDragStarted(HTCAPTION);
}

TabletModeFloatWindowResizer::~TabletModeFloatWindowResizer() {
  window_state_->DeleteDragDetails();
}

void TabletModeFloatWindowResizer::Drag(const gfx::PointF& location_in_parent,
                                        int event_flags) {
  last_location_in_parent_ = location_in_parent;

  aura::Window* window = GetTarget();
  gfx::Rect bounds = CalculateBoundsForDrag(location_in_parent);
  if (bounds != window->bounds())
    SetBoundsDuringResize(bounds);
}

void TabletModeFloatWindowResizer::CompleteDrag() {
  // We can reach this state if the user hits a state changing accelerator
  // mid-drag.
  aura::Window* float_window = GetTarget();
  if (!WindowState::Get(float_window)->IsFloated()) {
    return;
  }

  // Revert the drag if the window hasn't moved enough. This will prevent
  // accidental magnetisms.
  const gfx::Vector2dF distance =
      last_location_in_parent_ - details().initial_location_in_parent;
  if (distance.Length() < kMinimumDragDistance) {
    RevertDrag();
    return;
  }

  // `FloatController` will magnetize windows to one of the corners if it
  // remains in float state and not tucked.
  Shell::Get()->float_controller()->OnDragCompletedForTablet(float_window);
  window_state_->OnCompleteDrag(last_location_in_parent_);
}

void TabletModeFloatWindowResizer::RevertDrag() {
  GetTarget()->SetBounds(details().initial_bounds_in_parent);
  window_state_->OnRevertDrag(details().initial_location_in_parent);
}

void TabletModeFloatWindowResizer::FlingOrSwipe(ui::GestureEvent* event) {
  CHECK(window_state_->IsFloated());

  const ui::GestureEventDetails& details = event->details();
  float velocity_x = 0.f, velocity_y = 0.f;
  if (event->type() == ui::EventType::kScrollFlingStart) {
    velocity_x = details.velocity_x();
    velocity_y = details.velocity_y();

    // If the fling wasn't large enough, update the window position based on its
    // drag location.
    float fling_amount = velocity_x * velocity_x + velocity_y * velocity_y;
    if (fling_amount <= kFlingToTuckVelocityThresholdSquared) {
      CompleteDrag();
      return;
    }
  } else {
    CHECK_EQ(ui::EventType::kGestureSwipe, event->type());

    // Use any negative value if `swipe_left()` or `swipe_up()`, otherwise use
    // any positive value.
    if (details.swipe_left()) {
      velocity_x = -1.f;
    } else if (details.swipe_right()) {
      velocity_x = 1.f;
    }

    if (details.swipe_up()) {
      velocity_y = -1.f;
    } else if (details.swipe_down()) {
      velocity_y = 1.f;
    }
  }
  Shell::Get()->float_controller()->OnFlingOrSwipeForTablet(
      GetTarget(), velocity_x, velocity_y);
  window_state_->OnCompleteDrag(last_location_in_parent_);
}

}  // namespace ash
