// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/snap_group/snap_group.h"

#include <optional>

#include "ash/screen_util.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/overview/scoped_overview_hide_windows.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ash/wm/snap_group/snap_group_metrics.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_types.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/auto_reset.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/metrics/user_metrics.h"
#include "base/time/time.h"
#include "ui/base/hit_test.h"
#include "ui/display/display_observer.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/range/range_f.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

using chromeos::WindowStateType;

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

  Shell::Get()->activation_client()->RemoveObserver(this);
  display::Screen::GetScreen()->RemoveObserver(this);

  // Restore the snapped window bounds that were adjusted to make room for
  // divider when snap group was created.
  UpdateGroupWindowsBounds(/*account_for_divider_width=*/false);

  // Shelf defaults to rounded corners. We square them when a Snap Group is
  // created and fully visible. Maybe restore rounded corners on Snap Group
  // removal if no visible snap groups remain.
  Shelf::ForWindow(GetRootWindow())->MaybeUpdateShelfBackground();

  StopObservingWindows();
}

aura::Window* SnapGroup::GetPhysicallyLeftOrTopWindow() {
  return IsPhysicallyLeftOrTop(window1_) ? window1_ : window2_;
}

aura::Window* SnapGroup::GetPhysicallyRightOrBottomWindow() {
  return IsPhysicallyLeftOrTop(window1_) ? window2_ : window1_;
}

const aura::Window* SnapGroup::GetWindowOfSnapViewType(
    SnapViewType snap_type) const {
  return snap_type == SnapViewType::kPrimary ? window1_ : window2_;
}

void SnapGroup::ShowDivider() {
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

  // When the window is dragged via the caption bar to unsnap, we early break
  // the group to avoid re-stacking the divider on top of the dragged window.
  if (window1_->Contains(target) || window2_->Contains(target)) {
    SnapGroupController::Get()->RemoveSnapGroup(this);
    RecordSnapGroupExitPoint(SnapGroupExitPoint::kDragWindowOut);
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
  RecordSnapGroupExitPoint(SnapGroupExitPoint::kWindowDestruction);
  // `this` will be shut down and removed from the controller immediately, and
  // then destroyed asynchronously soon.
  SnapGroupController::Get()->RemoveSnapGroup(this);
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
  // this case.
  if (did_parent_change && desks_util::IsDeskContainer(parent)) {
    window_util::FixWindowStackingAccordingToGlobalMru(to_be_moved_window);
  }

  // Restore the divider visibility after both windows are moved to the target
  // parent container.
  snap_group_divider_.SetVisible(cached_divider_visibility);

  RefreshSnapGroup();
}

void SnapGroup::OnPreWindowStateTypeChange(WindowState* window_state,
                                           chromeos::WindowStateType old_type) {
  CHECK(old_type == WindowStateType::kPrimarySnapped ||
        old_type == WindowStateType::kSecondarySnapped);
  if (window_state->GetStateType() != old_type) {
    RecordSnapGroupExitPoint(SnapGroupExitPoint::kWindowStateChange);
    // `this` will be shut down and removed from the controller immediately, and
    // then destroyed asynchronously soon.
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
  CHECK(CanWindowsFitInWorkArea(window1_, window2_));
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

void SnapGroup::RefreshSnapGroup() {
  CHECK_EQ(window1_->GetRootWindow(), window2_->GetRootWindow());
  // If the windows + divider no longer fit in the work area, break the group.
  if (!CanWindowsFitInWorkArea(window1_, window2_)) {
    // `this` will be shut down and removed from the controller immediately, and
    // then destroyed asynchronously soon.
    SnapGroupController::Get()->RemoveSnapGroup(this);
    return;
  }

  // Otherwise call `ApplyPrimarySnapRatio()`, which will clamp the divider
  // position to between the windows' minimum sizes.
  ApplyPrimarySnapRatio(WindowState::Get(window1_)->snap_ratio().value_or(
      chromeos::kDefaultSnapRatio));
}

void SnapGroup::OnOverviewModeStarting() {
  // It's unnecessary to hide windows on inactive desks in partial Overview.
  // Since `window1_` and `window2_` are guaranteed to be on the same parent
  // container in ctor, it's enough to check just one of them to determine if
  // both windows are on the active desk.
  if (!desks_util::BelongsToActiveDesk(window1_)) {
    return;
  }

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

  // On Overview mode ending, call `RefreshSnapGroup()` to refresh the bounds
  // of the snapped windows and divider. This ensures they either maintain a
  // proper fit within the work area or are gracefully broken from the group
  // if they no longer fit due to potential device scale factor in Overview.
  // By doing this refresh after exiting Overview, we prevent heavy visual
  // updates and re-layout (break `OverviewGroupItem` back to two individual
  // `Overviewitem`s) while in Overview mode.
  RefreshSnapGroup();
}

}  // namespace ash
