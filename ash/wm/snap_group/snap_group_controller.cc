// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/snap_group/snap_group_controller.h"

#include "ash/shell.h"
#include "ash/wm/snap_group/snap_group.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/wm_event.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/containers/unique_ptr_adapters.h"

namespace ash {

SnapGroupController::SnapGroupController() = default;

SnapGroupController::~SnapGroupController() = default;

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

  if (window_to_snap_group_map_.find(window1) !=
          window_to_snap_group_map_.end() ||
      window_to_snap_group_map_.find(window2) !=
          window_to_snap_group_map_.end()) {
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
  aura::Window* window1 = snap_group->window1();
  aura::Window* window2 = snap_group->window2();
  CHECK(window_to_snap_group_map_.find(window1) !=
            window_to_snap_group_map_.end() &&
        window_to_snap_group_map_.find(window2) !=
            window_to_snap_group_map_.end());

  snap_group->RestoreWindowsBoundsOnSnapGroupRemoved();
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
    aura::Window* window) {
  if (window_to_snap_group_map_.find(window) ==
      window_to_snap_group_map_.end()) {
    return nullptr;
  }

  return window_to_snap_group_map_.find(window)->second;
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

}  // namespace ash