// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/apps/intent_helper/chromeos_apps_navigation_throttle.h"

#include <utility>

#include "base/bind.h"
#include "chrome/browser/apps/intent_helper/apps_navigation_types.h"
#include "chrome/browser/apps/intent_helper/intent_picker_auto_display_service.h"
#include "chrome/browser/chromeos/apps/metrics/intent_handling_metrics.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/arc/arc_web_contents_data.h"
#include "chrome/browser/chromeos/arc/intent_helper/arc_intent_picker_app_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "url/origin.h"

namespace chromeos {

// static
std::unique_ptr<apps::AppsNavigationThrottle>
ChromeOsAppsNavigationThrottle::MaybeCreate(content::NavigationHandle* handle) {
  if (!handle->IsInMainFrame())
    return nullptr;

  content::WebContents* web_contents = handle->GetWebContents();
  const bool arc_enabled = arc::IsArcPlayStoreEnabledForProfile(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  if (!arc_enabled && !apps::AppsNavigationThrottle::CanCreate(web_contents))
    return nullptr;

  return std::make_unique<ChromeOsAppsNavigationThrottle>(handle, arc_enabled);
}

// static
void ChromeOsAppsNavigationThrottle::ShowIntentPickerBubble(
    content::WebContents* web_contents,
    IntentPickerAutoDisplayService* ui_auto_display_service,
    const GURL& url) {
  arc::ArcIntentPickerAppFetcher::GetArcAppsForPicker(
      web_contents, url,
      base::BindOnce(&ChromeOsAppsNavigationThrottle::
                         FindPwaForUrlAndShowIntentPickerForApps,
                     web_contents, ui_auto_display_service, url));
}

// static
void ChromeOsAppsNavigationThrottle::OnIntentPickerClosed(
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
  switch (entry_type) {
    case apps::PickerEntryType::kArc:
      if (arc::ArcIntentPickerAppFetcher::MaybeLaunchOrPersistArcApp(
              url, launch_name, should_launch_app, should_persist)) {
        CloseOrGoBack(web_contents);
      } else {
        close_reason = apps::IntentPickerCloseReason::ERROR_AFTER_PICKER;
      }
      apps::IntentHandlingMetrics::RecordIntentPickerUserInteractionMetrics(
          launch_name, entry_type, close_reason, apps::Source::kHttpOrHttps,
          should_persist);
      return;
    case apps::PickerEntryType::kUnknown:
      // TODO(crbug.com/826982): This workaround can be removed when preferences
      // are no longer persisted within the ARC container, it was necessary
      // since chrome browser is neither a PWA or ARC app.
      if (close_reason == apps::IntentPickerCloseReason::STAY_IN_CHROME &&
          should_persist) {
        arc::ArcIntentPickerAppFetcher::MaybeLaunchOrPersistArcApp(
            url, arc::ArcIntentHelperBridge::kArcIntentHelperPackageName,
            /*should_launch_app=*/false,
            /*should_persist=*/true);
      }
      // Fall through to super class method to increment counter.
      break;
    case apps::PickerEntryType::kWeb:
    case apps::PickerEntryType::kDevice:
    case apps::PickerEntryType::kMacNative:
      break;
  }

  apps::AppsNavigationThrottle::PickerAction action =
      apps::AppsNavigationThrottle::GetPickerAction(entry_type, close_reason,
                                                    should_persist);
  apps::AppsNavigationThrottle::Platform platform =
      apps::AppsNavigationThrottle::GetDestinationPlatform(launch_name, action);
  apps::IntentHandlingMetrics::RecordIntentPickerMetrics(
      apps::Source::kHttpOrHttps, should_persist, action, platform);

  apps::AppsNavigationThrottle::OnIntentPickerClosed(
      web_contents, ui_auto_display_service, url, launch_name, entry_type,
      close_reason, should_persist);
}

ChromeOsAppsNavigationThrottle::ChromeOsAppsNavigationThrottle(
    content::NavigationHandle* navigation_handle,
    bool arc_enabled)
    : apps::AppsNavigationThrottle(navigation_handle),
      arc_enabled_(arc_enabled) {}

ChromeOsAppsNavigationThrottle::~ChromeOsAppsNavigationThrottle() = default;

// static
void ChromeOsAppsNavigationThrottle::FindPwaForUrlAndShowIntentPickerForApps(
    content::WebContents* web_contents,
    IntentPickerAutoDisplayService* ui_auto_display_service,
    const GURL& url,
    std::vector<apps::IntentPickerAppInfo> apps) {
  std::vector<apps::IntentPickerAppInfo> apps_for_picker =
      FindPwaForUrl(web_contents, url, std::move(apps));
  bool show_persistence_options = ShouldShowPersistenceOptions(apps_for_picker);
  apps::AppsNavigationThrottle::ShowIntentPickerBubbleForApps(
      web_contents, std::move(apps_for_picker),
      /*show_stay_in_chrome=*/show_persistence_options,
      /*show_remember_selection=*/show_persistence_options,
      base::BindOnce(&OnIntentPickerClosed, web_contents,
                     ui_auto_display_service, url));
}

// static
apps::AppsNavigationThrottle::Platform
ChromeOsAppsNavigationThrottle::GetDestinationPlatform(
    const std::string& selected_launch_name,
    PickerAction picker_action) {
  switch (picker_action) {
    case PickerAction::PREFERRED_ACTIVITY_FOUND:
      return selected_launch_name ==
                     apps::AppsNavigationThrottle::kUseBrowserForLink
                 ? Platform::CHROME
                 : Platform::ARC;
    case PickerAction::ARC_APP_PRESSED:
    case PickerAction::ARC_APP_PREFERRED_PRESSED:
    case PickerAction::PWA_APP_PRESSED:
    case PickerAction::ERROR_BEFORE_PICKER:
    case PickerAction::ERROR_AFTER_PICKER:
    case PickerAction::DIALOG_DEACTIVATED:
    case PickerAction::CHROME_PRESSED:
    case PickerAction::CHROME_PREFERRED_PRESSED:
    case PickerAction::OBSOLETE_ALWAYS_PRESSED:
    case PickerAction::OBSOLETE_JUST_ONCE_PRESSED:
    case PickerAction::INVALID:
    case PickerAction::DEVICE_PRESSED:
    case PickerAction::MAC_NATIVE_APP_PRESSED:
      break;
  }
  return apps::AppsNavigationThrottle::GetDestinationPlatform(
      selected_launch_name, picker_action);
}

// Removes the flag signaling that the current tab was started via
// ChromeShellDelegate if the current throttle corresponds to a navigation
// passing thru different domains or schemes, except if |current_url| has a
// scheme different than http(s).
void ChromeOsAppsNavigationThrottle::MaybeRemoveComingFromArcFlag(
    content::WebContents* web_contents,
    const GURL& previous_url,
    const GURL& current_url) {
  // Let ArcExternalProtocolDialog handle these cases.
  if (!current_url.SchemeIsHTTPOrHTTPS())
    return;

  if (url::Origin::Create(previous_url)
          .IsSameOriginWith(url::Origin::Create(current_url))) {
    return;
  }

  const char* key =
      arc::ArcWebContentsData::ArcWebContentsData::kArcTransitionFlag;
  arc::ArcWebContentsData* arc_data =
      static_cast<arc::ArcWebContentsData*>(web_contents->GetUserData(key));
  if (arc_data)
    web_contents->RemoveUserData(key);
}

void ChromeOsAppsNavigationThrottle::CancelNavigation() {
  content::WebContents* web_contents = navigation_handle()->GetWebContents();
  if (web_contents && web_contents->GetController().IsInitialNavigation()) {
    // Workaround for b/79167225, closing |web_contents| here may be dangerous.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&ChromeOsAppsNavigationThrottle::CloseTab,
                                  weak_factory_.GetWeakPtr()));
  } else {
    CancelDeferredNavigation(content::NavigationThrottle::CANCEL_AND_IGNORE);
  }
}

bool ChromeOsAppsNavigationThrottle::ShouldDeferNavigation(
    content::NavigationHandle* handle) {
  // Query for ARC apps, and if we are handling a link navigation, allow the
  // preferred app (if it exists) to be launched unless we are on a device
  // of tablet form factor, which will only launch the app if the user has
  // explicitly set that app as preferred and persisted that setting via the
  // intent picker previously.
  if (arc_enabled_ &&
      arc::ArcIntentPickerAppFetcher::WillGetArcAppsForNavigation(
          handle,
          base::BindOnce(
              &ChromeOsAppsNavigationThrottle::OnDeferredNavigationProcessed,
              weak_factory_.GetWeakPtr()),
          ShouldLaunchPreferredApp(handle->GetURL()))) {
    return true;
  }
  return false;
}

void ChromeOsAppsNavigationThrottle::OnDeferredNavigationProcessed(
    apps::AppsNavigationAction action,
    std::vector<apps::IntentPickerAppInfo> apps) {
  if (action == apps::AppsNavigationAction::CANCEL) {
    // We found a preferred ARC app to open; cancel the navigation and don't do
    // anything else.
    CancelNavigation();
    return;
  }

  content::NavigationHandle* handle = navigation_handle();
  content::WebContents* web_contents = handle->GetWebContents();
  const GURL& url = handle->GetURL();

  std::vector<apps::IntentPickerAppInfo> apps_for_picker =
      FindPwaForUrl(web_contents, url, std::move(apps));

  ShowIntentPickerForApps(
      web_contents, ui_auto_display_service_, url, std::move(apps_for_picker),
      GetOnPickerClosedCallback(web_contents, ui_auto_display_service_, url));

  // We are about to resume the navigation, which may destroy this object.
  Resume();
}

apps::AppsNavigationThrottle::PickerShowState
ChromeOsAppsNavigationThrottle::GetPickerShowState(
    const std::vector<apps::IntentPickerAppInfo>& apps_for_picker,
    content::WebContents* web_contents,
    const GURL& url) {
  // On devices with tablet form factor we should not pop out the intent
  // picker if Chrome has been chosen by the user as the platform for this URL.
  if (chromeos::switches::IsTabletFormFactor()) {
    if (ui_auto_display_service_->GetLastUsedPlatformForTablets(url) ==
        IntentPickerAutoDisplayPref::Platform::kChrome) {
      return PickerShowState::kOmnibox;
    }
  }
  return ShouldAutoDisplayUi(apps_for_picker, web_contents, url) &&
                 navigate_from_link()
             ? PickerShowState::kPopOut
             : PickerShowState::kOmnibox;
}

IntentPickerResponse ChromeOsAppsNavigationThrottle::GetOnPickerClosedCallback(
    content::WebContents* web_contents,
    IntentPickerAutoDisplayService* ui_auto_display_service,
    const GURL& url) {
  return base::BindOnce(&OnIntentPickerClosed, web_contents,
                        ui_auto_display_service, url);
}

void ChromeOsAppsNavigationThrottle::CloseTab() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::WebContents* web_contents = navigation_handle()->GetWebContents();
  if (web_contents)
    web_contents->ClosePage();
}

bool ChromeOsAppsNavigationThrottle::ShouldAutoDisplayUi(
    const std::vector<apps::IntentPickerAppInfo>& apps_for_picker,
    content::WebContents* web_contents,
    const GURL& url) {
  // If we only have PWAs in the app list, do not show the intent picker.
  // Instead just show the omnibox icon. This is to reduce annoyance to users
  // until "Remember my choice" is available for desktop PWAs.
  // TODO(crbug.com/826982): show the intent picker when the app registry is
  // available to persist "Remember my choice" for PWAs.
  if (ContainsOnlyPwasAndMacApps(apps_for_picker))
    return false;

  return AppsNavigationThrottle::ShouldAutoDisplayUi(apps_for_picker,
                                                     web_contents, url);
}

bool ChromeOsAppsNavigationThrottle::ShouldLaunchPreferredApp(const GURL& url) {
  DCHECK(ui_auto_display_service_);
  // Devices of tablet form factor should only launch a preferred app
  // from Chrome if it has been explicitly set and persisted by the user in the
  // intent picker previously.
  if (chromeos::switches::IsTabletFormFactor() &&
      ui_auto_display_service_->GetLastUsedPlatformForTablets(url) !=
          IntentPickerAutoDisplayPref::Platform::kArc) {
    return false;
  }
  return navigate_from_link();
}

}  // namespace chromeos
