// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FULL_RESTORE_FULL_RESTORE_APP_LAUNCH_HANDLER_H_
#define CHROME_BROWSER_ASH_FULL_RESTORE_FULL_RESTORE_APP_LAUNCH_HANDLER_H_

#include <utility>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/full_restore/app_launch_handler.h"
#include "components/full_restore/restore_data.h"
#include "components/services/app_service/public/mojom/types.mojom.h"

namespace apps {
class AppUpdate;
enum class AppTypeName;
}  // namespace apps

class Profile;

namespace ash {
namespace full_restore {

// This is used for logging, so do not remove or reorder existing entries.
enum class RestoreTabResult {
  kHasTabs = 0,
  kNoTabs = 1,
  kError = 2,

  // Add any new values above this one, and update kMaxValue to the highest
  // enumerator value.
  kMaxValue = kError,
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
class FullRestoreAppLaunchHandler : public AppLaunchHandler {
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

  base::TimeTicks restore_start_time() const { return restore_start_time_; }

  // AppLaunchHandler:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppTypeInitialized(apps::mojom::AppType app_type) override;

  // Force launch browser for testing.
  void ForceLaunchBrowserForTesting();

 protected:
  void OnExtensionLaunching(const std::string& app_id) override;
  base::WeakPtr<AppLaunchHandler> GetWeakPtrAppLaunchHandler() override;

 private:
  friend class FullRestoreAppLaunchHandlerArcAppBrowserTest;
  friend class ArcAppLaunchHandler;

  void OnGetRestoreData(
      std::unique_ptr<::full_restore::RestoreData> restore_data);

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

  // AppLaunchHandler:
  void RecordRestoredAppLaunch(apps::AppTypeName app_type_name) override;

  void RecordLaunchBrowserResult();

  void LogRestoreData();

  // If the restore process finish, start the save timer.
  void MaybeStartSaveTimer();

  bool should_restore_ = false;

  // Specifies whether it is the first time to run the full restore feature.
  bool first_run_full_restore_ = false;

  bool are_web_apps_initialized_ = false;
  bool are_chrome_apps_initialized_ = false;

  // The time when `should_restore_` has been set to true.
  base::TimeTicks restore_start_time_;

  bool should_launch_browser_ = false;

  // Specifies whether init FullRestoreService.
  bool should_init_service_ = false;

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

}  // namespace full_restore
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_FULL_RESTORE_FULL_RESTORE_APP_LAUNCH_HANDLER_H_
