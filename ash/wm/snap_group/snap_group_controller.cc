// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/snap_group/snap_group_controller.h"

#include <optional>
#include <utility>
#include <vector>

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/snap_group/snap_group.h"
#include "ash/wm/snap_group/snap_group_constants.h"
#include "ash/wm/snap_group/snap_group_metrics.h"
#include "ash/wm/snap_group/snap_group_observer.h"
#include "ash/wm/splitview/layout_divider_controller.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_metrics.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/metrics/user_metrics.h"
#include "base/numerics/ranges.h"
#include "base/time/time.h"
#include "chromeos/ui/base/window_state_type.h"
#include "ui/display/screen.h"
#include "ui/display/tablet_state.h"

namespace ash {

namespace {

SnapGroupController* g_instance = nullptr;

// Returns true if both of the windows in `snap_group` are visible.
// TODO(b/333772909): Precautionary check for group minimize. See if we still
// need this after group minimize is removed.
bool AreSnapGroupWindowsVisible(const SnapGroup* snap_group) {
  return snap_group->window1()->IsVisible() &&
         snap_group->window2()->IsVisible();
}

}  // namespace

SnapGroupController::SnapGroupController() {
  OverviewController::Get()->AddObserver(this);
  CHECK_EQ(g_instance, nullptr);
  g_instance = this;
}

SnapGroupController::~SnapGroupController() {
  OverviewController::Get()->RemoveObserver(this);
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
SnapGroupController* SnapGroupController::Get() {
  // TODO(michelefan): Add CHECK(g_instance) after the snap group controller
  // feature is enabled by default.
  return g_instance;
}

bool SnapGroupController::AreWindowsInSnapGroup(aura::Window* window1,
                                                aura::Window* window2) const {
  DCHECK(window1);
  DCHECK(window2);
  return window1 == RetrieveTheOtherWindowInSnapGroup(window2) &&
         window2 == RetrieveTheOtherWindowInSnapGroup(window1);
}

bool SnapGroupController::OnWindowSnapped(
    aura::Window* window,
    WindowSnapActionSource snap_action_source) {
  // Only consider valid snap action sources or the window is being
  // auto-snapped or transitioned from tablet mode.
  const bool can_group_or_replace =
      CanSnapActionSourceStartFasterSplitView(snap_action_source) ||
      snap_action_source ==
          WindowSnapActionSource::kSnapByClamshellTabletTransition ||
      snap_action_source == WindowSnapActionSource::kAutoSnapInSplitView;
  if (!can_group_or_replace) {
    return false;
  }

  // If there is no opposite snapped window, we are done.
  aura::Window* opposite = GetOppositeVisibleSnappedWindow(window);
  if (!opposite) {
    return false;
  }

  // Disallow adding to group if the snap ratio gap exceeds the allowed
  // threshold.
  if (!IsSnapRatioGapWithinThreshold(window, opposite)) {
    base::RecordAction(base::UserMetricsAction("SnapGroups_SnapDirect"));
    return false;
  }

  // First attempt snap to replace. Snap groups in overview would be excluded by
  // `GetOppositeVisibleSnappedWindow()`.
  if (MaybeSnapToReplace(window, opposite, snap_action_source)) {
    return true;
  }

  if (auto* snap_group =
          AddSnapGroup(window, opposite, /*replace=*/false,
                       /*carry_over_creation_time=*/std::nullopt)) {
    aura::Window* target_window = nullptr;
    switch (snap_action_source) {
      case WindowSnapActionSource::kSnapByWindowLayoutMenu:
      case WindowSnapActionSource::kLacrosSnapButtonOrWindowLayoutMenu:
      case WindowSnapActionSource::kSnapByClamshellTabletTransition:
        // If the window was snapped via the layout menu, respect its
        // requested snap ratio. We also refresh the bounds for tablet
        // transition to ensure no divider gap.
        target_window = window;
        break;
      case WindowSnapActionSource::kDragWindowToEdgeToSnap:
      case WindowSnapActionSource::kLongPressCaptionButtonToSnap:
      case WindowSnapActionSource::kDragOrSelectOverviewWindowToSnap:
      case WindowSnapActionSource::kAutoSnapInSplitView:
        // Else if using a drag to snap or auto-snap action source, respect the
        // opposite window's snap ratio. This is to give the impression of
        // filling the layout and feels more intuitive to the user.
        target_window = opposite;
        break;
      default:
        // Else no need to refresh the group bounds.
        return true;
    }

    const float snap_ratio = window_util::GetSnapRatioForWindow(target_window);
    // Apply the target window's snap ratio *after* the group creation so we
    // can actually enforce it.
    snap_group->ApplyPrimarySnapRatio(
        IsPhysicallyLeftOrTop(target_window) ? snap_ratio : 1.f - snap_ratio);
    return true;
  }

  return false;
}

SnapGroup* SnapGroupController::AddSnapGroup(
    aura::Window* window1,
    aura::Window* window2,
    bool replace,
    std::optional<base::TimeTicks> carry_over_creation_time) {
  // The windows may already be in a snap group, if for example a snap group is
  // formed, then a window is re-snapped via the window layout menu.
  if (AreWindowsInSnapGroup(window1, window2)) {
    return GetSnapGroupForGivenWindow(window1);
  }

  // We should only allow snap group to be created for windows that have the
  // same parent.
  if (window1->parent() != window2->parent()) {
    return nullptr;
  }

  // Disallow snap group creation for unresizable windows.
  if (!WindowState::Get(window1)->CanResize() ||
      !WindowState::Get(window2)->CanResize()) {
    return nullptr;
  }

  // We only allow snap group to be created if the windows fit the work area.
  if (!CanWindowsFitInWorkArea(window1, window2)) {
    return nullptr;
  }

  // Disallow forming a Snap Group if either of the windows is configured to be
  // "visible on all workspaces".
  if (desks_util::IsWindowVisibleOnAllWorkspaces(window1) ||
      desks_util::IsWindowVisibleOnAllWorkspaces(window2)) {
    return nullptr;
  }

  if (base::Contains(window_to_snap_group_map_, window1) ||
      base::Contains(window_to_snap_group_map_, window2)) {
    return nullptr;
  }

  std::unique_ptr<SnapGroup> snap_group =
      std::make_unique<SnapGroup>(window1, window2, carry_over_creation_time);

  window_to_snap_group_map_.emplace(window1, snap_group.get());
  window_to_snap_group_map_.emplace(window2, snap_group.get());

  // Bounds have to be refreshed after snap group is created together with
  // divider and added to `window_to_snap_group_map_`. Otherwise, the snap ratio
  // will not be precisely calculated see `GetCurrentSnapRatio()` in
  // window_state.cc.
  auto* snap_group_ptr = snap_group.get();

  snap_groups_.push_back(std::move(snap_group));

  for (auto& observer : observers_) {
    observer.OnSnapGroupAdded(snap_group_ptr);
  }

  if (!replace) {
    ReportSnapGroupsCountHistogram(/*count=*/snap_groups_.size());
    base::RecordAction(base::UserMetricsAction("SnapGroups_AddSnapGroup"));
  }

  return snap_group_ptr;
}

bool SnapGroupController::RemoveSnapGroup(SnapGroup* snap_group,
                                          SnapGroupExitPoint exit_point) {
  if (snap_group->is_shutting_down_) {
    return false;
  }

  CHECK(snap_group);

  const bool snap_to_replace = exit_point == SnapGroupExitPoint::kSnapToReplace;
  if (!snap_to_replace) {
    // Records persistence duration and Snap Groups count when the removal of
    // `group_to_remove` is not due to 'Snap to Replace', as this is considered
    // an extension of the snap group's lifespan.
    RecordSnapGroupPersistenceDuration(base::TimeTicks::Now() -
                                       snap_group->carry_over_creation_time_);
  }

  // We should always record the actual duration of the Snap Group upon removal.
  RecordSnapGroupActualDuration(base::TimeTicks::Now() -
                                snap_group->actual_creation_time_);

  aura::Window* window1 = snap_group->window1();
  aura::Window* window2 = snap_group->window2();

  CHECK_EQ(window_to_snap_group_map_.erase(window1), 1u);
  CHECK_EQ(window_to_snap_group_map_.erase(window2), 1u);

  auto iter =
      base::ranges::find_if(snap_groups_, base::MatchesUniquePtr(snap_group));
  DCHECK(iter != snap_groups_.end());

  for (auto& observer : observers_) {
    observer.OnSnapGroupRemoving(snap_group, exit_point);
  }

  auto group_to_remove = std::move(*iter);
  snap_groups_.erase(iter);
  group_to_remove->Shutdown();
  base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(
      FROM_HERE, std::move(group_to_remove));

  if (!snap_to_replace) {
    ReportSnapGroupsCountHistogram(/*count=*/snap_groups_.size());
    base::RecordAction(base::UserMetricsAction("SnapGroups_RemoveSnapGroup"));
  }

  RecordSnapGroupExitPoint(exit_point);

  return true;
}

bool SnapGroupController::RemoveSnapGroupContainingWindow(
    aura::Window* window,
    SnapGroupExitPoint exit_point) {
  SnapGroup* snap_group = GetSnapGroupForGivenWindow(window);
  if (snap_group == nullptr) {
    return false;
  }

  return RemoveSnapGroup(snap_group, exit_point);
}

SnapGroup* SnapGroupController::GetSnapGroupForGivenWindow(
    const aura::Window* window) const {
  if (!window) {
    return nullptr;
  }
  auto iter = window_to_snap_group_map_.find(window);
  return iter != window_to_snap_group_map_.end() ? iter->second : nullptr;
}

SnapGroup* SnapGroupController::GetTopmostVisibleSnapGroup(
    const aura::Window* target_root) const {
  for (const aura::Window* top_window : GetActiveDeskAppWindowsInZOrder(
           const_cast<aura::Window*>(target_root))) {
    // Skip to the topmost window on `target_root`, ignoring occlusion-exempt
    // windows.
    if (ShouldExcludeForOcclusionCheck(top_window, target_root)) {
      continue;
    }
    // Note that if `top_window` is floated or pip'ed, it would not belong to a
    // snap group.
    if (auto* snap_group = GetSnapGroupForGivenWindow(top_window);
        snap_group && AreSnapGroupWindowsVisible(snap_group)) {
      return snap_group;
    }
    // Else if `top_window` does not belong to a snap group, we are done.
    break;
  }
  return nullptr;
}

SnapGroup* SnapGroupController::GetTopmostSnapGroup() const {
  // Use `BuildMruWindowList()` to include all windows on the active desk across
  // all root windows.
  for (const aura::Window* window :
       Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk)) {
    if (auto* snap_group = GetSnapGroupForGivenWindow(window);
        snap_group && AreSnapGroupWindowsVisible(snap_group)) {
      return snap_group;
    }
  }
  return nullptr;
}

std::optional<std::pair<aura::Window*, aura::Window*>>
SnapGroupController::GetWindowPairForSnapToReplaceWithKeyboardShortcut() {
  // Snap-to-replace targets only partially obscured Snap Group, which is the
  // topmost Snap Group.
  SnapGroup* top_snap_group = GetTopmostSnapGroup();
  if (!top_snap_group) {
    return std::nullopt;
  }

  aura::Window* root_window = window_util::GetRootWindowAt(
      display::Screen::GetScreen()->GetCursorScreenPoint());
  aura::Window::Windows windows = GetActiveDeskAppWindowsInZOrder(root_window);
  for (size_t i = 0; i < windows.size(); i++) {
    aura::Window* window = windows[i];
    const auto* window_state = WindowState::Get(window);
    if (!window->IsVisible() || window_state->IsMinimized() ||
        desks_util::IsWindowVisibleOnAllWorkspaces(window)) {
      continue;
    }

    // If the `window` being traversed belongs to a Snap Group and is the first
    // window encountered in the list, we can immediately exit the loop. Since
    // the other window in the group will also be on top, indicating the group
    // is not partially obscured (a condition we need for snap-to-replace).
    if (SnapGroup* snap_group_being_traversed =
            GetSnapGroupForGivenWindow(window);
        snap_group_being_traversed && i == 0 &&
        window == snap_group_being_traversed->GetTopMostWindowInGroup()) {
      break;
    }

    // Snap-to-Replace Eligibility Check:
    //   - Upon finding a snapped window, assess its potential for
    //   snap-to-replace.
    //   - This entails checking against the `top_snap_group`.
    //   - The combined snap ratios of the snapped window and the opposite
    //   window within the `top_snap_group` must equal one. This signifies that
    //   the two windows would perfectly fill the workspace if snapped together.
    //  - If this eligibility check passes:
    //     i. The snapped window is confirmed as a valid candidate for
    //     snap-to-replace.
    //     ii. The opposite snapped window within the `top_snap_group` is
    //     identified as another member of the `window_pair` required to form
    //     the new Snap Group after the snap-to-replace.
    const auto window_state_type = window_state->GetStateType();
    const auto snap_ratio = window_state->snap_ratio();
    aura::Window* visible_snapped_window_in_snap_group = nullptr;
    if (window_state_type == chromeos::WindowStateType::kPrimarySnapped) {
      CHECK(snap_ratio);
      visible_snapped_window_in_snap_group = top_snap_group->window2();
      if (base::IsApproximatelyEqual(
              *WindowState::Get(visible_snapped_window_in_snap_group)
                      ->snap_ratio() +
                  *snap_ratio,
              1.f, std::numeric_limits<float>::epsilon())) {
        return std::make_pair(window, visible_snapped_window_in_snap_group);
      }
    }

    if (window_state_type == chromeos::WindowStateType::kSecondarySnapped) {
      CHECK(snap_ratio);
      visible_snapped_window_in_snap_group = top_snap_group->window1();
      if (base::IsApproximatelyEqual(
              *WindowState::Get(visible_snapped_window_in_snap_group)
                      ->snap_ratio() +
                  *snap_ratio,
              1.f, std::numeric_limits<float>::epsilon())) {
        return std::make_pair(visible_snapped_window_in_snap_group, window);
      }
    }
  }

  return std::nullopt;
}

void SnapGroupController::AddObserver(SnapGroupObserver* observer) {
  observers_.AddObserver(observer);
}

void SnapGroupController::RemoveObserver(SnapGroupObserver* observer) {
  observers_.RemoveObserver(observer);
}

void SnapGroupController::OnFloatUnfloatCompleted(aura::Window* window) {
  // Needed because float -> snap will trigger a workspace event, during which
  // we want to ignore bounds events since the window is unfloating. Only when
  // all the nested unfloat events triggered by the original snap event is
  // finished and `WindowState::is_handling_float_event_` is reset can we
  // refresh the group and send bounds events.
  if (auto* snap_group = GetSnapGroupForGivenWindow(window)) {
    snap_group->RefreshSnapGroup();
  }
}

void SnapGroupController::OnOverviewModeStarting() {
  if (display::Screen::GetScreen()->InTabletMode()) {
    return;
  }

  for (const auto& snap_group : snap_groups_) {
    snap_group->HideDivider();
  }
}

void SnapGroupController::OnOverviewModeEnding(
    OverviewSession* overview_session) {
  if (display::Screen::GetScreen()->InTabletMode()) {
    return;
  }

  // On Overview mode ending, call `RefreshSnapGroup()` to refresh the bounds
  // of the snapped windows and divider. This ensures they either maintain a
  // proper fit within the work area or are gracefully broken from the group
  // if they no longer fit due to potential device scale factor in Overview.
  // By doing this refresh after exiting Overview, we prevent heavy visual
  // updates and re-layout (break `OverviewGroupItem` back to two individual
  // `Overviewitem`s) while in Overview mode.
  for (const auto& snap_group : snap_groups_) {
    snap_group->RefreshSnapGroup();
  }
}

void SnapGroupController::OnOverviewModeEndingAnimationComplete(bool canceled) {
  if (display::Screen::GetScreen()->InTabletMode()) {
    return;
  }

  for (const auto& snap_group : snap_groups_) {
    snap_group->ShowDivider();
  }
}

void SnapGroupController::OnDisplayTabletStateChanged(
    display::TabletState state) {
  switch (state) {
    case display::TabletState::kInClamshellMode:
    case display::TabletState::kEnteringTabletMode:
      break;
    case display::TabletState::kInTabletMode:
      OnTabletModeStarted();
      break;
    case display::TabletState::kExitingTabletMode:
      // TODO(b/327269057): Consider moving split view transition here.
      // Currently it's handled by `MaybeEndSplitViewAndOverview()` in
      // `TabletModeWindowManager`.
      RestoreSnapGroups();
      break;
  }
}

bool SnapGroupController::MaybeSnapToReplace(
    aura::Window* to_be_snapped_window,
    aura::Window* opposite_snapped_window,
    WindowSnapActionSource snap_action_source) {
  // Early return when
  // 1. In tablet mode;
  // 2. `to_be_snapped_window` belongs to a snap group, this can happen when
  // moving a snap group to another desk with snap groups.
  if (display::Screen::GetScreen()->InTabletMode() ||
      GetSnapGroupForGivenWindow(to_be_snapped_window)) {
    return false;
  }

  // TODO(b/331305840): Come up with an API to retrieve the snapped window on
  // the same side as the `to_be_snapped_window` to simplify the logic.
  SnapGroup* group_to_replace =
      GetSnapGroupForGivenWindow(opposite_snapped_window);
  if (!group_to_replace) {
    return false;
  }

  WindowState* window_state = WindowState::Get(to_be_snapped_window);
  const auto window_state_type = window_state->GetStateType();

  aura::Window* curr_primary_window = group_to_replace->window1();
  aura::Window* curr_secondary_window = group_to_replace->window2();
  aura::Window* new_primary_window = nullptr;
  aura::Window* new_secondary_window = nullptr;
  aura::Window* to_be_replaced_window = nullptr;
  if (window_state_type == chromeos::WindowStateType::kPrimarySnapped) {
    to_be_replaced_window = curr_primary_window;
    new_primary_window = to_be_snapped_window;
    new_secondary_window = curr_secondary_window;
  } else {
    CHECK_EQ(window_state_type, chromeos::WindowStateType::kSecondarySnapped);

    to_be_replaced_window = curr_secondary_window;
    new_primary_window = curr_primary_window;
    new_secondary_window = to_be_snapped_window;
  }

  // Disallow snap-to-replace if `to_be_replaced_window` is on a different
  // parent container with the `to_be_snapped_window`.
  if (to_be_replaced_window->parent() != to_be_snapped_window->parent()) {
    return false;
  }

  // If the new windows can't fit, do not allow snap to replace.
  if (!CanWindowsFitInWorkArea(new_primary_window, new_secondary_window)) {
    return false;
  }

  // TODO(b/331470570): Consider directly replacing the `to_be_snapped_window`
  // within the `snap_group`.
  const auto carry_over_creation_time =
      group_to_replace->carry_over_creation_time_;
  RemoveSnapGroup(group_to_replace, SnapGroupExitPoint::kSnapToReplace);
  SnapGroup* new_snap_group = AddSnapGroup(
      new_primary_window, new_secondary_window, /*replace=*/true,
      /*carry_over_creation_time=*/
      std::make_optional<base::TimeTicks>(carry_over_creation_time));

  // Snap Group may not be formed successfully.
  if (!new_snap_group) {
    return false;
  }

  base::RecordAction(base::UserMetricsAction("SnapGroups_SnapToReplace"));

  // Apply the `primary_window_snap_ratio` to the `new_snap_group` such that the
  // snap ratio of the `group_to_replace` is preserved.
  const float snapped_window_snap_ratio =
      window_util::GetSnapRatioForWindow(to_be_replaced_window);
  const float primary_window_snap_ratio =
      new_primary_window == to_be_snapped_window
          ? snapped_window_snap_ratio
          : 1 - snapped_window_snap_ratio;
  new_snap_group->ApplyPrimarySnapRatio(primary_window_snap_ratio);
  return true;
}

aura::Window* SnapGroupController::RetrieveTheOtherWindowInSnapGroup(
    aura::Window* window) const {
  if (window_to_snap_group_map_.find(window) ==
      window_to_snap_group_map_.end()) {
    return nullptr;
  }

  SnapGroup* snap_group = window_to_snap_group_map_.find(window)->second;
  return window == snap_group->window1() ? snap_group->window2()
                                         : snap_group->window1();
}

void SnapGroupController::RestoreSnapGroups() {
  // TODO(b/288335850): Currently `SplitViewController` only supports two
  // windows, the group at the end will overwrite any split view operations.
  // This will be addressed in multiple snap groups feature.
  // TODO(b/288334530): Iterate through all the displays and restore the snap
  // groups based on the mru order.
  for (const auto& snap_group : snap_groups_) {
    RestoreSnapState(snap_group.get());
  }
}

void SnapGroupController::RestoreSnapState(SnapGroup* snap_group) {
  CHECK(snap_group);

  auto* window1 = snap_group->window1();
  const auto window1_snap_ratio = WindowState::Get(window1)->snap_ratio();
  CHECK(window1_snap_ratio);

  auto* window2 = snap_group->window2();
  const auto window2_snap_ratio = WindowState::Get(window2)->snap_ratio();
  CHECK(window2_snap_ratio);

  // Preferably to use `SplitViewController::SnapWindow()` as it also handles
  // asynchronous operations from client controlled state.
  SplitViewController* split_view_controller =
      SplitViewController::Get(window1->GetRootWindow());
  split_view_controller->SnapWindow(
      window1, SnapPosition::kPrimary,
      WindowSnapActionSource::kSnapByWindowStateRestore, *window1_snap_ratio);
  split_view_controller->SnapWindow(
      window2, SnapPosition::kSecondary,
      WindowSnapActionSource::kSnapByWindowStateRestore, *window2_snap_ratio);
}

void SnapGroupController::OnTabletModeStarted() {
  // TODO(b/327269057): Define tablet <-> clamshell transition.
  while (!snap_groups_.empty()) {
    RemoveSnapGroup(snap_groups_.back().get(),
                    SnapGroupExitPoint::kTabletTransition);
  }
}

}  // namespace ash
