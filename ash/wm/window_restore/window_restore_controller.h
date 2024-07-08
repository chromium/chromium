// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_RESTORE_WINDOW_RESTORE_CONTROLLER_H_
#define ASH_WM_WINDOW_RESTORE_WINDOW_RESTORE_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/cancelable_callback.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "components/account_id/account_id.h"
#include "components/app_restore/app_restore_info.h"
#include "components/app_restore/window_info.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display_observer.h"

namespace aura {
class Window;
}  // namespace aura

namespace display {
enum class TabletState;
}  // namespace display

namespace views {
class Widget;
}  // namespace views

namespace ash {

class WindowState;

class ASH_EXPORT WindowRestoreController
    : public display::DisplayObserver,
      public app_restore::AppRestoreInfo::Observer,
      public aura::WindowObserver {
 public:
  using SaveWindowCallback =
      base::RepeatingCallback<void(const app_restore::WindowInfo&)>;

  WindowRestoreController();
  WindowRestoreController(const WindowRestoreController&) = delete;
  WindowRestoreController& operator=(const WindowRestoreController&) = delete;
  ~WindowRestoreController() override;

  // Convenience function to get the controller which is created and owned by
  // Shell.
  static WindowRestoreController* Get();

  // Returns whether a app restored window can be activated. Only ghost
  // windows, windows without the `app_restore::kLaunchedFromAppRestoreKey`,
  // and topmost app restored windows return true.
  static bool CanActivateRestoredWindow(const aura::Window* window);

  // When windows are restored, they're restored inactive so during tablet mode
  // a window may be restored above the app list while the app list is still
  // active. To prevent this situation, the app list is deactivated and this
  // function should be called when determining the next focus target to prevent
  // the app list from being reactivated. Returns true if we're in tablet mode,
  // |window| is the window for the app list, and the topmost window of any
  // active desk container is a restored window.
  static bool CanActivateAppList(const aura::Window* window);

  // Given a set of windows, ordered from LRU to MRU, returns where `window`
  // should be inserted. The insertion point is determined by iterating from LRU
  // to MRU, returning the an iter to the first window that has no activation
  // index or a lower activation index.
  static std::vector<raw_ptr<aura::Window, VectorExperimental>>::const_iterator
  GetWindowToInsertBefore(
      aura::Window* window,
      const std::vector<raw_ptr<aura::Window, VectorExperimental>>& windows);

  const aura::Window* to_be_snapped_window() const {
    return to_be_snapped_window_.get();
  }

  // Calls SaveWindowImpl for |window_state|. The activation index will be
  // calculated in SaveWindowImpl.
  void SaveWindow(WindowState* window_state);

  // Gets all windows on all desk in the MRU window tracker and saves them to
  // file.
  void SaveAllWindows();

  // Called from MruWindowTracker when |gained_active| gets activation. This is
  // not done as an observer to ensure changes to the MRU list get handled first
  // before this is called.
  void OnWindowActivated(aura::Window* gained_active);

  // Stacks the window according to its activation index.
  void StackWindow(aura::Window* window);

  // Returns true if `window` is currently in the process of being restored by
  // `this`.
  bool IsRestoringWindow(aura::Window* window) const;

  // display::DisplayObserver:
  void OnDisplayTabletStateChanged(display::TabletState state) override;

  // app_restore::AppRestoreInfo::Observer:
  void OnRestorePrefChanged(const AccountId& account_id,
                            bool could_restore) override;
  void OnAppLaunched(aura::Window* window) override;
  void OnWidgetInitialized(views::Widget* widget) override;
  void OnParentWindowToValidContainer(aura::Window* window) override;

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;
  void OnWindowDestroying(aura::Window* window) override;

 private:
  friend class WindowRestoreControllerTest;

  // Updates the window state, activation and stacking of `window`. Also
  // observes `window` as we need to do further updates when the window is
  // shown.
  void UpdateAndObserveWindow(aura::Window* window);

  // Calls full_restore::FullRestoreSaveHandler to save to file. The handler has
  // timer to prevent too many writes, but we should limit calls regardless if
  // possible. Optionally passes |activation_index|, which is calculated with
  // respect to the MRU tracker. Calling SaveAllWindows will iterate through
  // the MRU tracker list, so we can pass the activation index during that loop
  // instead of building the MRU list again for each window.
  void SaveWindowImpl(WindowState* window_state,
                      std::optional<int> activation_index);

  // Retrieves the saved `WindowInfo` of `window` and restores its
  // `WindowStateType`. Also creates a post task to clear `window`s
  // `app_restore::kLaunchedFromAppRestoreKey`.
  void RestoreStateTypeAndClearLaunchedKey(aura::Window* window);

  // Calls `CancelAndRemoveRestorePropertyClearCallback()`. Also sets the
  // `window`'s `app_restore::kLaunchedFromAppRestoreKey` to false if the
  // window is not destroying.
  void ClearLaunchedKey(aura::Window* window);

  // Cancels and removes the App Restore property clear callback for `window`
  // from `restore_property_clear_callbacks_`.
  void CancelAndRemoveRestorePropertyClearCallback(aura::Window* window);

  // Sets a callback for testing that will be fired immediately when
  // `SaveWindowImpl()` is about to notify the window restore component we want
  // to write to file.
  void SetSaveWindowCallbackForTesting(SaveWindowCallback callback);

  // True whenever we are attempting to restore snap state.
  // The window that is about to be snapped by window restore. Reset to nullptr
  // if we aren't directly working with the window anymore.
  raw_ptr<aura::Window> to_be_snapped_window_ = nullptr;

  // The set of windows that have had their widgets initialized and will be
  // shown later.
  base::flat_set<raw_ptr<aura::Window, CtnExperimental>> to_be_shown_windows_;

  // When a window is restored, we post a task to clear its
  // `app_restore::kLaunchedFromAppRestoreKey` property. However, a window can
  // be closed before this task occurs, deleting the window. This map keeps
  // track of these posted tasks so we can cancel them upon window deletion.
  std::map<aura::Window*, base::CancelableOnceClosure>
      restore_property_clear_callbacks_;

  display::ScopedDisplayObserver display_observer_{this};

  base::ScopedObservation<app_restore::AppRestoreInfo,
                          app_restore::AppRestoreInfo::Observer>
      app_restore_info_observation_{this};

  // Observes windows launched by window restore.
  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      windows_observation_{this};

  base::WeakPtrFactory<WindowRestoreController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_RESTORE_WINDOW_RESTORE_CONTROLLER_H_
