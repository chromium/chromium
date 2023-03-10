// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/float/tablet_mode_float_window_resizer.h"

#include "ash/shell.h"
#include "ash/wm/drag_details.h"
#include "ash/wm/float/float_controller.h"
#include "ash/wm/splitview/split_view_drag_indicators.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/window_state.h"
#include "chromeos/ui/wm/features.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// TODO(crbug.com/1351562): The following constants are the same the drag window
// from shelf feature. They need to be changed for this feature, or moved to a
// shared location.

// If the window drag starts within `kDistanceFromEdge` from screen edge, it
// will get snapped if the drag ends in the snap region, no matter how far the
// window has been dragged.
constexpr int kDistanceFromEdge = 8;

// The minimum distance that will be considered as a drag event.
constexpr float kMinimumDragDistance = 5.f;

// Minimum fling velocity required to tuck the window.
const int kFlingToTuckVelocityThresholdSquared = 800 * 800;

}  // namespace

TabletModeFloatWindowResizer::TabletModeFloatWindowResizer(
    WindowState* window_state)
    : WindowResizer(window_state),
      split_view_drag_indicators_(std::make_unique<SplitViewDragIndicators>(
          window_state->window()->GetRootWindow())),
      last_location_in_parent_(details().initial_location_in_parent) {
  DCHECK(chromeos::wm::features::IsWindowLayoutMenuEnabled());
  // TODO(sophiewen): Remove this once the untuck window widget is implemented.
  Shell::Get()->float_controller()->MaybeUntuckFloatedWindowForTablet(
      GetTarget());
  split_view_drag_indicators_->SetDraggedWindow(GetTarget());
  window_state->OnDragStarted(HTCAPTION);
}

TabletModeFloatWindowResizer::~TabletModeFloatWindowResizer() {
  // `SplitViewDragIndicators` has a default delayed animation. Setting the
  // state to no drag instantly hides the indicators so we don't see this
  // delayed hide.
  split_view_drag_indicators_->SetWindowDraggingState(
      SplitViewDragIndicators::WindowDraggingState::kNoDrag);
  window_state_->DeleteDragDetails();
}

void TabletModeFloatWindowResizer::Drag(const gfx::PointF& location_in_parent,
                                        int event_flags) {
  last_location_in_parent_ = location_in_parent;

  aura::Window* window = GetTarget();
  gfx::Rect bounds = CalculateBoundsForDrag(location_in_parent);
  if (bounds != window->bounds())
    SetBoundsDuringResize(bounds);

  // Update `snap_position_` and the snap drag indicators.
  gfx::PointF location_in_screen = location_in_parent;
  gfx::PointF initial_location_in_screen = details().initial_location_in_parent;
  wm::ConvertPointToScreen(window->parent(), &location_in_screen);
  wm::ConvertPointToScreen(window->parent(), &initial_location_in_screen);

  snap_position_ = GetSnapPosition(
      window->GetRootWindow(), window, gfx::ToRoundedPoint(location_in_screen),
      gfx::ToRoundedPoint(initial_location_in_screen),
      /*snap_distance_from_edge=*/kDistanceFromEdge,
      /*minimum_drag_distance=*/kMinDragDistance,
      /*horizontal_edge_inset=*/kScreenEdgeInsetForSnap,
      /*vertical_edge_inset=*/kScreenEdgeInsetForSnap);
  split_view_drag_indicators_->SetWindowDraggingState(
      SplitViewDragIndicators::ComputeWindowDraggingState(
          /*is_dragging=*/true,
          SplitViewDragIndicators::WindowDraggingState::kFromFloat,
          snap_position_));
}

void TabletModeFloatWindowResizer::CompleteDrag() {
  // We can reach this state if the user hits a state changing accelerator
  // mid-drag.
  aura::Window* float_window = GetTarget();
  if (!WindowState::Get(float_window)->IsFloated())
    return;

  // Revert the drag if the window hasn't moved enough. This will prevent
  // accidental magnetisms.
  const gfx::Vector2dF distance =
      last_location_in_parent_ - details().initial_location_in_parent;
  if (distance.Length() < kMinimumDragDistance) {
    RevertDrag();
    return;
  }

  if (snap_position_ != SplitViewController::SnapPosition::kNone) {
    // Let `SplitViewController` handle windows that should be snapped.
    auto* split_view_controller =
        SplitViewController::Get(Shell::GetPrimaryRootWindow());
    DCHECK(split_view_controller->CanSnapWindow(float_window));
    gfx::PointF location_in_screen = last_location_in_parent_;
    wm::ConvertPointToScreen(float_window->parent(), &location_in_screen);
    split_view_controller->OnWindowDragEnded(
        float_window, snap_position_, gfx::ToRoundedPoint(location_in_screen));
    window_state_->OnCompleteDrag(last_location_in_parent_);
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
  DCHECK(window_state_->IsFloated());
  const ui::GestureEventDetails& details = event->details();
  float velocity_x = 0.f, velocity_y = 0.f;
  if (event->type() == ui::ET_SCROLL_FLING_START) {
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
    DCHECK_EQ(ui::ET_GESTURE_SWIPE, event->type());

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
