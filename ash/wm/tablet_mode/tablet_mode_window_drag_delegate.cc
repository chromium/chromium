// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_window_drag_delegate.h"

#include "ash/root_window_controller.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/overview/window_grid.h"
#include "ash/wm/overview/window_selector.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/overview/window_selector_item.h"
#include "ash/wm/root_window_finder.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_drag_indicators.h"
#include "ash/wm/window_transient_descendant_iterator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/transform_util.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// The threshold to compute the minimum vertical distance to start showing the
// drag indicators and preview window when dragging a window into splitscreen in
// tablet mode.
constexpr float kIndicatorsThresholdRatio = 0.1;

// Returns the window selector if overview mode is active, otherwise returns
// nullptr.
WindowSelector* GetWindowSelector() {
  return Shell::Get()->window_selector_controller()->IsSelecting()
             ? Shell::Get()->window_selector_controller()->window_selector()
             : nullptr;
}

WindowGrid* GetWindowGrid(aura::Window* dragged_window) {
  if (!GetWindowSelector())
    return nullptr;

  return GetWindowSelector()->GetGridWithRootWindow(
      dragged_window->GetRootWindow());
}

// Returns the drop target in overview during drag.
WindowSelectorItem* GetDropTarget(aura::Window* dragged_window) {
  WindowGrid* window_grid = GetWindowGrid(dragged_window);
  if (!window_grid || window_grid->empty())
    return nullptr;

  return window_grid->GetDropTarget();
}

// Gets the bounds of selected drop target in overview grid that is displaying
// in the same root window as |dragged_window|. Note that the returned bounds is
// scaled-up.
gfx::Rect GetBoundsOfSelectedDropTarget(aura::Window* dragged_window) {
  WindowSelectorItem* drop_target = GetDropTarget(dragged_window);
  if (!drop_target)
    return gfx::Rect();

  return drop_target->GetBoundsOfSelectedItem();
}

}  // namespace

TabletModeWindowDragDelegate::TabletModeWindowDragDelegate()
    : split_view_controller_(Shell::Get()->split_view_controller()),
      split_view_drag_indicators_(std::make_unique<SplitViewDragIndicators>()),
      weak_ptr_factory_(this) {}

TabletModeWindowDragDelegate::~TabletModeWindowDragDelegate() {
  Shell::Get()->UpdateShelfVisibility();
}

void TabletModeWindowDragDelegate::StartWindowDrag(
    aura::Window* dragged_window,
    const gfx::Point& location_in_screen) {
  dragged_window_ = dragged_window;
  initial_location_in_screen_ = location_in_screen;

  PrepareWindowDrag(location_in_screen);

  // Update the shelf's visibility to keep shelf visible during drag.
  RootWindowController::ForWindow(dragged_window_)
      ->GetShelfLayoutManager()
      ->UpdateVisibilityState();

  // Disable the backdrop on the dragged window.
  original_backdrop_mode_ = dragged_window_->GetProperty(kBackdropWindowMode);
  dragged_window_->SetProperty(kBackdropWindowMode,
                               BackdropWindowMode::kDisabled);

  WindowSelectorController* controller =
      Shell::Get()->window_selector_controller();
  bool was_overview_open = controller->IsSelecting();

  // If the dragged window is one of the snapped windows, SplitViewController
  // might open overview in the dragged window side of the screen.
  split_view_controller_->OnWindowDragStarted(dragged_window_);

  if (ShouldOpenOverviewWhenDragStarts() && !controller->IsSelecting()) {
    controller->ToggleOverview(
        WindowSelector::EnterExitOverviewType::kWindowDragged);
  }

  if (controller->IsSelecting()) {
    // Only do animation if overview was open before the drag started. If the
    // overview is opened because of the window drag, do not do animation.
    GetWindowSelector()->OnWindowDragStarted(dragged_window_,
                                             /*animate=*/was_overview_open);
  }

  bounds_of_selected_drop_target_ =
      GetBoundsOfSelectedDropTarget(dragged_window_);

  // Update the dragged window's shadow. It should have the active window
  // shadow during dragging.
  original_shadow_elevation_ =
      ::wm::GetShadowElevationConvertDefault(dragged_window_);
  ::wm::SetShadowElevation(dragged_window_, ::wm::kShadowElevationActiveWindow);
}

void TabletModeWindowDragDelegate::ContinueWindowDrag(
    const gfx::Point& location_in_screen,
    UpdateDraggedWindowType type,
    const gfx::Rect& target_bounds) {
  if (!did_move_) {
    const gfx::Rect work_area_bounds =
        display::Screen::GetScreen()
            ->GetDisplayNearestWindow(dragged_window_)
            .work_area();
    if (location_in_screen.y() >=
        GetIndicatorsVerticalThreshold(work_area_bounds)) {
      did_move_ = true;
    }
  }

  if (type == UpdateDraggedWindowType::UPDATE_BOUNDS) {
    // UPDATE_BOUNDS is used when dragging tab(s) out of a browser window.
    // Changing bounds might delete ourselves as the dragged (browser) window
    // tab(s) might merge into another browser window.
    base::WeakPtr<TabletModeWindowDragDelegate> delegate(
        weak_ptr_factory_.GetWeakPtr());
    if (target_bounds != dragged_window_->bounds()) {
      dragged_window_->SetBounds(target_bounds);
      if (!delegate)
        return;
    }
  } else {  // type == UpdateDraggedWindowType::UPDATE_TRANSFORM
    // UPDATE_TRANSFORM is used when dragging an entire window around, either
    // it's a browser window or an app window.
    UpdateDraggedWindowTransform(location_in_screen);
  }

  // For child classes to do their special handling if any.
  UpdateWindowDrag(location_in_screen);

  // Update drag indicators and preview window if necessary.
  IndicatorState indicator_state = GetIndicatorState(location_in_screen);
  split_view_drag_indicators_->SetIndicatorState(indicator_state,
                                                 location_in_screen);

  if (GetWindowSelector()) {
    GetWindowSelector()->OnWindowDragContinued(
        dragged_window_, location_in_screen, indicator_state);
  }
}

void TabletModeWindowDragDelegate::EndWindowDrag(
    wm::WmToplevelWindowEventHandler::DragResult result,
    const gfx::Point& location_in_screen) {
  EndingWindowDrag(result, location_in_screen);

  dragged_window_->SetProperty(kBackdropWindowMode, original_backdrop_mode_);
  SplitViewController::SnapPosition snap_position = SplitViewController::NONE;
  if (result == wm::WmToplevelWindowEventHandler::DragResult::SUCCESS &&
      split_view_controller_->CanSnap(dragged_window_)) {
    snap_position = GetSnapPosition(location_in_screen);
  }

  // The window might merge into an overview window or become a new window item
  // in overview mode.
  if (GetWindowSelector()) {
    GetWindowSelector()->OnWindowDragEnded(
        dragged_window_, location_in_screen,
        ShouldDropWindowIntoOverview(snap_position, location_in_screen));
  }
  split_view_controller_->OnWindowDragEnded(dragged_window_, snap_position,
                                            location_in_screen);
  split_view_drag_indicators_->SetIndicatorState(IndicatorState::kNone,
                                                 location_in_screen);

  // Reset the dragged window's window shadow elevation.
  ::wm::SetShadowElevation(dragged_window_, original_shadow_elevation_);

  // For child class to do its special handling if any.
  EndedWindowDrag(location_in_screen);
  dragged_window_ = nullptr;
  did_move_ = false;
}

void TabletModeWindowDragDelegate::FlingOrSwipe(ui::GestureEvent* event) {
  StartFling(event);
  EndWindowDrag(wm::WmToplevelWindowEventHandler::DragResult::SUCCESS,
                GetEventLocationInScreen(event));
}

gfx::Point TabletModeWindowDragDelegate::GetEventLocationInScreen(
    const ui::GestureEvent* event) const {
  gfx::Point location_in_screen(event->location());
  ::wm::ConvertPointToScreen(static_cast<aura::Window*>(event->target()),
                             &location_in_screen);
  return location_in_screen;
}

IndicatorState TabletModeWindowDragDelegate::GetIndicatorState(
    const gfx::Point& location_in_screen) const {
  SplitViewController::SnapPosition snap_position =
      GetSnapPosition(location_in_screen);
  const bool can_snap = split_view_controller_->CanSnap(dragged_window_);
  if (snap_position != SplitViewController::NONE &&
      !split_view_controller_->IsSplitViewModeActive() && can_snap) {
    return snap_position == SplitViewController::LEFT
               ? IndicatorState::kPreviewAreaLeft
               : IndicatorState::kPreviewAreaRight;
  }

  // Do not show the drag indicators if split view mode is active.
  if (split_view_controller_->IsSplitViewModeActive())
    return IndicatorState::kNone;

  // If the event location hasn't passed the indicator vertical threshold, do
  // not show the drag indicators.
  const gfx::Rect work_area_bounds =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(dragged_window_)
          .work_area();
  if (!did_move_ && location_in_screen.y() <
                        GetIndicatorsVerticalThreshold(work_area_bounds)) {
    return IndicatorState::kNone;
  }

  // No top drag indicator if in portrait screen orientation.
  if (split_view_controller_->IsCurrentScreenOrientationLandscape())
    return can_snap ? IndicatorState::kDragArea : IndicatorState::kCannotSnap;

  return can_snap ? IndicatorState::kDragAreaRight
                  : IndicatorState::kCannotSnapRight;
}

bool TabletModeWindowDragDelegate::ShouldOpenOverviewWhenDragStarts() {
  DCHECK(dragged_window_);
  return true;
}

int TabletModeWindowDragDelegate::GetIndicatorsVerticalThreshold(
    const gfx::Rect& work_area_bounds) const {
  return work_area_bounds.y() +
         work_area_bounds.height() * kIndicatorsThresholdRatio;
}

SplitViewController::SnapPosition TabletModeWindowDragDelegate::GetSnapPosition(
    const gfx::Point& location_in_screen) const {
  // If split view mode is active during dragging, the dragged window will be
  // either snapped left or right (if it's not merged into overview window),
  // depending on the relative position of |location_in_screen| and the current
  // divider position.
  const bool is_landscape =
      split_view_controller_->IsCurrentScreenOrientationLandscape();
  const bool is_primary =
      split_view_controller_->IsCurrentScreenOrientationPrimary();
  if (split_view_controller_->IsSplitViewModeActive()) {
    const int position =
        is_landscape ? location_in_screen.x() : location_in_screen.y();
    if (position < split_view_controller_->divider_position()) {
      return is_primary ? SplitViewController::LEFT
                        : SplitViewController::RIGHT;
    } else {
      return is_primary ? SplitViewController::RIGHT
                        : SplitViewController::LEFT;
    }
  }

  // Otherwise, the user has to drag pass the indicator vertical threshold to
  // snap the window.
  gfx::Rect work_area_bounds = display::Screen::GetScreen()
                                   ->GetDisplayNearestWindow(dragged_window_)
                                   .work_area();
  if (!did_move_ && location_in_screen.y() <
                        GetIndicatorsVerticalThreshold(work_area_bounds)) {
    return SplitViewController::NONE;
  }

  // Check to see if the current event location |location_in_screen|is within
  // the drag indicators bounds.
  if (is_landscape) {
    const int screen_edge_inset =
        work_area_bounds.width() * kHighlightScreenPrimaryAxisRatio +
        kHighlightScreenEdgePaddingDp;
    work_area_bounds.Inset(screen_edge_inset, 0);
    if (location_in_screen.x() < work_area_bounds.x()) {
      return is_primary ? SplitViewController::LEFT
                        : SplitViewController::RIGHT;
    }
    if (location_in_screen.x() >= work_area_bounds.right()) {
      return is_primary ? SplitViewController::RIGHT
                        : SplitViewController::LEFT;
    }
    return SplitViewController::NONE;
  }
  // For portrait mode, since the drag always starts from the top of the
  // screen, we only allow the window to be dragged to snap to the bottom of
  // the screen.
  const int screen_edge_inset =
      work_area_bounds.height() * kHighlightScreenPrimaryAxisRatio +
      kHighlightScreenEdgePaddingDp;
  work_area_bounds.Inset(0, screen_edge_inset);
  if (location_in_screen.y() >= work_area_bounds.bottom())
    return is_primary ? SplitViewController::RIGHT : SplitViewController::LEFT;

  return SplitViewController::NONE;
}

void TabletModeWindowDragDelegate::UpdateDraggedWindowTransform(
    const gfx::Point& location_in_screen) {
  DCHECK(Shell::Get()->window_selector_controller()->IsSelecting());

  // Calculate the desired scale along the y-axis. The scale of the window
  // during drag is based on the distance from |y_location_in_screen| to the y
  // position of |bounds_of_selected_drop_target_|. The dragged windowt will
  // become smaller when it becomes nearer to the drop target. And then keep the
  // minimum scale if it has been dragged further than the drop target.
  float scale = static_cast<float>(bounds_of_selected_drop_target_.height()) /
                static_cast<float>(dragged_window_->bounds().height());
  int y_full = bounds_of_selected_drop_target_.y();
  int y_diff = y_full - location_in_screen.y();
  if (y_diff >= 0)
    scale = (1.0f - scale) * y_diff / y_full + scale;

  gfx::Transform transform;
  const gfx::Rect window_bounds = dragged_window_->bounds();
  transform.Translate(
      (location_in_screen.x() - window_bounds.x()) -
          (initial_location_in_screen_.x() - window_bounds.x()) * scale,
      (location_in_screen.y() - window_bounds.y()) -
          (initial_location_in_screen_.y() - window_bounds.y()) * scale);
  transform.Scale(scale, scale);
  SetTransform(dragged_window_, transform);
}

bool TabletModeWindowDragDelegate::ShouldDropWindowIntoOverview(
    SplitViewController::SnapPosition snap_position,
    const gfx::Point& location_in_screen) {
  bool is_split_view_active = split_view_controller_->IsSplitViewModeActive();
  // Do not drop the dragged window into overview if preview area is shown.
  if (snap_position != SplitViewController::NONE && !is_split_view_active)
    return false;

  WindowSelectorItem* drop_target = GetDropTarget(dragged_window_);
  if (!drop_target)
    return false;

  WindowGrid* window_grid = GetWindowGrid(dragged_window_);
  aura::Window* target_window =
      window_grid->GetTargetWindowOnLocation(location_in_screen);
  const bool is_drop_target_selected =
      target_window && window_grid->IsDropTargetWindow(target_window);

  // TODO(crbug.com/878294): Should also consider drag distance when splitview
  // is active.
  if (is_split_view_active)
    return is_drop_target_selected;

  const gfx::Rect work_area_bounds =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(dragged_window_)
          .work_area();
  return is_drop_target_selected ||
         (location_in_screen.y() - work_area_bounds.y()) >=
             kDragPositionToOverviewRatio *
                 (drop_target->GetTransformedBounds().y() -
                  work_area_bounds.y());
}

bool TabletModeWindowDragDelegate::ShouldFlingIntoOverview(
    const ui::GestureEvent* event) const {
  if (event->type() != ui::ET_SCROLL_FLING_START)
    return false;

  // Only fling into overview if overview is currently open. In some case,
  // overview is not opened when drag starts (if it's tab-dragging and the
  // dragged window is not the same with the source window), we should not fling
  // the dragged window into overview in this case.
  if (!Shell::Get()->window_selector_controller()->IsSelecting())
    return false;

  const gfx::Point location_in_screen = GetEventLocationInScreen(event);
  const IndicatorState indicator_state = GetIndicatorState(location_in_screen);
  const bool is_landscape =
      split_view_controller_->IsCurrentScreenOrientationLandscape();
  const float velocity = is_landscape ? event->details().velocity_x()
                                      : event->details().velocity_y();

  // Drop the window into overview if fling with large enough velocity to the
  // opposite snap position when preview area is shown.
  if (split_view_controller_->IsCurrentScreenOrientationPrimary()) {
    if (indicator_state == IndicatorState::kPreviewAreaLeft)
      return velocity > kFlingToOverviewFromSnappingAreaThreshold;
    else if (indicator_state == IndicatorState::kPreviewAreaRight)
      return -velocity > kFlingToOverviewFromSnappingAreaThreshold;
  } else {
    if (indicator_state == IndicatorState::kPreviewAreaLeft)
      return -velocity > kFlingToOverviewFromSnappingAreaThreshold;
    else if (indicator_state == IndicatorState::kPreviewAreaRight)
      return velocity > kFlingToOverviewFromSnappingAreaThreshold;
  }

  const SplitViewController::State snap_state = split_view_controller_->state();
  const int end_position =
      is_landscape ? location_in_screen.x() : location_in_screen.y();
  // Fling the window when splitview is active. Since each snapping area in
  // splitview has a corresponding snap position. Fling the window to the
  // opposite position of the area's snap position with large enough velocity
  // should drop the window into overview grid.
  if (snap_state == SplitViewController::LEFT_SNAPPED ||
      snap_state == SplitViewController::RIGHT_SNAPPED) {
    return end_position > split_view_controller_->divider_position()
               ? -velocity > kFlingToOverviewFromSnappingAreaThreshold
               : velocity > kFlingToOverviewFromSnappingAreaThreshold;
  }

  // Consider only the velocity_y if splitview is not active and preview area is
  // not shown.
  return event->details().velocity_y() > kFlingToOverviewThreshold;
}

}  // namespace ash
