// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_preload_service/app_preload_service.h"

#include <memory>
#include <vector>

#include "base/barrier_callback.h"
#include "base/check_is_test.h"
#include "base/containers/cxx20_erase_set.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "chrome/browser/apps/almanac_api_client/device_info_manager.h"
#include "chrome/browser/apps/app_preload_service/app_preload_service_factory.h"
#include "chrome/browser/apps/app_preload_service/preload_app_definition.h"
#include "chrome/browser/apps/app_preload_service/web_app_preload_installer.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "components/user_manager/user_manager.h"

namespace {

// The pref dict is:
// {
//  ...
//  "apps.app_preload_service.state_manager": {
//    "first_login_flow_started": <bool>,
//    "first_login_flow_completed": <bool>
//  },
//  ...
// }

static constexpr char kFirstLoginFlowStartedKey[] = "first_login_flow_started";
static constexpr char kFirstLoginFlowCompletedKey[] =
    "first_login_flow_completed";

static constexpr char kFirstLoginFlowHistogramSuccessName[] =
    "AppPreloadService.FirstLoginFlowTime.Success";
static constexpr char kFirstLoginFlowHistogramFailureName[] =
    "AppPreloadService.FirstLoginFlowTime.Failure";

}  // namespace

namespace apps {

namespace prefs {
static constexpr char kApsStateManager[] =
    "apps.app_preload_service.state_manager";
}  // namespace prefs

BASE_FEATURE(kAppPreloadServiceForceRun,
             "AppPreloadServiceForceRun",
             base::FEATURE_DISABLED_BY_DEFAULT);

AppPreloadService::AppPreloadService(Profile* profile)
    : profile_(profile),
      server_connector_(std::make_unique<AppPreloadServerConnector>()),
      device_info_manager_(std::make_unique<DeviceInfoManager>(profile)),
      web_app_installer_(std::make_unique<WebAppPreloadInstaller>(profile)) {
  StartFirstLoginFlow();
}

AppPreloadService::~AppPreloadService() = default;

// static
AppPreloadService* AppPreloadService::Get(Profile* profile) {
  return AppPreloadServiceFactory::GetForProfile(profile);
}

// static
void AppPreloadService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kApsStateManager);
}

void AppPreloadService::StartFirstLoginFlowForTesting(
    base::OnceCallback<void(bool)> callback) {
  SetInstallationCompleteCallbackForTesting(std::move(callback));  // IN-TEST
  StartFirstLoginFlow();
}

void AppPreloadService::StartFirstLoginFlow() {
  auto start_time = base::TimeTicks::Now();

  // Preloads currently run for new users only. The "completed" pref is only set
  // when preloads finish successfully, so preloads will be retried if they have
  // been "started" but never "completed".
  if (user_manager::UserManager::Get()->IsCurrentUserNew()) {
    ScopedDictPrefUpdate(profile_->GetPrefs(), prefs::kApsStateManager)
        ->Set(kFirstLoginFlowStartedKey, true);
  }

  bool first_run_started =
      GetStateManager().FindBool(kFirstLoginFlowStartedKey).value_or(false);
  bool first_run_complete =
      GetStateManager().FindBool(kFirstLoginFlowCompletedKey).value_or(false);

  if ((first_run_started && !first_run_complete) ||
      base::FeatureList::IsEnabled(kAppPreloadServiceForceRun)) {
    device_info_manager_->GetDeviceInfo(
        base::BindOnce(&AppPreloadService::StartAppInstallationForFirstLogin,
                       weak_ptr_factory_.GetWeakPtr(), start_time));
  }
}

void AppPreloadService::StartAppInstallationForFirstLogin(
    base::TimeTicks start_time,
    DeviceInfo device_info) {
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      profile_->GetURLLoaderFactory();
  if (!url_loader_factory.get()) {
    // `url_loader_factory` should only be null if we are in a non-preload
    // related test. Tests that use profile builder to create their profile
    // won't have `url_loader_factory` set up by default, so we bypass preloads
    // code being called for those tests.
    CHECK_IS_TEST();
    return;
  }
  server_connector_->GetAppsForFirstLogin(
      device_info, url_loader_factory,
      base::BindOnce(&AppPreloadService::OnGetAppsForFirstLoginCompleted,
                     weak_ptr_factory_.GetWeakPtr(), start_time));
}

void AppPreloadService::OnGetAppsForFirstLoginCompleted(
    base::TimeTicks start_time,
    absl::optional<std::vector<PreloadAppDefinition>> apps) {
  if (!apps.has_value()) {
    OnFirstLoginFlowComplete(/*success=*/false, start_time);
    return;
  }

  // Filter out any apps that should not be installed.
  base::EraseIf(apps.value(), [this](const PreloadAppDefinition& app) {
    return !ShouldInstallApp(app);
  });

  // Request installation of any remaining apps. If there are no apps to
  // install, OnAllAppInstallationFinished will be called immediately.
  const auto install_barrier_callback_ = base::BarrierCallback<bool>(
      apps.value().size(),
      base::BindOnce(&AppPreloadService::OnAllAppInstallationFinished,
                     weak_ptr_factory_.GetWeakPtr(), start_time));

  for (const PreloadAppDefinition& app : apps.value()) {
    web_app_installer_->InstallApp(app, install_barrier_callback_);
  }
}

void AppPreloadService::OnAllAppInstallationFinished(
    base::TimeTicks start_time,
    const std::vector<bool>& results) {
  OnFirstLoginFlowComplete(
      base::ranges::all_of(results, [](bool b) { return b; }), start_time);
}

void AppPreloadService::OnFirstLoginFlowComplete(bool success,
                                                 base::TimeTicks start_time) {
  if (success) {
    ScopedDictPrefUpdate(profile_->GetPrefs(), prefs::kApsStateManager)
        ->Set(kFirstLoginFlowCompletedKey, true);
  }

  base::UmaHistogramMediumTimes(success ? kFirstLoginFlowHistogramSuccessName
                                        : kFirstLoginFlowHistogramFailureName,
                                base::TimeTicks::Now() - start_time);

  if (installation_complete_callback_) {
    std::move(installation_complete_callback_).Run(success);
  }
}

bool AppPreloadService::ShouldInstallApp(const PreloadAppDefinition& app) {
  // We currently only preload web apps.
  if (app.GetPlatform() != AppType::kWeb) {
    return false;
  }

  // We currently only install apps which were requested by the device OEM.
  if (!app.IsOemApp()) {
    return false;
  }

  // If the app is already OEM-installed, we do not need to reinstall it. This
  // avoids extra work in the case where we are retrying the flow after an
  // install error for a different app.
  AppServiceProxy* proxy = AppServiceProxyFactory::GetForProfile(profile_);
  bool oem_installed = false;

  proxy->AppRegistryCache().ForOneApp(
      web_app_installer_->GetAppId(app),
      [&oem_installed](const AppUpdate& app) {
        oem_installed = apps_util::IsInstalled(app.Readiness()) &&
                        app.InstallReason() == InstallReason::kOem;
      });

  return !oem_installed;
}

const base::Value::Dict& AppPreloadService::GetStateManager() const {
  const base::Value::Dict& value =
      profile_->GetPrefs()->GetDict(prefs::kApsStateManager);
  return value;
}

}  // namespace apps
