// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FULL_RESTORE_APP_LAUNCH_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_FULL_RESTORE_APP_LAUNCH_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "components/full_restore/restore_data.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/mojom/types.mojom.h"

namespace apps {
class AppUpdate;
}

class Profile;

namespace chromeos {
namespace full_restore {

// The AppLaunchHandler class calls FullRestoreReadHandler to read the full
// restore data from the full restore data file on a background task runner, and
// restore apps and web pages based on the user preference or the user's choice.
//
// The apps can be re-launched for the restoration when:
// 1. There is the restore data for the app.
// 2. The user preference sets always restore or the user selects 'Restore' from
// the notification dialog.
// 3. The app is ready.
class AppLaunchHandler : public apps::AppRegistryCache::Observer {
 public:
  explicit AppLaunchHandler(Profile* profile);
  AppLaunchHandler(const AppLaunchHandler&) = delete;
  AppLaunchHandler& operator=(const AppLaunchHandler&) = delete;
  ~AppLaunchHandler() override;

  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  // Launches the browser, When the restore data is loaded, and the user chooses
  // to restore.
  void LaunchBrowserWhenReady();

  // If the user preference sets always restore or the user selects 'Restore'
  // from the notification dialog, sets the restore flag |should_restore_| as
  // true to allow the restoration.
  void SetShouldRestore();

  // Set force_launch_browser_ to launch browser for testing.
  void SetForceLaunchBrowserForTesting();

 private:
  void OnGetRestoreData(
      std::unique_ptr<::full_restore::RestoreData> restore_data);

  void MaybePostRestore();

  // If there is the restore data, and the restore flag |should_restore_| is
  // true, launches apps based on the restore data when apps are ready.
  void MaybeRestore();

  void LaunchBrowser();

  void LaunchApp(apps::mojom::AppType app_type, const std::string& app_id);

  void LaunchSystemWebAppOrChromeApp(
      const std::string& app_id,
      const ::full_restore::RestoreData::LaunchList& launch_list);

  void LaunchArcApp(const std::string& app_id,
                    const ::full_restore::RestoreData::LaunchList& launch_list);

  Profile* profile_ = nullptr;

  bool should_restore_ = false;

  bool should_launch_browser_ = false;

  bool force_launch_browser_ = false;

  std::unique_ptr<::full_restore::RestoreData> restore_data_;

  base::WeakPtrFactory<AppLaunchHandler> weak_ptr_factory_{this};
};

}  // namespace full_restore
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FULL_RESTORE_APP_LAUNCH_HANDLER_H_
