// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_utils.h"

#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/guest_os/guest_os_shelf_utils.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/metrics/usertype_by_devicetype_metrics_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#include "chromeos/components/mgs/managed_guest_session_utils.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/app_constants/constants.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#include "components/services/app_service/public/cpp/instance_update.h"
#include "components/sync/base/data_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_utils.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"

namespace {

constexpr auto kAppTypeNameMap =
    base::MakeFixedFlatMap<std::string_view, apps::AppTypeName>({
        {apps::kArcHistogramName, apps::AppTypeName::kArc},
        {apps::kBuiltInHistogramName, apps::AppTypeName::kBuiltIn},
        {apps::kCrostiniHistogramName, apps::AppTypeName::kCrostini},
        {apps::kChromeAppHistogramName, apps::AppTypeName::kChromeApp},
        {apps::kWebAppHistogramName, apps::AppTypeName::kWeb},
        {apps::kPluginVmHistogramName, apps::AppTypeName::kPluginVm},
        {apps::kStandaloneBrowserHistogramName,
         apps::AppTypeName::kStandaloneBrowser},
        {apps::kRemoteHistogramName, apps::AppTypeName::kRemote},
        {apps::kBorealisHistogramName, apps::AppTypeName::kBorealis},
        {apps::kSystemWebAppHistogramName, apps::AppTypeName::kSystemWeb},
        {apps::kChromeBrowserHistogramName, apps::AppTypeName::kChromeBrowser},
        {apps::kStandaloneBrowserChromeAppHistogramName,
         apps::AppTypeName::kStandaloneBrowserChromeApp},
        {apps::kExtensionHistogramName, apps::AppTypeName::kExtension},
        {apps::kStandaloneBrowserExtensionHistogramName,
         apps::AppTypeName::kStandaloneBrowserExtension},
        {apps::kStandaloneBrowserWebAppHistogramName,
         apps::AppTypeName::kStandaloneBrowserWebApp},
        {apps::kBruschettaHistogramName, apps::AppTypeName::kBruschetta},
    });

constexpr char kInstallReasonUnknownHistogram[] = "Unknown";
constexpr char kInstallReasonSystemHistogram[] = "System";
constexpr char kInstallReasonPolicyHistogram[] = "Policy";
constexpr char kInstallReasonOemHistogram[] = "Oem";
constexpr char kInstallReasonPreloadHistogram[] = "Preload";
constexpr char kInstallReasonSyncHistogram[] = "Sync";
constexpr char kInstallReasonUserHistogram[] = "User";
constexpr char kInstallReasonSubAppHistogram[] = "SubApp";
constexpr char kInstallReasonKioskHistogram[] = "Kiosk";
constexpr char kInstallReasonCommandLineHistogram[] = "CommandLine";

// Determines what app type a Chrome App should be logged as based on its launch
// container and app id. In particular, Chrome apps in tabs are logged as part
// of Chrome browser.
apps::AppTypeName GetAppTypeNameForChromeApp(Profile* profile,
                                             const std::string& app_id,
                                             apps::LaunchContainer container) {
  if (app_id == app_constants::kChromeAppId) {
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
    case apps::LaunchContainer::kLaunchContainerWindow:
      return apps::AppTypeName::kChromeApp;
    case apps::LaunchContainer::kLaunchContainerTab:
      return apps::AppTypeName::kChromeBrowser;
    default:
      break;
  }

  apps::LaunchContainer launch_container = extensions::GetLaunchContainer(
      extensions::ExtensionPrefs::Get(profile), extension);
  if (launch_container == apps::LaunchContainer::kLaunchContainerTab) {
    return apps::AppTypeName::kChromeBrowser;
  }

  return apps::AppTypeName::kChromeApp;
}

apps::AppTypeName GetWebAppTypeName() {
  return crosapi::browser_util::IsLacrosEnabled()
             ? apps::AppTypeName::kStandaloneBrowserWebApp
             : apps::AppTypeName::kWeb;
}

bool UkmReportingIsAllowedForAppInManagedGuestSession(
    const std::string& app_id,
    const apps::AppRegistryCache& cache) {
  CHECK(chromeos::IsManagedGuestSession());

  bool is_allowed = false;
  cache.ForOneApp(app_id, [&is_allowed](const apps::AppUpdate& app) {
    is_allowed = app.InstallReason() == apps::InstallReason::kSystem ||
                 app.InstallReason() == apps::InstallReason::kPolicy ||
                 app.InstallReason() == apps::InstallReason::kOem ||
                 app.InstallReason() == apps::InstallReason::kDefault;
  });
  return is_allowed;
}

}  // namespace

namespace apps {

constexpr base::TimeDelta kMinDuration = base::Seconds(1);
constexpr base::TimeDelta kMaxUsageDuration = base::Minutes(5);
constexpr int kDurationBuckets = 100;
constexpr int kUsageTimeBuckets = 50;

AppTypeName GetAppTypeNameForWebApp(Profile* profile,
                                    const std::string& app_id,
                                    apps::LaunchContainer container) {
  AppTypeName default_type_name = crosapi::browser_util::IsLacrosEnabled()
                                      ? AppTypeName::kStandaloneBrowser
                                      : AppTypeName::kChromeBrowser;
  AppTypeName type_name = default_type_name;
  WindowMode window_mode = WindowMode::kBrowser;
  AppServiceProxyFactory::GetForProfile(profile)->AppRegistryCache().ForOneApp(
      app_id, [&type_name, &window_mode](const AppUpdate& update) {
        DCHECK(update.AppType() == AppType::kWeb ||
               update.AppType() == AppType::kSystemWeb);

        // For system web apps, the install source is |kSystem|.
        // The app type may be kSystemWeb (system web apps in Ash when
        // Lacros web apps are enabled), or kWeb (all other cases).
        type_name = (update.InstallReason() == InstallReason::kSystem)
                        ? AppTypeName::kSystemWeb
                        : AppTypeName::kWeb;
        window_mode = update.WindowMode();
      });

  if (type_name != AppTypeName::kWeb) {
    return type_name;
  }

  switch (container) {
    case apps::LaunchContainer::kLaunchContainerWindow:
      return GetWebAppTypeName();
    case apps::LaunchContainer::kLaunchContainerTab:
      return default_type_name;
    default:
      break;
  }

  return window_mode == WindowMode::kBrowser ? default_type_name
                                             : GetWebAppTypeName();
}

AppTypeName GetAppTypeNameForStandaloneBrowserChromeApp(
    Profile* profile,
    const std::string& app_id,
    apps::LaunchContainer container) {
  AppTypeName app_type_name = AppTypeName::kStandaloneBrowser;
  WindowMode window_mode = WindowMode::kUnknown;
  AppServiceProxyFactory::GetForProfile(profile)->AppRegistryCache().ForOneApp(
      app_id, [&app_type_name, &window_mode](const AppUpdate& update) {
        DCHECK(update.AppType() == AppType::kStandaloneBrowserChromeApp);

        // For platform apps, app type name is kStandaloneBrowserChromeApp;
        if (update.IsPlatformApp().value_or(false)) {
          app_type_name = AppTypeName::kStandaloneBrowserChromeApp;
          return;
        }

        window_mode = update.WindowMode();
      });

  if (app_type_name == AppTypeName::kStandaloneBrowserChromeApp) {
    return app_type_name;
  }

  switch (container) {
    case apps::LaunchContainer::kLaunchContainerWindow:
      return AppTypeName::kStandaloneBrowserChromeApp;
    case apps::LaunchContainer::kLaunchContainerTab:
      return AppTypeName::kStandaloneBrowser;
    case apps::LaunchContainer::kLaunchContainerPanelDeprecated:
    case apps::LaunchContainer::kLaunchContainerNone:
      break;
  }
  return window_mode == WindowMode::kWindow ||
                 window_mode == WindowMode::kTabbedWindow
             ? AppTypeName::kStandaloneBrowserChromeApp
             : AppTypeName::kStandaloneBrowser;
}

bool IsAshBrowserWindow(aura::Window* window) {
  Browser* browser = chrome::FindBrowserWithWindow(window->GetToplevelWindow());
  if (!browser || browser->is_type_app() || browser->is_type_app_popup()) {
    return false;
  }
  return true;
}

bool IsLacrosBrowserWindow(Profile* profile, aura::Window* window) {
  if (!crosapi::browser_util::IsLacrosEnabled()) {
    return false;
  }

  bool ret = false;
  AppServiceProxyFactory::GetForProfile(profile)
      ->InstanceRegistry()
      .ForInstancesWithWindow(window, [&](const apps::InstanceUpdate& update) {
        if (update.AppId() == app_constants::kLacrosAppId) {
          ret = true;
        }
      });
  return ret;
}

bool IsLacrosWindow(aura::Window* window) {
  return window->GetProperty(chromeos::kAppTypeKey) ==
         chromeos::AppType::LACROS;
}

bool IsAppOpenedInTab(AppTypeName app_type_name, const std::string& app_id) {
  return (app_type_name == apps::AppTypeName::kChromeBrowser &&
          app_id != app_constants::kChromeAppId) ||
         (app_type_name == apps::AppTypeName::kStandaloneBrowser &&
          app_id != app_constants::kLacrosAppId);
}

bool IsAppOpenedWithBrowserWindow(Profile* profile,
                                  AppType app_type,
                                  const std::string& app_id) {
  if (app_type == AppType::kWeb || app_type == AppType::kSystemWeb ||
      app_type == AppType::kExtension ||
      app_type == AppType::kStandaloneBrowser ||
      app_type == AppType::kStandaloneBrowserChromeApp ||
      app_type == AppType::kStandaloneBrowserExtension) {
    return true;
  }

  if (app_type != AppType::kChromeApp) {
    return false;
  }

  DCHECK(profile);
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile);
  DCHECK(registry);
  const extensions::Extension* extension =
      registry->GetInstalledExtension(app_id);

  return extension && !extension->is_platform_app();
}

AppTypeName GetAppTypeNameForWebAppWindow(Profile* profile,
                                          const std::string& app_id,
                                          aura::Window* window) {
  if (IsAshBrowserWindow(window)) {
    return AppTypeName::kChromeBrowser;
  }

  if (GetAppTypeNameForWebApp(profile, app_id,
                              apps::LaunchContainer::kLaunchContainerNone) ==
      AppTypeName::kSystemWeb) {
    return AppTypeName::kSystemWeb;
  }

  return IsLacrosBrowserWindow(profile, window)
             ? AppTypeName::kStandaloneBrowser
             : GetWebAppTypeName();
}

AppTypeName GetAppTypeNameForWindow(Profile* profile,
                                    AppType app_type,
                                    const std::string& app_id,
                                    aura::Window* window) {
  switch (app_type) {
    case AppType::kUnknown:
      return apps::AppTypeName::kUnknown;
    case AppType::kArc:
      return apps::AppTypeName::kArc;
    case AppType::kBuiltIn:
      return apps::AppTypeName::kBuiltIn;
    case AppType::kCrostini:
      return apps::AppTypeName::kCrostini;
    case AppType::kChromeApp:
      return IsAshBrowserWindow(window) ? apps::AppTypeName::kChromeBrowser
                                        : apps::AppTypeName::kChromeApp;
    case AppType::kWeb:
      return GetAppTypeNameForWebAppWindow(profile, app_id, window);
    case AppType::kPluginVm:
      return apps::AppTypeName::kPluginVm;
    case AppType::kStandaloneBrowser:
      return apps::AppTypeName::kStandaloneBrowser;
    case AppType::kRemote:
      return apps::AppTypeName::kRemote;
    case AppType::kBorealis:
      return apps::AppTypeName::kBorealis;
    case AppType::kSystemWeb:
      return apps::AppTypeName::kSystemWeb;
    case AppType::kStandaloneBrowserChromeApp:
      return IsLacrosBrowserWindow(profile, window)
                 ? AppTypeName::kStandaloneBrowser
                 : AppTypeName::kStandaloneBrowserChromeApp;
    case AppType::kExtension:
      return apps::AppTypeName::kExtension;
    case AppType::kStandaloneBrowserExtension:
      return apps::AppTypeName::kStandaloneBrowserExtension;
    case AppType::kBruschetta:
      return apps::AppTypeName::kBruschetta;
  }
}

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
    case apps::AppTypeName::kStandaloneBrowserExtension:
      return kStandaloneBrowserExtensionHistogramName;
    case apps::AppTypeName::kStandaloneBrowserWebApp:
      return kStandaloneBrowserWebAppHistogramName;
    case apps::AppTypeName::kBruschetta:
      return kBruschettaHistogramName;
  }
}

AppTypeName GetAppTypeNameFromString(const std::string& app_type_name) {
  auto it = kAppTypeNameMap.find(app_type_name);
  return it != kAppTypeNameMap.end() ? it->second : apps::AppTypeName::kUnknown;
}

std::string GetInstallReason(InstallReason install_reason) {
  switch (install_reason) {
    case apps::InstallReason::kUnknown:
      return kInstallReasonUnknownHistogram;
    case apps::InstallReason::kSystem:
      return kInstallReasonSystemHistogram;
    case apps::InstallReason::kPolicy:
      return kInstallReasonPolicyHistogram;
    case apps::InstallReason::kOem:
      return kInstallReasonOemHistogram;
    case apps::InstallReason::kDefault:
      return kInstallReasonPreloadHistogram;
    case apps::InstallReason::kSync:
      return kInstallReasonSyncHistogram;
    case apps::InstallReason::kUser:
      return kInstallReasonUserHistogram;
    case apps::InstallReason::kSubApp:
      return kInstallReasonSubAppHistogram;
    case apps::InstallReason::kKiosk:
      return kInstallReasonKioskHistogram;
    case apps::InstallReason::kCommandLine:
      return kInstallReasonCommandLineHistogram;
  }
}

bool ShouldRecordAppKM(Profile* profile) {
  // Bypass AppKM App Sync check for Demo Mode devices to collect app metrics.
  if (ash::DemoSession::IsDeviceInDemoMode()) {
    return true;
  }

  // Bypass AppKM App Sync check in Kiosk and MGS to collect app metrics.
  if (chromeos::IsKioskSession() || chromeos::IsManagedGuestSession()) {
    return true;
  }

  switch (syncer::GetUploadToGoogleState(
      SyncServiceFactory::GetForProfile(profile), syncer::DataType::APPS)) {
    case syncer::UploadState::NOT_ACTIVE:
      return false;
    case syncer::UploadState::INITIALIZING:
      // Note that INITIALIZING is considered good enough, because syncing apps
      // is known to be enabled, and transient errors don't really matter here.
    case syncer::UploadState::ACTIVE:
      return true;
  }
}

bool ShouldRecordAppKMForAppId(Profile* profile,
                               const AppRegistryCache& cache,
                               const std::string& app_id) {
  if (!ShouldRecordAppKM(profile)) {
    return false;
  }

  if (chromeos::IsManagedGuestSession() &&
      !UkmReportingIsAllowedForAppInManagedGuestSession(app_id, cache)) {
    return false;
  }
  return true;
}

bool ShouldRecordAppKMForAppTypeName(AppType app_type) {
  switch (app_type) {
    case AppType::kArc:
    case AppType::kBuiltIn:
    case AppType::kChromeApp:
    case AppType::kWeb:
    case AppType::kSystemWeb:
    case AppType::kCrostini:
    case AppType::kBorealis:
    case AppType::kExtension:
    case AppType::kStandaloneBrowser:
    case AppType::kStandaloneBrowserChromeApp:
    case AppType::kStandaloneBrowserExtension:
      return true;
    case AppType::kBruschetta:
    case AppType::kUnknown:
    case AppType::kPluginVm:
    case AppType::kRemote:
      return false;
  }
}

int GetUserTypeByDeviceTypeMetrics() {
  const user_manager::User* primary_user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  DCHECK(primary_user);
  UserTypeByDeviceTypeMetricsProvider::UserSegment user_segment =
      UserTypeByDeviceTypeMetricsProvider::UserSegment::kUnmanaged;
  // In some tast tests, primary_user->is_profile_created() might return false
  // for some unknown reasons.
  if (primary_user->is_profile_created()) {
    Profile* profile =
        ash::ProfileHelper::Get()->GetProfileByUser(primary_user);
    DCHECK(profile);

    user_segment = UserTypeByDeviceTypeMetricsProvider::GetUserSegment(profile);
  }

  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  policy::MarketSegment device_segment =
      connector->GetEnterpriseMarketSegment();

  return UserTypeByDeviceTypeMetricsProvider::ConstructUmaValue(user_segment,
                                                                device_segment);
}

AppTypeName GetAppTypeName(Profile* profile,
                           AppType app_type,
                           const std::string& app_id,
                           apps::LaunchContainer container) {
  switch (app_type) {
    case AppType::kUnknown:
      return apps::AppTypeName::kUnknown;
    case AppType::kArc:
      return apps::AppTypeName::kArc;
    case AppType::kBuiltIn:
      return apps::AppTypeName::kBuiltIn;
    case AppType::kCrostini:
      return apps::AppTypeName::kCrostini;
    case AppType::kChromeApp:
      return GetAppTypeNameForChromeApp(profile, app_id, container);
    case AppType::kWeb:
      return GetAppTypeNameForWebApp(profile, app_id, container);
    case AppType::kPluginVm:
      return apps::AppTypeName::kPluginVm;
    case AppType::kStandaloneBrowser:
      return apps::AppTypeName::kStandaloneBrowser;
    case AppType::kRemote:
      return apps::AppTypeName::kRemote;
    case AppType::kBorealis:
      return apps::AppTypeName::kBorealis;
    case AppType::kSystemWeb:
      return apps::AppTypeName::kSystemWeb;
    case AppType::kStandaloneBrowserChromeApp:
      return GetAppTypeNameForStandaloneBrowserChromeApp(profile, app_id,
                                                         container);
    case AppType::kExtension:
      return apps::AppTypeName::kExtension;
    case AppType::kStandaloneBrowserExtension:
      return apps::AppTypeName::kStandaloneBrowserExtension;
    case AppType::kBruschetta:
      return apps::AppTypeName::kBruschetta;
  }
}

AppType GetAppType(Profile* profile, const std::string& app_id) {
  DCHECK(AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile));
  auto type = apps::AppServiceProxyFactory::GetForProfile(profile)
                  ->AppRegistryCache()
                  .GetAppType(app_id);
  if (type != AppType::kUnknown) {
    return type;
  }
  if (guest_os::IsCrostiniShelfAppId(profile, app_id)) {
    return AppType::kCrostini;
  }
  return AppType::kUnknown;
}

bool IsSystemWebApp(Profile* profile, const std::string& app_id) {
  AppType app_type = GetAppType(profile, app_id);

  InstallReason install_reason;
  apps::AppServiceProxyFactory::GetForProfile(profile)
      ->AppRegistryCache()
      .ForOneApp(app_id, [&install_reason](const apps::AppUpdate& update) {
        install_reason = update.InstallReason();
      });

  return app_type == AppType::kSystemWeb ||
         install_reason == apps::InstallReason::kSystem;
}

}  // namespace apps
