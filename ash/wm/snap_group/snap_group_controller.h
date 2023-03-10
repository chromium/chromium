// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SNAP_GROUP_SNAP_GROUP_CONTROLLER_H_
#define ASH_WM_SNAP_GROUP_SNAP_GROUP_CONTROLLER_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "base/containers/flat_map.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

class SnapGroup;

// Works as the centralized place to manage the `SnapGroup`. A single instance
// of this class will be created and owned by `Shell`. It controls the creation
// and destruction of the `SnapGroup`. TODO: It also implements the
// `OverviewObserver` and `TabletObserver`.
class ASH_EXPORT SnapGroupController {
 public:
  using SnapGroups = std::vector<std::unique_ptr<SnapGroup>>;
  using WindowToSnapGroupMap = base::flat_map<aura::Window*, SnapGroup*>;

  SnapGroupController();
  SnapGroupController(const SnapGroupController&) = delete;
  SnapGroupController& operator=(const SnapGroupController&) = delete;
  ~SnapGroupController();

  // Returns true if `window1` and `window2` are in the same snap group.
  bool AreWindowsInSnapGroup(aura::Window* window1,
                             aura::Window* window2) const;

  // Returns true if the corresponding SnapGroup for the given `window1` and
  // `window2` gets created, added to the `snap_groups_` and updated
  // `window_to_snap_group_map_` successfully. False otherwise.
  bool AddSnapGroup(aura::Window* window1, aura::Window* window2);

  // Returns true if the corresponding `snap_group` has
  // been successfully removed from the `snap_groups_` and
  // `window_to_snap_group_map_`. False otherwise.
  bool RemoveSnapGroup(SnapGroup* snap_group);

  // Returns true if the corresponding snap group that contains the
  // given `window` has been removed successfully. Returns false otherwise.
  bool RemoveSnapGroupContainingWindow(aura::Window* window);

  // Returns true if the feature flag `kSnapGroup` is enabled and the feature
  // param `kAutomaticallyLockGroup` is true, i.e. a snap group will be created
  // automatically on two windows snapped.
  bool IsArm1AutomaticallyLockEnabled() const;

  // Returns true if the feature flag `kSnapGroup` is enabled and the feature
  // param `kAutomaticallyLockGroup` is false, i.e. the user has to explicitly
  // create the snap group when the lock option shows up on two windows snapped.
  bool IsArm2ManuallyLockEnabled() const;

  const SnapGroups& snap_groups_for_testing() const { return snap_groups_; }
  const WindowToSnapGroupMap& window_to_snap_group_map_for_testing() const {
    return window_to_snap_group_map_;
  }

 private:
  // Retrieves the other window that is in the same snap group if any. Returns
  // nullptr if such window can't be found i.e. the window is not in a snap
  // group.
  aura::Window* RetrieveTheOtherWindowInSnapGroup(aura::Window* window) const;

  // Restores the windows bounds on snap group removed as the windows bounds are
  // shrunk either horizontally or vertically to make room for the split view
  // divider during `UpdateSnappedWindowsAndDividerBounds()` in
  // `SplitViewController`.
  void RestoreWindowsBoundsOnSnapGroupRemoved(aura::Window* window1,
                                              aura::Window* window2);

  // Contains all the `SnapGroup`(s), we will have one `SnapGroup` globally for
  // the first iteration but will have multiple in the future iteration.
  SnapGroups snap_groups_;

  // Maps the `SnapGroup` by the `aura::Window*`. It will be used to get the
  // `SnapGroup` with the `aura::Window*` and can also be used to decide if a
  // window is in a `SnapGroup` or not.
  WindowToSnapGroupMap window_to_snap_group_map_;
};

}  // namespace ash

#endif  // ASH_WM_SNAP_GROUP_SNAP_GROUP_CONTROLLER_H_