// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_platform_metrics.h"

#include <set>

#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/util/values/values_util.h"
#include "chrome/browser/apps/app_service/app_service_metrics.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"

namespace {

// UMA metrics for a snapshot count of installed apps.
constexpr char kAppsCountHistogramPrefix[] = "Apps.AppsCount.";
constexpr char kAppsCountPerInstallSourceHistogramPrefix[] =
    "Apps.AppsCountPerInstallSource.";
constexpr char kAppsRunningDurationHistogramPrefix[] = "Apps.RunningDuration.";
constexpr char kAppsRunningPercentageHistogramPrefix[] =
    "Apps.RunningPercentage.";
constexpr char kAppsActivatedCountHistogramPrefix[] = "Apps.ActivatedCount.";
constexpr char kAppsUsageTimeHistogramPrefix[] = "Apps.UsageTime.";

constexpr char kInstallSourceUnknownHistogram[] = "Unknown";
constexpr char kInstallSourceSystemHistogram[] = "System";
constexpr char kInstallSourcePolicyHistogram[] = "Policy";
constexpr char kInstallSourceOemHistogram[] = "Oem";
constexpr char kInstallSourcePreloadHistogram[] = "Preload";
constexpr char kInstallSourceSyncHistogram[] = "Sync";
constexpr char kInstallSourceUserHistogram[] = "User";

constexpr base::TimeDelta kMinDuration = base::TimeDelta::FromSeconds(1);
constexpr base::TimeDelta kMaxDuration = base::TimeDelta::FromDays(1);
constexpr base::TimeDelta kMaxUsageDuration = base::TimeDelta::FromMinutes(5);
constexpr int kDurationBuckets = 100;
constexpr int kUsageTimeBuckets = 50;

std::set<apps::AppTypeName>& GetAppTypeNameSet() {
  static base::NoDestructor<std::set<apps::AppTypeName>> app_type_name_map;
  if (app_type_name_map->empty()) {
    app_type_name_map->insert(apps::AppTypeName::kArc);
    app_type_name_map->insert(apps::AppTypeName::kBuiltIn);
    app_type_name_map->insert(apps::AppTypeName::kCrostini);
    app_type_name_map->insert(apps::AppTypeName::kChromeApp);
    app_type_name_map->insert(apps::AppTypeName::kWeb);
    app_type_name_map->insert(apps::AppTypeName::kMacOs);
    app_type_name_map->insert(apps::AppTypeName::kPluginVm);
    app_type_name_map->insert(apps::AppTypeName::kStandaloneBrowser);
    app_type_name_map->insert(apps::AppTypeName::kRemote);
    app_type_name_map->insert(apps::AppTypeName::kBorealis);
    app_type_name_map->insert(apps::AppTypeName::kSystemWeb);
    app_type_name_map->insert(apps::AppTypeName::kChromeBrowser);
  }
  return *app_type_name_map;
}

// Determines what app type a Chrome App should be logged as based on its launch
// container and app id. In particular, Chrome apps in tabs are logged as part
// of Chrome browser.
apps::AppTypeName GetAppTypeNameForChromeApp(
    Profile* profile,
    const std::string& app_id,
    apps::mojom::LaunchContainer container) {
  if (app_id == extension_misc::kChromeAppId) {
    return apps::AppTypeName::kChromeBrowser;
  }

  DCHECK(profile);
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile);
  DCHECK(registry);
  const extensions::Extension* extension =
      registry->GetInstalledExtension(app_id);

  if (!extension || !extension->is_app()) {
    return apps::AppTypeName::kUnknown;
  }

  if (CanLaunchViaEvent(extension)) {
    return apps::AppTypeName::kChromeApp;
  }

  switch (container) {
    case apps::mojom::LaunchContainer::kLaunchContainerWindow:
      return apps::AppTypeName::kChromeApp;
    case apps::mojom::LaunchContainer::kLaunchContainerTab:
      return apps::AppTypeName::kChromeBrowser;
    default:
      break;
  }

  apps::mojom::LaunchContainer launch_container =
      extensions::GetLaunchContainer(extensions::ExtensionPrefs::Get(profile),
                                     extension);
  if (launch_container == apps::mojom::LaunchContainer::kLaunchContainerTab) {
    return apps::AppTypeName::kChromeBrowser;
  }

  return apps::AppTypeName::kChromeApp;
}

// Determines what app type a web app should be logged as based on its launch
// container and app id. In particular, web apps in tabs are logged as part of
// Chrome browser.
apps::AppTypeName GetAppTypeNameForWebApp(
    Profile* profile,
    const std::string& app_id,
    apps::mojom::LaunchContainer container) {
  auto* provider = web_app::WebAppProvider::Get(profile);
  DCHECK(provider);

  const auto* registrar = provider->registrar().AsWebAppRegistrar();
  if (!registrar) {
    return apps::AppTypeName::kChromeBrowser;
  }

  const auto* web_app = registrar->GetAppById(app_id);
  if (!web_app) {
    return apps::AppTypeName::kChromeBrowser;
  }

  if (web_app->IsSystemApp()) {
    return apps::AppTypeName::kSystemWeb;
  }

  switch (container) {
    case apps::mojom::LaunchContainer::kLaunchContainerWindow:
      return apps::AppTypeName::kWeb;
    case apps::mojom::LaunchContainer::kLaunchContainerTab:
      return apps::AppTypeName::kChromeBrowser;
    default:
      break;
  }

  if (web_app::ConvertDisplayModeToAppLaunchContainer(
          registrar->GetAppEffectiveDisplayMode(app_id)) ==
      apps::mojom::LaunchContainer::kLaunchContainerTab) {
    return apps::AppTypeName::kChromeBrowser;
  }

  return apps::AppTypeName::kWeb;
}

// Returns AppTypeName used for app launch metrics.
apps::AppTypeName GetAppTypeName(Profile* profile,
                                 apps::mojom::AppType app_type,
                                 const std::string& app_id,
                                 apps::mojom::LaunchContainer container) {
  switch (app_type) {
    case apps::mojom::AppType::kUnknown:
      return apps::AppTypeName::kUnknown;
    case apps::mojom::AppType::kArc:
      return apps::AppTypeName::kArc;
    case apps::mojom::AppType::kBuiltIn:
      return apps::AppTypeName::kBuiltIn;
    case apps::mojom::AppType::kCrostini:
      return apps::AppTypeName::kCrostini;
    case apps::mojom::AppType::kExtension:
      return GetAppTypeNameForChromeApp(profile, app_id, container);
    case apps::mojom::AppType::kWeb:
      return GetAppTypeNameForWebApp(profile, app_id, container);
    case apps::mojom::AppType::kMacOs:
      return apps::AppTypeName::kMacOs;
    case apps::mojom::AppType::kPluginVm:
      return apps::AppTypeName::kPluginVm;
    case apps::mojom::AppType::kStandaloneBrowser:
      return apps::AppTypeName::kStandaloneBrowser;
    case apps::mojom::AppType::kRemote:
      return apps::AppTypeName::kRemote;
    case apps::mojom::AppType::kBorealis:
      return apps::AppTypeName::kBorealis;
    case apps::mojom::AppType::kSystemWeb:
      return apps::AppTypeName::kSystemWeb;
  }
}

std::string GetInstallSource(apps::mojom::InstallSource install_source) {
  switch (install_source) {
    case apps::mojom::InstallSource::kUnknown:
      return kInstallSourceUnknownHistogram;
    case apps::mojom::InstallSource::kSystem:
      return kInstallSourceSystemHistogram;
    case apps::mojom::InstallSource::kPolicy:
      return kInstallSourcePolicyHistogram;
    case apps::mojom::InstallSource::kOem:
      return kInstallSourceOemHistogram;
    case apps::mojom::InstallSource::kDefault:
      return kInstallSourcePreloadHistogram;
    case apps::mojom::InstallSource::kSync:
      return kInstallSourceSyncHistogram;
    case apps::mojom::InstallSource::kUser:
      return kInstallSourceUserHistogram;
  }
}

// Returns false if |window| is a Chrome app window or a standalone web app
// window. Otherwise, return true;
bool IsBrowser(aura::Window* window) {
  Browser* browser = chrome::FindBrowserWithWindow(window->GetToplevelWindow());
  if (!browser || browser->is_type_app() || browser->is_type_app_popup()) {
    return false;
  }
  return true;
}

// Determines what app type a web app should be logged as based on |window|. In
// particular, web apps in tabs are logged as part of Chrome browser.
apps::AppTypeName GetAppTypeNameForWebAppWindow(Profile* profile,
                                                const std::string& app_id,
                                                aura::Window* window) {
  if (IsBrowser(window)) {
    return apps::AppTypeName::kChromeBrowser;
  }

  if (GetAppTypeNameForWebApp(
          profile, app_id,
          apps::mojom::LaunchContainer::kLaunchContainerNone) ==
      apps::AppTypeName::kSystemWeb) {
    return apps::AppTypeName::kSystemWeb;
  }

  return apps::AppTypeName::kWeb;
}

// Returns AppTypeName used for app running metrics.
apps::AppTypeName GetAppTypeName(Profile* profile,
                                 apps::mojom::AppType app_type,
                                 const std::string& app_id,
                                 aura::Window* window) {
  switch (app_type) {
    case apps::mojom::AppType::kUnknown:
      return apps::AppTypeName::kUnknown;
    case apps::mojom::AppType::kArc:
      return apps::AppTypeName::kArc;
    case apps::mojom::AppType::kBuiltIn:
      return apps::AppTypeName::kBuiltIn;
    case apps::mojom::AppType::kCrostini:
      return apps::AppTypeName::kCrostini;
    case apps::mojom::AppType::kExtension:
      return IsBrowser(window) ? apps::AppTypeName::kChromeBrowser
                               : apps::AppTypeName::kChromeApp;
    case apps::mojom::AppType::kWeb:
      return GetAppTypeNameForWebAppWindow(profile, app_id, window);
    case apps::mojom::AppType::kMacOs:
      return apps::AppTypeName::kMacOs;
    case apps::mojom::AppType::kPluginVm:
      return apps::AppTypeName::kPluginVm;
    case apps::mojom::AppType::kStandaloneBrowser:
      return apps::AppTypeName::kStandaloneBrowser;
    case apps::mojom::AppType::kRemote:
      return apps::AppTypeName::kRemote;
    case apps::mojom::AppType::kBorealis:
      return apps::AppTypeName::kBorealis;
    case apps::mojom::AppType::kSystemWeb:
      return apps::AppTypeName::kSystemWeb;
  }
}

// Records the number of times Chrome OS apps are launched grouped by the launch
// source.
void RecordAppLaunchSource(apps::mojom::LaunchSource launch_source) {
  base::UmaHistogramEnumeration("Apps.AppLaunchSource", launch_source);
}

// Records the number of times Chrome OS apps are launched grouped by the app
// type.
void RecordAppLaunchPerAppType(apps::AppTypeName app_type_name) {
  if (app_type_name == apps::AppTypeName::kUnknown) {
    return;
  }

  base::UmaHistogramEnumeration("Apps.AppLaunchPerAppType", app_type_name);
}

}  // namespace

namespace apps {

constexpr char kAppRunningDuration[] =
    "app_platform_metrics.app_running_duration";
constexpr char kAppActivatedCount[] =
    "app_platform_metrics.app_activated_count";

constexpr char kArcHistogramName[] = "Arc";
constexpr char kBuiltInHistogramName[] = "BuiltIn";
constexpr char kCrostiniHistogramName[] = "Crostini";
constexpr char kChromeAppHistogramName[] = "ChromeApp";
constexpr char kWebAppHistogramName[] = "WebApp";
constexpr char kMacOsHistogramName[] = "MacOs";
constexpr char kPluginVmHistogramName[] = "PluginVm";
constexpr char kStandaloneBrowserHistogramName[] = "StandaloneBrowser";
constexpr char kRemoteHistogramName[] = "RemoteApp";
constexpr char kBorealisHistogramName[] = "Borealis";
constexpr char kSystemWebAppHistogramName[] = "SystemWebApp";
constexpr char kChromeBrowserHistogramName[] = "ChromeBrowser";

std::string GetAppTypeHistogramName(apps::AppTypeName app_type_name) {
  switch (app_type_name) {
    case apps::AppTypeName::kUnknown:
      return std::string();
    case apps::AppTypeName::kArc:
      return kArcHistogramName;
    case apps::AppTypeName::kBuiltIn:
      return kBuiltInHistogramName;
    case apps::AppTypeName::kCrostini:
      return kCrostiniHistogramName;
    case apps::AppTypeName::kChromeApp:
      return kChromeAppHistogramName;
    case apps::AppTypeName::kWeb:
      return kWebAppHistogramName;
    case apps::AppTypeName::kMacOs:
      return kMacOsHistogramName;
    case apps::AppTypeName::kPluginVm:
      return kPluginVmHistogramName;
    case apps::AppTypeName::kStandaloneBrowser:
      return kStandaloneBrowserHistogramName;
    case apps::AppTypeName::kRemote:
      return kRemoteHistogramName;
    case apps::AppTypeName::kBorealis:
      return kBorealisHistogramName;
    case apps::AppTypeName::kSystemWeb:
      return kSystemWebAppHistogramName;
    case apps::AppTypeName::kChromeBrowser:
      return kChromeBrowserHistogramName;
  }
}

const std::set<apps::AppTypeName>& GetAppTypeNameSet() {
  return ::GetAppTypeNameSet();
}

void RecordAppLaunchMetrics(Profile* profile,
                            apps::mojom::AppType app_type,
                            const std::string& app_id,
                            apps::mojom::LaunchSource launch_source,
                            apps::mojom::LaunchContainer container) {
  if (app_type == apps::mojom::AppType::kUnknown) {
    return;
  }

  RecordAppLaunchSource(launch_source);
  RecordAppLaunchPerAppType(
      GetAppTypeName(profile, app_type, app_id, container));
}

AppPlatformMetrics::AppPlatformMetrics(
    Profile* profile,
    apps::AppRegistryCache& app_registry_cache,
    InstanceRegistry& instance_registry)
    : profile_(profile), app_registry_cache_(app_registry_cache) {
  apps::AppRegistryCache::Observer::Observe(&app_registry_cache);
  apps::InstanceRegistry::Observer::Observe(&instance_registry);
  InitRunningDuration();
}

AppPlatformMetrics::~AppPlatformMetrics() {
  for (auto it : running_start_time_) {
    running_duration_[it.second.app_type_name] +=
        base::TimeTicks::Now() - it.second.start_time;
  }

  OnTenMinutes();
  RecordAppsUsageTime();
}

// static
std::string AppPlatformMetrics::GetAppsCountHistogramNameForTest(
    AppTypeName app_type_name) {
  return kAppsCountHistogramPrefix + GetAppTypeHistogramName(app_type_name);
}

// static
std::string
AppPlatformMetrics::GetAppsCountPerInstallSourceHistogramNameForTest(
    AppTypeName app_type_name,
    apps::mojom::InstallSource install_source) {
  return kAppsCountPerInstallSourceHistogramPrefix +
         GetAppTypeHistogramName(app_type_name) + "." +
         GetInstallSource(install_source);
}

// static
std::string AppPlatformMetrics::GetAppsRunningDurationHistogramNameForTest(
    AppTypeName app_type_name) {
  return kAppsRunningDurationHistogramPrefix +
         GetAppTypeHistogramName(app_type_name);
}

// static
std::string AppPlatformMetrics::GetAppsRunningPercentageHistogramNameForTest(
    AppTypeName app_type_name) {
  return kAppsRunningPercentageHistogramPrefix +
         GetAppTypeHistogramName(app_type_name);
}

// static
std::string AppPlatformMetrics::GetAppsActivatedCountHistogramNameForTest(
    AppTypeName app_type_name) {
  return kAppsActivatedCountHistogramPrefix +
         GetAppTypeHistogramName(app_type_name);
}

std::string AppPlatformMetrics::GetAppsUsageTimeHistogramNameForTest(
    AppTypeName app_type_name) {
  return kAppsUsageTimeHistogramPrefix + GetAppTypeHistogramName(app_type_name);
}

void AppPlatformMetrics::OnNewDay() {
  should_record_metrics_on_new_day_ = true;
  RecordAppsCount(apps::mojom::AppType::kUnknown);
  RecordAppsRunningDuration();
}

void AppPlatformMetrics::OnTenMinutes() {
  if (should_refresh_activated_count_pref) {
    should_refresh_activated_count_pref = false;
    DictionaryPrefUpdate activated_count_update(profile_->GetPrefs(),
                                                kAppActivatedCount);
    for (auto it : activated_count_) {
      std::string app_type_name = GetAppTypeHistogramName(it.first);
      DCHECK(!app_type_name.empty());
      activated_count_update->SetIntKey(app_type_name, it.second);
    }
  }

  if (should_refresh_duration_pref) {
    should_refresh_duration_pref = false;
    DictionaryPrefUpdate running_duration_update(profile_->GetPrefs(),
                                                 kAppRunningDuration);
    for (auto it : running_duration_) {
      std::string app_type_name = GetAppTypeHistogramName(it.first);
      DCHECK(!app_type_name.empty());
      running_duration_update->SetPath(app_type_name,
                                       util::TimeDeltaToValue(it.second));
    }
  }
}

void AppPlatformMetrics::OnFiveMinutes() {
  RecordAppsUsageTime();
}

void AppPlatformMetrics::OnAppTypeInitialized(apps::mojom::AppType app_type) {
  if (should_record_metrics_on_new_day_) {
    RecordAppsCount(app_type);
  }
}

void AppPlatformMetrics::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  apps::AppRegistryCache::Observer::Observe(nullptr);
}

void AppPlatformMetrics::OnAppUpdate(const apps::AppUpdate& update) {}

void AppPlatformMetrics::OnInstanceUpdate(const apps::InstanceUpdate& update) {
  if (!update.StateChanged()) {
    return;
  }

  auto app_id = update.AppId();
  auto app_type = app_registry_cache_.GetAppType(app_id);
  if (app_type == apps::mojom::AppType::kUnknown) {
    return;
  }

  bool is_active = update.State() & apps::InstanceState::kActive;
  auto it = running_start_time_.find(update.Window());
  if (is_active) {
    if (it == running_start_time_.end()) {
      AppTypeName app_type_name =
          GetAppTypeName(profile_, app_type, update.AppId(),
                         update.Window()->GetToplevelWindow());
      if (app_type_name == apps::AppTypeName::kUnknown) {
        return;
      }

      // For browser tabs, record the top browser window running duration only,
      // and skip web apps opened in browser tabs to avoid duration duplicated
      // calculation.
      if (app_type_name == AppTypeName::kChromeBrowser &&
          app_id != extension_misc::kChromeAppId) {
        return;
      }

      running_start_time_[update.Window()].start_time = base::TimeTicks::Now();
      running_start_time_[update.Window()].app_type_name = app_type_name;

      ++activated_count_[app_type_name];
      should_refresh_activated_count_pref = true;

      start_time_per_five_minutes_[update.Window()].start_time =
          base::TimeTicks::Now();
      start_time_per_five_minutes_[update.Window()].app_type_name =
          app_type_name;
    }
    return;
  }

  // The app window is inactivated.
  if (it == running_start_time_.end()) {
    return;
  }

  AppTypeName app_type_name = it->second.app_type_name;
  running_duration_[app_type_name] +=
      base::TimeTicks::Now() - it->second.start_time;
  running_start_time_.erase(it);

  running_time_per_five_minutes_[app_type_name] +=
      base::TimeTicks::Now() -
      start_time_per_five_minutes_[update.Window()].start_time;
  start_time_per_five_minutes_.erase(update.Window());

  should_refresh_duration_pref = true;
}

void AppPlatformMetrics::OnInstanceRegistryWillBeDestroyed(
    apps::InstanceRegistry* cache) {
  apps::InstanceRegistry::Observer::Observe(nullptr);
}

void AppPlatformMetrics::InitRunningDuration() {
  DictionaryPrefUpdate running_duration_update(profile_->GetPrefs(),
                                               kAppRunningDuration);
  DictionaryPrefUpdate activated_count_update(profile_->GetPrefs(),
                                              kAppActivatedCount);

  for (auto app_type_name : GetAppTypeNameSet()) {
    std::string key = GetAppTypeHistogramName(app_type_name);
    if (key.empty()) {
      continue;
    }

    absl::optional<base::TimeDelta> unreported_duration =
        util::ValueToTimeDelta(running_duration_update->FindPath(key));
    if (unreported_duration.has_value()) {
      running_duration_[app_type_name] = unreported_duration.value();
    }

    absl::optional<int> count = activated_count_update->FindIntPath(key);
    if (count.has_value()) {
      activated_count_[app_type_name] = count.value();
    }
  }
}

void AppPlatformMetrics::ClearRunningDuration() {
  running_duration_.clear();
  activated_count_.clear();

  DictionaryPrefUpdate running_duration_update(profile_->GetPrefs(),
                                               kAppRunningDuration);
  running_duration_update->Clear();
  DictionaryPrefUpdate activated_count_update(profile_->GetPrefs(),
                                              kAppActivatedCount);
  activated_count_update->Clear();
}

void AppPlatformMetrics::RecordAppsCount(apps::mojom::AppType app_type) {
  std::map<AppTypeName, int> app_count;
  std::map<AppTypeName, std::map<apps::mojom::InstallSource, int>>
      app_count_per_install_source;
  app_registry_cache_.ForEachApp(
      [app_type, this, &app_count,
       &app_count_per_install_source](const apps::AppUpdate& update) {
        if (app_type != apps::mojom::AppType::kUnknown &&
            (update.AppType() != app_type ||
             update.AppId() == extension_misc::kChromeAppId)) {
          return;
        }

        AppTypeName app_type_name =
            GetAppTypeName(profile_, update.AppType(), update.AppId(),
                           apps::mojom::LaunchContainer::kLaunchContainerNone);

        if (app_type_name == AppTypeName::kChromeBrowser ||
            app_type_name == AppTypeName::kUnknown) {
          return;
        }

        ++app_count[app_type_name];
        ++app_count_per_install_source[app_type_name][update.InstallSource()];
      });

  for (auto it : app_count) {
    std::string histogram_name = GetAppTypeHistogramName(it.first);
    if (!histogram_name.empty() &&
        histogram_name != kChromeBrowserHistogramName) {
      // If there are more than a thousand apps installed, then that count is
      // going into an overflow bucket. We don't expect this scenario to happen
      // often.
      base::UmaHistogramCounts1000(kAppsCountHistogramPrefix + histogram_name,
                                   it.second);
      for (auto install_source_it : app_count_per_install_source[it.first]) {
        base::UmaHistogramCounts1000(
            kAppsCountPerInstallSourceHistogramPrefix + histogram_name + "." +
                GetInstallSource(install_source_it.first),
            install_source_it.second);
      }
    }
  }
}

void AppPlatformMetrics::RecordAppsRunningDuration() {
  for (auto& it : running_start_time_) {
    running_duration_[it.second.app_type_name] +=
        base::TimeTicks::Now() - it.second.start_time;
    it.second.start_time = base::TimeTicks::Now();
  }

  base::TimeDelta total_running_duration;
  for (auto it : running_duration_) {
    base::UmaHistogramCustomTimes(
        kAppsRunningDurationHistogramPrefix + GetAppTypeHistogramName(it.first),
        it.second, kMinDuration, kMaxDuration, kDurationBuckets);
    total_running_duration += it.second;
  }

  if (!total_running_duration.is_zero()) {
    for (auto it : running_duration_) {
      base::UmaHistogramPercentage(kAppsRunningPercentageHistogramPrefix +
                                       GetAppTypeHistogramName(it.first),
                                   100 * (it.second / total_running_duration));
    }
  }

  for (auto it : activated_count_) {
    base::UmaHistogramCounts10000(
        kAppsActivatedCountHistogramPrefix + GetAppTypeHistogramName(it.first),
        it.second);
  }

  ClearRunningDuration();
}

void AppPlatformMetrics::RecordAppsUsageTime() {
  for (auto& it : start_time_per_five_minutes_) {
    running_time_per_five_minutes_[it.second.app_type_name] +=
        base::TimeTicks::Now() - it.second.start_time;
    it.second.start_time = base::TimeTicks::Now();
  }

  for (auto it : running_time_per_five_minutes_) {
    base::UmaHistogramCustomTimes(
        kAppsUsageTimeHistogramPrefix + GetAppTypeHistogramName(it.first),
        it.second, kMinDuration, kMaxUsageDuration, kUsageTimeBuckets);
  }
  running_time_per_five_minutes_.clear();
}

}  // namespace apps
