// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SPLITVIEW_AUTO_SNAP_CONTROLLER_H_
#define ASH_WM_SPLITVIEW_AUTO_SNAP_CONTROLLER_H_

#include "base/scoped_multi_source_observation.h"
#include "ui/aura/window_observer.h"
#include "ui/wm/public/activation_change_observer.h"

namespace ash {

// The controller that observes the window state and performs auto snapping
// for the window if needed. When it's created, it observes the root window
// and all windows in a current active desk. When 1) an observed window is
// activated or 2) changed to visible from minimized, this class performs
// auto snapping for the window if it's possible.
class AutoSnapController : public wm::ActivationChangeObserver,
                           public aura::WindowObserver {
 public:
  explicit AutoSnapController(aura::Window* root_window);

  AutoSnapController(const AutoSnapController&) = delete;
  AutoSnapController& operator=(const AutoSnapController&) = delete;

  ~AutoSnapController() override;

  // Called by `OverviewSession` when the `gained_active` window is being
  // activated. Returns true if `gained_active` was snapped, false otherwise.
  bool OnWindowActivatingFromOverview(ActivationReason reason,
                                      aura::Window* gained_active);

  // wm::ActivationChangeObserver:
  void OnWindowActivating(ActivationReason reason,
                          aura::Window* gained_active,
                          aura::Window* lost_active) override;
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override {}

  // aura::WindowObserver:
  void OnWindowVisibilityChanging(aura::Window* window, bool visible) override;
  void OnWindowAddedToRootWindow(aura::Window* window) override;
  void OnWindowRemovingFromRootWindow(aura::Window* window,
                                      aura::Window* new_root) override;
  void OnWindowDestroying(aura::Window* window) override;

 private:
  // Auto-snaps `window` in split view upon gaining active or becoming visible.
  // Returns true if `window` was snapped, false otherwise.
  bool AutoSnapWindowIfNeeded(aura::Window* window);

  void AddWindow(aura::Window* window);
  void RemoveWindow(aura::Window* window);

  raw_ptr<aura::Window> root_window_;

  // Tracks observed windows.
  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      window_observations_{this};
};

}  // namespace ash

#endif  // ASH_WM_SPLITVIEW_AUTO_SNAP_CONTROLLER_H_
