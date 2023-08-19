// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_RESTORE_FULL_RESTORE_APP_LAUNCH_HANDLER_H_
#define CHROME_BROWSER_ASH_APP_RESTORE_FULL_RESTORE_APP_LAUNCH_HANDLER_H_

#include <utility>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/app_restore/app_launch_handler.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/crosapi/browser_manager_observer.h"
#include "chrome/browser/sessions/session_restore_observer.h"
#include "components/app_restore/restore_data.h"
#include "components/services/app_service/public/cpp/app_types.h"

class Profile;

namespace apps {
class AppUpdate;
enum class AppTypeName;
}  // namespace apps

namespace ash::full_restore {

// This is used for logging, so do not remove or reorder existing entries.
enum class RestoreTabResult {
  kHasTabs = 0,
  kNoTabs = 1,
  kError = 2,

  // Add any new values above this one, and update kMaxValue to the highest
  // enumerator value.
  kMaxValue = kError,
};

// This is used for logging, so do not remove or reorder existing entries.
enum class SessionRestoreExitResult {
  kNoExit = 0,
  kIsFirstServiceDidSchedule = 1,
  kIsFirstServiceNoSchedule = 2,
  kNotFirstServiceDidSchedule = 3,
  kNotFirstServiceNoSchedule = 4,

  // Add any new values above this one, and update kMaxValue to the highest
  // enumerator value.
  kMaxValue = kNotFirstServiceNoSchedule,
};

// This is used for logging, so do not remove or reorder existing entries.
enum class SessionRestoreWindowCount {
  kNoWindow = 0,
  kNoAppWindowHasNormalWindow = 1,
  kHasAppWindowNoNormalWindow = 2,
  kHasAppWindowHasNormalWindow = 3,

  // Add any new values above this one, and update kMaxValue to the highest
  // enumerator value.
  kMaxValue = kHasAppWindowHasNormalWindow,
};

// The FullRestoreAppLaunchHandler class calls FullRestoreReadHandler to read
// the full restore data from the full restore data file on a background task
// runner, and restore apps and web pages based on the user preference or the
// user's choice.
//
// The apps can be re-launched for the restoration when:
// 1. There is the restore data for the app.
// 2. The user preference sets always restore or the user selects 'Restore' from
// the notification dialog.
// 3. The app is ready.
class FullRestoreAppLaunchHandler : public AppLaunchHandler,
                                    public SessionRestoreObserver,
                                    public crosapi::BrowserManagerObserver {
 public:
  explicit FullRestoreAppLaunchHandler(Profile* profile,
                                       bool should_init_service = false);
  FullRestoreAppLaunchHandler(const FullRestoreAppLaunchHandler&) = delete;
  FullRestoreAppLaunchHandler& operator=(const FullRestoreAppLaunchHandler&) =
      delete;
  ~FullRestoreAppLaunchHandler() override;

  // Launches the browser when the restore data is loaded and the user chooses
  // to restore, or the first time running the full restore feature if
  // `first_run_full_restore` is true.
  void LaunchBrowserWhenReady(bool first_run_full_restore);

  // If the user preference sets always restore or the user selects 'Restore'
  // from the notification dialog, sets the restore flag `should_restore_` as
  // true to allow the restoration.
  void SetShouldRestore();

  // Returns true if the full restore data from the full restore file is loaded.
  bool IsRestoreDataLoaded();

  // AppLaunchHandler:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppTypeInitialized(apps::AppType app_type) override;

  // SessionRestoreObserver:
  void OnGotSession(Profile* profile, bool for_apps, int window_count) override;

  // crosapi::BrowserManagerObserver:
  void OnMojoDisconnected() override;
  void OnStateChanged() override;

  // Force launch browser for testing.
  void ForceLaunchBrowserForTesting();

 protected:
  void OnExtensionLaunching(const std::string& app_id) override;
  base::WeakPtr<AppLaunchHandler> GetWeakPtrAppLaunchHandler() override;

 private:
  friend class FullRestoreAppLaunchHandlerArcAppBrowserTest;
  friend class ArcAppLaunchHandler;

  void OnGetRestoreData(
      std::unique_ptr<::app_restore::RestoreData> restore_data);

  void MaybePostRestore();

  // If there is the restore data, and the restore flag `should_restore_` is
  // true, launches apps based on the restore data when apps are ready.
  void MaybeRestore();

  // Returns true if the browser can be restored. Otherwise, returns false.
  bool CanLaunchBrowser();

  // Goes through the normal startup browser session restore flow for launching
  // browsers.
  void LaunchBrowser();

  // Goes through the normal startup browser session restore flow for launching
  // browsers when upgrading to the full restore version.
  void LaunchBrowserForFirstRunFullRestore();

  void MaybeRestoreLacros();

  // AppLaunchHandler:
  void RecordRestoredAppLaunch(apps::AppTypeName app_type_name) override;

  void RecordLaunchBrowserResult();

  void LogRestoreData();

  // If the restore process finish, start the save timer.
  void MaybeStartSaveTimer();

  // Returns true if the previous session is reported to have ended with a
  // crash.
  bool IsLastSessionExitTypeCrashed();

  bool should_restore_ = false;

  // Specifies whether it is the first time to run the full restore feature.
  bool first_run_full_restore_ = false;

  bool are_web_apps_initialized_ = false;
  bool are_chrome_apps_initialized_ = false;

  bool should_launch_browser_ = false;

  // Specifies whether init FullRestoreService.
  bool should_init_service_ = false;

  // Restored browser window count. This is used for debugging and metrics.
  int browser_app_window_count_ = 0;
  int browser_window_count_ = 0;

  base::ScopedObservation<crosapi::BrowserManager,
                          crosapi::BrowserManagerObserver>
      observation_{this};

  base::WeakPtrFactory<FullRestoreAppLaunchHandler> weak_ptr_factory_{this};
};

class ScopedLaunchBrowserForTesting {
 public:
  ScopedLaunchBrowserForTesting();
  ScopedLaunchBrowserForTesting(const ScopedLaunchBrowserForTesting&) = delete;
  ScopedLaunchBrowserForTesting& operator=(
      const ScopedLaunchBrowserForTesting&) = delete;
  ~ScopedLaunchBrowserForTesting();
};

}  // namespace ash::full_restore

#endif  // CHROME_BROWSER_ASH_APP_RESTORE_FULL_RESTORE_APP_LAUNCH_HANDLER_H_
