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
enum class AppTypeName;
}

class Profile;

namespace chromeos {

// The AppLaunchHandler class launches apps from `restore_data_` as well as
// observes app updates.
class AppLaunchHandler : public apps::AppRegistryCache::Observer {
 public:
  explicit AppLaunchHandler(Profile* profile);
  AppLaunchHandler(const AppLaunchHandler&) = delete;
  AppLaunchHandler& operator=(const AppLaunchHandler&) = delete;
  ~AppLaunchHandler() override;

  // Returns true if there are some restore data. Otherwise, returns false.
  bool HasRestoreData();

  void LaunchApps();

  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

 protected:
  Profile* profile_;
  std::unique_ptr<::full_restore::RestoreData> restore_data_;
  virtual base::WeakPtr<AppLaunchHandler> GetWeakPtrAppLaunchHandler() = 0;

 private:
  void LaunchApp(apps::mojom::AppType app_type, const std::string& app_id);

  void LaunchSystemWebAppOrChromeApp(
      apps::mojom::AppType app_type,
      const std::string& app_id,
      const ::full_restore::RestoreData::LaunchList& launch_list);

  virtual void LaunchBrowser() = 0;
  virtual void LaunchArcApp(
      const std::string& app_id,
      const ::full_restore::RestoreData::LaunchList& launch_list) = 0;
  virtual void RecordRestoredAppLaunch(apps::AppTypeName app_type_name) = 0;
  virtual void RecordArcGhostWindowLaunch(bool is_arc_ghost_window) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FULL_RESTORE_APP_LAUNCH_HANDLER_H_
