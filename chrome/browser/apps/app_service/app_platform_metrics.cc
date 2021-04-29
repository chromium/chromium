// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_platform_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/apps/app_service/app_service_metrics.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"

namespace {

// UMA metrics for a snapshot count of installed apps.
constexpr char kArcAppsCountHistogramName[] = "Apps.AppsCount.Arc";
constexpr char kBuiltInAppsCountHistogramName[] = "Apps.AppsCount.BuiltIn";
constexpr char kCrostiniAppsCountHistogramName[] = "Apps.AppsCount.Crostini";
constexpr char kExtensionAppsCountHistogramName[] = "Apps.AppsCount.ChromeApp";
constexpr char kWebAppsCountHistogramName[] = "Apps.AppsCount.WebApp";
constexpr char kMacOsAppsCountHistogramName[] = "Apps.AppsCount.MacOs";
constexpr char kPluginVmAppsCountHistogramName[] = "Apps.AppsCount.PluginVm";
constexpr char kStandaloneBrowserAppsCountHistogramName[] =
    "Apps.AppsCount.StandaloneBrowser";
constexpr char kRemoteAppsCountHistogramName[] = "Apps.AppsCount.RemoteApp";
constexpr char kBorealisAppsCountHistogramName[] = "Apps.AppsCount.Borealis";
constexpr char kSystemWebAppsCountHistogramName[] =
    "Apps.AppsCount.SystemWebApp";

const char* GetAppsCountHistogramName(apps::AppTypeName app_type_name) {
  switch (app_type_name) {
    case apps::AppTypeName::kUnknown:
      return nullptr;
    case apps::AppTypeName::kArc:
      return kArcAppsCountHistogramName;
    case apps::AppTypeName::kBuiltIn:
      return kBuiltInAppsCountHistogramName;
    case apps::AppTypeName::kCrostini:
      return kCrostiniAppsCountHistogramName;
    case apps::AppTypeName::kChromeApp:
      return kExtensionAppsCountHistogramName;
    case apps::AppTypeName::kWeb:
      return kWebAppsCountHistogramName;
    case apps::AppTypeName::kMacOs:
      return kMacOsAppsCountHistogramName;
    case apps::AppTypeName::kPluginVm:
      return kPluginVmAppsCountHistogramName;
    case apps::AppTypeName::kStandaloneBrowser:
      return kStandaloneBrowserAppsCountHistogramName;
    case apps::AppTypeName::kRemote:
      return kRemoteAppsCountHistogramName;
    case apps::AppTypeName::kBorealis:
      return kBorealisAppsCountHistogramName;
    case apps::AppTypeName::kSystemWeb:
      return kSystemWebAppsCountHistogramName;
    case apps::AppTypeName::kChromeBrowser:
      return nullptr;
  }
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

void RecordAppLaunchMetrics(Profile* profile,
                            const apps::AppUpdate& update,
                            apps::mojom::LaunchSource launch_source,
                            apps::mojom::LaunchContainer container) {
  apps::mojom::AppType app_type = update.AppType();
  if (app_type == apps::mojom::AppType::kUnknown) {
    return;
  }

  const std::string& app_id = update.AppId();

  RecordAppLaunchSource(launch_source);
  RecordAppLaunchPerAppType(
      GetAppTypeName(profile, app_type, app_id, container));
}

AppPlatformMetrics::AppPlatformMetrics(
    Profile* profile,
    apps::AppRegistryCache& app_registry_cache)
    : profile_(profile),
      app_registry_cache_(app_registry_cache),
      first_report_on_current_device_(true) {
  Observe(&app_registry_cache);
}

AppPlatformMetrics::~AppPlatformMetrics() = default;

// static
const char* AppPlatformMetrics::GetAppsCountHistogramNameForTest(
    AppTypeName app_type_name) {
  return GetAppsCountHistogramName(app_type_name);
}

void AppPlatformMetrics::OnNewDay() {
  // Ignores the first report. Apps and extensions may sync slowly after the
  // OOBE process, biasing the metrics downwards toward zero.
  if (first_report_on_current_device_) {
    first_report_on_current_device_ = false;
    return;
  }

  should_record_metrics_on_new_day_ = true;
  RecordAppsCount(apps::mojom::AppType::kUnknown);
}

void AppPlatformMetrics::OnAppTypeInitialized(apps::mojom::AppType app_type) {
  if (should_record_metrics_on_new_day_)
    RecordAppsCount(app_type);
}

void AppPlatformMetrics::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  Observe(nullptr);
}

void AppPlatformMetrics::OnAppUpdate(const apps::AppUpdate& update) {}

void AppPlatformMetrics::RecordAppsCount(apps::mojom::AppType app_type) {
  std::map<AppTypeName, int> app_count;
  app_registry_cache_.ForEachApp(
      [app_type, this, &app_count](const apps::AppUpdate& update) {
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
      });

  for (auto it : app_count) {
    const char* histogram_name = GetAppsCountHistogramName(it.first);
    if (histogram_name) {
      // If there are more than a thousand apps installed, then that count is
      // going into an overflow bucket. We don't expect this scenario to happen
      // often.
      base::UmaHistogramCounts1000(histogram_name, it.second);
    }
  }
}

}  // namespace apps
