// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SNAP_GROUP_SNAP_GROUP_CONTROLLER_H_
#define ASH_WM_SNAP_GROUP_SNAP_GROUP_CONTROLLER_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/wm/overview/overview_observer.h"
#include "ash/wm/snap_group/snap_group_metrics.h"
#include "ash/wm/wm_metrics.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "ui/aura/window.h"
#include "ui/display/display_observer.h"

namespace display {
enum class TabletState;
}  // namespace display

namespace ash {

class SnapGroup;
class SnapGroupObserver;

// Works as the centralized place to manage the `SnapGroup`. A single instance
// of this class will be created and owned by `Shell`. It controls the creation
// and destruction of the `SnapGroup`.
class ASH_EXPORT SnapGroupController : public OverviewObserver,
                                       public display::DisplayObserver {
 public:
  using SnapGroups = std::vector<std::unique_ptr<SnapGroup>>;
  using WindowToSnapGroupMap =
      base::flat_map<aura::Window*, raw_ptr<SnapGroup, CtnExperimental>>;

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

  // Called by `SplitViewController` when `window` is snapped. Returns true if
  // `window` was added to a group, either by normal group creation or snap
  // to replace.
  bool OnWindowSnapped(aura::Window* window,
                       WindowSnapActionSource snap_action_source);

  // Attempts to add `window1` and `window2` as a `SnapGroup`. Returns the
  // `SnapGroup`, if the creation is successful. Returns nullptr, otherwise.
  // Currently, both windows must reside within the same parent container for
  // successful creation. If `replace` is true, the group was snapped to replace
  // and we shouldn't record the count change. `carry_over_creation_time`
  // indicates the creation time of a prior Snap Group from which the current
  // one was derived using the Snap to Replace feature.
  // TODO(b/333772909): Remove `replace` param when snap to replace updates
  // window in SnapGroup instead of removing and re-adding a SnapGroup.
  SnapGroup* AddSnapGroup(
      aura::Window* window1,
      aura::Window* window2,
      bool replace,
      std::optional<base::TimeTicks> carry_over_creation_time);

  // Removes the specified `snap_group`, recording the `exit_point` metric.
  // Returns true if the corresponding `snap_group` has been successfully
  // removed from the `snap_groups_` and `window_to_snap_group_map_`. False
  // otherwise.
  bool RemoveSnapGroup(SnapGroup* snap_group, SnapGroupExitPoint exit_point);

  // Returns true if the corresponding snap group that contains the
  // given `window` has been removed successfully. Returns false otherwise.
  bool RemoveSnapGroupContainingWindow(aura::Window* window,
                                       SnapGroupExitPoint exit_point);

  // Returns the corresponding `SnapGroup` if the given `window` belongs to a
  // snap group or nullptr otherwise.
  SnapGroup* GetSnapGroupForGivenWindow(const aura::Window* window) const;

  // Returns the topmost fully visible non-occluded snap group on `target_root`.
  SnapGroup* GetTopmostVisibleSnapGroup(const aura::Window* target_root) const;

  // Returns the topmost snap group in unminimized state.
  SnapGroup* GetTopmostSnapGroup() const;

  // Determines which windows can be used for snap-to-replace with keyboard
  // shortcut:
  // 1. Finds the topmost snapped window.
  // 2. Identifies the window within a partially obscured Snap Group that isn't
  // hidden by the topmost snapped window.
  //  Returns the window pair for snap-to-replace: [primary snapped window,
  //  secondary snapped window].
  std::optional<std::pair<aura::Window*, aura::Window*>>
  GetWindowPairForSnapToReplaceWithKeyboardShortcut();

  void AddObserver(SnapGroupObserver* observer);
  void RemoveObserver(SnapGroupObserver* observer);

  // Called by `WindowState` when a float or unfloat event for `window` has
  // completed.
  void OnFloatUnfloatCompleted(aura::Window* window);

  // OverviewObserver:
  void OnOverviewModeStarting() override;
  void OnOverviewModeEnding(OverviewSession* overview_session) override;
  void OnOverviewModeEndingAnimationComplete(bool canceled) override;

  // display::DisplayObserver:
  void OnDisplayTabletStateChanged(display::TabletState state) override;

  const SnapGroups& snap_groups_for_testing() const { return snap_groups_; }
  const WindowToSnapGroupMap& window_to_snap_group_map_for_testing() const {
    return window_to_snap_group_map_;
  }

 private:
  // Returns true if the attempt to replace the window within the snap group of
  // `opposite_snapped_window` positioned directly below with the given
  // `to_be_snapped_window` is successful, returns false otherwise. The
  // `snap_action_source` determines the need for snap ratio difference
  // calculations during 'snap to replace'.
  bool MaybeSnapToReplace(aura::Window* to_be_snapped_window,
                          aura::Window* opposite_snapped_window,
                          WindowSnapActionSource snap_action_source);

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

  // Called when the display tablet state is changed.
  void OnTabletModeStarted();

  // Contains all the `SnapGroup`(s), we will have one `SnapGroup` globally for
  // the first iteration but will have multiple in the future iteration.
  SnapGroups snap_groups_;

  // Maps the `SnapGroup` by the `aura::Window*`. It will be used to get the
  // `SnapGroup` with the `aura::Window*` and can also be used to decide if a
  // window is in a `SnapGroup` or not.
  WindowToSnapGroupMap window_to_snap_group_map_;

  base::ObserverList<SnapGroupObserver> observers_;

  display::ScopedDisplayObserver display_observer_{this};
};

}  // namespace ash

#endif  // ASH_WM_SNAP_GROUP_SNAP_GROUP_CONTROLLER_H_
