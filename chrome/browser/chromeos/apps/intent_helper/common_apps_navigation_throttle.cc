// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/apps/intent_helper/common_apps_navigation_throttle.h"

#include <utility>

#include "base/bind.h"
#include "base/stl_util.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/intent_helper/apps_navigation_types.h"
#include "chrome/browser/apps/intent_helper/intent_picker_auto_display_service.h"
#include "chrome/browser/chromeos/apps/metrics/intent_handling_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/intent_picker_tab_helper.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "ui/display/types/display_constants.h"

namespace {

apps::PickerEntryType GetPickerEntryType(apps::mojom::AppType app_type) {
  apps::PickerEntryType picker_entry_type = apps::PickerEntryType::kUnknown;
  switch (app_type) {
    case apps::mojom::AppType::kUnknown:
    case apps::mojom::AppType::kBuiltIn:
    case apps::mojom::AppType::kCrostini:
    case apps::mojom::AppType::kPluginVm:
    case apps::mojom::AppType::kExtension:
    case apps::mojom::AppType::kLacros:
    case apps::mojom::AppType::kRemote:
      break;
    case apps::mojom::AppType::kArc:
      picker_entry_type = apps::PickerEntryType::kArc;
      break;
    case apps::mojom::AppType::kWeb:
      picker_entry_type = apps::PickerEntryType::kWeb;
      break;
    case apps::mojom::AppType::kMacNative:
      picker_entry_type = apps::PickerEntryType::kMacNative;
      break;
  }
  return picker_entry_type;
}

}  // namespace

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

  IntentPickerTabHelper::LoadAppIcons(
      web_contents, std::move(apps_for_picker),
      base::BindOnce(&OnAppIconsLoaded, web_contents, ui_auto_display_service,
                     url));
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
  if (chromeos::switches::IsTabletFormFactor() && should_persist) {
    // On devices of tablet form factor, until the user has decided to persist
    // the setting, the browser-side intent picker should always be seen.
    auto platform = IntentPickerAutoDisplayPref::Platform::kNone;
    if (entry_type == apps::PickerEntryType::kArc) {
      platform = IntentPickerAutoDisplayPref::Platform::kArc;
    } else if (entry_type == apps::PickerEntryType::kUnknown &&
               close_reason == apps::IntentPickerCloseReason::STAY_IN_CHROME) {
      platform = IntentPickerAutoDisplayPref::Platform::kChrome;
    }
    IntentPickerAutoDisplayService::Get(
        Profile::FromBrowserContext(web_contents->GetBrowserContext()))
        ->UpdatePlatformForTablets(url, platform);
  }

  const bool should_launch_app =
      close_reason == apps::IntentPickerCloseReason::OPEN_APP;

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);

  // If the picker was closed without an app being chosen,
  // e.g. due to the tab being closed. Keep count of this scenario so we can
  // stop the UI from showing after 2+ dismissals.
  if (entry_type == PickerEntryType::kUnknown &&
      close_reason == IntentPickerCloseReason::DIALOG_DEACTIVATED) {
    if (ui_auto_display_service)
      ui_auto_display_service->IncrementCounter(url);
  }

  if (should_persist)
    proxy->AddPreferredApp(launch_name, url);

  if (should_launch_app) {
    if (entry_type == PickerEntryType::kWeb) {
      web_app::ReparentWebContentsIntoAppBrowser(web_contents, launch_name);
    } else {
      // TODO(crbug.com/853604): Distinguish the source from link and omnibox.
      apps::mojom::LaunchSource launch_source =
          apps::mojom::LaunchSource::kFromLink;
      proxy->LaunchAppWithUrl(
          launch_name,
          GetEventFlags(apps::mojom::LaunchContainer::kLaunchContainerWindow,
                        WindowOpenDisposition::NEW_WINDOW,
                        /*prefer_container=*/true),
          url, launch_source, display::kDefaultDisplayId);
      CloseOrGoBack(web_contents);
    }
  }

  apps::AppsNavigationThrottle::PickerAction action =
      apps::AppsNavigationThrottle::GetPickerAction(entry_type, close_reason,
                                                    should_persist);
  apps::AppsNavigationThrottle::Platform platform =
      apps::AppsNavigationThrottle::GetDestinationPlatform(launch_name, action);
  apps::IntentHandlingMetrics::RecordIntentPickerMetrics(
      apps::Source::kHttpOrHttps, should_persist, action, platform);
}

// static
void CommonAppsNavigationThrottle::OnAppIconsLoaded(
    content::WebContents* web_contents,
    IntentPickerAutoDisplayService* ui_auto_display_service,
    const GURL& url,
    std::vector<apps::IntentPickerAppInfo> apps) {
  apps::AppsNavigationThrottle::ShowIntentPickerBubbleForApps(
      web_contents, std::move(apps),
      /*show_stay_in_chrome=*/true,
      /*show_remember_selection=*/true,
      base::BindOnce(&OnIntentPickerClosed, web_contents,
                     ui_auto_display_service, url));
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

  std::vector<std::string> app_ids =
      proxy->GetAppIdsForUrl(url, /*exclude_browser=*/true);

  for (const std::string& app_id : app_ids) {
    proxy->AppRegistryCache().ForOneApp(
        app_id, [&apps](const apps::AppUpdate& update) {
          apps.emplace(apps.begin(), GetPickerEntryType(update.AppType()),
                       gfx::Image(), update.AppId(), update.Name());
        });
  }
  return apps;
}

apps::AppsNavigationThrottle::PickerShowState
CommonAppsNavigationThrottle::GetPickerShowState(
    const std::vector<apps::IntentPickerAppInfo>& apps_for_picker,
    content::WebContents* web_contents,
    const GURL& url) {
  return ShouldAutoDisplayUi(apps_for_picker, web_contents, url) &&
                 navigate_from_link()
             ? PickerShowState::kPopOut
             : PickerShowState::kOmnibox;
}

IntentPickerResponse CommonAppsNavigationThrottle::GetOnPickerClosedCallback(
    content::WebContents* web_contents,
    IntentPickerAutoDisplayService* ui_auto_display_service,
    const GURL& url) {
  return base::BindOnce(&OnIntentPickerClosed, web_contents,
                        ui_auto_display_service, url);
}

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
        return true;
      }
    }
  }
  return false;
}

bool CommonAppsNavigationThrottle::ShouldDeferNavigation(
    content::NavigationHandle* handle) {
  content::WebContents* web_contents = handle->GetWebContents();

  const GURL& url = handle->GetURL();

  std::vector<apps::IntentPickerAppInfo> apps_for_picker =
      FindAllAppsForUrl(web_contents, url, {});

  if (apps_for_picker.empty())
    return false;

  if (GetPickerShowState(apps_for_picker, web_contents, url) ==
      PickerShowState::kOmnibox) {
    return false;
  }

  IntentPickerTabHelper::LoadAppIcons(
      web_contents, std::move(apps_for_picker),
      base::BindOnce(
          &CommonAppsNavigationThrottle::OnDeferredNavigationProcessed,
          weak_factory_.GetWeakPtr()));
  return true;
}

void CommonAppsNavigationThrottle::OnDeferredNavigationProcessed(
    std::vector<apps::IntentPickerAppInfo> apps) {
  content::NavigationHandle* handle = navigation_handle();
  content::WebContents* web_contents = handle->GetWebContents();
  const GURL& url = handle->GetURL();

  ShowIntentPickerForApps(web_contents, ui_auto_display_service_, url,
                          std::move(apps),
                          base::BindOnce(&OnIntentPickerClosed, web_contents,
                                         ui_auto_display_service_, url));

  // We are about to resume the navigation, which may destroy this object.
  Resume();
}

bool CommonAppsNavigationThrottle::ShouldAutoDisplayUi(
    const std::vector<apps::IntentPickerAppInfo>& apps_for_picker,
    content::WebContents* web_contents,
    const GURL& url) {
  if (apps_for_picker.empty())
    return false;

  // On devices with tablet form factor we should not pop out the intent
  // picker if Chrome has been chosen by the user as the platform for this URL.
  if (chromeos::switches::IsTabletFormFactor()) {
    if (ui_auto_display_service_->GetLastUsedPlatformForTablets(url) ==
        IntentPickerAutoDisplayPref::Platform::kChrome) {
      return false;
    }
  }

  // If we only have PWAs in the app list, do not show the intent picker.
  // Instead just show the omnibox icon. This is to reduce annoyance to users
  // until "Remember my choice" is available for desktop PWAs.
  // TODO(crbug.com/826982): show the intent picker when the app registry is
  // available to persist "Remember my choice" for PWAs.
  if (!base::FeatureList::IsEnabled(features::kIntentPickerPWAPersistence) &&
      ContainsOnlyPwasAndMacApps(apps_for_picker)) {
    return false;
  }

  // If the preferred app is use browser, do not show the intent picker.
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);

  if (proxy) {
    auto preferred_app_id = proxy->PreferredApps().FindPreferredAppForUrl(url);
    if (preferred_app_id.has_value() &&
        preferred_app_id.value() == kUseBrowserForLink) {
      return false;
    }
  }

  DCHECK(ui_auto_display_service_);
  return ui_auto_display_service_->ShouldAutoDisplayUi(url);
}

}  // namespace apps
