// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_FULL_RESTORE_FULL_RESTORE_CONTROLLER_H_
#define ASH_WM_FULL_RESTORE_FULL_RESTORE_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "components/full_restore/full_restore_info.h"
#include "components/full_restore/window_info.h"

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
      public full_restore::FullRestoreInfo::Observer {
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
  void OnAppLaunched(aura::Window* window) override;
  void OnWidgetInitialized(views::Widget* widget) override;

 private:
  friend class FullRestoreControllerTest;

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
                      base::Optional<int> activation_index);

  // Sets a callback for testing that will be read from in
  // `OnWidgetInitialized()`.
  void SetReadWindowCallbackForTesting(ReadWindowCallback callback);

  // Sets a callback for testing that will be fired immediately when
  // SaveWindowImpl is about to notify the full restore component we want to
  // write to file.
  void SetSaveWindowCallbackForTesting(SaveWindowCallback callback);

  ScopedSessionObserver scoped_session_observer_{this};

  base::ScopedObservation<TabletModeController, TabletModeObserver>
      tablet_mode_observation_{this};

  base::ScopedObservation<full_restore::FullRestoreInfo,
                          full_restore::FullRestoreInfo::Observer>
      full_restore_info_observation_{this};
};

}  // namespace ash

#endif  // ASH_WM_FULL_RESTORE_FULL_RESTORE_CONTROLLER_H_
