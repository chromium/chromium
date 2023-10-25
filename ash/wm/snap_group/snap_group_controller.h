// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SNAP_GROUP_SNAP_GROUP_CONTROLLER_H_
#define ASH_WM_SNAP_GROUP_SNAP_GROUP_CONTROLLER_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/wm/overview/overview_observer.h"
#include "ash/wm/wm_metrics.h"
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
class ASH_EXPORT SnapGroupController : public OverviewObserver,
                                       public TabletModeObserver {
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

  // Called by `WindowState::OnWMEvent()` after a window snap event. This will
  // decide whether to start `SplitViewOverviewSession` or snap `window` to
  // complete the window layout.
  void OnWindowSnapped(aura::Window* window,
                       WindowSnapActionSource snap_action_source);

  // Returns true if `window1` and `window2` are in the same snap group.
  bool AreWindowsInSnapGroup(aura::Window* window1,
                             aura::Window* window2) const;

  // Returns true if the corresponding SnapGroup for the given `window1` and
  // `window2` gets created, added to the `snap_groups_` and updated
  // `window_to_snap_group_map_` successfully. False otherwise.
  // Currently, we make the assumption that the two windows need to be on the
  // same parent container.
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
  SnapGroup* GetSnapGroupForGivenWindow(const aura::Window* window);

  // Used to decide whether showing overview on window snapped is allowed in
  // clamshell.
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

  // Minimizes the most recently used and unminimized snap groups.
  void MinimizeTopMostSnapGroup();

  // Returns the topmost snap group in unminimized state.
  SnapGroup* GetTopmostSnapGroup();

  // Restores the most recent used snap group to be at the default snapped state
  // i.e. two windows in the most recent snap group are positioned at primary
  // and secondary snapped location.
  void RestoreTopmostSnapGroup();

  // OverviewObserver:
  void OnOverviewModeEnded() override;

  // TabletModeObserver:
  void OnTabletModeEnding() override;

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

  // Restores the snapped state of the `snap_groups_` upon completion of certain
  // transitions such as overview mode or tablet mode. Disallow overview to be
  // shown on the other side of the screen when restoring snap groups so that
  // the restore will be instant and the recursive snapping behavior will be
  // avoided.
  void RestoreSnapGroups();

  // Restore the snap state of the windows in the given `snap_group`.
  void RestoreSnapState(SnapGroup* snap_group);

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