// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_MRU_WINDOW_TRACKER_H_
#define ASH_WM_MRU_WINDOW_TRACKER_H_

#include <vector>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/window_observer.h"
#include "ui/wm/public/activation_change_observer.h"

namespace ash {

enum DesksMruType {
  // The MRU window list will include windows from all active and inactive
  // desks.
  kAllDesks,

  // The MRU window list will exclude windows from the inactive desks.
  //
  // NOTE: During an on-going desk-switch animation, getting the MRU window list
  // for the active desk can be inconsistent, depending on at which stage of the
  // animation it is done. If you want the MRU windows in the soon-to-be active
  // desk, then wait for the animation to finish.
  kActiveDesk,
};

// A predicate that determines whether `window` can be included in the MRU
// window list.
bool CanIncludeWindowInMruList(aura::Window* window);

// A predicate that determines whether `window` is an app type.
bool CanIncludeWindowInAppMruList(aura::Window* window);

// Maintains a most recently used list of windows. This is used for window
// cycling using Alt+Tab and overview mode.
class ASH_EXPORT MruWindowTracker : public wm::ActivationChangeObserver,
                                    public aura::WindowObserver {
 public:
  using WindowList = std::vector<raw_ptr<aura::Window, VectorExperimental>>;

  MruWindowTracker();

  MruWindowTracker(const MruWindowTracker&) = delete;
  MruWindowTracker& operator=(const MruWindowTracker&) = delete;

  ~MruWindowTracker() override;

  // Returns the set windows in the mru list regardless of whether they can be
  // included in the cycler or not.
  // |desks_mru_type| determines whether to include or exclude windows from the
  // inactive desks.
  // TODO(oshima|afakhry): Investigate if we can consolidate BuildXXXList
  // methods with parameters.
  WindowList BuildAppWindowList(DesksMruType desks_mru_type) const;

  // Returns the set of windows which can be cycled through using the tracked
  // list of most recently used windows.
  // |desks_mru_type| determines whether to include or exclude windows from the
  // inactive desks.
  WindowList BuildMruWindowList(DesksMruType desks_mru_type) const;

  // This does the same thing as the above, but ignores the system modal dialog
  // state and hence the returned list could contain more windows if a system
  // modal dialog window is present.
  // |desks_mru_type| determines whether to include or exclude windows from the
  // inactive desks.
  WindowList BuildWindowListIgnoreModal(DesksMruType desks_mru_type) const;

  // This does the same thing as |BuildMruWindowList()| but with some
  // exclusions. This list is used for cycling through by the keyboard via
  // alt-tab.
  // |desks_mru_type| determines whether to include or exclude windows from the
  // inactive desks.
  WindowList BuildWindowForCycleList(DesksMruType desks_mru_type) const;

  // This does the same thing as |BuildWindowForCycleList()| but includes
  // ARC PIP windows if they exist. Entering PIP for Android can consume the
  // window (in contrast to Chrome PIP, which creates a new window). To support
  // the same interaction as Chrome PIP auto-pip, include the Android PIP window
  // in alt-tab. This will let alt tabbing back to the 'original window' restore
  // that window from PIP, which matches behaviour for Chrome PIP, where
  // alt-tabbing back to the original Chrome tab or app ends auto-PIP.
  WindowList BuildWindowForCycleWithPipList(DesksMruType desks_mru_type) const;

  // Starts or stops ignoring window activations. If no longer ignoring
  // activations the currently active window is moved to the front of the
  // MRU window list. Used by WindowCycleList to avoid adding all cycled
  // windows to the front of the MRU window list.
  void SetIgnoreActivations(bool ignore);

  // Called after |window| moved out of its about-to-be-removed desk, to a new
  // target desk's container. This causes |window| to be made the least-recently
  // used window across all desks.
  void OnWindowMovedOutFromRemovingDesk(aura::Window* window);

  // Called when a window is moved to another desk or created by a window
  // restore feature. This function should be only called by
  // `WindowRestoreController`.
  void OnWindowAlteredByWindowRestore(aura::Window* window);

  const std::vector<raw_ptr<aura::Window, VectorExperimental>>&
  GetMruWindowsForTesting() {
    return mru_windows_;
  }

 private:
  // Updates the `mru_windows_` list to insert/move `active_window` at/to the
  // back.
  void SetActiveWindow(aura::Window* active_window);

  // wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // aura::WindowObserver:
  void OnWindowDestroyed(aura::Window* window) override;

  // List of windows that have been activated in containers that we cycle
  // through, sorted such that the most recently used window comes last. Note
  // that this ordering differs from the lists returned by the
  // `Build*Window*List` functions, which are reversed.
  std::vector<raw_ptr<aura::Window, VectorExperimental>> mru_windows_;

  bool ignore_window_activations_ = false;
};

}  // namespace ash

#endif  // ASH_WM_MRU_WINDOW_TRACKER_H_
