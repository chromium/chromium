// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_utils.h"

#include "base/containers/fixed_flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/crostini/crostini_shelf_utils.h"
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
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/app_constants/constants.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_service_utils.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "ui/aura/window.h"

namespace {

constexpr auto kAppTypeNameMap =
    base::MakeFixedFlatMap<base::StringPiece, apps::AppTypeName>({
        {apps::kArcHistogramName, apps::AppTypeName::kArc},
        {apps::kBuiltInHistogramName, apps::AppTypeName::kBuiltIn},
        {apps::kCrostiniHistogramName, apps::AppTypeName::kCrostini},
        {apps::kChromeAppHistogramName, apps::AppTypeName::kChromeApp},
        {apps::kWebAppHistogramName, apps::AppTypeName::kWeb},
        {apps::kMacOsHistogramName, apps::AppTypeName::kMacOs},
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
    });

// Determines what app type a Chrome App should be logged as based on its launch
// container and app id. In particular, Chrome apps in tabs are logged as part
// of Chrome browser.
apps::AppTypeName GetAppTypeNameForChromeApp(
    Profile* profile,
    const std::string& app_id,
    apps::mojom::LaunchContainer container) {
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

}  // namespace

namespace apps {

constexpr base::TimeDelta kMinDuration = base::Seconds(1);
constexpr base::TimeDelta kMaxUsageDuration = base::Minutes(5);
constexpr int kDurationBuckets = 100;
constexpr int kUsageTimeBuckets = 50;

AppTypeName GetAppTypeNameForWebApp(Profile* profile,
                                    const std::string& app_id,
                                    apps::mojom::LaunchContainer container) {
  apps::AppTypeName type_name = apps::AppTypeName::kChromeBrowser;
  WindowMode window_mode = WindowMode::kBrowser;
  apps::AppServiceProxyFactory::GetForProfile(profile)
      ->AppRegistryCache()
      .ForOneApp(
          app_id, [&type_name, &window_mode](const apps::AppUpdate& update) {
            DCHECK(update.AppType() == apps::AppType::kWeb ||
                   update.AppType() == apps::AppType::kSystemWeb);

            // For system web apps, the install source is |kSystem|.
            // The app type may be kSystemWeb (system web apps in Ash when
            // Lacros web apps are enabled), or kWeb (all other cases).
            type_name = (update.InstallReason() == apps::InstallReason::kSystem)
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

  if (window_mode == WindowMode::kBrowser) {
    return apps::AppTypeName::kChromeBrowser;
  }

  return apps::AppTypeName::kWeb;
}

bool IsBrowser(aura::Window* window) {
  Browser* browser = chrome::FindBrowserWithWindow(window->GetToplevelWindow());
  if (!browser || browser->is_type_app() || browser->is_type_app_popup()) {
    return false;
  }
  return true;
}

bool IsAppOpenedInTab(AppTypeName app_type_name, const std::string& app_id) {
  return app_type_name == apps::AppTypeName::kChromeBrowser &&
         app_id != app_constants::kChromeAppId;
}

bool IsAppOpenedWithBrowserWindow(Profile* profile,
                                  AppType app_type,
                                  const std::string& app_id) {
  if (app_type == AppType::kWeb || app_type == AppType::kSystemWeb ||
      app_type == AppType::kExtension) {
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
      return IsBrowser(window) ? apps::AppTypeName::kChromeBrowser
                               : apps::AppTypeName::kChromeApp;
    case AppType::kWeb:
      return GetAppTypeNameForWebAppWindow(profile, app_id, window);
    case AppType::kMacOs:
      return apps::AppTypeName::kMacOs;
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
      return apps::AppTypeName::kStandaloneBrowserChromeApp;
    case AppType::kExtension:
      return apps::AppTypeName::kExtension;
    case AppType::kStandaloneBrowserExtension:
      return apps::AppTypeName::kStandaloneBrowserExtension;
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
    case apps::AppTypeName::kStandaloneBrowserExtension:
      return kStandaloneBrowserExtensionHistogramName;
  }
}

AppTypeName GetAppTypeNameFromString(const std::string& app_type_name) {
  auto* it = kAppTypeNameMap.find(app_type_name);
  return it != kAppTypeNameMap.end() ? it->second : apps::AppTypeName::kUnknown;
}

bool ShouldRecordUkm(Profile* profile) {
  switch (syncer::GetUploadToGoogleState(
      SyncServiceFactory::GetForProfile(profile), syncer::ModelType::APPS)) {
    case syncer::UploadState::NOT_ACTIVE:
      return false;
    case syncer::UploadState::INITIALIZING:
      // Note that INITIALIZING is considered good enough, because syncing apps
      // is known to be enabled, and transient errors don't really matter here.
    case syncer::UploadState::ACTIVE:
      return true;
  }
}

bool ShouldRecordUkmForAppTypeName(AppType app_type) {
  switch (app_type) {
    case AppType::kArc:
    case AppType::kBuiltIn:
    case AppType::kChromeApp:
    case AppType::kWeb:
    case AppType::kSystemWeb:
    case AppType::kCrostini:
    case AppType::kBorealis:
    case AppType::kExtension:
      return true;
    case AppType::kUnknown:
    case AppType::kMacOs:
    case AppType::kPluginVm:
    case AppType::kStandaloneBrowser:
    case AppType::kStandaloneBrowserChromeApp:
    case AppType::kRemote:
    case AppType::kStandaloneBrowserExtension:
      return false;
  }
}

int GetUserTypeByDeviceTypeMetrics() {
  const user_manager::User* primary_user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  DCHECK(primary_user);
  DCHECK(primary_user->is_profile_created());
  Profile* profile = ash::ProfileHelper::Get()->GetProfileByUser(primary_user);
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

AppTypeName GetAppTypeName(Profile* profile,
                           AppType app_type,
                           const std::string& app_id,
                           apps::mojom::LaunchContainer container) {
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
    case AppType::kMacOs:
      return apps::AppTypeName::kMacOs;
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
      return apps::AppTypeName::kStandaloneBrowserChromeApp;
    case AppType::kExtension:
      return apps::AppTypeName::kExtension;
    case AppType::kStandaloneBrowserExtension:
      return apps::AppTypeName::kStandaloneBrowserExtension;
  }
}

AppType GetAppType(Profile* profile, const std::string& app_id) {
  DCHECK(AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile));
  auto type = ConvertMojomAppTypToAppType(
      apps::AppServiceProxyFactory::GetForProfile(profile)
          ->AppRegistryCache()
          .GetAppType(app_id));
  if (type != AppType::kUnknown) {
    return type;
  }
  if (crostini::IsCrostiniShelfAppId(profile, app_id)) {
    return AppType::kCrostini;
  }
  return AppType::kUnknown;
}

}  // namespace apps
