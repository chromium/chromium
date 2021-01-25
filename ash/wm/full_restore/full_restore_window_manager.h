// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_FULL_RESTORE_FULL_RESTORE_WINDOW_MANAGER_H_
#define ASH_WM_FULL_RESTORE_FULL_RESTORE_WINDOW_MANAGER_H_

#include "ash/ash_export.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_observer.h"
#include "base/scoped_multi_source_observation.h"
#include "chromeos/ui/base/window_state_type.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

// Observes windows that are can be full restored and notifies
// FullRestoreController to write to the database when interesting changes are
// made to the windows.
class FullRestoreWindowManager : public aura::WindowObserver,
                                 public WindowStateObserver {
 public:
  FullRestoreWindowManager();
  FullRestoreWindowManager(const FullRestoreWindowManager&) = delete;
  FullRestoreWindowManager& operator=(const FullRestoreWindowManager&) = delete;
  ~FullRestoreWindowManager() override;

  // Called when the user turns full restore on or off via the os setting.
  // Starts observing and notifies FullRestoreService to save all current app
  // windows to the database if |enabled| is true. Stops observing and notifies
  // FullRestoreService to remove all current app windows from the database if
  // |enabled| is false.
  void SetEnabled(bool enabled);

  // aura::WindowObserver:
  void OnWindowAdded(aura::Window* new_window) override;
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowRemovingFromRootWindow(aura::Window* window,
                                      aura::Window* new_root) override;

  // WindowStateObserver:
  void OnPostWindowStateTypeChange(WindowState* window_state,
                                   chromeos::WindowStateType old_type) override;

 private:
  void StopObserving();

  // If false, we do not observe any windows since the user chose to not use the
  // full restore feature.
  bool enabled_ = false;

  // The windows that can be the parent of an app windows. This includes the
  // desk containers and the always on top container. All windows in this set
  // are observed for when a child gets added or removed. Empty if the full
  // restore setting is disabled.
  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      app_window_parents_observations_{this};

  // The app windows we are currently observing. Empty if the full restore
  // setting is disabled.
  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      app_window_observations_{this};

  // The app window states we are currently observing. This should match the
  // size of |app_window_observations_|.
  // TODO(crbug.com/1164472): We may just want to call
  // FullRestoreController::SaveWindows in WindowState..
  base::ScopedMultiSourceObservation<WindowState, WindowStateObserver>
      app_window_state_observations_{this};
};

}  // namespace ash

#endif  // ASH_WM_FULL_RESTORE_FULL_RESTORE_WINDOW_MANAGER_H_
