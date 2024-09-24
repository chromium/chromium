// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/snap_group/snap_group.h"

#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ash/wm/snap_group/snap_group_metrics.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_types.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/metrics/user_metrics.h"
#include "base/time/time.h"
#include "chromeos/ui/base/window_state_type.h"
#include "ui/base/hit_test.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

using chromeos::WindowStateType;

// Maps `SnapGroupExitPoint` related to window state change with the given
// `window_state` type.
SnapGroupExitPoint GetWindowStateChangeExitPoint(WindowState* window_state) {
  WindowStateType state_type = window_state->GetStateType();
  switch (state_type) {
    case WindowStateType::kDefault:
      return SnapGroupExitPoint::kWindowStateChangedDefault;
    case WindowStateType::kNormal:
      return SnapGroupExitPoint::kWindowStateChangedNormal;
    case WindowStateType::kMinimized:
      return SnapGroupExitPoint::kWindowStateChangedMinimized;
    case WindowStateType::kMaximized:
      return SnapGroupExitPoint::kWindowStateChangedMaximized;
    case WindowStateType::kInactive:
      return SnapGroupExitPoint::kWindowStateChangedInactive;
    case WindowStateType::kFullscreen:
      return SnapGroupExitPoint::kWindowStateChangedFullscreen;
    case WindowStateType::kPrimarySnapped:
      return SnapGroupExitPoint::kWindowStateChangedPrimarySnapped;
    case WindowStateType::kSecondarySnapped:
      return SnapGroupExitPoint::kWindowStateChangedSecondarySnapped;
    case WindowStateType::kPinned:
      return SnapGroupExitPoint::kWindowStateChangedPinned;
    case WindowStateType::kTrustedPinned:
      return SnapGroupExitPoint::kWindowStateChangedTrustedPinned;
    case WindowStateType::kPip:
      return SnapGroupExitPoint::kWindowStateChangedPip;
    case WindowStateType::kFloated:
      return SnapGroupExitPoint::kWindowStateChangedFloated;
  }
}

// Note this is different from `CalculateDividerPosition()` in
// `split_view_utils` which subtracts `kSplitviewDividerShortSideLength` instead
// of `kSplitviewDividerShortSideLength / 2`. Needed because of the different
// calculations in `SnapGroup::GetSnappedWindowBoundsInScreen()`.
// TODO(b/331304137): Remove the cyclic dependencies between snapped window
// bounds calculation and divider position calculation.
// TODO(b/347723336): See if we can unify the two `CalculateDividerPosition()`s.
int CalculateDividerPosition(aura::Window* root_window,
                             float primary_snap_ratio) {
  const int upper_limit = GetDividerPositionUpperLimit(root_window);
  const int requested_divider_position =
      upper_limit * primary_snap_ratio - kSplitviewDividerShortSideLength / 2.f;
  return requested_divider_position;
}

}  // namespace

SnapGroup::SnapGroup(aura::Window* window1,
                     aura::Window* window2,
                     std::optional<base::TimeTicks> sticky_creation_time)
    : snap_group_divider_(this),
      carry_over_creation_time_(
          sticky_creation_time.value_or(base::TimeTicks().Now())),
      actual_creation_time_(base::TimeTicks().Now()) {
  CHECK_EQ(window1->parent(), window2->parent());

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

  // We manually add ourselves as a display observer so we can early remove
  // ourselves in `Shutdown()`.
  display::Screen::GetScreen()->AddObserver(this);
  Shell::Get()->activation_client()->AddObserver(this);
}

SnapGroup::~SnapGroup() {
  if (!is_shutting_down_) {
    Shutdown();
  }
}

void SnapGroup::Shutdown() {
  is_shutting_down_ = true;

  window_to_target_snap_position_map_.clear();

  Shell::Get()->activation_client()->RemoveObserver(this);
  display::Screen::GetScreen()->RemoveObserver(this);

  // Restore the snapped window bounds that were adjusted to make room for
  // divider when snap group was created.
  UpdateGroupWindowsBounds(/*account_for_divider_width=*/false);

  StopObservingWindows();
}

gfx::Rect SnapGroup::GetSnappedWindowBoundsInRoot(
    aura::Window* window,
    const chromeos::WindowStateType state_type,
    float snap_ratio) {
  // TODO(b/347723336): Find a deterministic way to determine
  // `account_for_divider_width`.
  // First update the divider position so we can get the correct bounds in
  // `GetSnappedWindowBoundsInScreen()`.
  // TODO(b/331304137): Remove the cyclic dependencies between snapped window
  // bounds calculation and divider position calculation.
  const float primary_snap_ratio =
      state_type == chromeos::WindowStateType::kPrimarySnapped
          ? snap_ratio
          : 1.f - snap_ratio;
  snap_group_divider_.SetDividerPosition(
      CalculateDividerPosition(GetRootWindow(), primary_snap_ratio));
  gfx::Rect bounds_in_parent = GetSnappedWindowBoundsInScreen(
      ToSnapPosition(state_type), window, snap_ratio,
      /*account_for_divider_width=*/
      snap_group_divider_.IsDividerWidgetVisible());
  wm::ConvertRectFromScreen(window->GetRootWindow(), &bounds_in_parent);
  return bounds_in_parent;
}

aura::Window* SnapGroup::GetPhysicallyLeftOrTopWindow() {
  return IsPhysicallyLeftOrTop(window1_) ? window1_ : window2_;
}

aura::Window* SnapGroup::GetPhysicallyRightOrBottomWindow() {
  return IsPhysicallyLeftOrTop(window1_) ? window2_ : window1_;
}

void SnapGroup::ShowDivider() {
  // No-op if the divider is visible already. This may happen if the window is
  // selected from partial overview to form a snap group, upon which
  // `SnapGroupController::OnOverviewModeEndingAnimationComplete()` will attempt
  // to show the divider again.
  if (snap_group_divider_.IsDividerWidgetVisible()) {
    return;
  }

  // TODO(b/338130287): Determine whether `window1_` should always be
  // `primary_window`.
  const bool is_left_or_top = IsPhysicallyLeftOrTop(window1_);
  aura::Window* primary_window = is_left_or_top ? window1_ : window2_;
  aura::Window* secondary_window = is_left_or_top ? window2_ : window1_;

  const gfx::Rect window1_bounds = primary_window->GetTargetBounds();
  const gfx::Rect window2_bounds = secondary_window->GetTargetBounds();

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
      GetEquivalentDividerPosition(primary_window, account_for_divider_width));
  snap_group_divider_.SetVisible(true);
}

void SnapGroup::HideDivider() {
  snap_group_divider_.SetVisible(false);
}

bool SnapGroup::IsSnapGroupLayoutHorizontal() const {
  return IsLayoutHorizontal(GetRootWindow());
}

void SnapGroup::OnLocatedEvent(ui::LocatedEvent* event) {
  if (is_shutting_down_) {
    return;
  }

  // `ToplevelWindowEventHandler` continues to process drag events in Overview
  // mode, potentially leading to group removal and crashes in
  // `OverviewGrid::RemoveItem()`. To prevent groups from being removed in
  // Overview (forwarded from `ToplevelWindowEventHandler::HandleDrag()`) and
  // subsequent crashes, early return here.
  if (IsInOverviewSession()) {
    return;
  }

  CHECK(event->type() == ui::EventType::kMouseDragged ||
        event->type() == ui::EventType::kTouchMoved ||
        event->type() == ui::EventType::kGestureScrollUpdate);

  aura::Window* target = static_cast<aura::Window*>(event->target());
  const int client_component =
      window_util::GetNonClientComponent(target, event->location());
  if (client_component != HTCAPTION && client_component != HTCLIENT) {
    return;
  }

  // When the window is dragged via the caption bar to unsnap, we early break
  // the group to avoid re-stacking the divider on top of the dragged window.
  if (window1_->Contains(target) || window2_->Contains(target)) {
    SnapGroupController::Get()->RemoveSnapGroup(
        this, SnapGroupExitPoint::kDragWindowOut);
  }
}

aura::Window* SnapGroup::GetTopMostWindowInGroup() const {
  // Two windows can be on different roots during the process of being moved to
  // another display, return the one on the same root as the current cursor
  // position.
  aura::Window* window1_root_window = window1_->GetRootWindow();
  aura::Window* window2_root_window = window2_->GetRootWindow();
  if (window1_root_window != window2_root_window) {
    aura::Window* cursor_root_window = window_util::GetRootWindowAt(
        display::Screen::GetScreen()->GetCursorScreenPoint());
    return window1_root_window == cursor_root_window ? window1_root_window
                                                     : window2_root_window;
  }

  // Two windows can be on the same root but different desk containers during
  // the process of being moved to another desk, return the one on the active
  // desk container.
  if (window1_->parent() != window2_->parent()) {
    return desks_util::BelongsToActiveDesk(window1_) ? window1_ : window2_;
  }

  return window_util::IsStackedBelow(window1_, window2_) ? window2_ : window1_;
}

void SnapGroup::RefreshSnapGroup() {
  if (is_shutting_down_) {
    return;
  }

  // `RefreshSnapGroup()` may be called during a work area change triggered by
  // other pre-window state type change events, during which the windows may no
  // longer be snapped. No-op until we receive the state type change, upon which
  // `this` will be removed.
  if (!IsSnapped(window1_) || !IsSnapped(window2_)) {
    return;
  }

  CHECK_EQ(window1_->GetRootWindow(), window2_->GetRootWindow());
  // If the windows + divider no longer fit in the work area, break the group.
  if (!CanWindowsFitInWorkArea(window1_, window2_)) {
    // `this` will be shut down and removed from the controller immediately, and
    // then destroyed asynchronously soon.
    SnapGroupController::Get()->RemoveSnapGroup(
        this, SnapGroupExitPoint::kCanNotFitInWorkArea);
    return;
  }

  // Otherwise call `ApplyPrimarySnapRatio()`, which will clamp the divider
  // position to between the windows' minimum sizes.
  ApplyPrimarySnapRatio(WindowState::Get(GetPhysicallyLeftOrTopWindow())
                            ->snap_ratio()
                            .value_or(chromeos::kDefaultSnapRatio));
}

void SnapGroup::OnWindowDestroying(aura::Window* window) {
  if (is_shutting_down_) {
    return;
  }

  DCHECK(window == window1_ || window == window2_);
  // `this` will be shut down and removed from the controller immediately, and
  // then destroyed asynchronously soon.
  SnapGroupController::Get()->RemoveSnapGroup(
      this, SnapGroupExitPoint::kWindowDestruction);
}

void SnapGroup::OnWindowParentChanged(aura::Window* window,
                                      aura::Window* parent) {
  // Skip any recursive updates during the other window move.
  if (parent == nullptr || is_moving_snap_group_) {
    return;
  }

  DCHECK(window == window1_ || window == window2_);

  base::AutoReset<bool> lock(&is_moving_snap_group_, true);

  const bool cached_divider_visibility =
      snap_group_divider_.target_visibility();

  // Hide the divider, then move the to-be-moved window to the same `parent`
  // container as the moved `window`.
  snap_group_divider_.SetVisible(false);

  aura::Window* to_be_moved_window = window == window1_ ? window2_ : window1_;
  bool did_parent_change = false;
  if (window->GetRootWindow() != to_be_moved_window->GetRootWindow()) {
    base::RecordAction(
        base::UserMetricsAction("SnapGroups_MoveSnapGroupToDisplay"));
    window_util::MoveWindowToDisplay(
        to_be_moved_window,
        display::Screen::GetScreen()->GetDisplayNearestWindow(parent).id());
    did_parent_change = true;
  } else if (parent != to_be_moved_window->parent()) {
    base::RecordAction(
        base::UserMetricsAction("SnapGroups_MoveSnapGroupToDesk"));
    parent->AddChild(to_be_moved_window);
    did_parent_change = true;
  }

  // The `window` may be temporarily moved under
  // `kShellWindowId_UnparentedContainer`, skip the stacking order fixing in
  // this case. While "visible on all workspaces" windows should never belong to
  // Snap Groups, this check is still necessary as the group removal can be
  // asynchronous.
  if (did_parent_change && desks_util::IsDeskContainer(parent) &&
      !desks_util::IsWindowVisibleOnAllWorkspaces(to_be_moved_window)) {
    window_util::FixWindowStackingAccordingToGlobalMru(to_be_moved_window);
  }

  // Restore the divider visibility after both windows are moved to the target
  // parent container.
  snap_group_divider_.SetVisible(cached_divider_visibility);

  RefreshSnapGroup();
}

void SnapGroup::OnPreWindowStateTypeChange(WindowState* window_state,
                                           chromeos::WindowStateType old_type) {
  if (is_shutting_down_) {
    return;
  }

  if (swapping_windows_) {
    return;
  }

  CHECK(old_type == WindowStateType::kPrimarySnapped ||
        old_type == WindowStateType::kSecondarySnapped);
  const chromeos::WindowStateType new_type = window_state->GetStateType();
  if (new_type != old_type) {
    // `this` will be shut down and removed from the controller immediately, and
    // then destroyed asynchronously soon.
    SnapGroupController::Get()->RemoveSnapGroup(
        this, GetWindowStateChangeExitPoint(window_state));
  }
}

void SnapGroup::OnPostWindowStateTypeChange(
    WindowState* window_state,
    chromeos::WindowStateType old_type) {
  if (window_to_target_snap_position_map_.empty()) {
    return;
  }

  aura::Window* window = window_state->window();
  auto iter = window_to_target_snap_position_map_.find(window);
  if (iter == window_to_target_snap_position_map_.end()) {
    return;
  }

  const WindowState* window1_state = WindowState::Get(window1_);
  const WindowState* window2_state = WindowState::Get(window2_);

  if (window_state->GetStateType() ==
      GetWindowStateTypeFromSnapPosition(iter->second)) {
    window_to_target_snap_position_map_.erase(iter);
  }

  // After both windows are snapped to their target snap position, updating the
  // member variables and adjust snapped windows bounds to account to divider
  // width holistically.
  if (window_to_target_snap_position_map_.empty() &&
      window1_state->GetStateType() == WindowStateType::kSecondarySnapped &&
      window2_state->GetStateType() == WindowStateType::kPrimarySnapped) {
    std::swap(window1_, window2_);

    auto new_window1_snap_ratio = WindowState::Get(window1_)->snap_ratio();
    CHECK(new_window1_snap_ratio);

    // `WindowState::OnWMEvent()` doesn't account for divider width. Explicitly
    // adjust snapped window state post-event to include divider.
    ApplyPrimarySnapRatio(*new_window1_snap_ratio);

    base::RecordAction(
        base::UserMetricsAction("SnapGroups_DoubleTapWindowSwapSuccess"));

    swapping_windows_ = false;
  }
}

aura::Window* SnapGroup::GetRootWindow() const {
  // This can be called during dragging window out of a snap group to another
  // display.
  // TODO(b/331993231): Update the root window in `OnWindowParentChanged()`.
  return window1_->GetRootWindow();
}

void SnapGroup::StartResizeWithDivider(const gfx::Point& location_in_screen) {
  // `SplitViewDivider` will do the work to start resizing.
  base::RecordAction(base::UserMetricsAction("SnapGroups_ResizeSnapGroup"));
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
  if (swapping_windows_) {
    return;
  }

  swapping_windows_ = true;

  WindowState* window1_state = WindowState::Get(window1_);
  const auto window1_snap_ratio = window1_state->snap_ratio();
  CHECK(window1_snap_ratio);

  WindowState* window2_state = WindowState::Get(window2_);
  const auto window2_snap_ratio = window2_state->snap_ratio();
  CHECK(window2_snap_ratio);

  window_to_target_snap_position_map_[window1_.get()] =
      SnapPosition::kSecondary;
  window_to_target_snap_position_map_[window2_.get()] = SnapPosition::kPrimary;

  const WindowSnapWMEvent secondary_snap_event(WM_EVENT_SNAP_SECONDARY,
                                               *window1_snap_ratio);
  window1_state->OnWMEvent(&secondary_snap_event);
  const WindowSnapWMEvent primary_snap_event(WM_EVENT_SNAP_PRIMARY,
                                             *window2_snap_ratio);
  window2_state->OnWMEvent(&primary_snap_event);

  base::RecordAction(
      base::UserMetricsAction("SnapGroups_DoubleTapWindowSwapAttempts"));
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
  const auto state_type = WindowState::Get(window)->GetStateType();
  // Reaching here; we may be updating bounds for a window that is about to be
  // unsnapped. If this is the case use the original window position, since the
  // snap position will not be changing at this point. Use extra caution since
  // `SplitViewDivider::GetEndDragLocationInScreen()` may also call this.
  if (!chromeos::IsSnappedWindowStateType(state_type)) {
    return window == window1_ ? SnapPosition::kPrimary
                              : SnapPosition::kSecondary;
  }
  return ToSnapPosition(state_type);
}

void SnapGroup::OnDisplayMetricsChanged(const display::Display& display,
                                        uint32_t metrics) {
  if (window1_->GetRootWindow() !=
      Shell::GetRootWindowForDisplayId(display.id())) {
    return;
  }

  // The divider widget is invisible in Overview mode, skip the
  // `RefreshSnapGroup()` since it would need to consider the divider bounds.
  // Additionally, we want to avoid intensive visual updates and grid re-layout
  // in Overview when the snapped windows no longer fit in the work area due to
  // changes in device scale (in which case, the `OverviewGroupItem` is split
  // into two separate `OverviewItem`s). `RefreshSnapGroup()` will be called in
  // `SnapGroupController::OnOverviewModeEndingAnimationComplete()` instead.
  auto* divider_widget = snap_group_divider_.divider_widget();
  if (!divider_widget || !divider_widget->IsVisible()) {
    return;
  }

  if (!(metrics &
        (DISPLAY_METRIC_BOUNDS | DISPLAY_METRIC_ROTATION |
         DISPLAY_METRIC_DEVICE_SCALE_FACTOR | DISPLAY_METRIC_WORK_AREA))) {
    return;
  }

  RefreshSnapGroup();
}

void SnapGroup::OnWindowActivated(ActivationReason reason,
                                  aura::Window* gained_active,
                                  aura::Window* lost_active) {
  // We are only interested when neither of the windows was active.
  if (lost_active == window1_ || lost_active == window2_) {
    return;
  }
  if (gained_active == window1_ || gained_active == window2_) {
    base::RecordAction(base::UserMetricsAction("SnapGroups_RecallSnapGroup"));
  }
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
  // Hide the divider first to avoid unnecessary updates while we're removing
  // the observers.
  HideDivider();
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
    // We only need to update the bounds to expand for the divider width if the
    // window is still snapped; `SnapGroup` will no longer manage the bounds if
    // the window is unsnapped.
    if (IsSnapped(window)) {
      UpdateSnappedWindowBounds(window, account_for_divider_width,
                                std::nullopt);
    }
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
  CHECK(CanWindowsFitInWorkArea(window1_, window2_));
  // TODO(b/331304137): Remove the cyclic dependencies between snapped window
  // bounds calculation and divider position calculation.
  // `SplitViewDivider::SetDividerPosition()` will account for the windows'
  // minimum sizes.
  snap_group_divider_.SetDividerPosition(
      CalculateDividerPosition(GetRootWindow(), primary_snap_ratio));

  UpdateSnappedWindowBounds(window1_, /*account_for_divider_width=*/true,
                            primary_snap_ratio);
  UpdateSnappedWindowBounds(window2_, /*account_for_divider_width=*/true,
                            1 - primary_snap_ratio);
}

}  // namespace ash
