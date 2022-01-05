// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_utils.h"

#include "base/metrics/histogram_functions.h"
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
#include "components/sync/base/model_type.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_service_utils.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "ui/aura/window.h"

namespace {

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

bool IsBrowser(aura::Window* window) {
  Browser* browser = chrome::FindBrowserWithWindow(window->GetToplevelWindow());
  if (!browser || browser->is_type_app() || browser->is_type_app_popup()) {
    return false;
  }
  return true;
}

bool IsAppOpenedInTab(AppTypeName app_type_name, const std::string& app_id) {
  return app_type_name == apps::AppTypeName::kChromeBrowser &&
         app_id != extension_misc::kChromeAppId;
}

bool IsAppOpenedWithBrowserWindow(Profile* profile,
                                  apps::mojom::AppType app_type,
                                  const std::string& app_id) {
  if (app_type == mojom::AppType::kWeb ||
      app_type == mojom::AppType::kSystemWeb ||
      app_type == mojom::AppType::kExtension) {
    return true;
  }

  if (app_type != mojom::AppType::kChromeApp) {
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
    case apps::mojom::AppType::kChromeApp:
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
    case apps::mojom::AppType::kStandaloneBrowserChromeApp:
      return apps::AppTypeName::kStandaloneBrowserChromeApp;
    case apps::mojom::AppType::kExtension:
      return apps::AppTypeName::kExtension;
  }
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

AppTypeName GetAppTypeName(Profile* profile,
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
    case apps::mojom::AppType::kChromeApp:
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
    case apps::mojom::AppType::kStandaloneBrowserChromeApp:
      return apps::AppTypeName::kStandaloneBrowserChromeApp;
    case apps::mojom::AppType::kExtension:
      return apps::AppTypeName::kExtension;
  }
}

mojom::AppType GetAppType(Profile* profile, const std::string& app_id) {
  DCHECK(AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile));
  auto type = apps::AppServiceProxyFactory::GetForProfile(profile)
                  ->AppRegistryCache()
                  .GetAppType(app_id);
  if (type != mojom::AppType::kUnknown) {
    return type;
  }
  if (crostini::IsCrostiniShelfAppId(profile, app_id)) {
    return mojom::AppType::kCrostini;
  }
  return mojom::AppType::kUnknown;
}

}  // namespace apps
