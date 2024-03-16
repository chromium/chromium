// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/snap_group/snap_group.h"

#include "ash/shell.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/check.h"
#include "base/check_op.h"
#include "chromeos/ui/base/display_util.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

using chromeos::WindowStateType;

}  // namespace

SnapGroup::SnapGroup(aura::Window* window1, aura::Window* window2)
    : split_view_divider_(this) {
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

  ShowDivider();
  StartObservingWindows();
}

SnapGroup::~SnapGroup() {
  // Close the divider before we stop observing windows, since
  // `~SplitViewDivider` will try to remove the observers again.
  HideDivider();
  StopObservingWindows();
}

void SnapGroup::HideDivider() {
  split_view_divider_.CloseDividerWidget();
}

void SnapGroup::ShowDivider() {
  // TODO(b/329890139): Verify whether we should be using
  // `GetEquivalentDividerPosition()` here.
  split_view_divider_.ShowFor(
      GetEquivalentDividerPosition(window1_, /*should_consider_divider=*/true));
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
  if (window != window1_ && window != window2_) {
    return;
  }

  // `this` will be destroyed after this line.
  SnapGroupController::Get()->RemoveSnapGroup(this);
}

void SnapGroup::OnPreWindowStateTypeChange(WindowState* window_state,
                                           chromeos::WindowStateType old_type) {
  if (is_swapping_) {
    // The windows can be swapped without breaking the group.
    return;
  }
  if (chromeos::IsSnappedWindowStateType(old_type) &&
      window_state->IsMinimized()) {
    // The windows can be minimized without breaking the group.
    return;
  }
  // Destroys `this`. Note if a window is still snapped but to the opposite
  // side, it will break the group and SnapGroupController will start overview.
  // If the window was still snapped in the same position and simply changed
  // snap ratios, it would not send a state change and reach here.
  SnapGroupController::Get()->RemoveSnapGroup(this);
}

void SnapGroup::StartResizeWithDivider(const gfx::Point& location_in_screen) {
  // `SplitViewDivider` will do the work to start resizing.
  // TODO(sophiewen): Maybe start performant resizing and add presentation time
  // metrics.
}

void SnapGroup::UpdateResizeWithDivider(const gfx::Point& location_in_screen) {
  CHECK(split_view_divider_.is_resizing_with_divider());
  UpdateSnappedBoundsDuringResize();
}

bool SnapGroup::EndResizeWithDivider(const gfx::Point& location_in_screen) {
  CHECK(!split_view_divider_.is_resizing_with_divider());
  UpdateSnappedBoundsDuringResize();
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
    float snap_ratio) const {
  return CalculateSnappedWindowBoundsInScreen(
      snap_position, window_for_minimum_size->GetRootWindow(),
      window_for_minimum_size, /*account_for_divider_width=*/true,
      split_view_divider_.divider_position(),
      split_view_divider_.is_resizing_with_divider());
}

SnapPosition SnapGroup::GetPositionOfSnappedWindow(
    const aura::Window* window) const {
  // TODO(b/326288377): Make sure this works with ARC windows.
  CHECK(window == window1_ || window == window2_);
  return window == window1_ ? SnapPosition::kPrimary : SnapPosition::kSecondary;
}

aura::Window::Windows SnapGroup::GetLayoutWindows() const {
  return {window1_.get(), window2_.get()};
}

void SnapGroup::StartObservingWindows() {
  CHECK(window1_);
  CHECK(window2_);
  for (aura::Window* window : {window1_, window2_}) {
    window->AddObserver(this);
    WindowState::Get(window)->AddObserver(this);
  }
}

void SnapGroup::StopObservingWindows() {
  for (aura::Window* window : {window1_, window2_}) {
    if (window) {
      window->RemoveObserver(this);
      WindowState::Get(window)->RemoveObserver(this);
    }
  }
  window1_ = nullptr;
  window2_ = nullptr;
}

void SnapGroup::RefreshWindowBoundsInSnapGroup(bool on_snap_group_added) {
  const display::Display& display1 =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window1_);
  const display::Display& display2 =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window2_);

  // TODO(michelefan@): Add multi-display support for snap group.
  DCHECK_EQ(display1, display2);

  gfx::Rect primary_window_bounds =
      window_util::GetTargetScreenBounds(window1_);
  const int primary_x = primary_window_bounds.x();
  const int primary_y = primary_window_bounds.y();
  const int primary_width = primary_window_bounds.width();
  const int primary_height = primary_window_bounds.height();

  gfx::Rect secondary_window_bounds =
      window_util::GetTargetScreenBounds(window2_);
  const int secondary_x = secondary_window_bounds.x();
  const int secondary_y = secondary_window_bounds.y();
  const int secondary_width = secondary_window_bounds.width();
  const int secondary_height = secondary_window_bounds.height();

  const int delta = on_snap_group_added ? kSplitviewDividerShortSideLength / 2
                                        : -kSplitviewDividerShortSideLength / 2;

  if (chromeos::IsLandscapeOrientation(GetSnapDisplayOrientation(display1))) {
    primary_window_bounds.SetRect(primary_x, primary_y, primary_width - delta,
                                  primary_height);
    secondary_window_bounds.SetRect(secondary_x + delta, secondary_y,
                                    secondary_width - delta, secondary_height);
  } else {
    primary_window_bounds.SetRect(primary_x, primary_y, primary_width,
                                  primary_height - delta);
    secondary_window_bounds.SetRect(secondary_x, secondary_y + delta,
                                    secondary_width, secondary_height - delta);
  }

  const SetBoundsWMEvent window1_event(primary_window_bounds, /*animate=*/true);
  WindowState::Get(window1_)->OnWMEvent(&window1_event);
  const SetBoundsWMEvent window2_event(secondary_window_bounds,
                                       /*animate=*/true);
  WindowState::Get(window2_)->OnWMEvent(&window2_event);
}

void SnapGroup::UpdateSnappedBoundsDuringResize() {
  // TODO(sophiewen): Consolidate with
  // `SplitViewController::UpdateSnappedBounds()`.
  for (aura::Window* window : {window1_, window2_}) {
    const SnapPosition snap_position = GetPositionOfSnappedWindow(window);
    const gfx::Rect requested_bounds = GetSnappedWindowBoundsInScreen(
        snap_position, window, window_util::GetSnapRatioForWindow(window));
    const SetBoundsWMEvent event(requested_bounds, /*animate=*/true);
    WindowState::Get(window)->OnWMEvent(&event);
  }

  split_view_divider_.UpdateDividerBounds();
}

}  // namespace ash
