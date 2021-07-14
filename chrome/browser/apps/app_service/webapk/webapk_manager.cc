// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/webapk/webapk_manager.h"

#include "base/check.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/webapk/webapk_install_queue.h"
#include "chrome/browser/apps/app_service/webapk/webapk_prefs.h"
#include "chrome/browser/ash/apps/apk_web_app_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/arc/mojom/app.mojom.h"
#include "components/arc/session/connection_holder.h"
#include "components/services/app_service/public/mojom/types.mojom-shared.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {
constexpr char kGeneratedWebApkPackagePrefix[] = "org.chromium.webapk.";
}  // namespace

namespace apps {

WebApkManager::WebApkManager(Profile* profile)
    : profile_(profile),
      web_app_registrar_(web_app::WebAppProvider::Get(profile)->registrar()),
      initialized_(false) {
  proxy_ = AppServiceProxyFactory::GetForProfile(profile);
  apk_service_ = ash::ApkWebAppService::Get(profile_);
  DCHECK(apk_service_);
  app_list_prefs_ = ArcAppListPrefs::Get(profile_);
  DCHECK(app_list_prefs_);
  install_queue_ = std::make_unique<WebApkInstallQueue>(profile);

  arc_app_list_prefs_observer_.Observe(app_list_prefs_);
  Observe(&proxy_->AppRegistryCache());
}

WebApkManager::~WebApkManager() = default;

void WebApkManager::OnAppUpdate(const AppUpdate& update) {
  if (!initialized_) {
    return;
  }

  if (IsAppEligibleForWebApk(update)) {
    if (webapk_prefs::GetWebApkPackageName(profile_, update.AppId())) {
      // Existing WebAPK.
      // Update if there are app metadata changes.
      if (update.ShortNameChanged() || update.NameChanged() ||
          update.IconKeyChanged() || update.IntentFiltersChanged()) {
        QueueUpdate(update.AppId());
      }
    } else {  // New WebAPK.
      // Install if it is eligible for installation.
      if (update.ReadinessChanged() &&
          update.Readiness() == apps::mojom::Readiness::kReady) {
        QueueInstall(update.AppId());
      }
    }
  } else {  // !IsAppEligibleForWebApk(update)
    // Uninstall WebAPKs when the app becomes no longer eligible (which includes
    // when the app is uninstalled).
    if (webapk_prefs::GetWebApkPackageName(profile_, update.AppId())) {
      QueueUninstall(update.AppId());
    }
  }
}

// Called once per app type during startup, once apps of that type are
// initialized.
void WebApkManager::OnAppTypeInitialized(apps::mojom::AppType type) {
  if (type == apps::mojom::AppType::kWeb) {
    initialized_ = true;
    base::flat_set<std::string> current_installs =
        webapk_prefs::GetWebApkAppIds(profile_);
    base::flat_set<std::string> eligible_installs;

    proxy_->AppRegistryCache().ForEachApp([&](const apps::AppUpdate& update) {
      if (IsAppEligibleForWebApk(update)) {
        eligible_installs.insert(update.AppId());
      }
    });

    // Install any WebAPK which should be installed but currently isn't.
    for (const std::string& id : eligible_installs) {
      if (!current_installs.contains(id)) {
        QueueInstall(id);
      }
    }

    // Uninstall any WebAPK which shouldn't be installed but currently is.
    for (const std::string& id : current_installs) {
      if (!eligible_installs.contains(id)) {
        QueueUninstall(id);
      }
    }

    // Update any WebAPK for which an update was previously queued but
    // unsuccessful.
    for (const std::string& id :
         webapk_prefs::GetUpdateNeededAppIds(profile_)) {
      QueueUpdate(id);
    }
  }
}

void WebApkManager::OnAppRegistryCacheWillBeDestroyed(AppRegistryCache* cache) {
  Observe(nullptr);
}

void WebApkManager::OnPackageListInitialRefreshed() {
  for (const std::string& app_id : uninstall_queue_) {
    UninstallInternal(app_id);
  }
  uninstall_queue_.clear();
}

void WebApkManager::OnPackageRemoved(const std::string& package_name,
                                     bool uninstalled) {
  if (!base::StartsWith(package_name, kGeneratedWebApkPackagePrefix)) {
    return;
  }

  // There are several possible cases for why a WebAPK can be uninstalled:
  //
  // 1. The WebAPK was removed through QueueUninstall below, either because:
  //    - The Chrome OS app was uninstalled, or
  //    - The app became ineligible for having a Web APK (e.g., the Share Target
  //      was removed)
  //    In both cases, we just need to remove the WebAPK from prefs.
  // 2. The WebAPK was uninstalled on the Android side (through Android
  //    settings). In this case, we need to remove the WebAPK and also remove
  //    the Chrome-side app.
  //
  // So in summary, we always remove the WebAPK from prefs, and also remove the
  // app if it is installed and still eligible to have a WebAPK.

  webapk_prefs::RemoveWebApkByPackageName(profile_, package_name);
  // TODO(crbug.com/1200199): Remove the web app as well, if it is still
  // installed and eligible.
}

WebApkInstallQueue* WebApkManager::GetInstallQueueForTest() {
  return install_queue_.get();
}

bool WebApkManager::IsAppEligibleForWebApk(const AppUpdate& app) {
  if (app.Readiness() != apps::mojom::Readiness::kReady) {
    return false;
  }

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

void WebApkManager::QueueInstall(const std::string& app_id) {
  install_queue_->InstallOrUpdate(app_id);
}

void WebApkManager::QueueUpdate(const std::string& app_id) {
  // Mark the WebAPK as needing an update. This will be cleared when the update
  // is successful, and ensures that the update can be retried if it fails
  // before completion.
  webapk_prefs::SetUpdateNeededForApp(profile_, app_id,
                                      /* update_needed= */ true);
  install_queue_->InstallOrUpdate(app_id);
}

void WebApkManager::QueueUninstall(const std::string& app_id) {
  if (app_list_prefs_->package_list_initial_refreshed()) {
    // ArcApps is started and ready to receive uninstalls.
    UninstallInternal(app_id);
  } else {
    // Queue the install to happen once ArcApps is started.
    uninstall_queue_.push_back(app_id);
  }
}

void WebApkManager::UninstallInternal(const std::string& app_id) {
  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      app_list_prefs_->app_connection_holder(), UninstallPackage);
  if (!instance) {
    return;
  }

  absl::optional<std::string> package_name =
      webapk_prefs::GetWebApkPackageName(profile_, app_id);
  // Ignore cases where we try to uninstall a package which doesn't exist, as
  // it's possible that the uninstall request was queued multiple times.
  if (package_name) {
    instance->UninstallPackage(package_name.value());
  }
}

}  // namespace apps
