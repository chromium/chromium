// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/metrics/app_platform_metrics.h"

#include <set>

#include "base/json/values_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/metrics/app_service_metrics.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "components/ukm/app_source_url_recorder.h"
#include "components/user_manager/user_manager.h"
#include "extensions/common/constants.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/aura/window.h"

namespace {

// UMA metrics for a snapshot count of installed apps.
constexpr char kAppsCountHistogramPrefix[] = "Apps.AppsCount.";
constexpr char kAppsCountPerInstallReasonHistogramPrefix[] =
    "Apps.AppsCountPerInstallReason.";
constexpr char kAppsRunningDurationHistogramPrefix[] = "Apps.RunningDuration.";
constexpr char kAppsRunningPercentageHistogramPrefix[] =
    "Apps.RunningPercentage.";
constexpr char kAppsActivatedCountHistogramPrefix[] = "Apps.ActivatedCount.";
constexpr char kAppsUsageTimeHistogramPrefix[] = "Apps.UsageTime.";
constexpr char kAppsUsageTimeHistogramPrefixV2[] = "Apps.UsageTimeV2.";

constexpr char kInstallReasonUnknownHistogram[] = "Unknown";
constexpr char kInstallReasonSystemHistogram[] = "System";
constexpr char kInstallReasonPolicyHistogram[] = "Policy";
constexpr char kInstallReasonOemHistogram[] = "Oem";
constexpr char kInstallReasonPreloadHistogram[] = "Preload";
constexpr char kInstallReasonSyncHistogram[] = "Sync";
constexpr char kInstallReasonUserHistogram[] = "User";
constexpr char kInstallReasonSubAppHistogram[] = "SubApp";

constexpr base::TimeDelta kMaxDuration = base::Days(1);

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
    app_type_name_map->insert(apps::AppTypeName::kStandaloneBrowserChromeApp);
    app_type_name_map->insert(apps::AppTypeName::kExtension);
  }
  return *app_type_name_map;
}

std::string GetInstallReason(apps::mojom::InstallReason install_reason) {
  switch (install_reason) {
    case apps::mojom::InstallReason::kUnknown:
      return kInstallReasonUnknownHistogram;
    case apps::mojom::InstallReason::kSystem:
      return kInstallReasonSystemHistogram;
    case apps::mojom::InstallReason::kPolicy:
      return kInstallReasonPolicyHistogram;
    case apps::mojom::InstallReason::kOem:
      return kInstallReasonOemHistogram;
    case apps::mojom::InstallReason::kDefault:
      return kInstallReasonPreloadHistogram;
    case apps::mojom::InstallReason::kSync:
      return kInstallReasonSyncHistogram;
    case apps::mojom::InstallReason::kUser:
      return kInstallReasonUserHistogram;
    case apps::mojom::InstallReason::kSubApp:
      return kInstallReasonSubAppHistogram;
  }
}

// Returns AppTypeNameV2 used for app running metrics.
apps::AppTypeNameV2 GetAppTypeNameV2(Profile* profile,
                                     apps::mojom::AppType app_type,
                                     const std::string& app_id,
                                     aura::Window* window) {
  switch (app_type) {
    case apps::mojom::AppType::kUnknown:
      return apps::AppTypeNameV2::kUnknown;
    case apps::mojom::AppType::kArc:
      return apps::AppTypeNameV2::kArc;
    case apps::mojom::AppType::kBuiltIn:
      return apps::AppTypeNameV2::kBuiltIn;
    case apps::mojom::AppType::kCrostini:
      return apps::AppTypeNameV2::kCrostini;
    case apps::mojom::AppType::kChromeApp:
      return apps::IsBrowser(window) ? apps::AppTypeNameV2::kChromeAppTab
                                     : apps::AppTypeNameV2::kChromeAppWindow;
    case apps::mojom::AppType::kWeb: {
      apps::AppTypeName app_type_name =
          apps::GetAppTypeNameForWebAppWindow(profile, app_id, window);
      if (app_type_name == apps::AppTypeName::kChromeBrowser) {
        return apps::AppTypeNameV2::kWebTab;
      } else if (app_type_name == apps::AppTypeName::kSystemWeb) {
        return apps::AppTypeNameV2::kSystemWeb;
      } else {
        return apps::AppTypeNameV2::kWebWindow;
      }
    }
    case apps::mojom::AppType::kMacOs:
      return apps::AppTypeNameV2::kMacOs;
    case apps::mojom::AppType::kPluginVm:
      return apps::AppTypeNameV2::kPluginVm;
    case apps::mojom::AppType::kStandaloneBrowser:
      return apps::AppTypeNameV2::kStandaloneBrowser;
    case apps::mojom::AppType::kRemote:
      return apps::AppTypeNameV2::kRemote;
    case apps::mojom::AppType::kBorealis:
      return apps::AppTypeNameV2::kBorealis;
    case apps::mojom::AppType::kSystemWeb:
      return apps::AppTypeNameV2::kSystemWeb;
    case apps::mojom::AppType::kStandaloneBrowserChromeApp:
      return apps::AppTypeNameV2::kStandaloneBrowserChromeApp;
    case apps::mojom::AppType::kExtension:
      return apps::AppTypeNameV2::kExtension;
  }
}

// Returns AppTypeNameV2 used for app launch metrics.
apps::AppTypeNameV2 GetAppTypeNameV2(Profile* profile,
                                     apps::mojom::AppType app_type,
                                     const std::string& app_id,
                                     apps::mojom::LaunchContainer container) {
  switch (app_type) {
    case apps::mojom::AppType::kUnknown:
      return apps::AppTypeNameV2::kUnknown;
    case apps::mojom::AppType::kArc:
      return apps::AppTypeNameV2::kArc;
    case apps::mojom::AppType::kBuiltIn:
      return apps::AppTypeNameV2::kBuiltIn;
    case apps::mojom::AppType::kCrostini:
      return apps::AppTypeNameV2::kCrostini;
    case apps::mojom::AppType::kChromeApp:
      return container == apps::mojom::LaunchContainer::kLaunchContainerWindow
                 ? apps::AppTypeNameV2::kChromeAppWindow
                 : apps::AppTypeNameV2::kChromeAppTab;
    case apps::mojom::AppType::kWeb: {
      apps::AppTypeName app_type_name =
          apps::GetAppTypeNameForWebApp(profile, app_id, container);
      if (app_type_name == apps::AppTypeName::kChromeBrowser) {
        return apps::AppTypeNameV2::kWebTab;
      } else if (app_type_name == apps::AppTypeName::kSystemWeb) {
        return apps::AppTypeNameV2::kSystemWeb;
      } else {
        return apps::AppTypeNameV2::kWebWindow;
      }
    }
    case apps::mojom::AppType::kMacOs:
      return apps::AppTypeNameV2::kMacOs;
    case apps::mojom::AppType::kPluginVm:
      return apps::AppTypeNameV2::kPluginVm;
    case apps::mojom::AppType::kStandaloneBrowser:
      return apps::AppTypeNameV2::kStandaloneBrowser;
    case apps::mojom::AppType::kRemote:
      return apps::AppTypeNameV2::kRemote;
    case apps::mojom::AppType::kBorealis:
      return apps::AppTypeNameV2::kBorealis;
    case apps::mojom::AppType::kSystemWeb:
      return apps::AppTypeNameV2::kSystemWeb;
    case apps::mojom::AppType::kStandaloneBrowserChromeApp:
      return apps::AppTypeNameV2::kStandaloneBrowserChromeApp;
    case apps::mojom::AppType::kExtension:
      return apps::AppTypeNameV2::kExtension;
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

  base::UmaHistogramEnumeration(apps::kAppLaunchPerAppTypeHistogramName,
                                app_type_name);
}

// Records the number of times Chrome OS apps are launched grouped by the app
// type V2.
void RecordAppLaunchPerAppTypeV2(apps::AppTypeNameV2 app_type_name_v2) {
  if (app_type_name_v2 == apps::AppTypeNameV2::kUnknown) {
    return;
  }

  base::UmaHistogramEnumeration(apps::kAppLaunchPerAppTypeV2HistogramName,
                                app_type_name_v2);
}

}  // namespace

namespace apps {

constexpr char kAppRunningDuration[] =
    "app_platform_metrics.app_running_duration";
constexpr char kAppActivatedCount[] =
    "app_platform_metrics.app_activated_count";

constexpr char kAppLaunchPerAppTypeHistogramName[] = "Apps.AppLaunchPerAppType";
constexpr char kAppLaunchPerAppTypeV2HistogramName[] =
    "Apps.AppLaunchPerAppTypeV2";

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
constexpr char kStandaloneBrowserChromeAppHistogramName[] =
    "StandaloneBrowserChromeApp";
constexpr char kExtensionHistogramName[] = "Extension";

constexpr char kChromeAppTabHistogramName[] = "ChromeAppTab";
constexpr char kChromeAppWindowHistogramName[] = "ChromeAppWindow";
constexpr char kWebAppTabHistogramName[] = "WebAppTab";
constexpr char kWebAppWindowHistogramName[] = "WebAppWindow";

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
    case apps::AppTypeName::kStandaloneBrowserChromeApp:
      return kStandaloneBrowserChromeAppHistogramName;
    case apps::AppTypeName::kExtension:
      return kExtensionHistogramName;
  }
}

std::string GetAppTypeHistogramNameV2(apps::AppTypeNameV2 app_type_name) {
  switch (app_type_name) {
    case apps::AppTypeNameV2::kUnknown:
      return std::string();
    case apps::AppTypeNameV2::kArc:
      return kArcHistogramName;
    case apps::AppTypeNameV2::kBuiltIn:
      return kBuiltInHistogramName;
    case apps::AppTypeNameV2::kCrostini:
      return kCrostiniHistogramName;
    case apps::AppTypeNameV2::kChromeAppWindow:
      return kChromeAppWindowHistogramName;
    case apps::AppTypeNameV2::kChromeAppTab:
      return kChromeAppTabHistogramName;
    case apps::AppTypeNameV2::kWebWindow:
      return kWebAppWindowHistogramName;
    case apps::AppTypeNameV2::kWebTab:
      return kWebAppTabHistogramName;
    case apps::AppTypeNameV2::kMacOs:
      return kMacOsHistogramName;
    case apps::AppTypeNameV2::kPluginVm:
      return kPluginVmHistogramName;
    case apps::AppTypeNameV2::kStandaloneBrowser:
      return kStandaloneBrowserHistogramName;
    case apps::AppTypeNameV2::kRemote:
      return kRemoteHistogramName;
    case apps::AppTypeNameV2::kBorealis:
      return kBorealisHistogramName;
    case apps::AppTypeNameV2::kSystemWeb:
      return kSystemWebAppHistogramName;
    case apps::AppTypeNameV2::kChromeBrowser:
      return kChromeBrowserHistogramName;
    case apps::AppTypeNameV2::kStandaloneBrowserChromeApp:
      return kStandaloneBrowserChromeAppHistogramName;
    case apps::AppTypeNameV2::kExtension:
      return kExtensionHistogramName;
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
  RecordAppLaunchPerAppTypeV2(
      GetAppTypeNameV2(profile, app_type, app_id, container));

  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
  if (proxy && proxy->AppPlatformMetrics()) {
    proxy->AppPlatformMetrics()->RecordAppLaunchUkm(app_type, app_id,
                                                    launch_source, container);
  }
}

AppPlatformMetrics::AppPlatformMetrics(
    Profile* profile,
    apps::AppRegistryCache& app_registry_cache,
    InstanceRegistry& instance_registry)
    : profile_(profile), app_registry_cache_(app_registry_cache) {
  apps::AppRegistryCache::Observer::Observe(&app_registry_cache);
  apps::InstanceRegistry::Observer::Observe(&instance_registry);
  user_type_by_device_type_ = GetUserTypeByDeviceTypeMetrics();
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
ukm::SourceId AppPlatformMetrics::GetSourceId(Profile* profile,
                                              const std::string& app_id) {
  if (!AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    return ukm::kInvalidSourceId;
  }

  ukm::SourceId source_id = ukm::kInvalidSourceId;
  apps::mojom::AppType app_type = GetAppType(profile, app_id);
  if (!ShouldRecordUkmForAppTypeName(ConvertMojomAppTypToAppType(app_type))) {
    return ukm::kInvalidSourceId;
  }

  switch (app_type) {
    case apps::mojom::AppType::kBuiltIn:
    case apps::mojom::AppType::kChromeApp:
    case apps::mojom::AppType::kExtension:
      source_id = ukm::AppSourceUrlRecorder::GetSourceIdForChromeApp(app_id);
      break;
    case apps::mojom::AppType::kArc:
    case apps::mojom::AppType::kWeb:
    case apps::mojom::AppType::kSystemWeb: {
      std::string publisher_id;
      apps::mojom::InstallReason install_reason;
      apps::AppServiceProxyFactory::GetForProfile(profile)
          ->AppRegistryCache()
          .ForOneApp(app_id, [&publisher_id,
                              &install_reason](const apps::AppUpdate& update) {
            publisher_id = update.PublisherId();
            install_reason = update.InstallReason();
          });
      if (publisher_id.empty()) {
        return ukm::kInvalidSourceId;
      }
      if (app_type == apps::mojom::AppType::kArc) {
        source_id = ukm::AppSourceUrlRecorder::GetSourceIdForArcPackageName(
            publisher_id);
        break;
      }
      if (app_type == apps::mojom::AppType::kSystemWeb ||
          install_reason == apps::mojom::InstallReason::kSystem) {
        // For system web apps, call GetSourceIdForChromeApp to record the app
        // id because the url could be filtered by the server side.
        source_id = ukm::AppSourceUrlRecorder::GetSourceIdForChromeApp(app_id);
        break;
      }
      source_id =
          ukm::AppSourceUrlRecorder::GetSourceIdForPWA(GURL(publisher_id));
      break;
    }
    case apps::mojom::AppType::kCrostini:
      source_id = GetSourceIdForCrostini(profile, app_id);
      break;
    case apps::mojom::AppType::kBorealis:
      source_id = GetSourceIdForBorealis(profile, app_id);
      break;
    case apps::mojom::AppType::kUnknown:
    case apps::mojom::AppType::kMacOs:
    case apps::mojom::AppType::kPluginVm:
    case apps::mojom::AppType::kStandaloneBrowser:
    case apps::mojom::AppType::kStandaloneBrowserChromeApp:
    case apps::mojom::AppType::kRemote:
      return ukm::kInvalidSourceId;
  }
  return source_id;
}

// static
ukm::SourceId AppPlatformMetrics::GetSourceIdForBorealis(
    Profile* profile,
    const std::string& app_id) {
  // Most Borealis apps are identified by a numeric ID, except these.
  if (app_id == borealis::kClientAppId) {
    return ukm::AppSourceUrlRecorder::GetSourceIdForBorealis("client");
  } else if (app_id == borealis::kInstallerAppId) {
    return ukm::AppSourceUrlRecorder::GetSourceIdForBorealis("installer");
  } else if (app_id.find(borealis::kIgnoredAppIdPrefix) != std::string::npos) {
    // These are not real apps from a user's point of view,
    // so it doesn't make sense to record metrics for them.
    return ukm::kInvalidSourceId;
  }

  auto* registry =
      guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile);
  auto registration = registry->GetRegistration(app_id);
  if (!registration) {
    // If there's no registration then we're not allowed to record anything that
    // could identify the app (and we don't know the app name anyway), but
    // recording every unregistered app in one big bucket is fine.
    //
    // In general all Borealis apps should be registered, so if we do see this
    // Source ID being reported, that's a bug.
    LOG(WARNING) << "Couldn't get Borealis ID for UNREGISTERED app " << app_id;
    return ukm::AppSourceUrlRecorder::GetSourceIdForBorealis("UNREGISTERED");
  }
  absl::optional<int> borealis_id =
      borealis::GetBorealisAppId(registration->Exec());
  if (!borealis_id)
    LOG(WARNING) << "Couldn't get Borealis ID for registered app " << app_id;
  return ukm::AppSourceUrlRecorder::GetSourceIdForBorealis(
      borealis_id ? base::NumberToString(borealis_id.value()) : "NoId");
}

// static
ukm::SourceId AppPlatformMetrics::GetSourceIdForCrostini(
    Profile* profile,
    const std::string& app_id) {
  if (app_id == crostini::kCrostiniTerminalSystemAppId) {
    // The terminal is special, since it's actually a web app (though one we
    // count as Crostini) it doesn't have a desktop id, so give it a fake one.
    return ukm::AppSourceUrlRecorder::GetSourceIdForCrostini("CrostiniTerminal",
                                                             "Terminal");
  }
  auto* registry =
      guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile);
  auto registration = registry->GetRegistration(app_id);
  if (!registration) {
    // If there's no registration then we're not allowed to record anything that
    // could identify the app (and we don't know the app name anyway), but
    // recording every unregistered app in one big bucket is fine.
    return ukm::AppSourceUrlRecorder::GetSourceIdForCrostini("UNREGISTERED",
                                                             "UNREGISTERED");
  }
  auto desktop_id = registration->DesktopFileId() == ""
                        ? "NoId"
                        : registration->DesktopFileId();
  return ukm::AppSourceUrlRecorder::GetSourceIdForCrostini(
      desktop_id, registration->Name());
}

// static
void AppPlatformMetrics::RemoveSourceId(ukm::SourceId source_id) {
  ukm::AppSourceUrlRecorder::MarkSourceForDeletion(source_id);
}

// static
std::string AppPlatformMetrics::GetAppsCountHistogramNameForTest(
    AppTypeName app_type_name) {
  return kAppsCountHistogramPrefix + GetAppTypeHistogramName(app_type_name);
}

// static
std::string
AppPlatformMetrics::GetAppsCountPerInstallReasonHistogramNameForTest(
    AppTypeName app_type_name,
    apps::mojom::InstallReason install_reason) {
  return kAppsCountPerInstallReasonHistogramPrefix +
         GetAppTypeHistogramName(app_type_name) + "." +
         GetInstallReason(install_reason);
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

std::string AppPlatformMetrics::GetAppsUsageTimeHistogramNameForTest(
    AppTypeNameV2 app_type_name) {
  return kAppsUsageTimeHistogramPrefix +
         GetAppTypeHistogramNameV2(app_type_name);
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
                                       base::TimeDeltaToValue(it.second));
    }
  }
}

void AppPlatformMetrics::OnFiveMinutes() {
  RecordAppsUsageTime();
}

void AppPlatformMetrics::RecordAppLaunchUkm(
    apps::mojom::AppType app_type,
    const std::string& app_id,
    apps::mojom::LaunchSource launch_source,
    apps::mojom::LaunchContainer container) {
  if (app_type == apps::mojom::AppType::kUnknown ||
      !ShouldRecordUkm(profile_)) {
    return;
  }

  apps::AppTypeName app_type_name =
      GetAppTypeName(profile_, app_type, app_id, container);

  ukm::SourceId source_id = GetSourceId(profile_, app_id);
  if (source_id == ukm::kInvalidSourceId) {
    return;
  }

  ukm::builders::ChromeOSApp_Launch builder(source_id);
  builder.SetAppType((int)app_type_name)
      .SetLaunchSource((int)launch_source)
      .SetUserDeviceMatrix(GetUserTypeByDeviceTypeMetrics())
      .Record(ukm::UkmRecorder::Get());
  RemoveSourceId(source_id);
}

void AppPlatformMetrics::RecordAppUninstallUkm(
    apps::mojom::AppType app_type,
    const std::string& app_id,
    apps::mojom::UninstallSource uninstall_source) {
  AppTypeName app_type_name =
      GetAppTypeName(profile_, app_type, app_id,
                     apps::mojom::LaunchContainer::kLaunchContainerNone);

  ukm::SourceId source_id = GetSourceId(profile_, app_id);
  if (source_id == ukm::kInvalidSourceId) {
    return;
  }

  ukm::builders::ChromeOSApp_UninstallApp builder(source_id);
  builder.SetAppType((int)app_type_name)
      .SetUninstallSource((int)uninstall_source)
      .SetUserDeviceMatrix(user_type_by_device_type_)
      .Record(ukm::UkmRecorder::Get());
  RemoveSourceId(source_id);
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

void AppPlatformMetrics::OnAppUpdate(const apps::AppUpdate& update) {
  if (!ShouldRecordUkm(profile_)) {
    return;
  }

  if (!update.ReadinessChanged() ||
      update.Readiness() != apps::mojom::Readiness::kReady ||
      apps_util::IsInstalled(update.PriorReadiness())) {
    return;
  }

  InstallTime install_time =
      app_registry_cache_.IsAppTypeInitialized(update.AppType())
          ? InstallTime::kRunning
          : InstallTime::kInit;
  RecordAppsInstallUkm(update, install_time);
}

void AppPlatformMetrics::OnInstanceUpdate(const apps::InstanceUpdate& update) {
  if (!update.StateChanged()) {
    return;
  }

  auto app_id = update.AppId();
  auto app_type = GetAppType(profile_, app_id);
  if (app_type == apps::mojom::AppType::kUnknown) {
    return;
  }

  bool is_active = update.State() & apps::InstanceState::kActive;
  if (is_active) {
    AppTypeName app_type_name =
        GetAppTypeNameForWindow(profile_, app_type, app_id, update.Window());
    if (app_type_name == apps::AppTypeName::kUnknown) {
      return;
    }

    apps::InstanceState kInActivated = static_cast<apps::InstanceState>(
        apps::InstanceState::kVisible | apps::InstanceState::kRunning);

    // For the browser window, if a tab of the browser is activated, we don't
    // need to calculate the browser window running time.
    if (app_id == extension_misc::kChromeAppId &&
        browser_to_tab_list_.HasActivatedTab(update.Window())) {
      SetWindowInActivated(app_id, update.InstanceId(), kInActivated);
      return;
    }

    // For web apps open in tabs, set the top browser window as inactive to stop
    // calculating the browser window running time.
    if (IsAppOpenedInTab(app_type_name, app_id)) {
      // When the tab is pulled to a separate browser window, the instance id is
      // not changed, but the parent browser window is changed. So remove the
      // tab window instance from previous browser window, and add it to the new
      // browser window.
      browser_to_tab_list_.RemoveActivatedTab(update.InstanceId());
      browser_to_tab_list_.AddActivatedTab(update.Window()->GetToplevelWindow(),
                                           update.InstanceId(), update.AppId());
      InstanceState state;
      base::UnguessableToken browser_id;
      GetBrowserIdAndState(update, browser_id, state);
      if (browser_id) {
        SetWindowInActivated(extension_misc::kChromeAppId, browser_id,
                             kInActivated);
      }
    }

    AppTypeNameV2 app_type_name_v2 =
        GetAppTypeNameV2(profile_, app_type, app_id, update.Window());

    SetWindowActivated(app_type, app_type_name, app_type_name_v2, app_id,
                       update.InstanceId());
    return;
  }

  AppTypeName app_type_name = AppTypeName::kUnknown;
  auto it = running_start_time_.find(update.InstanceId());
  if (it != running_start_time_.end()) {
    app_type_name = it->second.app_type_name;
  }

  if (IsAppOpenedInTab(app_type_name, app_id)) {
    UpdateBrowserWindowStatus(update);
  }

  SetWindowInActivated(app_id, update.InstanceId(), update.State());
}

void AppPlatformMetrics::OnInstanceRegistryWillBeDestroyed(
    apps::InstanceRegistry* cache) {
  apps::InstanceRegistry::Observer::Observe(nullptr);
}

void AppPlatformMetrics::GetBrowserIdAndState(
    const InstanceUpdate& update,
    base::UnguessableToken& browser_id,
    InstanceState& state) const {
  auto* browser_window = update.Window()->GetToplevelWindow();
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile_);
  DCHECK(proxy);
  browser_id = base::UnguessableToken();
  state = InstanceState::kUnknown;
  proxy->InstanceRegistry().ForInstancesWithWindow(
      browser_window, [&](const InstanceUpdate& browser_update) {
        if (browser_update.AppId() == extension_misc::kChromeAppId ||
            browser_update.AppId() == extension_misc::kLacrosAppId) {
          browser_id = browser_update.InstanceId();
          state = browser_update.State();
        }
      });
}

void AppPlatformMetrics::UpdateBrowserWindowStatus(
    const InstanceUpdate& update) {
  const base::UnguessableToken& tab_id = update.InstanceId();
  const auto* browser_window = browser_to_tab_list_.GetBrowserWindow(tab_id);
  if (!browser_window) {
    return;
  }

  // Remove the tab id from `active_browser_to_tabs_`.
  browser_to_tab_list_.RemoveActivatedTab(tab_id);

  // If there are other activated web app tab, we don't need to set the browser
  // window as activated.
  if (browser_to_tab_list_.HasActivatedTab(browser_window)) {
    return;
  }

  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile_);
  DCHECK(proxy);
  InstanceState state;
  base::UnguessableToken browser_id;
  GetBrowserIdAndState(update, browser_id, state);
  if (state & InstanceState::kActive) {
    // The browser window is activated, start calculating the browser window
    // running time.
    // TODO(crbug.com/1251501): Handle lacros window.
    SetWindowActivated(apps::mojom::AppType::kChromeApp,
                       AppTypeName::kChromeBrowser,
                       AppTypeNameV2::kChromeBrowser,
                       extension_misc::kChromeAppId, browser_id);
  }
}

void AppPlatformMetrics::SetWindowActivated(
    apps::mojom::AppType app_type,
    AppTypeName app_type_name,
    AppTypeNameV2 app_type_name_v2,
    const std::string& app_id,
    const base::UnguessableToken& instance_id) {
  auto it = running_start_time_.find(instance_id);
  if (it != running_start_time_.end()) {
    return;
  }

  running_start_time_[instance_id].start_time = base::TimeTicks::Now();
  running_start_time_[instance_id].app_type_name = app_type_name;
  running_start_time_[instance_id].app_type_name_v2 = app_type_name_v2;

  ++activated_count_[app_type_name];
  should_refresh_activated_count_pref = true;

  start_time_per_five_minutes_[instance_id].start_time = base::TimeTicks::Now();
  start_time_per_five_minutes_[instance_id].app_type_name = app_type_name;
  start_time_per_five_minutes_[instance_id].app_type_name_v2 = app_type_name_v2;
  start_time_per_five_minutes_[instance_id].app_id = app_id;
}

void AppPlatformMetrics::SetWindowInActivated(
    const std::string& app_id,
    const base::UnguessableToken& instance_id,
    apps::InstanceState state) {
  bool is_close = state & apps::InstanceState::kDestroyed;
  auto usage_time_it = usage_time_per_five_minutes_.find(instance_id);
  if (is_close && usage_time_it != usage_time_per_five_minutes_.end()) {
    usage_time_it->second.window_is_closed = true;
  }

  auto it = running_start_time_.find(instance_id);
  if (it == running_start_time_.end()) {
    return;
  }

  AppTypeName app_type_name = it->second.app_type_name;
  AppTypeNameV2 app_type_name_v2 = it->second.app_type_name_v2;

  running_duration_[app_type_name] +=
      base::TimeTicks::Now() - it->second.start_time;

  base::TimeDelta running_time =
      base::TimeTicks::Now() -
      start_time_per_five_minutes_[instance_id].start_time;
  app_type_running_time_per_five_minutes_[app_type_name] += running_time;
  app_type_v2_running_time_per_five_minutes_[app_type_name_v2] += running_time;

  if (usage_time_it == usage_time_per_five_minutes_.end()) {
    auto source_id = GetSourceId(profile_, app_id);
    if (source_id != ukm::kInvalidSourceId) {
      usage_time_per_five_minutes_[it->first].source_id = source_id;
      usage_time_it = usage_time_per_five_minutes_.find(it->first);
    }
  }
  if (usage_time_it != usage_time_per_five_minutes_.end()) {
    usage_time_it->second.app_type_name = app_type_name;
    usage_time_it->second.running_time += running_time;
  }

  running_start_time_.erase(it);
  start_time_per_five_minutes_.erase(instance_id);

  should_refresh_duration_pref = true;
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
        base::ValueToTimeDelta(running_duration_update->FindPath(key));
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
  running_duration_update->DictClear();
  DictionaryPrefUpdate activated_count_update(profile_->GetPrefs(),
                                              kAppActivatedCount);
  activated_count_update->DictClear();
}

void AppPlatformMetrics::RecordAppsCount(apps::mojom::AppType app_type) {
  std::map<AppTypeName, int> app_count;
  std::map<AppTypeName, std::map<apps::mojom::InstallReason, int>>
      app_count_per_install_reason;
  app_registry_cache_.ForEachApp(
      [app_type, this, &app_count,
       &app_count_per_install_reason](const apps::AppUpdate& update) {
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
        ++app_count_per_install_reason[app_type_name][update.InstallReason()];
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
      for (auto install_reason_it : app_count_per_install_reason[it.first]) {
        base::UmaHistogramCounts1000(
            kAppsCountPerInstallReasonHistogramPrefix + histogram_name + "." +
                GetInstallReason(install_reason_it.first),
            install_reason_it.second);
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
    base::TimeDelta running_time =
        base::TimeTicks::Now() - it.second.start_time;
    app_type_running_time_per_five_minutes_[it.second.app_type_name] +=
        running_time;
    app_type_v2_running_time_per_five_minutes_[it.second.app_type_name_v2] +=
        running_time;

    auto usage_time_it = usage_time_per_five_minutes_.find(it.first);
    if (usage_time_it == usage_time_per_five_minutes_.end()) {
      auto source_id = GetSourceId(profile_, it.second.app_id);
      if (source_id != ukm::kInvalidSourceId) {
        usage_time_per_five_minutes_[it.first].source_id = source_id;
        usage_time_it = usage_time_per_five_minutes_.find(it.first);
      }
    }
    if (usage_time_it != usage_time_per_five_minutes_.end()) {
      usage_time_it->second.app_type_name = it.second.app_type_name;
      usage_time_it->second.running_time += running_time;
    }

    it.second.start_time = base::TimeTicks::Now();
  }

  for (auto it : app_type_running_time_per_five_minutes_) {
    base::UmaHistogramCustomTimes(
        kAppsUsageTimeHistogramPrefix + GetAppTypeHistogramName(it.first),
        it.second, kMinDuration, kMaxUsageDuration, kUsageTimeBuckets);
  }

  for (auto it : app_type_v2_running_time_per_five_minutes_) {
    base::UmaHistogramCustomTimes(
        kAppsUsageTimeHistogramPrefixV2 + GetAppTypeHistogramNameV2(it.first),
        it.second, kMinDuration, kMaxUsageDuration, kUsageTimeBuckets);
  }

  app_type_running_time_per_five_minutes_.clear();
  app_type_v2_running_time_per_five_minutes_.clear();

  RecordAppsUsageTimeUkm();
}

void AppPlatformMetrics::RecordAppsUsageTimeUkm() {
  if (!ShouldRecordUkm(profile_)) {
    return;
  }

  std::vector<base::UnguessableToken> closed_instance_ids;
  for (auto& it : usage_time_per_five_minutes_) {
    apps::AppTypeName app_type_name = it.second.app_type_name;
    ukm::SourceId source_id = it.second.source_id;
    DCHECK_NE(source_id, ukm::kInvalidSourceId);
    if (!it.second.running_time.is_zero()) {
      ukm::builders::ChromeOSApp_UsageTime builder(source_id);
      builder.SetAppType((int)app_type_name)
          .SetDuration(it.second.running_time.InMilliseconds())
          .SetUserDeviceMatrix(user_type_by_device_type_)
          .Record(ukm::UkmRecorder::Get());
    }
    if (it.second.window_is_closed) {
      closed_instance_ids.push_back(it.first);
      RemoveSourceId(source_id);
    } else {
      it.second.running_time = base::TimeDelta();
    }
  }

  for (const auto& instance_id : closed_instance_ids) {
    usage_time_per_five_minutes_.erase(instance_id);
  }
}

void AppPlatformMetrics::RecordAppsInstallUkm(const apps::AppUpdate& update,
                                              InstallTime install_time) {
  AppTypeName app_type_name =
      GetAppTypeName(profile_, update.AppType(), update.AppId(),
                     apps::mojom::LaunchContainer::kLaunchContainerNone);

  ukm::SourceId source_id = GetSourceId(profile_, update.AppId());
  if (source_id == ukm::kInvalidSourceId) {
    return;
  }

  ukm::builders::ChromeOSApp_InstalledApp builder(source_id);
  builder.SetAppType((int)app_type_name)
      .SetInstallReason((int)update.InstallReason())
      .SetInstallSource2((int)update.InstallSource())
      .SetInstallTime((int)install_time)
      .SetUserDeviceMatrix(user_type_by_device_type_)
      .Record(ukm::UkmRecorder::Get());
  RemoveSourceId(source_id);
}

}  // namespace apps
