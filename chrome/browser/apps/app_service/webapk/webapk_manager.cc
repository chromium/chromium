// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/webapk/webapk_manager.h"

#include <memory>

#include "base/check.h"
#include "base/logging.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/apps/apk_web_app_service.h"
#include "chrome/browser/ash/apps/apk_web_app_service_factory.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "components/arc/mojom/intent_helper.mojom.h"
#include "components/services/app_service/public/mojom/types.mojom-shared.h"

namespace apps {

WebApkManager::WebApkManager(Profile* profile)
    : profile_(profile),
      web_app_registrar_(
          web_app::WebAppProviderBase::GetProviderBase(profile)->registrar()) {
  proxy_ = apps::AppServiceProxyFactory::GetForProfile(profile);
  apk_service_ = ash::ApkWebAppServiceFactory::GetForProfile(profile_);
  DCHECK(apk_service_);

  Observe(&proxy_->AppRegistryCache());
}

WebApkManager::~WebApkManager() = default;

void WebApkManager::OnAppUpdate(const AppUpdate& update) {
  // TODO(crbug.com/1198433): Observe new installations and updates.
}

void WebApkManager::OnAppTypeInitialized(apps::mojom::AppType type) {
  if (type == apps::mojom::AppType::kWeb) {
    proxy_->AppRegistryCache().ForEachApp(
        [this](const apps::AppUpdate& update) {
          if (IsAppEligibleForWebApk(update)) {
            QueueInstall(update);
          }
        });
  }
}

void WebApkManager::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  Observe(nullptr);
}

bool WebApkManager::IsAppEligibleForWebApk(const apps::AppUpdate& app) {
  if (app.AppType() != apps::mojom::AppType::kWeb) {
    return false;
  }

  if (app.InstallSource() == apps::mojom::InstallSource::kSystem) {
    return false;
  }

  if (apk_service_->IsWebAppInstalledFromArc(app.AppId())) {
    return false;
  }

  if (!(web_app_registrar_.IsInstalled(app.AppId()) &&
        web_app_registrar_.GetAppShareTarget(app.AppId()))) {
    return false;
  }

  return true;
}

void WebApkManager::QueueInstall(const apps::AppUpdate& update) {
  // TODO(crbug.com/1198433): Actually queue the install.
  VLOG(1) << "Queueing WebAPK install for app: " << update.AppId();
}

}  // namespace apps
