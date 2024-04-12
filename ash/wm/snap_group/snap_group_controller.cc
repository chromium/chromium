// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/snap_group/snap_group_controller.h"

#include <vector>

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/snap_group/snap_group.h"
#include "ash/wm/snap_group/snap_group_constants.h"
#include "ash/wm/splitview/layout_divider_controller.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_metrics.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/unique_ptr_adapters.h"
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

SnapGroup* SnapGroupController::AddSnapGroup(aura::Window* window1,
                                             aura::Window* window2) {
  // We should only allow snap group to be created for windows that have the
  // same parent.
  if (window1->parent() != window2->parent()) {
    return nullptr;
  }

  if (base::Contains(window_to_snap_group_map_, window1) ||
      base::Contains(window_to_snap_group_map_, window2)) {
    return nullptr;
  }

  std::unique_ptr<SnapGroup> snap_group =
      std::make_unique<SnapGroup>(window1, window2);

  window_to_snap_group_map_.emplace(window1, snap_group.get());
  window_to_snap_group_map_.emplace(window2, snap_group.get());

  // Bounds have to be refreshed after snap group is created together with
  // divider and added to `window_to_snap_group_map_`. Otherwise, the snap ratio
  // will not be precisely calculated see `GetCurrentSnapRatio()` in
  // window_state.cc.
  auto* snap_group_ptr = snap_group.get();
  snap_groups_.push_back(std::move(snap_group));
  snap_group_ptr->UpdateGroupWindowsBounds(
      /*account_for_divider_width=*/true);

  return snap_group_ptr;
}

bool SnapGroupController::RemoveSnapGroup(SnapGroup* snap_group) {
  CHECK(snap_group);
  aura::Window* window1 = snap_group->window1();
  aura::Window* window2 = snap_group->window2();
  CHECK(base::Contains(window_to_snap_group_map_, window1) &&
        base::Contains(window_to_snap_group_map_, window2));

  window_to_snap_group_map_.erase(window1);
  window_to_snap_group_map_.erase(window2);

  std::erase_if(snap_groups_, base::MatchesUniquePtr(snap_group));

  return true;
}

bool SnapGroupController::RemoveSnapGroupContainingWindow(
    aura::Window* window) {
  SnapGroup* snap_group = GetSnapGroupForGivenWindow(window);
  if (snap_group == nullptr) {
    return false;
  }

  return RemoveSnapGroup(snap_group);
}

SnapGroup* SnapGroupController::GetSnapGroupForGivenWindow(
    const aura::Window* window) const {
  auto iter = window_to_snap_group_map_.find(window);
  return iter != window_to_snap_group_map_.end() ? iter->second : nullptr;
}

bool SnapGroupController::OnSnappingWindow(
    aura::Window* to_be_snapped_window,
    WindowSnapActionSource snap_action_source) {
  // Early return when
  // 1. In tablet mode;
  // 2. `snap_action_source` originates from `kDragOrSelectOverviewWindowToSnap`
  // to avoid snap-to-replace within another snap group in Overview;
  // 3. `to_be_snapped_window` belongs to a snap group, this can happen when
  // moving a snap group to another desk with snap groups.
  if (display::Screen::GetScreen()->InTabletMode() ||
      snap_action_source ==
          WindowSnapActionSource::kDragOrSelectOverviewWindowToSnap ||
      GetSnapGroupForGivenWindow(to_be_snapped_window)) {
    return false;
  }

  // TODO(b/331305840): Come up with an API to retrieve the snapped window on
  // the same side as the `to_be_snapped_window` to simplify the logic.
  SnapGroup* group_to_replace = GetSnapGroupForGivenWindow(
      GetOppositeVisibleSnappedWindow(to_be_snapped_window));
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

  const float snapped_window_snap_ratio =
      WindowState::Get(to_be_replaced_window)
          ->snap_ratio()
          .value_or(chromeos::kDefaultSnapRatio);
  const float snapping_window_snap_ratio =
      window_state->snap_ratio().value_or(chromeos::kDefaultSnapRatio);

  // TODO(michelefan): The two snap action sources from Lacros are currently
  // bundled together. We should separate them.
  if (snap_action_source == WindowSnapActionSource::kSnapByWindowLayoutMenu ||
      snap_action_source ==
          WindowSnapActionSource::kLacrosSnapButtonOrWindowLayoutMenu) {
    const float snap_ratio_diff =
        std::abs(snapped_window_snap_ratio - snapping_window_snap_ratio);

    // Disallow snap-to-replace if the snap ratio difference exceeds the allowed
    // threshold.
    if (snap_ratio_diff > kSnapToReplaceRatioDiffThreshold) {
      return false;
    }
  }

  // TODO(b/331470570): Consider directly replacing the `to_be_snapped_window`
  // within the `snap_group`.
  RemoveSnapGroup(group_to_replace);
  SnapGroup* new_snap_group =
      AddSnapGroup(new_primary_window, new_secondary_window);

  // Apply the `primary_window_snap_ratio` to the `new_snap_group` such that the
  // snap ratio of the `group_to_replace` is preserved.
  const float primary_window_snap_ratio =
      new_primary_window == to_be_snapped_window
          ? snapped_window_snap_ratio
          : 1 - snapped_window_snap_ratio;
  new_snap_group->ApplyPrimarySnapRatio(primary_window_snap_ratio);
  return true;
}

void SnapGroupController::MinimizeTopMostSnapGroup() {
  auto* topmost_snap_group = GetTopmostSnapGroup();
  topmost_snap_group->MinimizeWindows();
}

SnapGroup* SnapGroupController::GetTopmostVisibleSnapGroup(
    const aura::Window* target_root) const {
  for (const aura::Window* top_window :
       Shell::Get()->mru_window_tracker()->BuildAppWindowList(kActiveDesk)) {
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
  for (const aura::Window* window :
       Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk)) {
    if (auto* snap_group = GetSnapGroupForGivenWindow(window);
        snap_group && AreSnapGroupWindowsVisible(snap_group)) {
      return snap_group;
    }
  }
  return nullptr;
}

void SnapGroupController::RestoreTopmostSnapGroup() {
  auto windows =
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk);
  for (aura::Window* window : windows) {
    if (auto* snap_group = GetSnapGroupForGivenWindow(window)) {
      CHECK(WindowState::Get(snap_group->window1())->IsMinimized());
      CHECK(WindowState::Get(snap_group->window2())->IsMinimized());
      RestoreSnapState(snap_group);
      return;
    }
  }
}

void SnapGroupController::OnOverviewModeStarting() {
  if (display::Screen::GetScreen()->InTabletMode()) {
    return;
  }

  for (const auto& snap_group : snap_groups_) {
    snap_group->OnOverviewModeStarting();
    snap_group->HideDivider();
  }
}

void SnapGroupController::OnOverviewModeEnding(
    OverviewSession* overview_session) {
  if (display::Screen::GetScreen()->InTabletMode()) {
    return;
  }

  for (const auto& snap_group : snap_groups_) {
    snap_group->OnOverviewModeEnding();
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
    RemoveSnapGroup(snap_groups_.back().get());
  }
}

}  // namespace ash
