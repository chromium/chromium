// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/apps/intent_helper/ash_intent_picker_helpers.h"

#include <algorithm>
#include <utility>

#include "ash/constants/ash_switches.h"
#include "base/bind.h"
#include "base/debug/dump_without_crashing.h"
#include "base/stl_util.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/intent_helper/apps_navigation_types.h"
#include "chrome/browser/apps/intent_helper/intent_picker_auto_display_service.h"
#include "chrome/browser/apps/intent_helper/intent_picker_constants.h"
#include "chrome/browser/apps/intent_helper/intent_picker_internal.h"
#include "chrome/browser/ash/apps/metrics/intent_handling_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/intent_picker_tab_helper.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/models/image_model.h"
#include "ui/display/types/display_constants.h"

namespace apps {

namespace {

std::vector<IntentPickerAppInfo> FindAppsForUrl(
    content::WebContents* web_contents,
    const GURL& url,
    std::vector<IntentPickerAppInfo> apps) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  AppServiceProxyChromeOs* proxy =
      AppServiceProxyFactory::GetForProfile(profile);

  std::vector<std::string> app_ids =
      proxy->GetAppIdsForUrl(url, /*exclude_browser=*/true);

  for (const std::string& app_id : app_ids) {
    proxy->AppRegistryCache().ForOneApp(
        app_id, [&apps](const AppUpdate& update) {
          apps.emplace(apps.begin(), GetPickerEntryType(update.AppType()),
                       ui::ImageModel(), update.AppId(), update.Name());
        });
  }
  return apps;
}

bool ShouldAutoDisplayUi(
    const std::vector<IntentPickerAppInfo>& apps_for_picker,
    content::NavigationHandle* navigation_handle) {
  content::WebContents* web_contents = navigation_handle->GetWebContents();

  if (web_contents->GetVisibility() == content::Visibility::HIDDEN) {
    return false;
  }

  const GURL& url = navigation_handle->GetURL();

  if (apps_for_picker.empty())
    return false;

  if (InAppBrowser(web_contents))
    return false;

  if (!ShouldOverrideUrlLoading(GetStartingGURL(navigation_handle), url))
    return false;

  IntentPickerAutoDisplayService* ui_auto_display_service =
      IntentPickerAutoDisplayService::Get(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()));

  // On devices with tablet form factor we should not pop out the intent
  // picker if Chrome has been chosen by the user as the platform for this URL.
  if (chromeos::switches::IsTabletFormFactor()) {
    if (ui_auto_display_service->GetLastUsedPlatformForTablets(url) ==
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

  AppServiceProxyChromeOs* proxy =
      AppServiceProxyFactory::GetForProfile(profile);

  if (proxy) {
    auto preferred_app_id = proxy->PreferredApps().FindPreferredAppForUrl(url);
    if (preferred_app_id.has_value() &&
        preferred_app_id.value() == apps::kUseBrowserForLink) {
      return false;
    }
  }

  return ui_auto_display_service->ShouldAutoDisplayUi(url);
}

PickerShowState GetPickerShowState(
    content::NavigationHandle* navigation_handle,
    const std::vector<IntentPickerAppInfo>& apps_for_picker) {
  return ShouldAutoDisplayUi(apps_for_picker, navigation_handle) &&
                 IsNavigateFromLink(navigation_handle)
             ? PickerShowState::kPopOut
             : PickerShowState::kOmnibox;
}

void OnIntentPickerClosed(
    content::WebContents* web_contents,
    IntentPickerAutoDisplayService* ui_auto_display_service,
    const GURL& url,
    const std::string& launch_name,
    PickerEntryType entry_type,
    IntentPickerCloseReason close_reason,
    bool should_persist) {
  if (chromeos::switches::IsTabletFormFactor() && should_persist) {
    // On devices of tablet form factor, until the user has decided to persist
    // the setting, the browser-side intent picker should always be seen.
    auto platform = IntentPickerAutoDisplayPref::Platform::kNone;
    if (entry_type == PickerEntryType::kArc) {
      platform = IntentPickerAutoDisplayPref::Platform::kArc;
    } else if (entry_type == PickerEntryType::kUnknown &&
               close_reason == IntentPickerCloseReason::STAY_IN_CHROME) {
      platform = IntentPickerAutoDisplayPref::Platform::kChrome;
    }
    IntentPickerAutoDisplayService::Get(
        Profile::FromBrowserContext(web_contents->GetBrowserContext()))
        ->UpdatePlatformForTablets(url, platform);
  }

  const bool should_launch_app =
      close_reason == IntentPickerCloseReason::OPEN_APP;

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  AppServiceProxyChromeOs* proxy =
      AppServiceProxyFactory::GetForProfile(profile);

  // If the picker was closed without an app being chosen,
  // e.g. due to the tab being closed. Keep count of this scenario so we can
  // stop the UI from showing after 2+ dismissals.
  if (entry_type == PickerEntryType::kUnknown &&
      close_reason == IntentPickerCloseReason::DIALOG_DEACTIVATED) {
    if (ui_auto_display_service) {
      ui_auto_display_service->IncrementCounter(url);
    }
  }

  if (should_persist) {
    // TODO(https://crbug.com/853604): Remove this and convert to a DCHECK
    // after finding out the root cause.
    if (launch_name.empty()) {
      base::debug::DumpWithoutCrashing();
    } else {
      proxy->AddPreferredApp(launch_name, url);
    }
  }

  if (should_launch_app) {
    if (entry_type == PickerEntryType::kWeb) {
      web_app::ReparentWebContentsIntoAppBrowser(web_contents, launch_name);
    } else {
      // TODO(crbug.com/853604): Distinguish the source from link and omnibox.
      mojom::LaunchSource launch_source = mojom::LaunchSource::kFromLink;
      proxy->LaunchAppWithUrl(
          launch_name,
          GetEventFlags(mojom::LaunchContainer::kLaunchContainerWindow,
                        WindowOpenDisposition::NEW_WINDOW,
                        /*prefer_container=*/true),
          url, launch_source, apps::MakeWindowInfo(display::kDefaultDisplayId));
      CloseOrGoBack(web_contents);
    }
  }
  IntentHandlingMetrics::PickerAction action =
      IntentHandlingMetrics::GetPickerAction(entry_type, close_reason,
                                             should_persist);
  IntentHandlingMetrics::Platform platform =
      IntentHandlingMetrics::GetDestinationPlatform(launch_name, action);
  IntentHandlingMetrics::RecordIntentPickerMetrics(
      Source::kHttpOrHttps, should_persist, action, platform);
}

void OnAppIconsLoaded(content::WebContents* web_contents,
                      IntentPickerAutoDisplayService* ui_auto_display_service,
                      const GURL& url,
                      std::vector<IntentPickerAppInfo> apps) {
  ShowIntentPickerBubbleForApps(
      web_contents, std::move(apps),
      /*show_stay_in_chrome=*/true,
      /*show_remember_selection=*/true,
      base::BindOnce(&OnIntentPickerClosed, web_contents,
                     ui_auto_display_service, url));
}

}  // namespace

void MaybeShowIntentPickerAsh(content::NavigationHandle* navigation_handle) {
  content::WebContents* web_contents = navigation_handle->GetWebContents();
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  if (!AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile))
    return;

  const GURL& url = navigation_handle->GetURL();
  std::vector<IntentPickerAppInfo> apps = FindAppsForUrl(web_contents, url, {});

  if (apps.empty()) {
    IntentPickerTabHelper::SetShouldShowIcon(web_contents, false);
    return;
  }

  IntentPickerTabHelper::SetShouldShowIcon(web_contents, true);

  IntentPickerAutoDisplayService* ui_auto_display_service =
      IntentPickerAutoDisplayService::Get(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()));

  if (GetPickerShowState(navigation_handle, apps) ==
      PickerShowState::kOmnibox) {
    return;
  }

  IntentPickerTabHelper::LoadAppIcons(
      web_contents, std::move(apps),
      base::BindOnce(&OnAppIconsLoaded, web_contents, ui_auto_display_service,
                     url));
}

void ShowIntentPickerBubbleAsh(content::WebContents* web_contents,
                               const GURL& url) {
  std::vector<IntentPickerAppInfo> apps = FindAppsForUrl(web_contents, url, {});
  if (apps.empty())
    return;
  IntentPickerTabHelper::LoadAppIcons(
      web_contents, std::move(apps),
      base::BindOnce(&OnAppIconsLoaded, web_contents,
                     /*ui_auto_display_service=*/nullptr, url));
}

bool ContainsOnlyPwasAndMacApps(const std::vector<IntentPickerAppInfo>& apps) {
  return std::all_of(apps.begin(), apps.end(),
                     [](const IntentPickerAppInfo& app_info) {
                       return app_info.type == PickerEntryType::kWeb ||
                              app_info.type == PickerEntryType::kMacOs;
                     });
}

PickerEntryType GetPickerEntryType(mojom::AppType app_type) {
  PickerEntryType picker_entry_type = PickerEntryType::kUnknown;
  switch (app_type) {
    case mojom::AppType::kUnknown:
    case mojom::AppType::kBuiltIn:
    case mojom::AppType::kCrostini:
    case mojom::AppType::kPluginVm:
    case mojom::AppType::kExtension:
    case mojom::AppType::kLacros:
    case mojom::AppType::kRemote:
    case mojom::AppType::kBorealis:
      break;
    case mojom::AppType::kArc:
      picker_entry_type = PickerEntryType::kArc;
      break;
    case mojom::AppType::kWeb:
    case mojom::AppType::kSystemWeb:
      picker_entry_type = PickerEntryType::kWeb;
      break;
    case mojom::AppType::kMacOs:
      picker_entry_type = PickerEntryType::kMacOs;
      break;
  }
  return picker_entry_type;
}

}  // namespace apps
