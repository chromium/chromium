// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/chromeos_intent_picker_helpers.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/debug/dump_without_crashing.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/intent_helper/apps_navigation_types.h"
#include "chrome/browser/apps/intent_helper/intent_picker_auto_display_service.h"
#include "chrome/browser/apps/intent_helper/intent_picker_features.h"
#include "chrome/browser/apps/intent_helper/intent_picker_internal.h"
#include "chrome/browser/apps/intent_helper/metrics/intent_handling_metrics.h"
#include "chrome/browser/apps/intent_helper/supported_links_infobar_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/intent_picker_tab_helper.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "components/services/app_service/public/cpp/intent_constants.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/models/image_model.h"
#include "ui/display/types/display_constants.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace apps {

namespace {

bool ShouldAutoDisplayUi(
    const std::vector<IntentPickerAppInfo>& apps_for_picker,
    content::NavigationHandle* navigation_handle) {
  content::WebContents* web_contents = navigation_handle->GetWebContents();

  if (web_contents->GetVisibility() == content::Visibility::HIDDEN) {
    return false;
  }

  const GURL& url = navigation_handle->GetURL();

  // Disable Auto-display when the Intent Chip is enabled.
  if (features::LinkCapturingUiUpdateEnabled())
    return false;

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
  // TODO(crbug.com/1225828): Handle this for lacros-chrome as well.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (ash::switches::IsTabletFormFactor()) {
    if (ui_auto_display_service->GetLastUsedPlatformForTablets(url) ==
        IntentPickerAutoDisplayService::Platform::kChrome) {
      return false;
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // If the preferred app is use browser, do not show the intent picker.
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  auto* proxy = AppServiceProxyFactory::GetForProfile(profile);

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

void OnAppIconsLoaded(content::WebContents* web_contents,
                      IntentPickerAutoDisplayService* ui_auto_display_service,
                      const GURL& url,
                      std::vector<IntentPickerAppInfo> apps) {
  ShowIntentPickerBubbleForApps(
      web_contents, std::move(apps),
      /*show_stay_in_chrome=*/true,
      /*show_remember_selection=*/true,
      base::BindOnce(&OnIntentPickerClosedChromeOs, web_contents,
                     ui_auto_display_service, PickerShowState::kPopOut, url));
}

}  // namespace

void MaybeShowIntentPickerBubble(content::NavigationHandle* navigation_handle,
                                 std::vector<IntentPickerAppInfo> apps) {
  if (apps.empty() || GetPickerShowState(navigation_handle, apps) ==
                          PickerShowState::kOmnibox) {
    return;
  }

  IntentHandlingMetrics::RecordIntentPickerIconEvent(
      IntentHandlingMetrics::IntentPickerIconEvent::kAutoPopOut);

  content::WebContents* web_contents = navigation_handle->GetWebContents();
  IntentPickerAutoDisplayService* ui_auto_display_service =
      IntentPickerAutoDisplayService::Get(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  const GURL& url = navigation_handle->GetURL();

  IntentPickerTabHelper::LoadAppIcons(
      web_contents, std::move(apps),
      base::BindOnce(&OnAppIconsLoaded, web_contents, ui_auto_display_service,
                     url));
}

void OnIntentPickerClosedChromeOs(
    content::WebContents* web_contents,
    IntentPickerAutoDisplayService* ui_auto_display_service,
    PickerShowState show_state,
    const GURL& url,
    const std::string& launch_name,
    PickerEntryType entry_type,
    IntentPickerCloseReason close_reason,
    bool should_persist) {
// TODO(crbug.com/1225828): Handle this for lacros-chrome as well.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (ash::switches::IsTabletFormFactor() && should_persist) {
    // On devices of tablet form factor, until the user has decided to persist
    // the setting, the browser-side intent picker should always be seen.
    auto platform = IntentPickerAutoDisplayService::Platform::kNone;
    if (entry_type == PickerEntryType::kArc) {
      platform = IntentPickerAutoDisplayService::Platform::kArc;
    } else if (entry_type == PickerEntryType::kUnknown &&
               close_reason == IntentPickerCloseReason::STAY_IN_CHROME) {
      platform = IntentPickerAutoDisplayService::Platform::kChrome;
    }
    IntentPickerAutoDisplayService::Get(
        Profile::FromBrowserContext(web_contents->GetBrowserContext()))
        ->UpdatePlatformForTablets(url, platform);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  const bool should_launch_app =
      close_reason == IntentPickerCloseReason::OPEN_APP;

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  auto* proxy = AppServiceProxyFactory::GetForProfile(profile);

  // If the picker was closed without an app being chosen,
  // e.g. due to the tab being closed. Keep count of this scenario so we can
  // stop the UI from showing after 2+ dismissals.
  if (entry_type == PickerEntryType::kUnknown &&
      close_reason == IntentPickerCloseReason::DIALOG_DEACTIVATED &&
      ui_auto_display_service) {
    ui_auto_display_service->IncrementPickerUICounter(url);
  }

  if (should_persist) {
    DCHECK(!launch_name.empty());
    proxy->AddPreferredApp(launch_name, url);
  }

  if (should_launch_app) {
    LaunchAppFromIntentPickerChromeOs(web_contents, url, launch_name,
                                      entry_type);
  }

  IntentHandlingMetrics::RecordIntentPickerMetrics(entry_type, close_reason,
                                                   should_persist, show_state);
}

void LaunchAppFromIntentPickerChromeOs(content::WebContents* web_contents,
                                       const GURL& url,
                                       const std::string& launch_name,
                                       PickerEntryType app_type) {
  DCHECK(!launch_name.empty());
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  if (base::FeatureList::IsEnabled(features::kLinkCapturingUiUpdate)) {
    IntentPickerAutoDisplayService::Get(profile)->ResetIntentChipCounter(url);
  }

  auto* proxy = AppServiceProxyFactory::GetForProfile(profile);

  if (app_type == PickerEntryType::kWeb) {
    web_app::ReparentWebContentsIntoAppBrowser(web_contents, launch_name);

#if BUILDFLAG(IS_CHROMEOS_ASH)
    // TODO(crbug.com/1293173): Lacros support for the infobar UI.
    if (features::LinkCapturingInfoBarEnabled()) {
      SupportedLinksInfoBarDelegate::MaybeShowSupportedLinksInfoBar(
          web_contents, launch_name);
    }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
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

}  // namespace apps
