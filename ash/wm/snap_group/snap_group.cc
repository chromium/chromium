// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/snap_group/snap_group.h"

#include <optional>

#include "ash/shell.h"
#include "ash/wm/overview/scoped_overview_hide_windows.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_types.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/check.h"
#include "base/check_op.h"
#include "ui/base/hit_test.h"
#include "ui/display/display_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/range/range_f.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

using chromeos::WindowStateType;

}  // namespace

SnapGroup::SnapGroup(aura::Window* window1, aura::Window* window2)
    : snap_group_divider_(this) {
  auto* window_state1 = WindowState::Get(window1);
  auto* window_state2 = WindowState::Get(window2);
  CHECK(window_state1->IsSnapped() && window_state2->IsSnapped() &&
        window_state1->GetStateType() != window_state2->GetStateType());

  // Always assign `window1_` to the primary window and `window2_` to the
  // secondary window.
  if (window_state1->GetStateType() ==
      chromeos::WindowStateType::kPrimarySnapped) {
    window1_ = window1;
    window2_ = window2;
  } else {
    window1_ = window2;
    window2_ = window1;
  }

  StartObservingWindows();
  ShowDivider();
}

SnapGroup::~SnapGroup() {
  // Restore the snapped window bounds that were adjusted to make room for
  // divider when snap group was created.
  UpdateGroupWindowsBounds(/*account_for_divider_width=*/false);

  // `SplitViewDivider::MaybeRemoveObservedWindow()` will close the divider.
  StopObservingWindows();
}

const aura::Window* SnapGroup::GetWindowOfSnapViewType(
    SnapViewType snap_type) const {
  return snap_type == SnapViewType::kPrimary ? window1_ : window2_;
}

void SnapGroup::ShowDivider() {
  const gfx::Rect window1_bounds = window1_->GetBoundsInScreen();
  const gfx::Rect window2_bounds = window2_->GetBoundsInScreen();
  int edge_gap = 0;
  if (IsSnapGroupLayoutHorizontal()) {
    edge_gap = window2_bounds.x() - window1_bounds.right();
  } else {
    edge_gap = window2_bounds.y() - window1_bounds.bottom();
  }

  // We should account for the divider width only if the space between two
  // windows is smaller than `kSplitviewDividerShortSideLength`. This adjustment
  // is necessary when restoring a snap group on Overview exit for example, as
  // the gap might have been created.
  // TODO(michelefan): See if there are other conditions where we need to
  // account for the divider.
  const bool account_for_divider_width =
      edge_gap < kSplitviewDividerShortSideLength;

  snap_group_divider_.SetDividerPosition(
      GetEquivalentDividerPosition(window1_, account_for_divider_width));
  snap_group_divider_.SetVisible(true);
}

void SnapGroup::HideDivider() {
  snap_group_divider_.SetVisible(false);
}

bool SnapGroup::IsSnapGroupLayoutHorizontal() {
  return IsLayoutHorizontal(GetRootWindow());
}

void SnapGroup::OnLocatedEvent(ui::LocatedEvent* event) {
  CHECK(event->type() == ui::ET_MOUSE_DRAGGED ||
        event->type() == ui::ET_TOUCH_MOVED ||
        event->type() == ui::ET_GESTURE_SCROLL_UPDATE);

  aura::Window* target = static_cast<aura::Window*>(event->target());
  const int client_component =
      window_util::GetNonClientComponent(target, event->location());
  if (client_component != HTCAPTION && client_component != HTCLIENT) {
    return;
  }
  // When the window is dragged via the caption bar to unsnap, we early hide the
  // divider to avoid re-stacking the divider on top of the dragged window.
  gfx::Point location_in_screen = event->location();
  wm::ConvertPointToScreen(target, &location_in_screen);
  if (window1_->GetBoundsInScreen().Contains(location_in_screen) ||
      window2_->GetBoundsInScreen().Contains(location_in_screen)) {
    HideDivider();
  }
}

aura::Window* SnapGroup::GetTopMostWindowInGroup() const {
  return window_util::IsStackedBelow(window1_, window2_) ? window2_ : window1_;
}

void SnapGroup::MinimizeWindows() {
  auto* window1_state = WindowState::Get(window1_);
  auto* window2_state = WindowState::Get(window2_);
  CHECK(!window1_state->IsMinimized() && !window2_state->IsMinimized());
  window1_state->Minimize();
  window2_state->Minimize();
}

void SnapGroup::OnWindowDestroying(aura::Window* window) {
  DCHECK(window == window1_ || window == window2_);
  // `this` will be destroyed after this line.
  SnapGroupController::Get()->RemoveSnapGroup(this);
}

void SnapGroup::OnWindowAddedToRootWindow(aura::Window* window) {
  DCHECK(window == window1_ || window == window2_);
  // Skip any recursive updates during the other window move.
  if (is_moving_display_) {
    return;
  }

  base::AutoReset<bool> lock(&is_moving_display_, true);

  const bool cached_divider_visibility =
      snap_group_divider_.divider_widget()->IsVisible();

  if (cached_divider_visibility) {
    // Hide the divider, then move the other window to the same display as the
    // moved `window`.
    snap_group_divider_.SetVisible(false);
  }

  window_util::MoveWindowToDisplay(
      window == window1_ ? window2_ : window1_,
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(window->GetRootWindow())
          .id());

  // Restore the divider visibility after both windows are moved to the target
  // display.
  snap_group_divider_.SetVisible(cached_divider_visibility);
  ApplyPrimarySnapRatio(WindowState::Get(window1_)->snap_ratio().value_or(
      chromeos::kDefaultSnapRatio));
}

void SnapGroup::OnPreWindowStateTypeChange(WindowState* window_state,
                                           chromeos::WindowStateType old_type) {
  CHECK(old_type == WindowStateType::kPrimarySnapped ||
        old_type == WindowStateType::kSecondarySnapped);
  if (window_state->GetStateType() != old_type) {
    SnapGroupController::Get()->RemoveSnapGroup(this);
  }
}

aura::Window* SnapGroup::GetRootWindow() {
  // This can be called during dragging window out of a snap group to another
  // display.
  // TODO(b/331993231): Update the root window in `OnWindowParentChanged()`.
  return window1_->GetRootWindow();
}

void SnapGroup::StartResizeWithDivider(const gfx::Point& location_in_screen) {
  // `SplitViewDivider` will do the work to start resizing.
  // TODO(sophiewen): Maybe start performant resizing and add presentation time
  // metrics.
}

void SnapGroup::UpdateResizeWithDivider(const gfx::Point& location_in_screen) {
  CHECK(snap_group_divider_.is_resizing_with_divider());
  UpdateGroupWindowsBounds(/*account_for_divider_width=*/true);
}

bool SnapGroup::EndResizeWithDivider(const gfx::Point& location_in_screen) {
  CHECK(!snap_group_divider_.is_resizing_with_divider());
  UpdateGroupWindowsBounds(/*account_for_divider_width=*/true);
  // We return true since we are done with resizing and can hand back work to
  // `SplitViewDivider`. See `SplitViewDivider::EndResizeWithDivider()`.
  return true;
}

void SnapGroup::OnResizeEnding() {}

void SnapGroup::OnResizeEnded() {}

void SnapGroup::SwapWindows() {
  // TODO(b/326481241): Currently disabled for Snap Groups. Re-enable this after
  // we have a holistic fix.
}

gfx::Rect SnapGroup::GetSnappedWindowBoundsInScreen(
    SnapPosition snap_position,
    aura::Window* window_for_minimum_size,
    float snap_ratio,
    bool account_for_divider_width) const {
  // Adjust the `snap_group_divider_` position, since
  // `CalculateSnappedWindowBoundsInScreen()` in split_view_utils.cc calculate
  // window bounds based on the divider position.
  const int original_divider_position = snap_group_divider_.divider_position();
  const int divider_position =
      account_for_divider_width
          ? original_divider_position
          : original_divider_position + kSplitviewDividerShortSideLength / 2.f;
  return CalculateSnappedWindowBoundsInScreen(
      snap_position, window_for_minimum_size->GetRootWindow(),
      window_for_minimum_size, account_for_divider_width, divider_position,
      snap_group_divider_.is_resizing_with_divider());
}

SnapPosition SnapGroup::GetPositionOfSnappedWindow(
    const aura::Window* window) const {
  // TODO(b/326288377): Make sure this works with ARC windows.
  CHECK(window == window1_ || window == window2_);
  return window == window1_ ? SnapPosition::kPrimary : SnapPosition::kSecondary;
}

void SnapGroup::OnDisplayMetricsChanged(const display::Display& display,
                                        uint32_t metrics) {
  if (window1_->GetRootWindow() !=
      Shell::GetRootWindowForDisplayId(display.id())) {
    return;
  }

  // Divider widiget can be invisible in Overview mode.
  auto* divider_widget = snap_group_divider_.divider_widget();
  if (!divider_widget || !divider_widget->IsVisible()) {
    return;
  }

  if (!(metrics &
        (DISPLAY_METRIC_BOUNDS | DISPLAY_METRIC_ROTATION |
         DISPLAY_METRIC_DEVICE_SCALE_FACTOR | DISPLAY_METRIC_WORK_AREA))) {
    return;
  }

  const auto window1_snap_ratio = WindowState::Get(window1_)->snap_ratio();
  CHECK(window1_snap_ratio);

  // Update the bounds of the snapped window and divider while preserving the
  // snap ratio.
  ApplyPrimarySnapRatio(*window1_snap_ratio);
}

void SnapGroup::StartObservingWindows() {
  CHECK(window1_);
  CHECK(window2_);
  for (aura::Window* window : {window1_, window2_}) {
    window->AddObserver(this);
    WindowState::Get(window)->AddObserver(this);
    snap_group_divider_.MaybeAddObservedWindow(window);
  }
}

void SnapGroup::StopObservingWindows() {
  for (aura::Window* window : {window1_, window2_}) {
    if (window) {
      window->RemoveObserver(this);
      WindowState::Get(window)->RemoveObserver(this);
      snap_group_divider_.MaybeRemoveObservedWindow(window);
    }
  }
  window1_ = nullptr;
  window2_ = nullptr;
}

void SnapGroup::UpdateGroupWindowsBounds(bool account_for_divider_width) {
  // Return early if in tablet mode, `SplitViewController` will handle window
  // bounds update.
  if (Shell::Get()->IsInTabletMode()) {
    return;
  }

  for (aura::Window* window : {window1_, window2_}) {
    UpdateSnappedWindowBounds(window, account_for_divider_width, std::nullopt);
  }
}

void SnapGroup::UpdateSnappedWindowBounds(aura::Window* window,
                                          bool account_for_divider_width,
                                          std::optional<float> snap_ratio) {
  gfx::Rect requested_bounds = GetSnappedWindowBoundsInScreen(
      GetPositionOfSnappedWindow(window), window,
      snap_ratio.value_or(window_util::GetSnapRatioForWindow(window)),
      account_for_divider_width);
  // Convert window bounds to parent coordinates to ensure correct window bounds
  // are applied when window is moved across displays (see regression
  // http://b/331663949).
  wm::ConvertRectFromScreen(window->GetRootWindow(), &requested_bounds);
  const SetBoundsWMEvent event(requested_bounds, /*animate=*/false);
  WindowState::Get(window)->OnWMEvent(&event);
}

void SnapGroup::ApplyPrimarySnapRatio(float primary_snap_ratio) {
  const int upper_limit = GetDividerPositionUpperLimit(GetRootWindow());
  const int requested_divider_position =
      upper_limit * primary_snap_ratio - kSplitviewDividerShortSideLength / 2.f;

  // TODO(b/5613837): Remove the cyclic dependencies between snapped window
  // bounds calculation and divider position calculation.
  // `SplitViewDivider::SetDividerPosition()` will account for the windows'
  // minimum sizes.
  snap_group_divider_.SetDividerPosition(requested_divider_position);

  UpdateSnappedWindowBounds(window1_, /*account_for_divider_width=*/true,
                            primary_snap_ratio);
  UpdateSnappedWindowBounds(window2_, /*account_for_divider_width=*/true,
                            1 - primary_snap_ratio);
}

void SnapGroup::OnOverviewModeStarting() {
  SplitViewController* split_view_constroller =
      SplitViewController::Get(GetRootWindow());
  SplitViewController::State split_view_state = split_view_constroller->state();

  // Hide windows in the snap group in partial Overview.
  if (split_view_state == SplitViewController::State::kPrimarySnapped ||
      split_view_state == SplitViewController::State::kSecondarySnapped) {
    const std::vector<raw_ptr<aura::Window, VectorExperimental>> hide_windows{
        window1_.get(), window2_.get()};

    hide_windows_in_partial_overview_ =
        std::make_unique<ScopedOverviewHideWindows>(
            /*windows=*/hide_windows,
            /*force_hidden=*/true);
  }
}

void SnapGroup::OnOverviewModeEnding() {
  hide_windows_in_partial_overview_.reset();
}

}  // namespace ash
