// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/snap_group/snap_group.h"

#include <optional>

#include "ash/shell.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_types.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/check.h"
#include "ui/base/hit_test.h"
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

  // Close the divider before we stop observing windows, since
  // `~SplitViewDivider` will try to remove the observers again.
  HideDivider();
  StopObservingWindows();
}

void SnapGroup::ShowDivider() {
  snap_group_divider_.ShowFor(GetEquivalentDividerPosition(
      window1_, /*account_for_divider_width=*/true));
}

void SnapGroup::HideDivider() {
  snap_group_divider_.CloseDividerWidget();
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
  if (window != window1_ && window != window2_) {
    return;
  }

  // `this` will be destroyed after this line.
  SnapGroupController::Get()->RemoveSnapGroup(this);
}

void SnapGroup::OnPreWindowStateTypeChange(WindowState* window_state,
                                           chromeos::WindowStateType old_type) {
  CHECK(old_type == WindowStateType::kPrimarySnapped ||
        old_type == WindowStateType::kSecondarySnapped);
  if (window_state->GetStateType() != old_type) {
    SnapGroupController::Get()->RemoveSnapGroup(this);
  }
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
  const gfx::Rect requested_bounds = GetSnappedWindowBoundsInScreen(
      GetPositionOfSnappedWindow(window), window,
      snap_ratio.value_or(window_util::GetSnapRatioForWindow(window)),
      account_for_divider_width);
  const SetBoundsWMEvent event(requested_bounds, /*animate=*/true);
  WindowState::Get(window)->OnWMEvent(&event);
}

void SnapGroup::ApplyPrimarySnapRatio(float primary_snap_ratio) {
  const int upper_limit =
      GetDividerPositionUpperLimit(window1_->GetRootWindow());
  const int requested_divider_position =
      upper_limit * primary_snap_ratio - kSplitviewDividerShortSideLength / 2.f;

  // TODO(b/5613837): Remove the cyclic dependencies between snapped window
  // bounds calculation and divider position calculation.
  const int actual_divider_position =
      snap_group_divider_.SetDividerPosition(requested_divider_position);

  UpdateSnappedWindowBounds(window1_, /*account_for_divider_width=*/true,
                            primary_snap_ratio);
  UpdateSnappedWindowBounds(window2_, /*account_for_divider_width=*/true,
                            1 - primary_snap_ratio);

  // The actual divider position might differ from the
  // `requested_divider_position` because it needs to be adjusted to accommodate
  // the windows' minimum sizes.
  snap_group_divider_.ShowFor(actual_divider_position);
}

}  // namespace ash
