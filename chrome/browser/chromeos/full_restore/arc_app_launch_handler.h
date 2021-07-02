// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FULL_RESTORE_ARC_APP_LAUNCH_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_FULL_RESTORE_ARC_APP_LAUNCH_HANDLER_H_

#include <set>

#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/resourced/resourced_client.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"

namespace apps {
class AppUpdate;
}  // namespace apps

namespace chromeos {
namespace full_restore {

class FullRestoreAppLaunchHandler;

// The ArcAppLaunchHandler class restores ARC apps during the system startup
// phase.
//
// TODO(crbug.com/1146900):
// 1. Add memory pressure checking before launch ARC apps.
// 2. Add app launch policy.
// 3. Check whether the ARC app is ready before launch the ARC apps.
class ArcAppLaunchHandler : public apps::AppRegistryCache::Observer,
                            public chromeos::ResourcedClient::Observer {
 public:
  explicit ArcAppLaunchHandler(FullRestoreAppLaunchHandler* handler);
  ArcAppLaunchHandler(const ArcAppLaunchHandler&) = delete;
  ArcAppLaunchHandler& operator=(const ArcAppLaunchHandler&) = delete;
  ~ArcAppLaunchHandler() override;

  // Checks whether the app of `app_id` is ready. If yes, launch the app.
  // Otherwise, add `app_id` to |app_ids|.
  void RestoreApp(const std::string& app_id);

  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

 protected:
  // Override chromeos::ResourcedClient::Observer
  void OnMemoryPressure(chromeos::ResourcedClient::PressureLevel level,
                        uint64_t reclaim_target_kb) override;

 private:
  void LaunchApp(const std::string& app_id);

  FullRestoreAppLaunchHandler* handler_ = nullptr;

  // If the ARC app is not ready, add it to `app_ids`. When the ARC app is
  // ready, and can be restored, launch the app, and remove it from `app_ids`.
  std::set<std::string> app_ids_;

  apps::AppRegistryCache& cache_;

  chromeos::ResourcedClient::PressureLevel pressure_level_;

  base::WeakPtrFactory<ArcAppLaunchHandler> weak_ptr_factory_{this};
};

}  // namespace full_restore
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FULL_RESTORE_ARC_APP_LAUNCH_HANDLER_H_
