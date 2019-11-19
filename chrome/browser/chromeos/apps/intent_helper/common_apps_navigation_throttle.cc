// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/apps/intent_helper/common_apps_navigation_throttle.h"

#include <utility>

#include "base/bind.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "ui/display/types/display_constants.h"

namespace apps {

// static
std::unique_ptr<apps::AppsNavigationThrottle>
CommonAppsNavigationThrottle::MaybeCreate(content::NavigationHandle* handle) {
  if (!handle->IsInMainFrame())
    return nullptr;

  content::WebContents* web_contents = handle->GetWebContents();

  if (!apps::AppsNavigationThrottle::CanCreate(web_contents))
    return nullptr;

  return std::make_unique<CommonAppsNavigationThrottle>(handle);
}

// static
void CommonAppsNavigationThrottle::ShowIntentPickerBubble(
    content::WebContents* web_contents,
    IntentPickerAutoDisplayService* ui_auto_display_service,
    const GURL& url) {
  std::vector<apps::IntentPickerAppInfo> apps_for_picker =
      FindAllAppsForUrl(web_contents, url, {});

  apps::AppsNavigationThrottle::ShowIntentPickerBubbleForApps(
      web_contents, std::move(apps_for_picker),
      /*show_stay_in_chrome=*/false,
      /*show_remember_selection=*/true,
      base::BindOnce(&OnIntentPickerClosed, web_contents,
                     ui_auto_display_service, url));
}

// static
void CommonAppsNavigationThrottle::OnIntentPickerClosed(
    content::WebContents* web_contents,
    IntentPickerAutoDisplayService* ui_auto_display_service,
    const GURL& url,
    const std::string& launch_name,
    apps::PickerEntryType entry_type,
    apps::IntentPickerCloseReason close_reason,
    bool should_persist) {
  const bool should_launch_app =
      close_reason == apps::IntentPickerCloseReason::OPEN_APP;

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);

  if (!proxy)
    return;

  if (should_persist)
    proxy->AddPreferredApp(launch_name, url);

  if (should_launch_app) {
    // TODO(crbug.com/853604): Distinguish the source from link and omnibox.
    apps::mojom::LaunchSource launch_source =
        apps::mojom::LaunchSource::kFromLink;
    proxy->LaunchAppWithUrl(launch_name, url, launch_source,
                            display::kDefaultDisplayId);
    CloseOrGoBack(web_contents);
  }
}

CommonAppsNavigationThrottle::CommonAppsNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : apps::AppsNavigationThrottle(navigation_handle) {}

CommonAppsNavigationThrottle::~CommonAppsNavigationThrottle() = default;

std::vector<apps::IntentPickerAppInfo>
CommonAppsNavigationThrottle::FindAppsForUrl(
    content::WebContents* web_contents,
    const GURL& url,
    std::vector<apps::IntentPickerAppInfo> apps) {
  return FindAllAppsForUrl(web_contents, url, std::move(apps));
}

// static
std::vector<apps::IntentPickerAppInfo>
CommonAppsNavigationThrottle::FindAllAppsForUrl(
    content::WebContents* web_contents,
    const GURL& url,
    std::vector<apps::IntentPickerAppInfo> apps) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);

  if (!proxy)
    return apps;

  std::vector<std::string> app_ids = proxy->GetAppIdsForUrl(url);
  auto* menu_manager =
      extensions::MenuManager::Get(web_contents->GetBrowserContext());
  auto preferred_app_id = proxy->PreferredApps().FindPreferredAppForUrl(url);

  for (const std::string app_id : app_ids) {
    proxy->AppRegistryCache().ForOneApp(
        app_id, [&apps, menu_manager,
                 &preferred_app_id](const apps::AppUpdate& update) {
          PickerEntryType type = PickerEntryType::kUnknown;
          switch (update.AppType()) {
            case apps::mojom::AppType::kUnknown:
            case apps::mojom::AppType::kBuiltIn:
            case apps::mojom::AppType::kCrostini:
            case apps::mojom::AppType::kExtension:
              break;
            case apps::mojom::AppType::kArc:
              type = PickerEntryType::kArc;
              break;
            case apps::mojom::AppType::kWeb:
              type = PickerEntryType::kWeb;
              break;
            default:
              NOTREACHED();
          }
          // TODO(crbug.com/853604): Automatically launch the app. At the moment
          // just mark the app as preferred to minimize the change to
          // AppsNavigationThrottle.
          std::string display_name = update.Name();
          if (update.AppId() == preferred_app_id)
            display_name = update.Name() + " (preferred)";
          // TODO(crbug.com/853604): Get icon from App Service.
          apps.emplace(apps.begin(), type,
                       menu_manager->GetIconForExtension(update.AppId()),
                       update.AppId(), display_name);
        });
  }

  return apps;
}

apps::AppsNavigationThrottle::PickerShowState
CommonAppsNavigationThrottle::GetPickerShowState(
    const std::vector<apps::IntentPickerAppInfo>& apps_for_picker,
    content::WebContents* web_contents,
    const GURL& url) {
  return PickerShowState::kOmnibox;
}

IntentPickerResponse CommonAppsNavigationThrottle::GetOnPickerClosedCallback(
    content::WebContents* web_contents,
    IntentPickerAutoDisplayService* ui_auto_display_service,
    const GURL& url) {
  return base::BindOnce(&OnIntentPickerClosed, web_contents,
                        ui_auto_display_service, url);
}
}  // namespace apps
