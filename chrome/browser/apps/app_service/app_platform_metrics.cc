// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_platform_metrics.h"

#include <set>

#include "base/containers/contains.h"
#include "base/json/values_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "chrome/browser/apps/app_service/app_service_metrics.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/crostini/crostini_shelf_utils.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/metrics/usertype_by_devicetype_metrics_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_service_utils.h"
#include "components/ukm/app_source_url_recorder.h"
#include "components/user_manager/user_manager.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
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
constexpr char kAppPreviousReadinessStatus[] = "Apps.PreviousReadinessStatus.";

constexpr char kInstallReasonUnknownHistogram[] = "Unknown";
constexpr char kInstallReasonSystemHistogram[] = "System";
constexpr char kInstallReasonPolicyHistogram[] = "Policy";
constexpr char kInstallReasonOemHistogram[] = "Oem";
constexpr char kInstallReasonPreloadHistogram[] = "Preload";
constexpr char kInstallReasonSyncHistogram[] = "Sync";
constexpr char kInstallReasonUserHistogram[] = "User";
constexpr char kInstallReasonSubAppHistogram[] = "SubApp";

constexpr base::TimeDelta kMinDuration = base::Seconds(1);
constexpr base::TimeDelta kMaxDuration = base::Days(1);
constexpr base::TimeDelta kMaxUsageDuration = base::Minutes(5);
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
    app_type_name_map->insert(apps::AppTypeName::kStandaloneBrowserExtension);
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
  apps::AppTypeName type_name = apps::AppTypeName::kChromeBrowser;
  apps::mojom::WindowMode window_mode = apps::mojom::WindowMode::kBrowser;
  apps::AppServiceProxyFactory::GetForProfile(profile)
      ->AppRegistryCache()
      .ForOneApp(
          app_id, [&type_name, &window_mode](const apps::AppUpdate& update) {
            DCHECK(update.AppType() == apps::mojom::AppType::kWeb ||
                   update.AppType() == apps::mojom::AppType::kSystemWeb);

            // For system web apps, the install source is |kSystem|.
            // The app type may be kSystemWeb (system web apps in Ash when
            // Lacros web apps are enabled), or kWeb (all other cases).
            type_name =
                (update.InstallReason() == apps::mojom::InstallReason::kSystem)
                    ? apps::AppTypeName::kSystemWeb
                    : apps::AppTypeName::kWeb;
            window_mode = update.WindowMode();
          });

  if (type_name != apps::AppTypeName::kWeb) {
    return type_name;
  }

  switch (container) {
    case apps::mojom::LaunchContainer::kLaunchContainerWindow:
      return apps::AppTypeName::kWeb;
    case apps::mojom::LaunchContainer::kLaunchContainerTab:
      return apps::AppTypeName::kChromeBrowser;
    default:
      break;
  }

  if (window_mode == apps::mojom::WindowMode::kBrowser) {
    return apps::AppTypeName::kChromeBrowser;
  }

  return apps::AppTypeName::kWeb;
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

// Returns false if |window| is a Chrome app window or a standalone web app
// window. Otherwise, return true;
bool IsBrowser(aura::Window* window) {
  Browser* browser = chrome::FindBrowserWithWindow(window->GetToplevelWindow());
  if (!browser || browser->is_type_app() || browser->is_type_app_popup()) {
    return false;
  }
  return true;
}

// Returns true if the app with |app_id| is opened as a tab in a browser window.
// Otherwise, return false;
bool IsAppOpenedInTab(apps::AppTypeName app_type_name,
                      const std::string& app_id) {
  return app_type_name == apps::AppTypeName::kChromeBrowser &&
         app_id != extension_misc::kChromeAppId;
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
apps::AppTypeName GetAppTypeNameForWindow(Profile* profile,
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
    case apps::mojom::AppType::kStandaloneBrowserExtension:
      return apps::AppTypeName::kStandaloneBrowserExtension;
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
    case apps::mojom::AppType::kExtension:
      return IsBrowser(window) ? apps::AppTypeNameV2::kChromeAppTab
                               : apps::AppTypeNameV2::kChromeAppWindow;
    case apps::mojom::AppType::kWeb: {
      apps::AppTypeName app_type_name =
          GetAppTypeNameForWebAppWindow(profile, app_id, window);
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
    case apps::mojom::AppType::kStandaloneBrowserExtension:
      return apps::AppTypeNameV2::kStandaloneBrowserExtension;
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
    case apps::mojom::AppType::kExtension:
      return container == apps::mojom::LaunchContainer::kLaunchContainerWindow
                 ? apps::AppTypeNameV2::kChromeAppWindow
                 : apps::AppTypeNameV2::kChromeAppTab;
    case apps::mojom::AppType::kWeb: {
      apps::AppTypeName app_type_name =
          GetAppTypeNameForWebApp(profile, app_id, container);
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
    case apps::mojom::AppType::kStandaloneBrowserExtension:
      return apps::AppTypeNameV2::kStandaloneBrowserExtension;
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

// Due to the privacy limitation, only ARC apps, Chrome apps and web apps(PWA),
// system web apps, builtin apps and Crostini apps are recorded because they are
// synced to server/cloud, or part of OS. Other app types, e.g. remote apps,
// etc, are not recorded. So returns true if the app_type_name is allowed to
// record UKM. Otherwise, returns false.
//
// See DD: go/app-platform-metrics-using-ukm for details.
bool ShouldRecordUkmForAppTypeName(apps::AppTypeName app_type_name) {
  switch (app_type_name) {
    case apps::AppTypeName::kArc:
    case apps::AppTypeName::kBuiltIn:
    case apps::AppTypeName::kChromeApp:
    case apps::AppTypeName::kChromeBrowser:
    case apps::AppTypeName::kWeb:
    case apps::AppTypeName::kSystemWeb:
    case apps::AppTypeName::kCrostini:
      return true;
    case apps::AppTypeName::kUnknown:
    case apps::AppTypeName::kMacOs:
    case apps::AppTypeName::kPluginVm:
    case apps::AppTypeName::kStandaloneBrowser:
    case apps::AppTypeName::kStandaloneBrowserExtension:
    case apps::AppTypeName::kRemote:
    case apps::AppTypeName::kBorealis:
      return false;
  }
}

int GetUserTypeByDeviceTypeMetrics() {
  const user_manager::User* primary_user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  DCHECK(primary_user);
  DCHECK(primary_user->is_profile_created());
  Profile* profile =
      chromeos::ProfileHelper::Get()->GetProfileByUser(primary_user);
  DCHECK(profile);

  UserTypeByDeviceTypeMetricsProvider::UserSegment user_segment =
      UserTypeByDeviceTypeMetricsProvider::GetUserSegment(profile);

  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  policy::MarketSegment device_segment =
      connector->GetEnterpriseMarketSegment();

  return UserTypeByDeviceTypeMetricsProvider::ConstructUmaValue(user_segment,
                                                                device_segment);
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
constexpr char kStandaloneBrowserExtensionHistogramName[] =
    "StandaloneBrowserExtension";

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
    case apps::AppTypeName::kStandaloneBrowserExtension:
      return kStandaloneBrowserExtensionHistogramName;
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
    case apps::AppTypeNameV2::kStandaloneBrowserExtension:
      return kStandaloneBrowserExtensionHistogramName;
  }
}

const std::set<apps::AppTypeName>& GetAppTypeNameSet() {
  return ::GetAppTypeNameSet();
}

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
    case apps::mojom::AppType::kStandaloneBrowserExtension:
      return apps::AppTypeName::kStandaloneBrowserExtension;
  }
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

AppPlatformMetrics::BrowserToTab::BrowserToTab(
    const Instance::InstanceKey& browser_key,
    const Instance::InstanceKey& tab_key)
    : browser_key(Instance::InstanceKey(browser_key)),
      tab_key(Instance::InstanceKey(tab_key)) {}

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
  if (app_type == apps::mojom::AppType::kUnknown || !ShouldRecordUkm()) {
    return;
  }

  apps::AppTypeName app_type_name =
      GetAppTypeName(profile_, app_type, app_id, container);

  if (!ShouldRecordUkmForAppTypeName(app_type_name)) {
    return;
  }

  ukm::SourceId source_id = GetSourceId(app_id);
  if (source_id == ukm::kInvalidSourceId) {
    return;
  }

  ukm::builders::ChromeOSApp_Launch builder(source_id);
  builder.SetAppType((int)app_type_name)
      .SetLaunchSource((int)launch_source)
      .SetUserDeviceMatrix(GetUserTypeByDeviceTypeMetrics())
      .Record(ukm::UkmRecorder::Get());
  ukm::AppSourceUrlRecorder::MarkSourceForDeletion(source_id);
}

void AppPlatformMetrics::RecordAppUninstallUkm(
    apps::mojom::AppType app_type,
    const std::string& app_id,
    apps::mojom::UninstallSource uninstall_source) {
  AppTypeName app_type_name =
      GetAppTypeName(profile_, app_type, app_id,
                     apps::mojom::LaunchContainer::kLaunchContainerNone);
  if (!ShouldRecordUkmForAppTypeName(app_type_name)) {
    return;
  }

  ukm::SourceId source_id = GetSourceId(app_id);
  if (source_id == ukm::kInvalidSourceId) {
    return;
  }

  ukm::builders::ChromeOSApp_UninstallApp builder(source_id);
  builder.SetAppType((int)app_type_name)
      .SetUninstallSource((int)uninstall_source)
      .SetUserDeviceMatrix(user_type_by_device_type_)
      .Record(ukm::UkmRecorder::Get());
  ukm::AppSourceUrlRecorder::MarkSourceForDeletion(source_id);
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
  if (!ShouldRecordUkm()) {
    return;
  }

  if (!update.ReadinessChanged() ||
      update.Readiness() != apps::mojom::Readiness::kReady ||
      apps_util::IsInstalled(update.PriorReadiness())) {
    return;
  }

  if (update.AppId() == extension_misc::kFilesManagerAppId ||
      update.AppId() == arc::kPlayStoreAppId ||
      update.AppId() == arc::kAndroidContactsAppId) {
    RecordAppReadinessStatus(update);
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
  auto app_type = GetAppType(app_id);
  if (app_type == apps::mojom::AppType::kUnknown) {
    return;
  }

  bool is_active = update.State() & apps::InstanceState::kActive;
  if (is_active) {
    auto* window = update.InstanceKey().GetEnclosingAppWindow();
    AppTypeName app_type_name =
        GetAppTypeNameForWindow(profile_, app_type, app_id, window);
    if (app_type_name == apps::AppTypeName::kUnknown) {
      return;
    }

    apps::InstanceState kInActivated = static_cast<apps::InstanceState>(
        apps::InstanceState::kVisible | apps::InstanceState::kRunning);

    // For the browser window, if a tab of the browser is activated, we don't
    // need to calculate the browser window running time.
    if (app_id == extension_misc::kChromeAppId &&
        HasActivatedTab(update.InstanceKey())) {
      SetWindowInActivated(app_id, update.InstanceKey(), kInActivated);
      return;
    }

    // For web apps open in tabs, set the top browser window as inactive to stop
    // calculating the browser window running time.
    if (IsAppOpenedInTab(app_type_name, app_id)) {
      RemoveActivatedTab(update.InstanceKey());
      auto browser_key = Instance::InstanceKey::ForWindowBasedApp(window);
      AddActivatedTab(browser_key, update.InstanceKey());
      SetWindowInActivated(extension_misc::kChromeAppId, browser_key,
                           kInActivated);
    }

    AppTypeNameV2 app_type_name_v2 =
        GetAppTypeNameV2(profile_, app_type, app_id, window);

    SetWindowActivated(app_type, app_type_name, app_type_name_v2, app_id,
                       update.InstanceKey());
    return;
  }

  AppTypeName app_type_name = AppTypeName::kUnknown;
  auto it = running_start_time_.find(update.InstanceKey());
  if (it != running_start_time_.end()) {
    app_type_name = it->second.app_type_name;
  }

  if (IsAppOpenedInTab(app_type_name, app_id)) {
    UpdateBrowserWindowStatus(update.InstanceKey());
  }

  SetWindowInActivated(app_id, update.InstanceKey(), update.State());
}

void AppPlatformMetrics::OnInstanceRegistryWillBeDestroyed(
    apps::InstanceRegistry* cache) {
  apps::InstanceRegistry::Observer::Observe(nullptr);
}

void AppPlatformMetrics::UpdateBrowserWindowStatus(
    const Instance::InstanceKey& tab_key) {
  auto* browser_window = GetBrowserWindow(tab_key);
  if (!browser_window) {
    return;
  }

  // Remove `instance_key` from `active_browser_to_tabs_`.
  RemoveActivatedTab(tab_key);

  // If there are other activated web app tab, we don't need to set the browser
  // window as activated.
  auto browser_key = Instance::InstanceKey::ForWindowBasedApp(browser_window);
  if (HasActivatedTab(browser_key)) {
    return;
  }

  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile_);
  DCHECK(proxy);
  auto state = proxy->InstanceRegistry().GetState(browser_key);
  if (state & InstanceState::kActive) {
    // The browser window is activated, start calculating the browser window
    // running time.
    SetWindowActivated(apps::mojom::AppType::kExtension,
                       AppTypeName::kChromeBrowser,
                       AppTypeNameV2::kChromeBrowser,
                       extension_misc::kChromeAppId, browser_key);
  }
}

bool AppPlatformMetrics::HasActivatedTab(
    const Instance::InstanceKey& browser_key) {
  for (const auto& it : active_browsers_to_tabs_) {
    if (it.browser_key == browser_key) {
      return true;
    }
  }
  return false;
}

aura::Window* AppPlatformMetrics::GetBrowserWindow(
    const Instance::InstanceKey& tab_key) const {
  for (const auto& it : active_browsers_to_tabs_) {
    if (it.tab_key == tab_key) {
      return it.browser_key.GetEnclosingAppWindow();
    }
  }
  return nullptr;
}

void AppPlatformMetrics::AddActivatedTab(
    const Instance::InstanceKey& browser_key,
    const Instance::InstanceKey& tab_key) {
  bool found = false;
  for (const auto& it : active_browsers_to_tabs_) {
    if (it.browser_key == browser_key && it.tab_key == tab_key) {
      found = true;
      break;
    }
  }

  if (!found) {
    active_browsers_to_tabs_.push_back(BrowserToTab(browser_key, tab_key));
  }
}

void AppPlatformMetrics::RemoveActivatedTab(
    const Instance::InstanceKey& tab_key) {
  active_browsers_to_tabs_.remove_if(
      [&tab_key](const BrowserToTab& item) { return item.tab_key == tab_key; });
}

void AppPlatformMetrics::SetWindowActivated(
    apps::mojom::AppType app_type,
    AppTypeName app_type_name,
    AppTypeNameV2 app_type_name_v2,
    const std::string& app_id,
    const apps::Instance::InstanceKey& instance_key) {
  auto it = running_start_time_.find(instance_key);
  if (it != running_start_time_.end()) {
    return;
  }

  running_start_time_[instance_key].start_time = base::TimeTicks::Now();
  running_start_time_[instance_key].app_type_name = app_type_name;
  running_start_time_[instance_key].app_type_name_v2 = app_type_name_v2;

  ++activated_count_[app_type_name];
  should_refresh_activated_count_pref = true;

  start_time_per_five_minutes_[instance_key].start_time =
      base::TimeTicks::Now();
  start_time_per_five_minutes_[instance_key].app_type_name = app_type_name;
  start_time_per_five_minutes_[instance_key].app_type_name_v2 =
      app_type_name_v2;
  start_time_per_five_minutes_[instance_key].app_id = app_id;
}

void AppPlatformMetrics::SetWindowInActivated(
    const std::string& app_id,
    const apps::Instance::InstanceKey& instance_key,
    apps::InstanceState state) {
  bool is_close = state & apps::InstanceState::kDestroyed;
  auto usage_time_it = usage_time_per_five_minutes_.find(instance_key);
  if (is_close && usage_time_it != usage_time_per_five_minutes_.end()) {
    usage_time_it->second.window_is_closed = true;
  }

  auto it = running_start_time_.find(instance_key);
  if (it == running_start_time_.end()) {
    return;
  }

  AppTypeName app_type_name = it->second.app_type_name;
  AppTypeNameV2 app_type_name_v2 = it->second.app_type_name_v2;

  if (usage_time_it == usage_time_per_five_minutes_.end()) {
    usage_time_per_five_minutes_[it->first].source_id = GetSourceId(app_id);
    usage_time_it = usage_time_per_five_minutes_.find(it->first);
  }
  usage_time_it->second.app_type_name = app_type_name;

  running_duration_[app_type_name] +=
      base::TimeTicks::Now() - it->second.start_time;

  base::TimeDelta running_time =
      base::TimeTicks::Now() -
      start_time_per_five_minutes_[instance_key].start_time;
  app_type_running_time_per_five_minutes_[app_type_name] += running_time;
  app_type_v2_running_time_per_five_minutes_[app_type_name_v2] += running_time;
  usage_time_it->second.running_time += running_time;

  running_start_time_.erase(it);
  start_time_per_five_minutes_.erase(instance_key);

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
  running_duration_update->Clear();
  DictionaryPrefUpdate activated_count_update(profile_->GetPrefs(),
                                              kAppActivatedCount);
  activated_count_update->Clear();
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
      usage_time_per_five_minutes_[it.first].source_id =
          GetSourceId(it.second.app_id);
      usage_time_it = usage_time_per_five_minutes_.find(it.first);
    }
    usage_time_it->second.app_type_name = it.second.app_type_name;
    usage_time_it->second.running_time += running_time;
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
  if (!ShouldRecordUkm()) {
    return;
  }

  std::vector<apps::Instance::InstanceKey> closed_window_instances;
  for (auto& it : usage_time_per_five_minutes_) {
    apps::AppTypeName app_type_name = it.second.app_type_name;
    if (!ShouldRecordUkmForAppTypeName(app_type_name)) {
      continue;
    }

    ukm::SourceId source_id = it.second.source_id;
    if (source_id != ukm::kInvalidSourceId &&
        !it.second.running_time.is_zero()) {
      ukm::builders::ChromeOSApp_UsageTime builder(source_id);
      builder.SetAppType((int)app_type_name)
          .SetDuration(it.second.running_time.InMilliseconds())
          .SetUserDeviceMatrix(user_type_by_device_type_)
          .Record(ukm::UkmRecorder::Get());
    }
    if (it.second.window_is_closed) {
      closed_window_instances.push_back(it.first);
      ukm::AppSourceUrlRecorder::MarkSourceForDeletion(source_id);
    } else {
      it.second.running_time = base::TimeDelta();
    }
  }

  for (const auto& closed_instance : closed_window_instances) {
    usage_time_per_five_minutes_.erase(closed_instance);
  }
}

void AppPlatformMetrics::RecordAppsInstallUkm(const apps::AppUpdate& update,
                                              InstallTime install_time) {
  AppTypeName app_type_name =
      GetAppTypeName(profile_, update.AppType(), update.AppId(),
                     apps::mojom::LaunchContainer::kLaunchContainerNone);
  if (!ShouldRecordUkmForAppTypeName(app_type_name)) {
    return;
  }

  ukm::SourceId source_id = GetSourceId(update.AppId());
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
  ukm::AppSourceUrlRecorder::MarkSourceForDeletion(source_id);
}

bool AppPlatformMetrics::ShouldRecordUkm() {
  switch (syncer::GetUploadToGoogleState(
      SyncServiceFactory::GetForProfile(profile_), syncer::ModelType::APPS)) {
    case syncer::UploadState::NOT_ACTIVE:
      return false;
    case syncer::UploadState::INITIALIZING:
      // Note that INITIALIZING is considered good enough, because syncing apps
      // is known to be enabled, and transient errors don't really matter here.
    case syncer::UploadState::ACTIVE:
      return true;
  }
}

ukm::SourceId AppPlatformMetrics::GetSourceId(const std::string& app_id) {
  ukm::SourceId source_id = ukm::kInvalidSourceId;
  apps::mojom::AppType app_type = app_registry_cache_.GetAppType(app_id);
  switch (app_type) {
    case apps::mojom::AppType::kBuiltIn:
    case apps::mojom::AppType::kExtension:
      source_id = ukm::AppSourceUrlRecorder::GetSourceIdForChromeApp(app_id);
      break;
    case apps::mojom::AppType::kArc:
    case apps::mojom::AppType::kWeb:
    case apps::mojom::AppType::kSystemWeb: {
      std::string publisher_id;
      apps::mojom::InstallReason install_reason;
      app_registry_cache_.ForOneApp(app_id, [&publisher_id, &install_reason](
                                                const apps::AppUpdate& update) {
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
      source_id = GetSourceIdForCrostini(app_id);
      break;
    case apps::mojom::AppType::kUnknown:
    case apps::mojom::AppType::kMacOs:
    case apps::mojom::AppType::kPluginVm:
    case apps::mojom::AppType::kStandaloneBrowser:
    case apps::mojom::AppType::kStandaloneBrowserExtension:
    case apps::mojom::AppType::kRemote:
    case apps::mojom::AppType::kBorealis:
      return ukm::kInvalidSourceId;
  }
  return source_id;
}

void AppPlatformMetrics::RecordAppReadinessStatus(
    const apps::AppUpdate& update) {
  std::string histogram_name = kAppPreviousReadinessStatus;
  auto readiness = update.PriorReadiness();
  const std::set<apps::mojom::AppType>& initialized_app_types =
      app_registry_cache_.GetInitializedAppTypes();
  if (update.AppId() == extension_misc::kFilesManagerAppId) {
    histogram_name += "FileManager";
    if (!base::Contains(initialized_app_types,
                        apps::mojom::AppType::kExtension)) {
      readiness = apps::mojom::Readiness::kReady;
    }
  } else if (update.AppId() == arc::kPlayStoreAppId) {
    histogram_name += "PlayStore";
    if (!base::Contains(initialized_app_types, apps::mojom::AppType::kArc)) {
      readiness = apps::mojom::Readiness::kReady;
    }
  } else if (update.AppId() == arc::kAndroidContactsAppId) {
    histogram_name += "Contacts";
    if (!base::Contains(initialized_app_types, apps::mojom::AppType::kArc)) {
      readiness = apps::mojom::Readiness::kReady;
    }
  } else {
    return;
  }

  base::UmaHistogramEnumeration(histogram_name, readiness);
}

ukm::SourceId AppPlatformMetrics::GetSourceIdForCrostini(
    const std::string& app_id) {
  if (app_id == crostini::kCrostiniTerminalSystemAppId) {
    // The terminal is special, since it's actually a web app (though one we
    // count as Crostini) it doesn't have a desktop id, so give it a fake one.
    return ukm::AppSourceUrlRecorder::GetSourceIdForCrostini("CrostiniTerminal",
                                                             "Terminal");
  }
  auto* registry =
      guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile_);
  auto registration = registry->GetRegistration(app_id);
  if (!registration) {
    return ukm::kInvalidSourceId;
  }
  auto desktop_id = registration->DesktopFileId() == ""
                        ? "NoId"
                        : registration->DesktopFileId();
  return ukm::AppSourceUrlRecorder::GetSourceIdForCrostini(
      desktop_id, registration->Name());
}

apps::mojom::AppType AppPlatformMetrics::GetAppType(const std::string& app_id) {
  auto type = app_registry_cache_.GetAppType(app_id);
  if (type != mojom::AppType::kUnknown) {
    return type;
  }
  if (crostini::IsCrostiniShelfAppId(profile_, app_id)) {
    return mojom::AppType::kCrostini;
  }
  return mojom::AppType::kUnknown;
}

}  // namespace apps
