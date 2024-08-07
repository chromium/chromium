// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/metrics/app_platform_metrics.h"

#include <memory>
#include <set>
#include <string_view>

#include "base/check_deref.h"
#include "base/containers/fixed_flat_set.h"
#include "base/json/values_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_utils.h"
#include "chrome/browser/apps/app_service/metrics/app_service_metrics.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/ash/guest_os/guest_os_shelf_utils.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/components/mgs/managed_guest_session_utils.h"
#include "components/app_constants/constants.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/instance.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "components/ukm/app_source_url_recorder.h"
#include "components/user_manager/user_manager.h"
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

constexpr base::TimeDelta kMaxDuration = base::Days(1);

constexpr auto kAppTypeNameSet = base::MakeFixedFlatSet<apps::AppTypeName>({
    apps::AppTypeName::kArc,
    apps::AppTypeName::kBuiltIn,
    apps::AppTypeName::kCrostini,
    apps::AppTypeName::kChromeApp,
    apps::AppTypeName::kWeb,
    apps::AppTypeName::kPluginVm,
    apps::AppTypeName::kStandaloneBrowser,
    apps::AppTypeName::kRemote,
    apps::AppTypeName::kBorealis,
    apps::AppTypeName::kSystemWeb,
    apps::AppTypeName::kChromeBrowser,
    apps::AppTypeName::kStandaloneBrowserChromeApp,
    apps::AppTypeName::kExtension,
    apps::AppTypeName::kStandaloneBrowserExtension,
    apps::AppTypeName::kStandaloneBrowserWebApp,
    apps::AppTypeName::kBruschetta,
});

// Returns AppTypeNameV2 used for app running metrics.
apps::AppTypeNameV2 GetAppTypeNameV2(Profile* profile,
                                     apps::AppType app_type,
                                     const std::string& app_id,
                                     aura::Window* window) {
  switch (app_type) {
    case apps::AppType::kUnknown:
      return apps::AppTypeNameV2::kUnknown;
    case apps::AppType::kArc:
      return apps::AppTypeNameV2::kArc;
    case apps::AppType::kBuiltIn:
      return apps::AppTypeNameV2::kBuiltIn;
    case apps::AppType::kCrostini:
      return apps::AppTypeNameV2::kCrostini;
    case apps::AppType::kChromeApp:
      return app_id == app_constants::kChromeAppId
                 ? apps::AppTypeNameV2::kChromeBrowser
             : apps::IsAshBrowserWindow(window)
                 ? apps::AppTypeNameV2::kChromeAppTab
                 : apps::AppTypeNameV2::kChromeAppWindow;
    case apps::AppType::kWeb: {
      apps::AppTypeName app_type_name =
          apps::GetAppTypeNameForWebAppWindow(profile, app_id, window);
      if (app_type_name == apps::AppTypeName::kChromeBrowser) {
        return apps::AppTypeNameV2::kWebTab;
      } else if (app_type_name == apps::AppTypeName::kStandaloneBrowser) {
        return apps::AppTypeNameV2::kStandaloneBrowserWebAppTab;
      } else if (app_type_name == apps::AppTypeName::kSystemWeb) {
        return apps::AppTypeNameV2::kSystemWeb;
      } else if (crosapi::browser_util::IsLacrosEnabled()) {
        return apps::AppTypeNameV2::kStandaloneBrowserWebAppWindow;
      } else {
        return apps::AppTypeNameV2::kWebWindow;
      }
    }
    case apps::AppType::kPluginVm:
      return apps::AppTypeNameV2::kPluginVm;
    case apps::AppType::kStandaloneBrowser:
      return apps::AppTypeNameV2::kStandaloneBrowser;
    case apps::AppType::kRemote:
      return apps::AppTypeNameV2::kRemote;
    case apps::AppType::kBorealis:
      return apps::AppTypeNameV2::kBorealis;
    case apps::AppType::kSystemWeb:
      return apps::AppTypeNameV2::kSystemWeb;
    case apps::AppType::kStandaloneBrowserChromeApp:
      return apps::IsLacrosBrowserWindow(profile, window)
                 ? apps::AppTypeNameV2::kStandaloneBrowserChromeAppTab
                 : apps::AppTypeNameV2::kStandaloneBrowserChromeAppWindow;
    case apps::AppType::kExtension:
      return apps::AppTypeNameV2::kExtension;
    case apps::AppType::kStandaloneBrowserExtension:
      return apps::AppTypeNameV2::kStandaloneBrowserExtension;
    case apps::AppType::kBruschetta:
      return apps::AppTypeNameV2::kBruschetta;
  }
}

// Returns AppTypeNameV2 used for app launch metrics.
apps::AppTypeNameV2 GetAppTypeNameV2(Profile* profile,
                                     apps::AppType app_type,
                                     const std::string& app_id,
                                     apps::LaunchContainer container) {
  switch (app_type) {
    case apps::AppType::kUnknown:
      return apps::AppTypeNameV2::kUnknown;
    case apps::AppType::kArc:
      return apps::AppTypeNameV2::kArc;
    case apps::AppType::kBuiltIn:
      return apps::AppTypeNameV2::kBuiltIn;
    case apps::AppType::kCrostini:
      return apps::AppTypeNameV2::kCrostini;
    case apps::AppType::kChromeApp:
      return container == apps::LaunchContainer::kLaunchContainerWindow
                 ? apps::AppTypeNameV2::kChromeAppWindow
                 : apps::AppTypeNameV2::kChromeAppTab;
    case apps::AppType::kWeb: {
      apps::AppTypeName app_type_name =
          apps::GetAppTypeNameForWebApp(profile, app_id, container);
      if (app_type_name == apps::AppTypeName::kChromeBrowser) {
        return apps::AppTypeNameV2::kWebTab;
      } else if (app_type_name == apps::AppTypeName::kStandaloneBrowser) {
        return apps::AppTypeNameV2::kStandaloneBrowserWebAppTab;
      } else if (app_type_name == apps::AppTypeName::kSystemWeb) {
        return apps::AppTypeNameV2::kSystemWeb;
      } else if (crosapi::browser_util::IsLacrosEnabled()) {
        return apps::AppTypeNameV2::kStandaloneBrowserWebAppWindow;
      } else {
        return apps::AppTypeNameV2::kWebWindow;
      }
    }
    case apps::AppType::kPluginVm:
      return apps::AppTypeNameV2::kPluginVm;
    case apps::AppType::kStandaloneBrowser:
      return apps::AppTypeNameV2::kStandaloneBrowser;
    case apps::AppType::kRemote:
      return apps::AppTypeNameV2::kRemote;
    case apps::AppType::kBorealis:
      return apps::AppTypeNameV2::kBorealis;
    case apps::AppType::kSystemWeb:
      return apps::AppTypeNameV2::kSystemWeb;
    case apps::AppType::kBruschetta:
      return apps::AppTypeNameV2::kBruschetta;
    case apps::AppType::kStandaloneBrowserChromeApp: {
      apps::AppTypeName app_type_name =
          apps::GetAppTypeNameForStandaloneBrowserChromeApp(profile, app_id,
                                                            container);
      return app_type_name == apps::AppTypeName::kStandaloneBrowser
                 ? apps::AppTypeNameV2::kStandaloneBrowserChromeAppTab
                 : apps::AppTypeNameV2::kStandaloneBrowserChromeAppWindow;
    }
    case apps::AppType::kExtension:
      return apps::AppTypeNameV2::kExtension;
    case apps::AppType::kStandaloneBrowserExtension:
      return apps::AppTypeNameV2::kStandaloneBrowserExtension;
  }
}

// Records the number of times Chrome OS apps are launched grouped by the launch
// source.
void RecordAppLaunchSource(apps::LaunchSource launch_source) {
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

base::TimeDelta GetDurationAndResetStartTime(base::TimeTicks& start_time) {
  base::TimeDelta duration = base::TimeTicks::Now() - start_time;
  start_time = base::TimeTicks::Now();
  return duration;
}

}  // namespace

namespace apps {

constexpr char kAppRunningDuration[] =
    "app_platform_metrics.app_running_duration";
constexpr char kAppActivatedCount[] =
    "app_platform_metrics.app_activated_count";
constexpr char kAppUsageTime[] = "app_platform_metrics.app_usage_time";

constexpr char kAppLaunchPerAppTypeHistogramName[] = "Apps.AppLaunchPerAppType";
constexpr char kAppLaunchPerAppTypeV2HistogramName[] =
    "Apps.AppLaunchPerAppTypeV2";

constexpr char kChromeAppTabHistogramName[] = "ChromeAppTab";
constexpr char kChromeAppWindowHistogramName[] = "ChromeAppWindow";
constexpr char kWebAppTabHistogramName[] = "WebAppTab";
constexpr char kWebAppWindowHistogramName[] = "WebAppWindow";

constexpr char kUsageTimeAppIdKey[] = "app_id";
constexpr char kUsageTimeAppPublisherIdKey[] = "app_publisher_id";
constexpr char kUsageTimeAppTypeKey[] = "app_type";
constexpr char kUsageTimeDurationKey[] = "time";
constexpr char kReportingUsageTimeDurationKey[] = "reporting_usage_time";

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
    case apps::AppTypeNameV2::kStandaloneBrowserExtension:
      return kStandaloneBrowserExtensionHistogramName;
    case apps::AppTypeNameV2::kStandaloneBrowserChromeAppWindow:
      return kStandaloneBrowserChromeAppWindowHistogramName;
    case apps::AppTypeNameV2::kStandaloneBrowserChromeAppTab:
      return kStandaloneBrowserChromeAppTabHistogramName;
    case apps::AppTypeNameV2::kStandaloneBrowserWebAppWindow:
      return kStandaloneBrowserWebAppWindowHistogramName;
    case apps::AppTypeNameV2::kStandaloneBrowserWebAppTab:
      return kStandaloneBrowserWebAppTabHistogramName;
    case apps::AppTypeNameV2::kBruschetta:
      return kBruschettaHistogramName;
  }
}

ApplicationInstallTime ConvertInstallTimeToProtoApplicationInstallTime(
    InstallTime install_time) {
  switch (install_time) {
    case InstallTime::kInit:
      return ApplicationInstallTime::APPLICATION_INSTALL_TIME_INIT;
    case InstallTime::kRunning:
      return ApplicationInstallTime::APPLICATION_INSTALL_TIME_RUNNING;
    default:
      return ApplicationInstallTime::APPLICATION_INSTALL_TIME_UNKNOWN;
  }
}

void RecordAppLaunchMetrics(Profile* profile,
                            AppType app_type,
                            const std::string& app_id,
                            apps::LaunchSource launch_source,
                            apps::LaunchContainer container) {
  if (app_type == AppType::kUnknown) {
    return;
  }

  RecordAppLaunchSource(launch_source);
  RecordAppLaunchPerAppType(
      GetAppTypeName(profile, app_type, app_id, container));
  RecordAppLaunchPerAppTypeV2(
      GetAppTypeNameV2(profile, app_type, app_id, container));

  // TODO(b/356937112): Refactor the metrics DemoMode.AppLaunchSource
  if (ash::DemoSession::IsDeviceInDemoMode()) {
    ash::DemoSession::AppLaunchSource source;
    bool will_report = true;
    // Apps launched from the demo mode app has the launch source of
    // kFromOtherApp, but we do not report it here since there could be other
    // places that launch apps with the same launch source of kFromOtherApp. So,
    // to be more accurate, we report it in
    // [chrome_demo_mode_app_delegate.cc]ChromeDemoModeAppDelegate::LaunchApp.
    // Additionally, we report only the following types of launch source based
    // on the need of demo mode.
    switch (launch_source) {
      case apps::LaunchSource::kFromAppListGrid:
        source = ash::DemoSession::AppLaunchSource::kAppList;
        break;
      case apps::LaunchSource::kFromAppListQuery:
        source = ash::DemoSession::AppLaunchSource::kAppListQuery;
        break;
      case apps::LaunchSource::kFromShelf:
        source = ash::DemoSession::AppLaunchSource::kShelf;
        break;
      default:
        will_report = false;
    }
    if (will_report) {
      ash::DemoSession::RecordAppLaunchSource(source);
    }
  }

  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
  if (proxy && proxy->AppPlatformMetrics()) {
    proxy->AppPlatformMetrics()->RecordAppLaunchUkm(app_type, app_id,
                                                    launch_source, container);
  }
}

AppPlatformMetrics::UsageTime::UsageTime() = default;

AppPlatformMetrics::UsageTime::~UsageTime() = default;

AppPlatformMetrics::UsageTime::UsageTime(const base::Value& value) {
  const base::Value::Dict* data_dict = value.GetIfDict();
  if (!data_dict) {
    return;
  }

  const std::string* const app_id_value =
      data_dict->FindString(kUsageTimeAppIdKey);
  if (!app_id_value) {
    return;
  }

  const std::string* const app_publisher_id_value =
      data_dict->FindString(kUsageTimeAppPublisherIdKey);
  if (app_publisher_id_value) {
    app_publisher_id = *app_publisher_id_value;
  }

  const std::string* const app_type_value =
      data_dict->FindString(kUsageTimeAppTypeKey);
  if (!app_type_value) {
    return;
  }

  const std::optional<const base::TimeDelta> running_time_value =
      base::ValueToTimeDelta(data_dict->Find(kUsageTimeDurationKey));
  const std::optional<const base::TimeDelta> reporting_usage_time_value =
      base::ValueToTimeDelta(data_dict->Find(kReportingUsageTimeDurationKey));
  if (!running_time_value.has_value() &&
      !reporting_usage_time_value.has_value()) {
    return;
  }

  if (running_time_value.has_value()) {
    running_time = running_time_value.value();
  }

  if (reporting_usage_time_value.has_value()) {
    reporting_usage_time = reporting_usage_time_value.value();
  }

  app_id = *app_id_value;
  app_type_name = GetAppTypeNameFromString(*app_type_value);

  // We normally use this as we load data from the pref store at the
  // beginning of a new session which is when windows are normally closed.
  window_is_closed = true;
}

base::Value::Dict AppPlatformMetrics::UsageTime::ConvertToDict() const {
  base::Value::Dict usage_time_dict;
  usage_time_dict.Set(kUsageTimeAppIdKey, app_id);
  usage_time_dict.Set(kUsageTimeAppPublisherIdKey, app_publisher_id);
  usage_time_dict.Set(kUsageTimeAppTypeKey,
                      GetAppTypeHistogramName(app_type_name));
  usage_time_dict.Set(kUsageTimeDurationKey,
                      base::TimeDeltaToValue(running_time));
  usage_time_dict.Set(kReportingUsageTimeDurationKey,
                      base::TimeDeltaToValue(reporting_usage_time));
  return usage_time_dict;
}

AppPlatformMetrics::AppPlatformMetrics(
    Profile* profile,
    apps::AppRegistryCache& app_registry_cache,
    InstanceRegistry& instance_registry)
    : profile_(profile), app_registry_cache_(app_registry_cache) {
  app_registry_cache_observer_.Observe(&app_registry_cache);
  instance_registry_observation_.Observe(&instance_registry);
  if (chromeos::IsManagedGuestSession()) {
    CHECK(ukm::UkmRecorder::Get());
    ukm_recorder_observer_.Observe(ukm::UkmRecorder::Get());
  }
  user_type_by_device_type_ = GetUserTypeByDeviceTypeMetrics();
  InitRunningDuration();
  LoadAppsUsageTimeUkmFromPref();
  ReadInstalledApps();
}

AppPlatformMetrics::~AppPlatformMetrics() {
  UpdateMetricsBeforeShutdown();
  OnTenMinutes();

  // Notify registered observers.
  for (auto& observer : observers_) {
    observer.OnAppPlatformMetricsDestroyed();
  }
}

// static
ukm::SourceId AppPlatformMetrics::GetSourceId(Profile* profile,
                                              const std::string& app_id) {
  if (!AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    return ukm::kInvalidSourceId;
  }

  AppType app_type = GetAppType(profile, app_id);
  if (!ShouldRecordAppKMForAppTypeName(app_type)) {
    return ukm::kInvalidSourceId;
  }

  GURL url = GetURLForApp(profile, app_id);
  if (url.is_empty()) {
    return ukm::kInvalidSourceId;
  }

  switch (app_type) {
    case AppType::kBuiltIn:
    case AppType::kChromeApp:
    case AppType::kExtension:
    case AppType::kStandaloneBrowser:
    case AppType::kStandaloneBrowserChromeApp:
    case AppType::kStandaloneBrowserExtension:
    case AppType::kSystemWeb:
      return ukm::AppSourceUrlRecorder::GetSourceIdForUrl(
          url, ukm::AppType::kChromeApp);
    case AppType::kArc:
      return ukm::AppSourceUrlRecorder::GetSourceIdForUrl(url,
                                                          ukm::AppType::kArc);
    case AppType::kWeb: {
      // Some system web-apps may be PWAs.
      if (IsSystemWebApp(profile, app_id)) {
        return ukm::AppSourceUrlRecorder::GetSourceIdForUrl(
            url, ukm::AppType::kChromeApp);
      }
      return ukm::AppSourceUrlRecorder::GetSourceIdForUrl(url,
                                                          ukm::AppType::kPWA);
    }
    case AppType::kCrostini:
      return ukm::AppSourceUrlRecorder::GetSourceIdForUrl(
          url, ukm::AppType::kCrostini);
    case AppType::kBorealis:
      return ukm::AppSourceUrlRecorder::GetSourceIdForUrl(
          url, ukm::AppType::kBorealis);
    // App types that are not supported by UKM.
    default:
      return ukm::kInvalidSourceId;
  }
}

// static
GURL AppPlatformMetrics::GetURLForApp(Profile* profile,
                                      const std::string& app_id) {
  AppType app_type = GetAppType(profile, app_id);

  // If the app should not be recorded, then emit an empty URL so the URL is not
  // recorded for the associated app.
  if (!ShouldRecordAppKMForAppTypeName(app_type)) {
    return GURL();
  }

  switch (app_type) {
    // |app_id| is already hashed for these apps and are of the format
    // app://{app_id}.
    case AppType::kBuiltIn:
    case AppType::kChromeApp:
    case AppType::kExtension:
    case AppType::kStandaloneBrowser:
    case AppType::kStandaloneBrowserChromeApp:
    case AppType::kStandaloneBrowserExtension:
    // For system web apps, call GetSourceIdForChromeApp to record the app
    // id because the url could be filtered by the server side.
    case AppType::kSystemWeb:
      return ukm::AppSourceUrlRecorder::GetURLForChromeApp(app_id);
    // ARC apps contain the app package name and the URL generated is of the
    // format app://{package_name}. The package name will be populated if it is
    // a properly registered ARC app.
    case AppType::kArc: {
      std::string package_name = GetPublisherId(profile, app_id);
      // Empty package name ID indicates that the ARC app is not properly
      // registered application.
      if (package_name.empty() || app_id.empty()) {
        return GURL();
      }
      return ukm::AppSourceUrlRecorder::GetURLForArcPackageName(package_name);
    }
    case AppType::kWeb: {
      // Some PWAs can be categorized as system web apps. System web apps should
      // be encoded as a ChromeApp hash.
      if (IsSystemWebApp(profile, app_id)) {
        return ukm::AppSourceUrlRecorder::GetURLForChromeApp(app_id);
      }

      std::string publisher_id = GetPublisherId(profile, app_id);
      // Empty publisher ID indicates that the app is not a properly registered
      // PWA.
      if (publisher_id.empty() || app_id.empty()) {
        return GURL();
      }

      return ukm::AppSourceUrlRecorder::GetURLForPWA(GURL(publisher_id));
    }
    case AppType::kCrostini: {
      auto crostini_app_id = GetIdForCrostini(profile, app_id);
      return ukm::AppSourceUrlRecorder::GetURLForCrostini(
          crostini_app_id.desktop_id, crostini_app_id.registration_name);
    }
    case AppType::kBorealis:
      return GetURLForBorealis(profile, app_id);
    // Other app types should not be logged. Return empty GURL so that
    // these app types are not recorded.
    default:
      return GURL();
  }
}

// static
std::string AppPlatformMetrics::GetPublisherId(Profile* profile,
                                               const std::string& app_id) {
  std::string publisher_id;
  apps::AppServiceProxyFactory::GetForProfile(profile)
      ->AppRegistryCache()
      .ForOneApp(app_id, [&publisher_id](const apps::AppUpdate& update) {
        publisher_id = update.PublisherId();
      });
  return publisher_id;
}

// static
GURL AppPlatformMetrics::GetURLForBorealis(Profile* profile,
                                           const std::string& app_id) {
  // Most Borealis apps are identified by a numeric ID, except these.
  if (app_id == borealis::kClientAppId) {
    return ukm::AppSourceUrlRecorder::GetURLForBorealis("client");
  } else if (app_id == borealis::kInstallerAppId) {
    return ukm::AppSourceUrlRecorder::GetURLForBorealis("installer");
  } else if (app_id.find(borealis::kIgnoredAppIdPrefix) != std::string::npos) {
    // These are not real apps from a user's point of view, so it doesn't make
    // sense to record metrics for them.
    return GURL();
  }

  // For most borealis apps, we convert to the "steam app id", which is a unique
  // number valve assigns to each game.
  //
  // This is more robust, as it handles some unidentified apps (if they have a
  // steam id).
  std::optional<int> borealis_id = borealis::SteamGameId(profile, app_id);
  if (borealis_id.has_value()) {
    return ukm::AppSourceUrlRecorder::GetURLForBorealis(
        base::NumberToString(borealis_id.value()));
  }

  // If there's no steam id then we're not allowed to record anything that
  // could identify the app (and we don't know the app name anyway), but
  // recording every unregistered app in one big bucket is fine.
  //
  // In general all Borealis apps should have a steam id, so if we do see this
  // Source ID being reported, that's a bug.
  LOG(WARNING) << "Couldn't get Borealis ID for UNREGISTERED app " << app_id;
  return ukm::AppSourceUrlRecorder::GetURLForBorealis("UNREGISTERED");
}

// static
CrostiniAppId AppPlatformMetrics::GetIdForCrostini(Profile* profile,
                                                   const std::string& app_id) {
  auto* registry =
      guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile);
  auto registration = registry->GetRegistration(app_id);
  if (!registration) {
    // If there's no registration then we're not allowed to record anything
    // that could identify the app (and we don't know the app name anyway),
    // but recording every unregistered app in one big bucket is fine.
    return CrostiniAppId{
        .desktop_id = "UNREGISTERED",
        .registration_name = "UNREGISTERED",
    };
  }
  auto desktop_id = registration->DesktopFileId() == ""
                        ? "NoId"
                        : registration->DesktopFileId();
  return CrostiniAppId{
      .desktop_id = desktop_id,
      .registration_name = registration->Name(),

  };
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
    apps::InstallReason install_reason) {
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
  return kAppsUsageTimeHistogramPrefixV2 +
         GetAppTypeHistogramNameV2(app_type_name);
}

void AppPlatformMetrics::OnNewDay() {
  should_record_metrics_on_new_day_ = true;
  RecordAppsCount(AppType::kUnknown);
  RecordAppsRunningDuration();
}

void AppPlatformMetrics::OnTenMinutes() {
  if (should_refresh_activated_count_pref) {
    should_refresh_activated_count_pref = false;
    ScopedDictPrefUpdate activated_count_update(profile_->GetPrefs(),
                                                kAppActivatedCount);
    for (auto it : activated_count_) {
      std::string app_type_name = GetAppTypeHistogramName(it.first);
      DCHECK(!app_type_name.empty());
      activated_count_update->Set(app_type_name, it.second);
    }
  }

  if (should_refresh_duration_pref) {
    should_refresh_duration_pref = false;
    ScopedDictPrefUpdate running_duration_update(profile_->GetPrefs(),
                                                 kAppRunningDuration);
    for (auto it : running_duration_) {
      std::string app_type_name = GetAppTypeHistogramName(it.first);
      DCHECK(!app_type_name.empty());
      running_duration_update->SetByDottedPath(
          app_type_name, base::TimeDeltaToValue(it.second));
    }
  }
}

void AppPlatformMetrics::OnFiveMinutes() {
  // If there is app usage time loaded from the user pref for previous login,
  // record the AppKM.
  if (!usage_times_from_pref_.empty()) {
    RecordAppsUsageTimeUkmFromPref();
    usage_times_from_pref_.clear();
  }
  RecordAppsUsageTime();
  SaveUsageTime();
}

void AppPlatformMetrics::OnTwoHours() {
  RecordAppsUsageTimeUkm();
}

void AppPlatformMetrics::RecordAppLaunchUkm(AppType app_type,
                                            const std::string& app_id,
                                            apps::LaunchSource launch_source,
                                            apps::LaunchContainer container) {
  // We do not tie observers to local app sync settings, so we notify them
  // first. It is the responsibility of observers to enforce appropriate checks
  // and restrictions with something appropriate before using this data.
  for (auto& observer : observers_) {
    observer.OnAppLaunched(app_id, app_type, launch_source);
  }

  if (app_type == AppType::kUnknown ||
      !ShouldRecordAppKMForAppId(profile_, app_registry_cache_.get(), app_id)) {
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
    AppType app_type,
    const std::string& app_id,
    UninstallSource uninstall_source) {
  // We do not tie observers to local app sync settings, so we notify them
  // first. It is the responsibility of observers to enforce appropriate checks
  // and restrictions with something appropriate before using this data.
  for (auto& observer : observers_) {
    observer.OnAppUninstalled(app_id, app_type, uninstall_source);
  }
  if (!ShouldRecordAppKMForAppId(profile_, app_registry_cache_.get(), app_id)) {
    return;
  }

  AppTypeName app_type_name = GetAppTypeName(
      profile_, app_type, app_id, apps::LaunchContainer::kLaunchContainerNone);

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

void AppPlatformMetrics::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AppPlatformMetrics::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AppPlatformMetrics::OnAppTypeInitialized(AppType app_type) {
  if (should_record_metrics_on_new_day_) {
    RecordAppsCount(app_type);
  }
}

void AppPlatformMetrics::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  app_registry_cache_observer_.Reset();
}

void AppPlatformMetrics::OnAppUpdate(const apps::AppUpdate& update) {
  if (!update.ReadinessChanged() ||
      update.Readiness() != apps::Readiness::kReady ||
      apps_util::IsInstalled(update.PriorReadiness())) {
    return;
  }

  InstallTime install_time =
      app_registry_cache_->IsAppTypeInitialized(update.AppType())
          ? InstallTime::kRunning
          : InstallTime::kInit;

  // We do not tie observers to local app sync settings, so we notify them
  // first. It is the responsibility of observers to enforce appropriate checks
  // and restrictions with something appropriate before using this data.
  for (auto& observer : observers_) {
    observer.OnAppInstalled(update.AppId(), update.AppType(),
                            update.InstallSource(), update.InstallReason(),
                            install_time);
  }

  if (!ShouldRecordAppKMForAppId(profile_, app_registry_cache_.get(),
                                 update.AppId())) {
    return;
  }

  RecordAppsInstallUkm(update, install_time);
}

void AppPlatformMetrics::OnInstanceUpdate(const apps::InstanceUpdate& update) {
  if (!update.StateChanged()) {
    return;
  }

  auto app_id = update.AppId();
  auto app_type = GetAppType(profile_, app_id);
  if (app_type == AppType::kUnknown) {
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
    if ((app_id == app_constants::kChromeAppId ||
         app_id == app_constants::kLacrosAppId) &&
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
      auto* browser_window =
          app_type_name == apps::AppTypeName::kStandaloneBrowser
              ? update.Window()
              : update.Window()->GetToplevelWindow();
      browser_to_tab_list_.RemoveActivatedTab(update.InstanceId());
      browser_to_tab_list_.AddActivatedTab(browser_window, update.InstanceId(),
                                           update.AppId());
      InstanceState state;
      base::UnguessableToken browser_id;
      std::string browser_app_id;
      GetBrowserInstanceInfo(browser_window, browser_id, browser_app_id, state);
      if (browser_id) {
        SetWindowInActivated(browser_app_id, browser_id, kInActivated);
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
  instance_registry_observation_.Reset();
}

void AppPlatformMetrics::OnStartingShutdown() {
  CHECK(chromeos::IsManagedGuestSession());
  UpdateMetricsBeforeShutdown();
  RecordAppsUsageTimeUkm();
}

void AppPlatformMetrics::GetBrowserInstanceInfo(
    const aura::Window* browser_window,
    base::UnguessableToken& browser_id,
    std::string& browser_app_id,
    InstanceState& state) const {
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile_);
  DCHECK(proxy);
  browser_id = base::UnguessableToken();
  browser_app_id = std::string();
  state = InstanceState::kUnknown;
  proxy->InstanceRegistry().ForInstancesWithWindow(
      browser_window, [&](const InstanceUpdate& browser_update) {
        if (browser_update.AppId() == app_constants::kChromeAppId ||
            browser_update.AppId() == app_constants::kLacrosAppId) {
          browser_id = browser_update.InstanceId();
          browser_app_id = browser_update.AppId();
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
  std::string browser_app_id;
  GetBrowserInstanceInfo(browser_window, browser_id, browser_app_id, state);
  if (state & InstanceState::kActive) {
    AppType app_type = AppType::kChromeApp;
    AppTypeName app_type_name = AppTypeName::kChromeBrowser;
    AppTypeNameV2 app_type_name_v2 = AppTypeNameV2::kChromeBrowser;
    if (browser_app_id == app_constants::kLacrosAppId) {
      app_type = AppType::kStandaloneBrowser;
      app_type_name = AppTypeName::kStandaloneBrowser;
      app_type_name_v2 = AppTypeNameV2::kStandaloneBrowser;
    }
    // The browser window is activated, start calculating the browser window
    // running time.
    SetWindowActivated(app_type, app_type_name, app_type_name_v2,
                       browser_app_id, browser_id);
  }
}

void AppPlatformMetrics::SetWindowActivated(
    AppType app_type,
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
  auto two_hours_it = usage_time_per_two_hours_.find(instance_id);
  if (is_close && two_hours_it != usage_time_per_two_hours_.end()) {
    two_hours_it->second.window_is_closed = true;
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

  UpdateUsageTime(instance_id, app_id, app_type_name, running_time);

  running_start_time_.erase(it);
  start_time_per_five_minutes_.erase(instance_id);

  should_refresh_duration_pref = true;
}

void AppPlatformMetrics::InitRunningDuration() {
  ScopedDictPrefUpdate running_duration_update(profile_->GetPrefs(),
                                               kAppRunningDuration);
  ScopedDictPrefUpdate activated_count_update(profile_->GetPrefs(),
                                              kAppActivatedCount);

  for (auto app_type_name : kAppTypeNameSet) {
    std::string key = GetAppTypeHistogramName(app_type_name);
    if (key.empty()) {
      continue;
    }

    std::optional<base::TimeDelta> unreported_duration =
        base::ValueToTimeDelta(running_duration_update->FindByDottedPath(key));
    if (unreported_duration.has_value()) {
      running_duration_[app_type_name] = unreported_duration.value();
    }

    std::optional<int> count = activated_count_update->FindIntByDottedPath(key);
    if (count.has_value()) {
      activated_count_[app_type_name] = count.value();
    }
  }
}

void AppPlatformMetrics::ClearRunningDuration() {
  running_duration_.clear();
  activated_count_.clear();

  profile_->GetPrefs()->SetDict(kAppRunningDuration, base::Value::Dict());
  profile_->GetPrefs()->SetDict(kAppActivatedCount, base::Value::Dict());
}

void AppPlatformMetrics::ReadInstalledApps() {
  app_registry_cache_->ForEachApp([this](const apps::AppUpdate& update) {
    if (ShouldRecordAppKMForAppId(profile_, app_registry_cache_.get(),
                                  update.AppId())) {
      RecordAppsInstallUkm(update, InstallTime::kInit);
    }
  });
}

void AppPlatformMetrics::RecordAppsCount(AppType app_type) {
  std::map<AppTypeName, int> app_count;
  std::map<AppTypeName, std::map<apps::InstallReason, int>>
      app_count_per_install_reason;

  app_registry_cache_->ForEachApp(
      [app_type, this, &app_count,
       &app_count_per_install_reason](const apps::AppUpdate& update) {
        if (app_type != apps::AppType::kUnknown &&
            (update.AppType() != app_type ||
             update.AppId() == app_constants::kChromeAppId)) {
          return;
        }

        AppTypeName app_type_name =
            GetAppTypeName(profile_, update.AppType(), update.AppId(),
                           apps::LaunchContainer::kLaunchContainerNone);

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
        GetDurationAndResetStartTime(it.second.start_time);
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
        GetDurationAndResetStartTime(it.second.start_time);
    app_type_running_time_per_five_minutes_[it.second.app_type_name] +=
        running_time;
    app_type_v2_running_time_per_five_minutes_[it.second.app_type_name_v2] +=
        running_time;
    UpdateUsageTime(it.first, it.second.app_id, it.second.app_type_name,
                    running_time);
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
}

void AppPlatformMetrics::RecordAppsUsageTimeUkm() {
  if (!ShouldRecordAppKM(profile_)) {
    // Attempt to clean up pre-existing data in the pref store. This is useful
    // (and harmless) because we routinely clean up usage data that has already
    // been reported.
    CleanUpAppsUsageInfoInPrefStore();
    return;
  }

  std::vector<base::UnguessableToken> closed_instance_ids;
  for (auto& it : usage_time_per_two_hours_) {
    apps::AppTypeName app_type_name = it.second.app_type_name;
    ukm::SourceId source_id = it.second.source_id;
    DCHECK_NE(source_id, ukm::kInvalidSourceId);
    if (!it.second.running_time.is_zero()) {
      if (ShouldRecordAppKMForAppId(profile_, app_registry_cache_.get(),
                                    it.second.app_id)) {
        auto new_source_id = GetSourceId(profile_, it.second.app_id);
        if (new_source_id != ukm::kInvalidSourceId) {
          ukm::builders::ChromeOSApp_UsageTime builder(new_source_id);
          builder.SetAppType((int)it.second.app_type_name)
              .SetDuration(it.second.running_time.InMilliseconds())
              .SetUserDeviceMatrix(user_type_by_device_type_)
              .Record(ukm::UkmRecorder::Get());
          RemoveSourceId(new_source_id);
        }

        // Preserve a copy of UsageTime UKM to investigate the null app id
        // issue.
        ukm::builders::ChromeOSApp_UsageTimeReusedSourceId builder(source_id);
        builder.SetAppType((int)app_type_name)
            .SetDuration(it.second.running_time.InMilliseconds())
            .SetUserDeviceMatrix(user_type_by_device_type_)
            .Record(ukm::UkmRecorder::Get());
      }

      // UMA for Mall app.
      if (AppIdToName(it.second.app_id) == DefaultAppName::kMall) {
        base::UmaHistogramCustomTimes("Apps.AppDiscovery.MallUsageTime",
                                      it.second.running_time, base::Seconds(1),
                                      base::Hours(2), 100);
      }

      // Also reset time in the pref store now that we have reported this data.
      ClearAppsUsageTimeForInstance(it.first.ToString());
    }
    if (it.second.window_is_closed) {
      closed_instance_ids.push_back(it.first);
      RemoveSourceId(source_id);
    } else {
      it.second.running_time = base::TimeDelta();
    }
  }

  // `usage_time_per_two_hours_` can't be cleared to reuse the source id for
  // open windows. So only closed window records can be deleted from
  // `usage_time_per_two_hours_`.
  for (const auto& instance_id : closed_instance_ids) {
    usage_time_per_two_hours_.erase(instance_id);
  }

  CleanUpAppsUsageInfoInPrefStore();
}

void AppPlatformMetrics::RecordAppsInstallUkm(const apps::AppUpdate& update,
                                              InstallTime install_time) {
  AppTypeName app_type_name =
      GetAppTypeName(profile_, update.AppType(), update.AppId(),
                     apps::LaunchContainer::kLaunchContainerNone);

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

void AppPlatformMetrics::UpdateUsageTime(
    const base::UnguessableToken& instance_id,
    const std::string& app_id,
    AppTypeName app_type_name,
    const base::TimeDelta& running_time) {
  // Notify registered observers.
  for (auto& observer : observers_) {
    observer.OnAppUsage(app_id, GetAppType(profile_, app_id), instance_id,
                        running_time);
  }
  if (!ShouldRecordAppKM(profile_)) {
    // Avoid incrementing app usage counters if it cannot be reported. This
    // ensures we only track usage for the period the user has sync enabled.
    return;
  }

  auto usage_time_it = usage_time_per_two_hours_.find(instance_id);
  if (usage_time_it == usage_time_per_two_hours_.end()) {
    auto source_id = GetSourceId(profile_, app_id);
    if (source_id != ukm::kInvalidSourceId) {
      usage_time_per_two_hours_[instance_id].source_id = source_id;
      usage_time_per_two_hours_[instance_id].app_id = app_id;
      usage_time_it = usage_time_per_two_hours_.find(instance_id);
    }
  }
  if (usage_time_it != usage_time_per_two_hours_.end()) {
    usage_time_it->second.app_type_name = app_type_name;
    usage_time_it->second.running_time += running_time;
  }
}

void AppPlatformMetrics::SaveUsageTime() {
  if (!ShouldRecordAppKM(profile_)) {
    // Do not persist usage data to the pref store if it cannot be reported.
    // This will prevent unnecessary disk space usage.
    return;
  }

  ScopedDictPrefUpdate usage_dict_pref(profile_->GetPrefs(), kAppUsageTime);
  for (const auto& it : usage_time_per_two_hours_) {
    const std::string& instance_id = it.first.ToString();
    auto* const usage_info = usage_dict_pref->FindDictByDottedPath(instance_id);
    if (!usage_info) {
      // No entry in the pref store for this instance, so we create a new one.
      usage_dict_pref->SetByDottedPath(instance_id, it.second.ConvertToDict());
      continue;
    }

    // Only override the fields tracked by this component so we do not override
    // the reporting usage time.
    usage_info->Set(kUsageTimeAppIdKey, it.second.app_id);
    usage_info->Set(kUsageTimeAppTypeKey,
                    GetAppTypeHistogramName(it.second.app_type_name));
    usage_info->Set(kUsageTimeDurationKey,
                    base::TimeDeltaToValue(it.second.running_time));
  }
}

void AppPlatformMetrics::LoadAppsUsageTimeUkmFromPref() {
  const base::Value::Dict& usage_time_dict =
      profile_->GetPrefs()->GetDict(kAppUsageTime);

  for (auto it : usage_time_dict) {
    std::unique_ptr<UsageTime> usage_time =
        std::make_unique<UsageTime>(it.second);
    if (!usage_time->running_time.is_zero()) {
      usage_times_from_pref_.push_back(std::move(usage_time));
    }
  }
}

void AppPlatformMetrics::RecordAppsUsageTimeUkmFromPref() {
  if (!ShouldRecordAppKM(profile_) || usage_times_from_pref_.empty()) {
    return;
  }

  for (auto& it : usage_times_from_pref_) {
    if (ShouldRecordAppKMForAppId(profile_, app_registry_cache_.get(),
                                  it->app_id)) {
      auto source_id = GetSourceId(profile_, it->app_id);
      if (source_id != ukm::kInvalidSourceId) {
        ukm::builders::ChromeOSApp_UsageTime builder(source_id);
        builder.SetAppType((int)it->app_type_name)
            .SetDuration(it->running_time.InMilliseconds())
            .SetUserDeviceMatrix(user_type_by_device_type_)
            .Record(ukm::UkmRecorder::Get());
        RemoveSourceId(source_id);
      }

      // All windows read from the user pref have been closed before login, so
      // create a new source id here, since we don't have previous source ids
      // for them. This UKM record should not have the null app id issue. Still
      // preserve a copy of UsageTime UKM to investigate the null app id issue
      // for consistency.
      source_id = GetSourceId(profile_, it->app_id);
      if (source_id != ukm::kInvalidSourceId) {
        ukm::builders::ChromeOSApp_UsageTimeReusedSourceId builder(source_id);
        builder.SetAppType((int)it->app_type_name)
            .SetDuration(it->running_time.InMilliseconds())
            .SetUserDeviceMatrix(user_type_by_device_type_)
            .Record(ukm::UkmRecorder::Get());
        RemoveSourceId(source_id);
      }
    }

    // Clear app UKM usage from the pref store now that we have reported this
    // data.
    for (const auto usage_it : profile_->GetPrefs()->GetDict(kAppUsageTime)) {
      if (CHECK_DEREF(usage_it.second.GetDict().FindString(
              kUsageTimeAppIdKey)) == it->app_id) {
        ClearAppsUsageTimeForInstance(usage_it.first);
      }
    }
  }
}

void AppPlatformMetrics::CleanUpAppsUsageInfoInPrefStore() {
  ScopedDictPrefUpdate usage_time_pref_update(profile_->GetPrefs(),
                                              kAppUsageTime);
  auto usage_it = usage_time_pref_update->begin();
  while (usage_it != usage_time_pref_update->end()) {
    UsageTime usage_time(usage_it->second);
    if (usage_time.reporting_usage_time.is_zero() &&
        usage_time.running_time.is_zero()) {
      usage_it = usage_time_pref_update->erase(usage_it);
      continue;
    }
    usage_it++;
  }
}

void AppPlatformMetrics::ClearAppsUsageTimeForInstance(
    std::string_view instance_id) {
  ScopedDictPrefUpdate usage_time_pref_update(profile_->GetPrefs(),
                                              kAppUsageTime);
  auto* instance_dict =
      usage_time_pref_update->FindDictByDottedPath(instance_id);
  if (instance_dict) {
    instance_dict->Set(kUsageTimeDurationKey, base::Int64ToValue(0));
  }
}

void AppPlatformMetrics::UpdateMetricsBeforeShutdown() {
  for (auto& it : running_start_time_) {
    running_duration_[it.second.app_type_name] +=
        GetDurationAndResetStartTime(it.second.start_time);
  }

  RecordAppsUsageTime();
}

}  // namespace apps
