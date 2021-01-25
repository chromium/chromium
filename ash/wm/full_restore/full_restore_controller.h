// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_FULL_RESTORE_FULL_RESTORE_CONTROLLER_H_
#define ASH_WM_FULL_RESTORE_FULL_RESTORE_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/scoped_observation.h"

class PrefService;

namespace ash {

class FullRestoreWindowManager;

class ASH_EXPORT FullRestoreController : public SessionObserver,
                                         public TabletModeObserver {
 public:
  FullRestoreController();
  FullRestoreController(const FullRestoreController&) = delete;
  FullRestoreController& operator=(const FullRestoreController&) = delete;
  ~FullRestoreController() override;

  // Convenience function to get the controller which is created and owned by
  // Shell.
  static FullRestoreController* Get();

  // Calls full_restore::FullRestoreSaveHandler to save to the database. The
  // handler has timer to prevent too many writes, but we should limit calls
  // regardless if possible.
  void SaveWindows();

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  // TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;
  void OnTabletControllerDestroyed() override;

 private:
  friend class FullRestoreControllerTest;

  // Tracks how many times SaveWindows has been called.
  int save_windows_count_for_testing_ = 0;

  std::unique_ptr<FullRestoreWindowManager> full_restore_window_manager_;

  base::ScopedObservation<TabletModeController, TabletModeObserver>
      tablet_mode_observeration_{this};

  ScopedSessionObserver scoped_session_observer_{this};
};

}  // namespace ash

#endif  // ASH_WM_FULL_RESTORE_FULL_RESTORE_CONTROLLER_H_
