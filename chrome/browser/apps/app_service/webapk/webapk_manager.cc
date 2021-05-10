// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/webapk/webapk_manager.h"

#include <memory>

#include "base/check.h"
#include "base/logging.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/webapk/webapk_install_queue.h"
#include "chrome/browser/apps/app_service/webapk/webapk_prefs.h"
#include "chrome/browser/ash/apps/apk_web_app_service.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "components/arc/mojom/intent_helper.mojom.h"
#include "components/services/app_service/public/mojom/types.mojom-shared.h"

namespace apps {

WebApkManager::WebApkManager(Profile* profile)
    : profile_(profile),
      web_app_registrar_(
          web_app::WebAppProviderBase::GetProviderBase(profile)->registrar()),
      initialized_(false) {
  proxy_ = apps::AppServiceProxyFactory::GetForProfile(profile);
  apk_service_ = ash::ApkWebAppService::Get(profile_);
  DCHECK(apk_service_);
  install_queue_ = std::make_unique<WebApkInstallQueue>(profile);

  Observe(&proxy_->AppRegistryCache());
}

WebApkManager::~WebApkManager() = default;

void WebApkManager::OnAppUpdate(const AppUpdate& update) {
  // TODO(crbug.com/119433): Install WebAPKs for existing apps which become
  // eligible, and update existing WebAPKs when app metadata changes.
  if (!initialized_) {
    return;
  }

  // Install new WebAPKs when an eligible app is installed. Note that it
  // generally shouldn't be possible to have an existing WebAPK installed while
  // StateIsNull, but we include the check for completeness' sake.
  if (update.StateIsNull() && IsAppEligibleForWebApk(update) &&
      !apps::webapk_prefs::GetWebApkPackageName(profile_, update.AppId())) {
    QueueInstall(update);
    return;
  }
}

void WebApkManager::OnAppTypeInitialized(apps::mojom::AppType type) {
  if (type == apps::mojom::AppType::kWeb) {
    initialized_ = true;

    // Install any WebAPK which should be installed but currently isn't.
    proxy_->AppRegistryCache().ForEachApp([&](const apps::AppUpdate& update) {
      if (IsAppEligibleForWebApk(update) &&
          !apps::webapk_prefs::GetWebApkPackageName(profile_, update.AppId())) {
        QueueInstall(update);
      }
    });
  }
}

void WebApkManager::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  Observe(nullptr);
}

apps::WebApkInstallQueue* WebApkManager::GetInstallQueueForTest() {
  return install_queue_.get();
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
  install_queue_->Install(update.AppId());
}

}  // namespace apps
