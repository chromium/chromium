// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_preload_service/app_preload_service.h"

#include <memory>
#include <vector>

#include "base/auto_reset.h"
#include "base/barrier_callback.h"
#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_preload_service/app_preload_almanac_endpoint.h"
#include "chrome/browser/apps/app_preload_service/app_preload_service_factory.h"
#include "chrome/browser/apps/app_preload_service/preload_app_definition.h"
#include "chrome/browser/apps/app_service/app_install/app_install_service.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "components/user_manager/user_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

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

bool AreTestAppsEnabled() {
  return base::FeatureList::IsEnabled(apps::kAppPreloadServiceEnableTestApps);
}

bool g_disable_preloads_on_startup_for_testing_ = false;

}  // namespace

namespace apps {

namespace prefs {
static constexpr char kApsStateManager[] =
    "apps.app_preload_service.state_manager";
}  // namespace prefs

BASE_FEATURE(kAppPreloadServiceForceRun,
             "AppPreloadServiceForceRun",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAppPreloadServiceEnableTestApps,
             "AppPreloadServiceEnableTestApps",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAppPreloadServiceEnableArcApps,
             "AppPreloadServiceEnableArcApps",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAppPreloadServiceEnableShelfPin,
             "AppPreloadServiceEnableShelfPin",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAppPreloadServiceEnableLauncherOrder,
             "AppPreloadServiceEnableLauncherOrder",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAppPreloadServiceAllUserTypes,
             "AppPreloadServiceAllUserTypes",
             base::FEATURE_DISABLED_BY_DEFAULT);

AppPreloadService::AppPreloadService(Profile* profile) : profile_(profile) {
  if (g_disable_preloads_on_startup_for_testing_) {
    return;
  }

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
    PreloadStatusCallback callback) {
  installation_complete_callback_ = std::move(callback);
  StartFirstLoginFlow();
}

// static
base::AutoReset<bool> AppPreloadService::DisablePreloadsOnStartupForTesting() {
  return base::AutoReset<bool>(&g_disable_preloads_on_startup_for_testing_,
                               true);
}

void AppPreloadService::GetPinApps(GetPinAppsCallback callback) {
  if (data_ready_) {
    std::move(callback).Run(pin_apps_, pin_order_);
  } else {
    get_pin_apps_callbacks_.push_back(std::move(callback));
  }
}

void AppPreloadService::GetLauncherOrdering(
    base::OnceCallback<void(const LauncherOrdering&)> callback) {
  if (data_ready_) {
    std::move(callback).Run(launcher_ordering_);
  } else {
    get_launcher_ordering_callbacks_.push_back(std::move(callback));
  }
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
    StartAppInstallationForFirstLogin(start_time);
  } else {
    data_ready_ = true;
  }
}

void AppPreloadService::StartAppInstallationForFirstLogin(
    base::TimeTicks start_time) {
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
  app_preload_almanac_endpoint::GetAppsForFirstLogin(
      profile_,
      base::BindOnce(&AppPreloadService::OnGetAppsForFirstLoginCompleted,
                     weak_ptr_factory_.GetWeakPtr(), start_time));
}

void AppPreloadService::OnGetAppsForFirstLoginCompleted(
    base::TimeTicks start_time,
    std::optional<std::vector<PreloadAppDefinition>> apps,
    LauncherOrdering launcher_ordering,
    ShelfPinOrdering shelf_pin_ordering) {
  data_ready_ = true;
  if (!apps.has_value()) {
    OnFirstLoginFlowComplete(start_time, /*success=*/false);
    return;
  }

  // TODO(crbug.com/327058999): Implement launcher ordering.
  std::vector<const PreloadAppDefinition*> apps_to_install;
  for (const PreloadAppDefinition& app : apps.value()) {
    if (ShouldInstallApp(app)) {
      apps_to_install.push_back(&app);
    }
  }

  if (base::FeatureList::IsEnabled(apps::kAppPreloadServiceEnableShelfPin)) {
    pin_apps_.clear();
    pin_order_.clear();
    // Collect the apps that will be pinned after install.
    for (auto* const app : apps_to_install) {
      if (shelf_pin_ordering.contains(*app->GetPackageId())) {
        pin_apps_.push_back(*app->GetPackageId());
      }
    }
    // Sort shelf pin ordering.
    std::vector<std::pair<apps::PackageId, uint32_t>> pins(
        shelf_pin_ordering.begin(), shelf_pin_ordering.end());
    base::ranges::sort(pins, {}, &std::pair<apps::PackageId, uint32_t>::second);
    base::ranges::transform(pins, std::back_inserter(pin_order_),
                            &std::pair<apps::PackageId, uint32_t>::first);
  }
  for (auto& callback : get_pin_apps_callbacks_) {
    std::move(callback).Run(pin_apps_, pin_order_);
  }
  get_pin_apps_callbacks_.clear();

  if (base::FeatureList::IsEnabled(
          apps::kAppPreloadServiceEnableLauncherOrder)) {
    launcher_ordering_ = launcher_ordering;
  }
  for (auto& callback : get_launcher_ordering_callbacks_) {
    std::move(callback).Run(launcher_ordering_);
  }
  get_launcher_ordering_callbacks_.clear();

  const auto install_barrier_callback = base::BarrierCallback<bool>(
      apps_to_install.size(),
      base::BindOnce(&AppPreloadService::OnAppInstallationsCompleted,
                     weak_ptr_factory_.GetWeakPtr(), start_time));
  AppInstallService& install_service =
      AppServiceProxyFactory::GetForProfile(profile_)->AppInstallService();
  for (const PreloadAppDefinition* app : apps_to_install) {
    install_service.InstallAppHeadless(
        app->IsOemApp() ? AppInstallSurface::kAppPreloadServiceOem
                        : AppInstallSurface::kAppPreloadServiceDefault,
        app->ToAppInstallData(), install_barrier_callback);
  }
}

void AppPreloadService::OnAppInstallationsCompleted(
    base::TimeTicks start_time,
    const std::vector<bool>& results) {
  OnFirstLoginFlowComplete(start_time,
                           base::ranges::all_of(results, std::identity{}));
}

void AppPreloadService::OnFirstLoginFlowComplete(base::TimeTicks start_time,
                                                 bool success) {
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
  // We preload android apps (when feature enabled) and web apps.
  if (app.GetPlatform() == PackageType::kArc) {
    if (!base::FeatureList::IsEnabled(apps::kAppPreloadServiceEnableArcApps)) {
      return false;
    }
  } else if (app.GetPlatform() != PackageType::kWeb) {
    return false;
  }

  // We currently install apps which were requested by the device OEM or
  // installed by default (i.e. by Google). If the testing feature is enabled,
  // also install test apps.
  bool install_reason_allowed = app.IsOemApp() || app.IsDefaultApp() ||
                                (app.IsTestApp() && AreTestAppsEnabled());
  if (!install_reason_allowed) {
    return false;
  }

  // If the app is already installed with the relevant install reason, we do not
  // need to reinstall it. This avoids extra work in the case where we are
  // retrying the flow after an install error for a different app.
  InstallReason expected_reason =
      app.IsOemApp() ? InstallReason::kOem : InstallReason::kDefault;
  AppServiceProxy* proxy = AppServiceProxyFactory::GetForProfile(profile_);
  bool installed = false;

  proxy->AppRegistryCache().ForEachApp(
      [&installed, expected_reason, app](const apps::AppUpdate& update) {
        // It's possible that if APS requests the same app to be installed for
        // multiple reasons, this check could incorrectly return false, as App
        // Service only reports the highest priority install reason. This is
        // acceptable since the check is just an optimization.
        if (update.InstallerPackageId() == app.GetPackageId() &&
            apps_util::IsInstalled(update.Readiness()) &&
            update.InstallReason() == expected_reason) {
          installed = true;
        }
      });

  return !installed;
}

const base::Value::Dict& AppPreloadService::GetStateManager() const {
  const base::Value::Dict& value =
      profile_->GetPrefs()->GetDict(prefs::kApsStateManager);
  return value;
}

}  // namespace apps
