// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_platform_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/apps/app_service/app_service_metrics.h"
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

// This is used for logging, so do not remove or reorder existing entries.
enum class AppTypeName {
  kUnknown = 0,
  kArc = 1,
  kBuiltIn = 2,
  kCrostini = 3,
  kChromeApp = 4,
  kWeb = 5,
  kMacOs = 6,
  kPluginVm = 7,
  kStandaloneBrowser = 8,
  kRemote = 9,
  kBorealis = 10,
  kSystemWeb = 11,
  kChromeBrowser = 12,

  // Add any new values above this one, and update kMaxValue to the highest
  // enumerator value.
  kMaxValue = kChromeBrowser,
};

// Determines what app type a Chrome App should be logged as based on its launch
// container and app id. In particular, Chrome apps in tabs are logged as part
// of Chrome browser.
AppTypeName GetAppTypeNameForChromeApp(Profile* profile,
                                       const std::string& app_id,
                                       apps::mojom::LaunchContainer container) {
  if (app_id == extension_misc::kChromeAppId) {
    return AppTypeName::kChromeBrowser;
  }

  DCHECK(profile);
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile);
  DCHECK(registry);
  const extensions::Extension* extension =
      registry->GetInstalledExtension(app_id);

  if (CanLaunchViaEvent(extension)) {
    return AppTypeName::kChromeApp;
  }

  switch (container) {
    case apps::mojom::LaunchContainer::kLaunchContainerWindow:
      return AppTypeName::kChromeApp;
    case apps::mojom::LaunchContainer::kLaunchContainerTab:
      return AppTypeName::kChromeBrowser;
    default:
      break;
  }

  apps::mojom::LaunchContainer launch_container =
      extensions::GetLaunchContainer(extensions::ExtensionPrefs::Get(profile),
                                     extension);
  if (launch_container == apps::mojom::LaunchContainer::kLaunchContainerTab) {
    return AppTypeName::kChromeBrowser;
  }

  return AppTypeName::kChromeApp;
}

// Determines what app type a web app should be logged as based on its launch
// container and app id. In particular, web apps in tabs are logged as part of
// Chrome browser.
AppTypeName GetAppTypeNameForWebApp(Profile* profile,
                                    const std::string& app_id,
                                    apps::mojom::LaunchContainer container) {
  auto* provider = web_app::WebAppProvider::Get(profile);
  DCHECK(provider);

  const auto* registrar = provider->registrar().AsWebAppRegistrar();
  if (!registrar) {
    return AppTypeName::kChromeBrowser;
  }

  const auto* web_app = registrar->GetAppById(app_id);
  if (!web_app) {
    return AppTypeName::kChromeBrowser;
  }

  if (web_app->IsSystemApp()) {
    return AppTypeName::kSystemWeb;
  }

  switch (container) {
    case apps::mojom::LaunchContainer::kLaunchContainerWindow:
      return AppTypeName::kWeb;
    case apps::mojom::LaunchContainer::kLaunchContainerTab:
      return AppTypeName::kChromeBrowser;
    default:
      break;
  }

  if (web_app::ConvertDisplayModeToAppLaunchContainer(
          registrar->GetAppEffectiveDisplayMode(app_id)) ==
      apps::mojom::LaunchContainer::kLaunchContainerTab) {
    return AppTypeName::kChromeBrowser;
  }

  return AppTypeName::kWeb;
}

// Returns AppTypeName used for app launch metrics.
AppTypeName GetAppTypeName(Profile* profile,
                           apps::mojom::AppType app_type,
                           const std::string& app_id,
                           apps::mojom::LaunchContainer container) {
  switch (app_type) {
    case apps::mojom::AppType::kUnknown:
      return AppTypeName::kUnknown;
    case apps::mojom::AppType::kArc:
      return AppTypeName::kArc;
    case apps::mojom::AppType::kBuiltIn:
      return AppTypeName::kBuiltIn;
    case apps::mojom::AppType::kCrostini:
      return AppTypeName::kCrostini;
    case apps::mojom::AppType::kExtension:
      return GetAppTypeNameForChromeApp(profile, app_id, container);
    case apps::mojom::AppType::kWeb:
      return GetAppTypeNameForWebApp(profile, app_id, container);
    case apps::mojom::AppType::kMacOs:
      return AppTypeName::kMacOs;
    case apps::mojom::AppType::kPluginVm:
      return AppTypeName::kPluginVm;
    case apps::mojom::AppType::kStandaloneBrowser:
      return AppTypeName::kStandaloneBrowser;
    case apps::mojom::AppType::kRemote:
      return AppTypeName::kRemote;
    case apps::mojom::AppType::kBorealis:
      return AppTypeName::kBorealis;
    case apps::mojom::AppType::kSystemWeb:
      return AppTypeName::kSystemWeb;
  }
}

// Records the number of times Chrome OS apps are launched grouped by the launch
// source.
void RecordAppLaunchSource(apps::mojom::LaunchSource launch_source) {
  base::UmaHistogramEnumeration("Apps.AppLaunchSource", launch_source);
}

// Records the number of times Chrome OS apps are launched grouped by the app
// type.
void RecordAppLaunchPerAppType(AppTypeName app_type_name) {
  if (app_type_name == AppTypeName::kUnknown) {
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

}  // namespace apps
