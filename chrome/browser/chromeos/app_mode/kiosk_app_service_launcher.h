// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_SERVICE_LAUNCHER_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_SERVICE_LAUNCHER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "build/buildflag.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"
#include "chrome/browser/profiles/profile.h"
#include "components/services/app_service/public/cpp/app.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/instance_update.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/services/app_service/public/cpp/instance_registry.h"
#endif

namespace chromeos {

// This class launches a Kiosk app with the following steps:
// 1. Checks if the app is ready to be launched. If not then observes the
//    registry cache until the app is ready.
// 2. Starts the app using `AppServiceProxy::LaunchAppWithParams()` interface
//    and waits for the launch to complete.
class KioskAppServiceLauncher :
#if BUILDFLAG(IS_CHROMEOS_ASH)
    public apps::InstanceRegistry::Observer,
#endif
    public apps::AppRegistryCache::Observer {
 public:
  // Callback when the app is launched by App Service. App window instance is
  // not active at this point. If called with false then the app launch has
  // failed. Corresponds to `KioskLaunchController::OnAppLaunched()`.
  using AppLaunchedCallback = base::OnceCallback<void(bool)>;

  // Histogram to log the app readiness while launching app.
  static constexpr char kLaunchAppReadinessUMA[] =
      "Kiosk.AppService.Launch.AppReadiness";

  explicit KioskAppServiceLauncher(Profile* profile);
  KioskAppServiceLauncher(const KioskAppServiceLauncher&) = delete;
  KioskAppServiceLauncher& operator=(const KioskAppServiceLauncher&) = delete;
  ~KioskAppServiceLauncher() override;

  // Checks if the Kiosk app is ready to be launched by App Service. If it's
  // ready then launches the app immediately. Otherwise waits for it to be ready
  // and launches the app later. Should only be called once per Kiosk launch.
  // This function does not wait for app window to become active, which should
  // be handled in the caller of this class.
  void CheckAndMaybeLaunchApp(const std::string& app_id,
                              AppLaunchedCallback app_launched_callback);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Ensures that `app_type` is initialized in App Service.
  void EnsureAppTypeInitialized(
      apps::AppType app_type,
      base::OnceClosure app_type_initialized_callback);

  // Same as the other `CheckAndMaybeLaunchApp`, but also waits for app window
  // to be visible by observing `apps::InstanceRegistry`. Only works in Ash.
  void CheckAndMaybeLaunchApp(const std::string& app_id,
                              AppLaunchedCallback app_launched_callback,
                              base::OnceClosure app_visible_callback);
#endif
  void SetLaunchUrl(const GURL& launch_url);

 private:
  void LaunchAppInternal();

  void OnAppLaunched(apps::LaunchResult&& result);

  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppTypePublishing(const std::vector<apps::AppPtr>& deltas,
                           apps::AppType app_type) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // apps::InstanceRegistry::Observer:
  void OnInstanceUpdate(const apps::InstanceUpdate& update) override;
  void OnInstanceRegistryWillBeDestroyed(
      apps::InstanceRegistry* cache) override;
#endif

  std::string app_id_;

  apps::AppType app_type_;

  // A keyed service. Not owned by this class.
  raw_ptr<apps::AppServiceProxy> app_service_;

  base::OnceClosure app_type_initialized_callback_;

  AppLaunchedCallback app_launched_callback_;
  std::optional<GURL> launch_url_;

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_observation_{this};

#if BUILDFLAG(IS_CHROMEOS_ASH)
  base::OnceClosure app_visible_callback_;

  base::ScopedObservation<apps::InstanceRegistry,
                          apps::InstanceRegistry::Observer>
      instance_registry_observation_{this};
#endif

  base::WeakPtrFactory<KioskAppServiceLauncher> weak_ptr_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_SERVICE_LAUNCHER_H_
