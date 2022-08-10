// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_app_service_launcher.h"

#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "base/notreached.h"
#include "base/stl_util.h"
#include "base/syslog_logging.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"

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

void KioskAppServiceLauncher::OnAppUpdate(const apps::AppUpdate& update) {
  if (update.AppId() != app_id_ ||
      update.Readiness() != apps::Readiness::kReady) {
    return;
  }
  app_registry_observation_.Reset();
  LaunchAppInternal();
}

void KioskAppServiceLauncher::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  app_registry_observation_.Reset();
}

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
