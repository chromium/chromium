// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_window_resizer.h"

#include <memory>

#include "ash/display/screen_orientation_controller.h"
#include "ash/public/cpp/window_backdrop.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_drag_indicators.h"
#include "ash/wm/splitview/split_view_types.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/toplevel_window_event_handler.h"
#include "ash/wm/window_resizer.h"
#include "ash/wm/window_util.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace {

// Items dragged to within |kDistanceFromEdgeDp| of the screen will get snapped
// even if they have not moved by |kMinimumDragToSnapDistanceDp|.
constexpr float kDistanceFromEdgeDp = 16.f;

// The minimum distance that an item must be moved before it is snapped. This
// prevents accidental snaps.
constexpr float kMinimumDragToSnapDistanceDp = 96.f;

}  // namespace

namespace ash {

TabletModeWindowResizer::TabletModeWindowResizer(
    WindowState* window_state,
    std::unique_ptr<TabletModeWindowDragDelegate> drag_delegate)
    : WindowResizer(window_state), drag_delegate_(std::move(drag_delegate)) {
  CHECK_EQ(GetTarget(), window_state->window());
  drag_delegate_->StartWindowDrag(GetTarget(),
                                  ConvertAndSetPreviousLocationInScreen(
                                      details().initial_location_in_parent));
}

TabletModeWindowResizer::~TabletModeWindowResizer() {
  window_state_->DeleteDragDetails();
}

void TabletModeWindowResizer::Drag(const gfx::PointF& location_in_parent,
                                   int event_flags) {
  drag_delegate_->ContinueWindowDrag(
      ConvertAndSetPreviousLocationInScreen(location_in_parent),
      CalculateBoundsForDrag(location_in_parent));
  // !!!NOTE!!! `this` may no longer be alive after ContinueWindowDrag.
}

void TabletModeWindowResizer::CompleteDrag() {
  drag_delegate_->EndWindowDrag(ToplevelWindowEventHandler::DragResult::SUCCESS,
                                previous_location_in_screen_);
}

void TabletModeWindowResizer::RevertDrag() {
  drag_delegate_->EndWindowDrag(ToplevelWindowEventHandler::DragResult::REVERT,
                                previous_location_in_screen_);
}

void TabletModeWindowResizer::FlingOrSwipe(ui::GestureEvent*) {
  CompleteDrag();
}

const gfx::PointF&
TabletModeWindowResizer::ConvertAndSetPreviousLocationInScreen(
    const gfx::PointF& location_in_parent) {
  previous_location_in_screen_ = location_in_parent;
  ::wm::ConvertPointToScreen(GetTarget()->parent(),
                             &previous_location_in_screen_);
  return previous_location_in_screen_;
}

TabletModeWindowDragDelegate::TabletModeWindowDragDelegate()
    : split_view_controller_(
          CHECK_DEREF(SplitViewController::Get(Shell::GetPrimaryRootWindow()))),
      split_view_drag_indicators_(std::make_unique<SplitViewDragIndicators>(
          Shell::GetPrimaryRootWindow())) {}

TabletModeWindowDragDelegate::~TabletModeWindowDragDelegate() = default;

void TabletModeWindowDragDelegate::StartWindowDrag(
    aura::Window* dragged_window,
    const gfx::PointF& location_in_screen) {
  CHECK(!dragged_window_);
  dragged_window_ = dragged_window;
  CHECK(dragged_window_);

  initial_location_in_screen_ = location_in_screen;

  WindowBackdrop::Get(dragged_window_)->DisableBackdrop();

  // Prevent the snap ratio from getting updated while the window is resized
  // for dragging, as that could lead to an incorrect split divider position
  // when the window is dragged in split view.
  WindowState::Get(dragged_window_)->set_can_update_snap_ratio(false);

  split_view_controller_->OnWindowDragStarted(dragged_window_);
  split_view_drag_indicators_->SetDraggedWindow(dragged_window_);
}

void TabletModeWindowDragDelegate::ContinueWindowDrag(
    const gfx::PointF& location_in_screen,
    const gfx::Rect& target_bounds) {
  if (target_bounds != dragged_window_->bounds()) {
    // Changing bounds might delete `this` as the dragged tab(s) might merge
    // into another browser window.
    base::WeakPtr<TabletModeWindowDragDelegate> self(
        weak_ptr_factory_.GetWeakPtr());
    dragged_window_->SetBounds(target_bounds);
    if (!self) {
      return;
    }
  }

  SplitViewDragIndicators::WindowDraggingState window_dragging_state =
      SplitViewDragIndicators::ComputeWindowDraggingState(
          /*is_dragging=*/true,
          SplitViewDragIndicators::WindowDraggingState::kFromTop,
          GetSnapPosition(location_in_screen));
  split_view_drag_indicators_->SetWindowDraggingState(window_dragging_state);
}

void TabletModeWindowDragDelegate::EndWindowDrag(
    ToplevelWindowEventHandler::DragResult result,
    const gfx::PointF& location_in_screen) {
  SnapPosition snap_position =
      (result == ToplevelWindowEventHandler::DragResult::SUCCESS)
          ? GetSnapPosition(location_in_screen)
          : SnapPosition::kNone;

  split_view_drag_indicators_->SetWindowDraggingState(
      SplitViewDragIndicators::WindowDraggingState::kNoDrag);

  // Notify SplitViewController so it can do its work (e.g. snap the window).
  split_view_controller_->OnWindowDragEnded(
      dragged_window_, snap_position, gfx::ToRoundedPoint(location_in_screen),
      WindowSnapActionSource::kDragDownFromTopToSnap);

  WindowState::Get(dragged_window_)->set_can_update_snap_ratio(true);
  WindowBackdrop::Get(dragged_window_)->RestoreBackdrop();

  dragged_window_ = nullptr;
}

SnapPosition TabletModeWindowDragDelegate::GetSnapPosition(
    const gfx::PointF& location_in_screen) const {
  if (!split_view_controller_->CanSnapWindow(dragged_window_,
                                             chromeos::kDefaultSnapRatio)) {
    return SnapPosition::kNone;
  }

  // If we're already in split view, determine the snap position simply based on
  // which side of the split view `location_in_screen` is. As a consequence, the
  // snap indicator will show even if not dragging to the screen edge.
  if (split_view_controller_->InSplitViewMode()) {
    return split_view_controller_->ComputeSnapPosition(
        gfx::ToRoundedPoint(location_in_screen));
  }

  const gfx::Rect area =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          dragged_window_);
  SnapPosition snap_position = ::ash::GetSnapPosition(
      Shell::GetPrimaryRootWindow(), dragged_window_,
      gfx::ToRoundedPoint(location_in_screen),
      gfx::ToRoundedPoint(initial_location_in_screen_),
      /*snap_distance_from_edge=*/kDistanceFromEdgeDp,
      /*minimum_drag_distance=*/kMinimumDragToSnapDistanceDp,
      /*horizontal_edge_inset=*/area.width() *
              kHighlightScreenPrimaryAxisRatio +
          kHighlightScreenEdgePaddingDp,
      /*vertical_edge_inset=*/area.height() * kHighlightScreenPrimaryAxisRatio +
          kHighlightScreenEdgePaddingDp);

  // For portrait mode, since the drag always starts from the top of the
  // screen, we only allow the window to be dragged to snap to the bottom of
  // the screen.
  const bool is_landscape = IsCurrentScreenOrientationLandscape();
  const bool is_primary = IsCurrentScreenOrientationPrimary();
  if (!is_landscape &&
      ((is_primary && snap_position == SnapPosition::kPrimary) ||
       (!is_primary && snap_position == SnapPosition::kSecondary))) {
    snap_position = SnapPosition::kNone;
  }

  return snap_position;
}

}  // namespace ash
