// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SNAP_GROUP_SNAP_GROUP_CONTROLLER_H_
#define ASH_WM_SNAP_GROUP_SNAP_GROUP_CONTROLLER_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/wm/overview/overview_observer.h"
#include "base/containers/flat_map.h"
#include "base/observer_list.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

class SnapGroup;

// Works as the centralized place to manage the `SnapGroup`. A single instance
// of this class will be created and owned by `Shell`. It controls the creation
// and destruction of the `SnapGroup`.
// TODO: It should also implement the `OverviewObserver` and `TabletObserver`.
class ASH_EXPORT SnapGroupController : public OverviewObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called to notify with the creation of snap group.
    virtual void OnSnapGroupCreated() = 0;

    // Called to notify the removal of snap group.
    virtual void OnSnapGroupRemoved() = 0;
  };

  using SnapGroups = std::vector<std::unique_ptr<SnapGroup>>;
  using WindowToSnapGroupMap = base::flat_map<aura::Window*, SnapGroup*>;

  SnapGroupController();
  SnapGroupController(const SnapGroupController&) = delete;
  SnapGroupController& operator=(const SnapGroupController&) = delete;
  ~SnapGroupController() override;

  // Convenience function to get the snap group controller instance, which is
  // created and owned by Shell.
  static SnapGroupController* Get();

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

  // Returns the corresponding `SnapGroup` if the given `window` belongs to a
  // snap group or nullptr otherwise.
  SnapGroup* GetSnapGroupForGivenWindow(aura::Window* window);

  // Used to decide whether showing overview on window snapped is allowed in
  // clamshell with `kSnapGroup` arm1 enabled.
  bool CanEnterOverview() const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns true if the feature flag `kSnapGroup` is enabled and the feature
  // param `kAutomaticallyLockGroup` is true, i.e. a snap group will be created
  // automatically on two windows snapped.
  bool IsArm1AutomaticallyLockEnabled() const;

  // Returns true if the feature flag `kSnapGroup` is enabled and the feature
  // param `kAutomaticallyLockGroup` is false, i.e. the user has to explicitly
  // create the snap group when the lock option shows up on two windows snapped.
  bool IsArm2ManuallyLockEnabled() const;

  // OverviewObserver:
  void OnOverviewModeEnded() override;

  const SnapGroups& snap_groups_for_testing() const { return snap_groups_; }
  const WindowToSnapGroupMap& window_to_snap_group_map_for_testing() const {
    return window_to_snap_group_map_;
  }
  void set_can_enter_overview_for_testing(bool can_enter_overview) {
    can_enter_overview_ = can_enter_overview;
  }

 private:
  // Retrieves the other window that is in the same snap group if any. Returns
  // nullptr if such window can't be found i.e. the window is not in a snap
  // group.
  aura::Window* RetrieveTheOtherWindowInSnapGroup(aura::Window* window) const;

  // Contains all the `SnapGroup`(s), we will have one `SnapGroup` globally for
  // the first iteration but will have multiple in the future iteration.
  SnapGroups snap_groups_;

  // Maps the `SnapGroup` by the `aura::Window*`. It will be used to get the
  // `SnapGroup` with the `aura::Window*` and can also be used to decide if a
  // window is in a `SnapGroup` or not.
  WindowToSnapGroupMap window_to_snap_group_map_;

  base::ObserverList<Observer> observers_;

  // If false, overview will not be allowed to show on the other side of the
  // screen on one window snapped, which is an instant way to snap window when
  // restoring the window snapped state.
  bool can_enter_overview_ = true;
};

}  // namespace ash

#endif  // ASH_WM_SNAP_GROUP_SNAP_GROUP_CONTROLLER_H_