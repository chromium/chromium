// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_SERVICE_LAUNCHER_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_SERVICE_LAUNCHER_H_

#include <string>
#include "base/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"
#include "chrome/browser/profiles/profile.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_update.h"

namespace ash {

// This class launches a Kiosk app with the following steps:
// 1. Checks if the app is ready to be launched. If not then observes the
//    registry cache until the app is ready.
// 2. Starts the app using |AppServiceProxy::LaunchAppWithParams()| interface
//    and waits for the launch to complete.
// It does not wait for app window to become active, which should be handled
// in the caller of this class.
class KioskAppServiceLauncher : public apps::AppRegistryCache::Observer {
 public:
  // Callback when the app is launched by App Service. App window instance is
  // not active at this point. If called with false then the app launch has
  // failed. Corresponds to |KioskLaunchController::OnAppLaunched()|.
  using AppLaunchedCallback = base::OnceCallback<void(bool)>;

  explicit KioskAppServiceLauncher(Profile* profile);
  KioskAppServiceLauncher(const KioskAppServiceLauncher&) = delete;
  KioskAppServiceLauncher& operator=(const KioskAppServiceLauncher&) = delete;
  ~KioskAppServiceLauncher() override;

  // Checks if the Kiosk app is ready to be launched by App Service. If it's
  // ready then launches the app immediately. Otherwise waits for it to be ready
  // and launches the app later. Should only be called once per Kiosk launch.
  void CheckAndMaybeLaunchApp(const std::string& app_id,
                              AppLaunchedCallback app_launched_callback);

 private:
  void LaunchAppInternal();

  void OnAppLaunched(apps::LaunchResult&& result);

  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  std::string app_id_;

  // A keyed service. Not owned by this class.
  raw_ptr<apps::AppServiceProxy> app_service_;

  absl::optional<AppLaunchedCallback> app_launched_callback_;

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_observation_{this};

  base::WeakPtrFactory<KioskAppServiceLauncher> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_SERVICE_LAUNCHER_H_
