// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/snap_group/snap_group_controller.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/snap_group/snap_group.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/containers/unique_ptr_adapters.h"

namespace ash {

namespace {

SnapGroupController* g_instance = nullptr;

}  // namespace

SnapGroupController::SnapGroupController() {
  Shell::Get()->overview_controller()->AddObserver(this);
  TabletModeController::Get()->AddObserver(this);
  CHECK_EQ(g_instance, nullptr);
  g_instance = this;
}

SnapGroupController::~SnapGroupController() {
  TabletModeController::Get()->RemoveObserver(this);
  Shell::Get()->overview_controller()->RemoveObserver(this);
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
SnapGroupController* SnapGroupController::Get() {
  // TODO(michelefan): Add CHECK(g_instance) after the snap group controller
  // feature is enabled by default.
  return g_instance;
}

void SnapGroupController::OnWindowSnapped(
    aura::Window* window,
    WindowSnapActionSource snap_action_source) {
  // If `window` already belongs to a snap group, do nothing.
  if (!IsArm1AutomaticallyLockEnabled() || GetSnapGroupForGivenWindow(window)) {
    return;
  }

  window_util::MaybeStartSplitViewOverview(window, snap_action_source);
}

bool SnapGroupController::AreWindowsInSnapGroup(aura::Window* window1,
                                                aura::Window* window2) const {
  DCHECK(window1);
  DCHECK(window2);
  return window1 == RetrieveTheOtherWindowInSnapGroup(window2) &&
         window2 == RetrieveTheOtherWindowInSnapGroup(window1);
}

bool SnapGroupController::AddSnapGroup(aura::Window* window1,
                                       aura::Window* window2) {
  // We should only allow snap group to be created for windows that have the
  // same parent.
  // TODO(michelefan): Avoid showing the lock widget if given two windows are
  // not allowed to create a snap group.
  if (window1->parent() != window2->parent()) {
    return false;
  }

  if (base::Contains(window_to_snap_group_map_, window1) ||
      base::Contains(window_to_snap_group_map_, window2)) {
    return false;
  }

  std::unique_ptr<SnapGroup> snap_group =
      std::make_unique<SnapGroup>(window1, window2);

  for (Observer& observer : observers_) {
    observer.OnSnapGroupCreated();
  }

  window_to_snap_group_map_.emplace(window1, snap_group.get());
  window_to_snap_group_map_.emplace(window2, snap_group.get());
  snap_groups_.push_back(std::move(snap_group));
  return true;
}

bool SnapGroupController::RemoveSnapGroup(SnapGroup* snap_group) {
  CHECK(snap_group);
  aura::Window* window1 = snap_group->window1();
  aura::Window* window2 = snap_group->window2();
  CHECK(base::Contains(window_to_snap_group_map_, window1) &&
        base::Contains(window_to_snap_group_map_, window2));

  if (!Shell::Get()->IsInTabletMode()) {
    snap_group->RestoreWindowsBoundsOnSnapGroupRemoved();
  }

  window_to_snap_group_map_.erase(window1);
  window_to_snap_group_map_.erase(window2);
  snap_group->StopObservingWindows();
  base::EraseIf(snap_groups_, base::MatchesUniquePtr(snap_group));

  for (Observer& observer : observers_) {
    observer.OnSnapGroupRemoved();
  }

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
    const aura::Window* window) {
  auto iter = window_to_snap_group_map_.find(window);
  return iter != window_to_snap_group_map_.end() ? iter->second : nullptr;
}

bool SnapGroupController::CanEnterOverview() const {
  // `SnapGroupController` is currently available for clamshell only, tablet
  // mode check will not be handled here.
  // TODO(michelefan): Get the `SplitViewController` for the actual root window
  // instead of hard code it to be primary root window.
  if (Shell::Get()->tablet_mode_controller()->InTabletMode() ||
      !SplitViewController::Get(Shell::GetPrimaryRootWindow())
           ->InSplitViewMode()) {
    return true;
  }

  return IsArm1AutomaticallyLockEnabled() && can_enter_overview_;
}

void SnapGroupController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SnapGroupController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool SnapGroupController::IsArm1AutomaticallyLockEnabled() const {
  return features::IsSnapGroupEnabled() &&
         features::kAutomaticallyLockGroup.Get();
}

bool SnapGroupController::IsArm2ManuallyLockEnabled() const {
  return features::IsSnapGroupEnabled() &&
         !features::kAutomaticallyLockGroup.Get();
}

void SnapGroupController::MinimizeTopMostSnapGroup() {
  auto* topmost_snap_group = GetTopmostSnapGroup();
  topmost_snap_group->MinimizeWindows();
}

SnapGroup* SnapGroupController::GetTopmostSnapGroup() {
  auto windows =
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk);
  for (auto* window : windows) {
    if (auto* snap_group = GetSnapGroupForGivenWindow(window)) {
      if (!WindowState::Get(snap_group->window1())->IsMinimized() &&
          !WindowState::Get(snap_group->window2())->IsMinimized()) {
        return snap_group;
      }
    }
  }
  return nullptr;
}

void SnapGroupController::RestoreTopmostSnapGroup() {
  auto windows =
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk);
  for (auto* window : windows) {
    if (auto* snap_group = GetSnapGroupForGivenWindow(window)) {
      CHECK(WindowState::Get(snap_group->window1())->IsMinimized());
      CHECK(WindowState::Get(snap_group->window2())->IsMinimized());
      RestoreSnapState(snap_group);
      return;
    }
  }
}

void SnapGroupController::OnOverviewModeEnded() {
  RestoreSnapGroups();
}

void SnapGroupController::OnTabletModeEnding() {
  RestoreSnapGroups();
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
  // TODO(b/286968669): Restore the snap ratio when snapping the windows in snap
  // group.
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
  auto* window2 = snap_group->window2();
  auto* root_window = window1->GetRootWindow();
  SplitViewController* split_view_controller =
      SplitViewController::Get(root_window);

  base::AutoReset<bool> bypass(&can_enter_overview_, false);
  split_view_controller->SnapWindow(
      window1, SplitViewController::SnapPosition::kPrimary);
  split_view_controller->SnapWindow(
      window2, SplitViewController::SnapPosition::kSecondary);
}

}  // namespace ash
