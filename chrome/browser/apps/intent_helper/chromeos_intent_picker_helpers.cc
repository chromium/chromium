// Copyright 2020 The Chromium Authors
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
#include "chrome/browser/apps/intent_helper/intent_picker_auto_display_prefs.h"
#include "chrome/browser/apps/intent_helper/intent_picker_constants.h"
#include "chrome/browser/apps/intent_helper/intent_picker_features.h"
#include "chrome/browser/apps/intent_helper/intent_picker_internal.h"
#include "chrome/browser/apps/intent_helper/metrics/intent_handling_metrics.h"
#include "chrome/browser/apps/intent_helper/supported_links_infobar_delegate.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/intent_picker_tab_helper.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/web_applications/chromeos_web_app_experiments.h"
#include "chrome/common/chrome_features.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"
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
    NavigationInfo navigation_info) {
  content::WebContents* web_contents = navigation_info.web_contents;

  if (web_contents->GetVisibility() == content::Visibility::HIDDEN) {
    return false;
  }

  const GURL& url = navigation_info.url;

  // Disable Auto-display in the new Intent Picker UI unless it is specifically
  // re-enabled.
  if (!features::IntentPickerAutoDisplayEnabled())
    return false;

  if (apps_for_picker.empty())
    return false;

  if (InAppBrowser(web_contents))
    return false;

  if (!ShouldOverrideUrlLoading(navigation_info.starting_url, url))
    return false;

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  // On devices with tablet form factor we should not pop out the intent
  // picker if Chrome has been chosen by the user as the platform for this URL.
  // TODO(crbug.com/1225828): Handle this for lacros-chrome as well.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (ash::switches::IsTabletFormFactor()) {
    if (IntentPickerAutoDisplayPrefs::GetLastUsedPlatformForTablets(
            profile, url) == IntentPickerAutoDisplayPrefs::Platform::kChrome) {
      return false;
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // If the preferred app is use browser, do not show the intent picker.
  auto* proxy = AppServiceProxyFactory::GetForProfile(profile);

  if (proxy) {
    auto preferred_app_id =
        proxy->PreferredAppsList().FindPreferredAppForUrl(url);
    if (preferred_app_id.has_value() &&
        preferred_app_id.value() == apps_util::kUseBrowserForLink) {
      return false;
    }
  }

  return IntentPickerAutoDisplayPrefs::ShouldAutoDisplayUi(profile, url);
}

PickerShowState GetPickerShowState(
    NavigationInfo navigation_info,
    const std::vector<IntentPickerAppInfo>& apps_for_picker) {
  return ShouldAutoDisplayUi(apps_for_picker, navigation_info) &&
                 navigation_info.is_navigate_from_link
             ? PickerShowState::kPopOut
             : PickerShowState::kOmnibox;
}

void OnAppIconsLoaded(content::WebContents* web_contents,
                      const GURL& url,
                      std::vector<IntentPickerAppInfo> apps) {
  ShowIntentPickerBubbleForApps(
      web_contents, std::move(apps),
      /*show_stay_in_chrome=*/true,
      /*show_remember_selection=*/true,
      base::BindOnce(&OnIntentPickerClosedChromeOs, web_contents->GetWeakPtr(),
                     PickerShowState::kPopOut, url));
}

}  // namespace

void MaybeShowIntentPickerBubble(NavigationInfo navigation_info,
                                 std::vector<IntentPickerAppInfo> apps) {
  if (apps.empty() ||
      GetPickerShowState(navigation_info, apps) == PickerShowState::kOmnibox) {
    return;
  }

  IntentHandlingMetrics::RecordIntentPickerIconEvent(
      IntentHandlingMetrics::IntentPickerIconEvent::kAutoPopOut);

  content::WebContents* web_contents = navigation_info.web_contents;
  const GURL& url = navigation_info.url;

  IntentPickerTabHelper::LoadAppIcons(
      web_contents, std::move(apps),
      base::BindOnce(&OnAppIconsLoaded, web_contents, url));
}

void OnIntentPickerClosedChromeOs(
    base::WeakPtr<content::WebContents> web_contents,
    PickerShowState show_state,
    const GURL& url,
    const std::string& launch_name,
    PickerEntryType entry_type,
    IntentPickerCloseReason close_reason,
    bool should_persist) {
  if (!web_contents) {
    return;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
// TODO(crbug.com/1225828): Handle this for lacros-chrome as well.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (ash::switches::IsTabletFormFactor() && should_persist) {
    // On devices of tablet form factor, until the user has decided to persist
    // the setting, the browser-side intent picker should always be seen.
    auto platform = IntentPickerAutoDisplayPrefs::Platform::kNone;
    if (entry_type == PickerEntryType::kArc) {
      platform = IntentPickerAutoDisplayPrefs::Platform::kArc;
    } else if (entry_type == PickerEntryType::kUnknown &&
               close_reason == IntentPickerCloseReason::STAY_IN_CHROME) {
      platform = IntentPickerAutoDisplayPrefs::Platform::kChrome;
    }
    IntentPickerAutoDisplayPrefs::UpdatePlatformForTablets(profile, url,
                                                           platform);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  const bool should_launch_app =
      close_reason == IntentPickerCloseReason::OPEN_APP;

  // If the picker was closed without an app being chosen,
  // e.g. due to the tab being closed. Keep count of this scenario so we can
  // stop the UI from showing after 2+ dismissals.
  if (entry_type == PickerEntryType::kUnknown &&
      close_reason == IntentPickerCloseReason::DIALOG_DEACTIVATED &&
      show_state == PickerShowState::kPopOut) {
    IntentPickerAutoDisplayPrefs::IncrementPickerUICounter(profile, url);
  }

  if (should_persist) {
    DCHECK(!launch_name.empty());
    auto* proxy = AppServiceProxyFactory::GetForProfile(profile);
    DCHECK(proxy);
    proxy->AddPreferredApp(launch_name, url);
    apps::IntentHandlingMetrics::RecordLinkCapturingEvent(
        entry_type,
        apps::IntentHandlingMetrics::LinkCapturingEvent::kSettingsChanged);
  }

  if (should_launch_app) {
    LaunchAppFromIntentPickerChromeOs(web_contents.get(), url, launch_name,
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
    chrome::FindBrowserWithWebContents(web_contents)->window()
        ->NotifyFeatureEngagementEvent(kIntentChipOpensAppEvent);
    IntentPickerAutoDisplayPrefs::ResetIntentChipCounter(profile, url);
  }

  apps::IntentHandlingMetrics::RecordLinkCapturingEvent(
      app_type, apps::IntentHandlingMetrics::LinkCapturingEvent::kAppOpened);

  if (app_type == PickerEntryType::kWeb) {
    web_app::ReparentWebContentsIntoAppBrowser(web_contents, launch_name);

    if (features::LinkCapturingInfoBarEnabled()) {
      SupportedLinksInfoBarDelegate::MaybeShowSupportedLinksInfoBar(
          web_contents, launch_name);
    }
  } else {
    auto* proxy = AppServiceProxyFactory::GetForProfile(profile);

    // TODO(crbug.com/853604): Distinguish the source from link and omnibox.
    proxy->LaunchAppWithUrl(
        launch_name,
        GetEventFlags(WindowOpenDisposition::NEW_WINDOW,
                      /*prefer_container=*/true),
        url, LaunchSource::kFromLink,
        std::make_unique<WindowInfo>(display::kDefaultDisplayId));
    CloseOrGoBack(web_contents);
  }
}

bool ShouldOverrideUrlLoadingForOfficeExperiment(const GURL& previous_url,
                                                 const GURL& current_url) {
  if (base::FeatureList::IsEnabled(
          ::features::kMicrosoftOfficeWebAppExperiment)) {
    if (web_app::ChromeOsWebAppExperiments::ShouldOverrideUrlLoading(
            previous_url, current_url)) {
      return true;
    }
  }
  return false;
}

}  // namespace apps
