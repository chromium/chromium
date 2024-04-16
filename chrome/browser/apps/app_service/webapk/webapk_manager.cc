// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/webapk/webapk_manager.h"

#include <optional>

#include "ash/components/arc/mojom/app.mojom.h"
#include "ash/components/arc/session/connection_holder.h"
#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/webapk/webapk_install_queue.h"
#include "chrome/browser/apps/app_service/webapk/webapk_metrics.h"
#include "chrome/browser/apps/app_service/webapk/webapk_prefs.h"
#include "chrome/browser/ash/apps/apk_web_app_service.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/intent_util.h"

namespace {
constexpr char kGeneratedWebApkPackagePrefix[] = "org.chromium.webapk.";

bool HasShareIntentFilter(const apps::AppUpdate& app) {
  auto intent = std::make_unique<apps::Intent>(apps_util::kIntentActionSend);
  for (const auto& filter : app.IntentFilters()) {
    for (const auto& condition : filter->conditions) {
      if (intent->MatchCondition(condition)) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace

namespace apps {

WebApkManager::WebApkManager(Profile* profile)
    : profile_(profile),
      install_queue_(std::make_unique<WebApkInstallQueue>(profile_)),
      pref_change_registrar_(std::make_unique<PrefChangeRegistrar>()) {
  DCHECK(web_app::AreWebAppsEnabled(profile_));
  proxy_ = AppServiceProxyFactory::GetForProfile(profile_);
  apk_service_ = ash::ApkWebAppService::Get(profile_);
  DCHECK(apk_service_);
  app_list_prefs_ = ArcAppListPrefs::Get(profile_);
  DCHECK(app_list_prefs_);

  // Always observe AppListPrefs, even when the rest of WebAPKs is not enabled,
  // so that we can detect WebAPK uninstalls that happen when the feature is
  // disabled.
  arc_app_list_prefs_observer_.Observe(app_list_prefs_.get());
  arc::ArcSessionManager::Get()->AddObserver(this);
  pref_change_registrar_->Init(profile_->GetPrefs());
  pref_change_registrar_->Add(
      webapk_prefs::kGeneratedWebApksEnabled,
      base::BindRepeating(&WebApkManager::StartOrStopObserving,
                          base::Unretained(this)));

  StartOrStopObserving();
}

WebApkManager::~WebApkManager() {
  auto* arc_session_manager = arc::ArcSessionManager::Get();
  // ArcSessionManager can be destroyed early in unit tests.
  if (arc_session_manager) {
    arc_session_manager->RemoveObserver(this);
  }
}

void WebApkManager::StartOrStopObserving() {
  // WebApkManager is only created when arc::IsArcAllowedForProfile() is true.
  // We additionally check whether Play Store is enabled through Settings before
  // enabling anything.
  bool arc_enabled = arc::IsArcPlayStoreEnabledForProfile(profile_);
  bool policy_enabled =
      profile_->GetPrefs()->GetBoolean(webapk_prefs::kGeneratedWebApksEnabled);

  if (arc_enabled && policy_enabled) {
    auto* cache = &proxy_->AppRegistryCache();
    if (!app_registry_cache_observer_.IsObservingSource(cache)) {
      app_registry_cache_observer_.Reset();
      app_registry_cache_observer_.Observe(cache);
    }

    if (cache->IsAppTypeInitialized(AppType::kWeb)) {
      Synchronize();
    }
    return;
  }

  app_registry_cache_observer_.Reset();
  initialized_ = false;

  if (!policy_enabled) {
    // Remove any WebAPKs which were installed before the policy was enacted.
    // Ensures we don't end up in a confusing half-state with apps which can
    // never update, and allows us to start from scratch if the feature is
    // re-enabled.
    base::flat_set<std::string> current_installs =
        webapk_prefs::GetWebApkAppIds(profile_);
    for (const std::string& id : current_installs) {
      QueueUninstall(id);
    }
  }
}

void WebApkManager::Synchronize() {
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
  for (const std::string& id : webapk_prefs::GetUpdateNeededAppIds(profile_)) {
    QueueUpdate(id);
  }
}

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
          update.Readiness() == apps::Readiness::kReady) {
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
void WebApkManager::OnAppTypeInitialized(AppType type) {
  if (type == AppType::kWeb) {
    Synchronize();
  }
}

void WebApkManager::OnAppRegistryCacheWillBeDestroyed(AppRegistryCache* cache) {
  app_registry_cache_observer_.Reset();
}

void WebApkManager::OnPackageListInitialRefreshed() {
  for (const std::string& app_id : uninstall_queue_) {
    UninstallInternal(app_id);
  }
  uninstall_queue_.clear();

  // Uninstall any WebAPK packages which are installed in ARC but not linked to
  // an app in Prefs. This could happen if the WebAPK installation callback is
  // never delivered properly, or if the Play Store retries an error in the
  // background.
  // If an installed WebAPK is not listed in WebAPK prefs, then we will generate
  // and install a new WebAPK automatically, possibly resulting in duplicate
  // apps visible to the user.
  std::vector<std::string> installed_packages =
      app_list_prefs_->GetPackagesFromPrefs();
  base::flat_set<std::string> installed_webapk_packages =
      webapk_prefs::GetInstalledWebApkPackageNames(profile_);
  for (const auto& package_name : installed_packages) {
    if (base::StartsWith(package_name, kGeneratedWebApkPackagePrefix) &&
        !installed_webapk_packages.contains(package_name)) {
      auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
          app_list_prefs_->app_connection_holder(), UninstallPackage);
      if (!instance) {
        return;
      }
      instance->UninstallPackage(package_name);
    }
  }
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
  // 2. The WebAPK was uninstalled through Android settings. In this case, the
  //    Chrome OS-side app will still be installed and eligible for a WebAPK.

  // TODO(crbug.com/40178176): Remove the web app as well, if it is still
  // installed and eligible, and WebAPKs are not disabled by policy.
  webapk_prefs::RemoveWebApkByPackageName(profile_, package_name);
}

void WebApkManager::OnArcPlayStoreEnabledChanged(bool enabled) {
  StartOrStopObserving();
}

WebApkInstallQueue* WebApkManager::GetInstallQueueForTest() {
  return install_queue_.get();
}

bool WebApkManager::IsAppEligibleForWebApk(const AppUpdate& app) {
  if (app.Readiness() != apps::Readiness::kReady) {
    return false;
  }

  if (app.AppType() != apps::AppType::kWeb) {
    return false;
  }

  if (app.InstallReason() == apps::InstallReason::kSystem) {
    return false;
  }

  if (apk_service_->IsWebAppInstalledFromArc(app.AppId())) {
    return false;
  }

  if (!HasShareIntentFilter(app)) {
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

  std::optional<std::string> package_name =
      webapk_prefs::GetWebApkPackageName(profile_, app_id);
  // Ignore cases where we try to uninstall a package which doesn't exist, as
  // it's possible that the uninstall request was queued multiple times.
  if (package_name) {
    instance->UninstallPackage(package_name.value());
  }
}

}  // namespace apps
