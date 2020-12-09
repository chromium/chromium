// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/apps/intent_helper/common_apps_navigation_throttle.h"

#include <utility>

#include "base/containers/contains.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/intent_helper/apps_navigation_types.h"
#include "chrome/browser/apps/intent_helper/intent_picker_internal.h"
#include "chrome/browser/chromeos/apps/intent_helper/chromeos_intent_picker_helpers.h"
#include "chrome/browser/chromeos/apps/metrics/intent_handling_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
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

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  if (!AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile))
    return nullptr;

  if (!ShouldCheckAppsForUrl(web_contents))
    return nullptr;

  return std::make_unique<CommonAppsNavigationThrottle>(handle);
}

CommonAppsNavigationThrottle::CommonAppsNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : apps::AppsNavigationThrottle(navigation_handle) {}

CommonAppsNavigationThrottle::~CommonAppsNavigationThrottle() = default;

bool CommonAppsNavigationThrottle::ShouldCancelNavigation(
    content::NavigationHandle* handle) {
  content::WebContents* web_contents = handle->GetWebContents();

  const GURL& url = handle->GetURL();

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);

  std::vector<std::string> app_ids =
      proxy->GetAppIdsForUrl(url, /*exclude_browser=*/true);

  if (app_ids.empty())
    return false;

  if (navigate_from_link()) {
    auto preferred_app_id = proxy->PreferredApps().FindPreferredAppForUrl(url);

    if (preferred_app_id.has_value() &&
        base::Contains(app_ids, preferred_app_id.value())) {
      // Only automatically launch PWA if the flag is on.
      auto app_type =
          proxy->AppRegistryCache().GetAppType(preferred_app_id.value());
      if (app_type == apps::mojom::AppType::kArc ||
          (app_type == apps::mojom::AppType::kWeb &&
           base::FeatureList::IsEnabled(
               features::kIntentPickerPWAPersistence))) {
        auto launch_source = apps::mojom::LaunchSource::kFromLink;
        proxy->LaunchAppWithUrl(
            preferred_app_id.value(),
            GetEventFlags(apps::mojom::LaunchContainer::kLaunchContainerWindow,
                          WindowOpenDisposition::NEW_WINDOW,
                          /*prefer_container=*/true),
            url, launch_source, display::kDefaultDisplayId);
        CloseOrGoBack(web_contents);
        IntentHandlingMetrics::RecordIntentPickerUserInteractionMetrics(
            /*selected_app_package=*/preferred_app_id.value(),
            GetPickerEntryType(app_type),
            apps::IntentPickerCloseReason::PREFERRED_APP_FOUND,
            apps::Source::kHttpOrHttps, /*should_persist=*/false);
        return true;
      }
    }
  }
  return false;
}

}  // namespace apps
