// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_app_service_launcher.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/syslog_logging.h"
#include "base/types/cxx23_to_underlying.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/instance.h"

namespace ash {

KioskAppServiceLauncher::KioskAppServiceLauncher(Profile* profile) {
  app_service_ = apps::AppServiceProxyFactory::GetForProfile(profile);
  DCHECK(app_service_);
}

KioskAppServiceLauncher::~KioskAppServiceLauncher() = default;

void KioskAppServiceLauncher::CheckAndMaybeLaunchApp(
    const std::string& app_id,
    AppLaunchedCallback app_launched_callback) {
  DCHECK(!app_launched_callback_)
      << "CheckAndMaybeLaunchApp() should only be called once.";
  app_launched_callback_ = std::move(app_launched_callback);

  app_id_ = app_id;

  apps::Readiness readiness = apps::Readiness::kUnknown;
  app_service_->AppRegistryCache().ForOneApp(
      app_id_,
      [&readiness](apps::AppUpdate update) { readiness = update.Readiness(); });

  base::UmaHistogramEnumeration(kLaunchAppReadinessUMA, readiness);
  switch (readiness) {
    case apps::Readiness::kUnknown:
    case apps::Readiness::kTerminated:
      SYSLOG(WARNING) << "Kiosk app not ready yet: "
                      << base::to_underlying(readiness);
      app_registry_observation_.Observe(&app_service_->AppRegistryCache());
      break;
    case apps::Readiness::kReady:
      LaunchAppInternal();
      break;
    case apps::Readiness::kDisabledByBlocklist:
    case apps::Readiness::kDisabledByPolicy:
    case apps::Readiness::kDisabledByUser:
    case apps::Readiness::kUninstalledByUser:
    case apps::Readiness::kRemoved:
    case apps::Readiness::kUninstalledByMigration:
      SYSLOG(ERROR) << "Kiosk app should not have readiness "
                    << base::to_underlying(readiness);
      if (app_launched_callback_.has_value()) {
        std::move(app_launched_callback_.value()).Run(false);
      }
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

void KioskAppServiceLauncher::EnsureAppTypeInitialized(
    apps::AppType app_type,
    base::OnceClosure app_type_initialized_callback) {
  if (app_service_->AppRegistryCache().IsAppTypeInitialized(app_type)) {
    std::move(app_type_initialized_callback).Run();
    return;
  }

  app_type_ = app_type;
  app_type_initialized_callback_ = std::move(app_type_initialized_callback);
  app_registry_observation_.Observe(&app_service_->AppRegistryCache());
}

void KioskAppServiceLauncher::CheckAndMaybeLaunchApp(
    const std::string& app_id,
    AppLaunchedCallback app_launched_callback,
    base::OnceClosure app_visible_callback) {
  app_visible_callback_ = std::move(app_visible_callback);
  instance_registry_observation_.Observe(&app_service_->InstanceRegistry());

  CheckAndMaybeLaunchApp(app_id, std::move(app_launched_callback));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void KioskAppServiceLauncher::OnAppUpdate(const apps::AppUpdate& update) {
  if (update.AppId() != app_id_ ||
      update.Readiness() != apps::Readiness::kReady) {
    return;
  }
  app_registry_observation_.Reset();
  LaunchAppInternal();
}

void KioskAppServiceLauncher::OnAppTypeInitialized(apps::AppType app_type) {
  if (app_type == app_type_ && app_type_initialized_callback_.has_value()) {
    app_registry_observation_.Reset();
    std::move(app_type_initialized_callback_.value()).Run();
  }
}

void KioskAppServiceLauncher::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  app_registry_observation_.Reset();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

void KioskAppServiceLauncher::OnInstanceUpdate(
    const apps::InstanceUpdate& update) {
  if (update.AppId() != app_id_ ||
      !(update.State() & apps::InstanceState::kVisible)) {
    return;
  }

  instance_registry_observation_.Reset();
  if (app_visible_callback_.has_value()) {
    std::move(app_visible_callback_.value()).Run();
  }
}

void KioskAppServiceLauncher::OnInstanceRegistryWillBeDestroyed(
    apps::InstanceRegistry* cache) {
  instance_registry_observation_.Reset();
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void KioskAppServiceLauncher::LaunchAppInternal() {
  SYSLOG(INFO) << "Kiosk app is ready to launch with App Service";
  app_service_->LaunchAppWithParams(
      apps::AppLaunchParams(
          app_id_, apps::LaunchContainer::kLaunchContainerWindow,
          WindowOpenDisposition::NEW_POPUP, apps::LaunchSource::kFromKiosk),
      base::BindOnce(&KioskAppServiceLauncher::OnAppLaunched,
                     weak_ptr_factory_.GetWeakPtr()));
}

void KioskAppServiceLauncher::OnAppLaunched(apps::LaunchResult&& result) {
  // App window is not active at this moment. We need to close splash screen
  // after app window is activated which will be handled in subclasses.
  if (app_launched_callback_.has_value()) {
    std::move(app_launched_callback_.value()).Run(true);
  }
}

}  // namespace ash
