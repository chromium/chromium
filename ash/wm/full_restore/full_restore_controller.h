// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_FULL_RESTORE_FULL_RESTORE_CONTROLLER_H_
#define ASH_WM_FULL_RESTORE_FULL_RESTORE_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/cancelable_callback.h"
#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "components/full_restore/full_restore_info.h"
#include "components/full_restore/window_info.h"
#include "ui/aura/window_observer.h"

class PrefService;

namespace aura {
class Window;
}

namespace views {
class Widget;
}

namespace ash {

class WindowState;

class ASH_EXPORT FullRestoreController
    : public SessionObserver,
      public TabletModeObserver,
      public full_restore::FullRestoreInfo::Observer,
      public aura::WindowObserver {
 public:
  using ReadWindowCallback =
      base::RepeatingCallback<std::unique_ptr<full_restore::WindowInfo>(
          aura::Window*)>;
  using SaveWindowCallback =
      base::RepeatingCallback<void(const full_restore::WindowInfo&)>;

  FullRestoreController();
  FullRestoreController(const FullRestoreController&) = delete;
  FullRestoreController& operator=(const FullRestoreController&) = delete;
  ~FullRestoreController() override;

  // Convenience function to get the controller which is created and owned by
  // Shell.
  static FullRestoreController* Get();

  // When windows are restored, they're restored inactive so during tablet mode
  // a window may be restored above the app list while the app list is still
  // active. To prevent this situation, the app list is deactivated and this
  // function should be called when determining the next focus target to prevent
  // the app list from being reactivated. Returns true if we're in tablet mode,
  // |window| is the window for the app list, and the topmost window of any
  // active desk container is a restored window.
  static bool CanActivateAppList(const aura::Window* window);

  // Calls SaveWindowImpl for |window_state|. The activation index will be
  // calculated in SaveWindowImpl.
  void SaveWindow(WindowState* window_state);

  // Called from MruWindowTracker when |gained_active| gets activation. This is
  // not done as an observer to ensure changes to the MRU list get handled first
  // before this is called.
  void OnWindowActivated(aura::Window* gained_active);

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  // TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;
  void OnTabletControllerDestroyed() override;

  // full_restore::FullRestoreInfo::Observer:
  void OnWidgetInitialized(views::Widget* widget) override;
  void OnARCTaskReadyForUnparentedWindow(aura::Window* window) override;

  // aura::WindowObserver:
  void OnWindowStackingChanged(aura::Window* window) override;
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;
  void OnWindowDestroying(aura::Window* window) override;

  bool is_restoring_snap_state() const { return is_restoring_snap_state_; }

 private:
  friend class FullRestoreControllerTest;

  // Updates the window state, activation and stacking of `window`. Also
  // observes `window` as we need to do further updates when the window is
  // shown.
  void UpdateAndObserveWindow(aura::Window* window);

  // Gets all windows on all desk in the MRU window tracker and saves them to
  // file.
  void SaveAllWindows();

  // Calls full_restore::FullRestoreSaveHandler to save to file. The handler has
  // timer to prevent too many writes, but we should limit calls regardless if
  // possible. Optionally passes |activation_index|, which is calculated with
  // respect to the MRU tracker. Calling SaveAllWindows will iterate through
  // the MRU tracker list, so we can pass the activation index during that loop
  // instead of building the MRU list again for each window.
  void SaveWindowImpl(WindowState* window_state,
                      absl::optional<int> activation_index);

  // Retrieves the saved `WindowInfo` of `window` and restores its
  // `WindowStateType`. Also creates a post task to clear `window`s
  // `full_restore::kLaunchedFromFullRestoreKey`.
  void RestoreStateTypeAndClearLaunchedKey(aura::Window* window);

  // Cancels and removes the Full Restore property clear callback for `window`
  // from `restore_property_clear_callbacks_`. Also sets the `window`'s
  // `full_restore::kLaunchedFromFullRestoreKey` to false if `is_destroying` is
  // true.
  void ClearLaunchedKey(aura::Window* window, bool is_destroying);

  // Sets a callback for testing that will be read from in
  // `OnWidgetInitialized()`.
  void SetReadWindowCallbackForTesting(ReadWindowCallback callback);

  // Sets a callback for testing that will be fired immediately when
  // SaveWindowImpl is about to notify the full restore component we want to
  // write to file.
  void SetSaveWindowCallbackForTesting(SaveWindowCallback callback);

  // True whenever we are attempting to restore snap state.
  bool is_restoring_snap_state_ = false;

  // True whenever we are stacking windows to match saved activation order.
  bool is_stacking_ = false;

  // The set of windows that have had their widgets initialized and will be
  // shown later.
  base::flat_set<aura::Window*> to_be_shown_windows_;

  // When a window is restored, we post a task to clear its
  // `full_restore::kLaunchedFromFullRestoreKey` property. However, a window can
  // be closed before this task occurs, deleting the window. This map keeps
  // track of these posted tasks so we can cancel them upon window deletion.
  std::map<aura::Window*, base::CancelableOnceClosure>
      restore_property_clear_callbacks_;

  ScopedSessionObserver scoped_session_observer_{this};

  base::ScopedObservation<TabletModeController, TabletModeObserver>
      tablet_mode_observation_{this};

  base::ScopedObservation<full_restore::FullRestoreInfo,
                          full_restore::FullRestoreInfo::Observer>
      full_restore_info_observation_{this};

  // Observes windows launched by full restore.
  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      windows_observation_{this};

  base::WeakPtrFactory<FullRestoreController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_FULL_RESTORE_FULL_RESTORE_CONTROLLER_H_
