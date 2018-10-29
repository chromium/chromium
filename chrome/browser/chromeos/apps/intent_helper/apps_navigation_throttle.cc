// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/apps/intent_helper/apps_navigation_throttle.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "chrome/browser/apps/foundation/app_service/public/mojom/types.mojom.h"
#include "chrome/browser/chromeos/apps/intent_helper/apps_navigation_types.h"
#include "chrome/browser/chromeos/apps/intent_helper/page_transition_util.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/arc/arc_web_contents_data.h"
#include "chrome/browser/chromeos/arc/intent_helper/arc_navigation_throttle.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/common/chrome_features.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/arc/metrics/arc_metrics_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "url/origin.h"

namespace {

constexpr char kGoogleCom[] = "google.com";

// Removes the flag signaling that the current tab was started via
// ChromeShellDelegate if the current throttle corresponds to a navigation
// passing thru different domains or schemes, except if |current_url| has a
// scheme different than http(s).
void MaybeRemoveComingFromArcFlag(content::WebContents* web_contents,
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

// Compares the host name of the referrer and target URL to decide whether
// the navigation needs to be overridden.
bool ShouldOverrideUrlLoading(const GURL& previous_url,
                              const GURL& current_url) {
  // When the navigation is initiated in a web page where sending a referrer
  // is disabled, |previous_url| can be empty. In this case, we should open
  // it in the desktop browser.
  if (!previous_url.is_valid() || previous_url.is_empty())
    return false;

  // Also check |current_url| just in case.
  if (!current_url.is_valid() || current_url.is_empty()) {
    DVLOG(1) << "Unexpected URL: " << current_url << ", opening it in Chrome.";
    return false;
  }

  // Check the scheme for both |previous_url| and |current_url| since an
  // extension could have referred us (e.g. Google Docs).
  if (!current_url.SchemeIsHTTPOrHTTPS() ||
      !previous_url.SchemeIsHTTPOrHTTPS()) {
    return false;
  }

  // TODO(dominickn): this was added as a special case for ARC. Reconsider if
  // it's necessary for all app platforms.
  if (net::registry_controlled_domains::SameDomainOrHost(
          current_url, previous_url,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
    if (net::registry_controlled_domains::GetDomainAndRegistry(
            current_url,
            net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES) ==
        kGoogleCom) {
      // Navigation within the google.com domain are good candidates for this
      // throttle (and consecuently the picker UI) only if they have different
      // hosts, this is because multiple services are hosted within the same
      // domain e.g. play.google.com, mail.google.com and so on.
      return current_url.host_piece() != previous_url.host_piece();
    }

    return false;
  }
  return true;
}

GURL GetStartingGURL(content::NavigationHandle* navigation_handle) {
  // This helps us determine a reference GURL for the current NavigationHandle.
  // This is the order or preferrence: Referrer > LastCommittedURL > SiteURL,
  // GetSiteURL *should* only be used on very rare cases, e.g. when the
  // navigation goes from https: to http: on a new tab, thus losing the other
  // potential referrers.
  const GURL referrer_url = navigation_handle->GetReferrer().url;
  if (referrer_url.is_valid() && !referrer_url.is_empty())
    return referrer_url;

  const GURL last_committed_url =
      navigation_handle->GetWebContents()->GetLastCommittedURL();
  if (last_committed_url.is_valid() && !last_committed_url.is_empty())
    return last_committed_url;

  return navigation_handle->GetStartingSiteInstance()->GetSiteURL();
}

bool IsDesktopPwasEnabled() {
  return base::FeatureList::IsEnabled(features::kDesktopPWAWindowing);
}

}  // namespace

namespace chromeos {

// static
std::unique_ptr<content::NavigationThrottle>
AppsNavigationThrottle::MaybeCreate(content::NavigationHandle* handle) {
  content::WebContents* web_contents = handle->GetWebContents();
  const bool arc_enabled = arc::IsArcPlayStoreEnabledForProfile(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()));

  // Do not create the throttle if no apps can be installed.
  if (!arc_enabled && !IsDesktopPwasEnabled())
    return nullptr;

  // Do not create the throttle in incognito or for a prerender navigation.
  if (web_contents->GetBrowserContext()->IsOffTheRecord() ||
      prerender::PrerenderContents::FromWebContents(web_contents) != nullptr) {
    return nullptr;
  }

  // Do not create the throttle if there is no browser for the WebContents or we
  // are already in an app browser. The former can happen if an initial
  // navigation is reparented into a new app browser instance.
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser || browser->is_app())
    return nullptr;

  return std::make_unique<AppsNavigationThrottle>(handle, arc_enabled);
}

// static
void AppsNavigationThrottle::ShowIntentPickerBubble(
    content::WebContents* web_contents,
    const GURL& url) {
  arc::ArcNavigationThrottle::GetArcAppsForPicker(
      web_contents, url,
      base::BindOnce(
          &AppsNavigationThrottle::FindPwaForUrlAndShowIntentPickerForApps,
          web_contents, url));
}

// static
void AppsNavigationThrottle::OnIntentPickerClosed(
    content::WebContents* web_contents,
    const GURL& url,
    const std::string& launch_name,
    apps::mojom::AppType app_type,
    IntentPickerCloseReason close_reason,
    bool should_persist) {
  const bool should_launch_app =
      close_reason == IntentPickerCloseReason::OPEN_APP;
  switch (app_type) {
    case apps::mojom::AppType::kWeb:
      if (should_launch_app) {
        const extensions::Extension* extension =
            extensions::ExtensionRegistry::Get(
                web_contents->GetBrowserContext())
                ->GetExtensionById(launch_name,
                                   extensions::ExtensionRegistry::ENABLED);
        DCHECK(extension);
        ReparentWebContentsIntoAppBrowser(web_contents, extension);
      }
      break;
    case apps::mojom::AppType::kArc:
      if (arc::ArcNavigationThrottle::MaybeLaunchOrPersistArcApp(
              url, launch_name, should_launch_app, should_persist)) {
        CloseOrGoBack(web_contents);
      } else {
        close_reason = IntentPickerCloseReason::ERROR;
      }
      break;
    case apps::mojom::AppType::kUnknown:
      // TODO(crbug.com/826982): This workaround can be removed when preferences
      // are no longer persisted within the ARC container, it was necessary
      // since chrome browser is neither a PWA or ARC app.
      if (close_reason == chromeos::IntentPickerCloseReason::STAY_IN_CHROME &&
          should_persist) {
        arc::ArcNavigationThrottle::MaybeLaunchOrPersistArcApp(
            url, launch_name, /*should_launch_app=*/false,
            /*should_persist=*/true);
      }
      // We reach here if the picker was closed without an app being chosen,
      // e.g. due to the tab being closed. We don't want to do anything.
      break;
    case apps::mojom::AppType::kCrostini:
      NOTREACHED();
  }
  RecordUma(launch_name, app_type, close_reason, should_persist);
}

// static
void AppsNavigationThrottle::RecordUma(const std::string& selected_app_package,
                                       apps::mojom::AppType app_type,
                                       IntentPickerCloseReason close_reason,
                                       bool should_persist) {
  PickerAction action = GetPickerAction(app_type, close_reason, should_persist);
  Platform platform = GetDestinationPlatform(selected_app_package, action);

  UMA_HISTOGRAM_ENUMERATION("ChromeOS.Apps.IntentPickerAction", action);

  UMA_HISTOGRAM_ENUMERATION("ChromeOS.Apps.IntentPickerDestinationPlatform",
                            platform);

  if (app_type == apps::mojom::AppType::kArc &&
      (close_reason == IntentPickerCloseReason::PREFERRED_APP_FOUND ||
       close_reason == IntentPickerCloseReason::OPEN_APP)) {
    UMA_HISTOGRAM_ENUMERATION("Arc.UserInteraction",
                              arc::UserInteractionType::APP_STARTED_FROM_LINK);
  }
}

// static
bool AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
    const GURL& previous_url,
    const GURL& current_url) {
  return ShouldOverrideUrlLoading(previous_url, current_url);
}

AppsNavigationThrottle::AppsNavigationThrottle(
    content::NavigationHandle* navigation_handle,
    bool arc_enabled)
    : content::NavigationThrottle(navigation_handle),
      arc_enabled_(arc_enabled),
      ui_displayed_(false),
      weak_factory_(this) {}

AppsNavigationThrottle::~AppsNavigationThrottle() = default;

const char* AppsNavigationThrottle::GetNameForLogging() {
  return "AppsNavigationThrottle";
}

content::NavigationThrottle::ThrottleCheckResult
AppsNavigationThrottle::WillStartRequest() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  starting_url_ = GetStartingGURL(navigation_handle());
  Browser* browser =
      chrome::FindBrowserWithWebContents(navigation_handle()->GetWebContents());
  if (browser)
    browser->window()->SetIntentPickerViewVisibility(/*visible=*/false);
  return HandleRequest();
}

content::NavigationThrottle::ThrottleCheckResult
AppsNavigationThrottle::WillRedirectRequest() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // TODO(dominickn): Consider what to do when there is another URL during the
  // same navigation that could be handled by apps. Two ideas are:
  //  1) update the bubble with a mix of both app candidates (if different)
  //  2) show a bubble based on the last url, thus closing all the previous ones
  if (ui_displayed_)
    return content::NavigationThrottle::PROCEED;

  return HandleRequest();
}

// static
AppsNavigationThrottle::Platform AppsNavigationThrottle::GetDestinationPlatform(
    const std::string& selected_launch_name,
    PickerAction picker_action) {
  switch (picker_action) {
    case PickerAction::ARC_APP_PRESSED:
    case PickerAction::ARC_APP_PREFERRED_PRESSED:
      return Platform::ARC;
    case PickerAction::PWA_APP_PRESSED:
      return Platform::PWA;
    case PickerAction::ERROR:
    case PickerAction::DIALOG_DEACTIVATED:
    case PickerAction::CHROME_PRESSED:
    case PickerAction::CHROME_PREFERRED_PRESSED:
      return Platform::CHROME;
    case PickerAction::PREFERRED_ACTIVITY_FOUND:
      return arc::ArcIntentHelperBridge::IsIntentHelperPackage(
                 selected_launch_name)
                 ? Platform::CHROME
                 : Platform::ARC;
    case PickerAction::OBSOLETE_ALWAYS_PRESSED:
    case PickerAction::OBSOLETE_JUST_ONCE_PRESSED:
    case PickerAction::INVALID:
      NOTREACHED();
  }
  NOTREACHED();
  return Platform::ARC;
}

// static
AppsNavigationThrottle::PickerAction AppsNavigationThrottle::GetPickerAction(
    apps::mojom::AppType app_type,
    IntentPickerCloseReason close_reason,
    bool should_persist) {
  switch (close_reason) {
    case IntentPickerCloseReason::ERROR:
      return PickerAction::ERROR;
    case IntentPickerCloseReason::DIALOG_DEACTIVATED:
      return PickerAction::DIALOG_DEACTIVATED;
    case IntentPickerCloseReason::PREFERRED_APP_FOUND:
      return PickerAction::PREFERRED_ACTIVITY_FOUND;
    case IntentPickerCloseReason::STAY_IN_CHROME:
      return should_persist ? PickerAction::CHROME_PREFERRED_PRESSED
                            : PickerAction::CHROME_PRESSED;
    case IntentPickerCloseReason::OPEN_APP:
      switch (app_type) {
        case apps::mojom::AppType::kUnknown:
          return PickerAction::INVALID;
        case apps::mojom::AppType::kArc:
          return should_persist ? PickerAction::ARC_APP_PREFERRED_PRESSED
                                : PickerAction::ARC_APP_PRESSED;
        case apps::mojom::AppType::kWeb:
          return PickerAction::PWA_APP_PRESSED;
        case apps::mojom::AppType::kCrostini:
          NOTREACHED();
      }
  }

  NOTREACHED();
  return PickerAction::INVALID;
}

// static
void AppsNavigationThrottle::FindPwaForUrlAndShowIntentPickerForApps(
    content::WebContents* web_contents,
    const GURL& url,
    std::vector<IntentPickerAppInfo> apps) {
  std::vector<IntentPickerAppInfo> apps_for_picker =
      FindPwaForUrl(web_contents, url, std::move(apps));

  ShowIntentPickerBubbleForApps(web_contents, url, std::move(apps_for_picker));
}

// static
std::vector<IntentPickerAppInfo> AppsNavigationThrottle::FindPwaForUrl(
    content::WebContents* web_contents,
    const GURL& url,
    std::vector<IntentPickerAppInfo> apps) {
  if (IsDesktopPwasEnabled()) {
    // Check if the current URL has an installed desktop PWA, and add that to
    // the list of apps if it exists.
    const extensions::Extension* extension =
        extensions::util::GetInstalledPwaForUrl(
            web_contents->GetBrowserContext(), url,
            extensions::LAUNCH_CONTAINER_WINDOW);

    if (extension) {
      auto* menu_manager =
          extensions::MenuManager::Get(web_contents->GetBrowserContext());

      // Prefer the web and place apps of type PWA before apps of type ARC.
      // TODO(crbug.com/824598): deterministically sort this list.
      apps.emplace(apps.begin(), apps::mojom::AppType::kWeb,
                   menu_manager->GetIconForExtension(extension->id()),
                   extension->id(), extension->name());
    }
  }
  return apps;
}

// static
void AppsNavigationThrottle::ShowIntentPickerBubbleForApps(
    content::WebContents* web_contents,
    const GURL& url,
    std::vector<IntentPickerAppInfo> apps) {
  if (apps.empty())
    return;

  // It should be safe to bind |web_contents| since closing the current tab will
  // close the intent picker and run the callback prior to the WebContents being
  // deallocated.
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser)
    return;

  browser->window()->ShowIntentPickerBubble(
      std::move(apps),
      /*disable_stay_in_chrome=*/false,
      base::BindOnce(&AppsNavigationThrottle::OnIntentPickerClosed,
                     web_contents, url));
}

// static
void AppsNavigationThrottle::CloseOrGoBack(content::WebContents* web_contents) {
  DCHECK(web_contents);
  if (web_contents->GetController().CanGoBack())
    web_contents->GetController().GoBack();
  else
    web_contents->ClosePage();
}

void AppsNavigationThrottle::CancelNavigation() {
  content::WebContents* web_contents = navigation_handle()->GetWebContents();
  if (web_contents && web_contents->GetController().IsInitialNavigation()) {
    // Workaround for b/79167225, closing |web_contents| here may be dangerous.
    base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::UI},
                             base::BindOnce(&AppsNavigationThrottle::CloseTab,
                                            weak_factory_.GetWeakPtr()));
  } else {
    CancelDeferredNavigation(content::NavigationThrottle::CANCEL_AND_IGNORE);
  }
}

void AppsNavigationThrottle::OnDeferredNavigationProcessed(
    AppsNavigationAction action,
    std::vector<IntentPickerAppInfo> apps) {
  if (action == AppsNavigationAction::CANCEL) {
    // We found a preferred ARC app to open; cancel the navigation and don't do
    // anything else.
    CancelNavigation();
    return;
  }

  content::NavigationHandle* handle = navigation_handle();
  content::WebContents* web_contents = handle->GetWebContents();
  const GURL& url = handle->GetURL();

  std::vector<IntentPickerAppInfo> apps_for_picker =
      FindPwaForUrl(web_contents, url, std::move(apps));

  // We will not show the UI if the apps list is empty.
  if (apps_for_picker.empty())
    ui_displayed_ = false;

  // If we only have PWAs in the app list, do not show the intent picker.
  // Instead just show the omnibox icon. This is to reduce annoyance to users
  // until "Remember my choice" is available for desktop PWAs.
  // TODO(crbug.com/826982): show the intent picker when the app registry is
  // available to persist "Remember my choice" for PWAs.
  if (std::all_of(apps_for_picker.begin(), apps_for_picker.end(),
                  [](const IntentPickerAppInfo& app_info) {
                    return app_info.type == apps::mojom::AppType::kWeb;
                  })) {
    ui_displayed_ = false;
    Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
    if (browser)
      browser->window()->SetIntentPickerViewVisibility(/*visible=*/true);
  } else {
    ShowIntentPickerBubbleForApps(web_contents, url,
                                  std::move(apps_for_picker));
  }

  // We are about to resume the navigation, which may destroy this object.
  Resume();
}

content::NavigationThrottle::ThrottleCheckResult
AppsNavigationThrottle::HandleRequest() {
  content::NavigationHandle* handle = navigation_handle();
  DCHECK(!ui_displayed_);

  // Always handle http(s) <form> submissions in Chrome for two reasons: 1) we
  // don't have a way to send POST data to ARC, and 2) intercepting http(s) form
  // submissions is not very important because such submissions are usually
  // done within the same domain. ShouldOverrideUrlLoading() below filters out
  // such submissions anyway.
  constexpr bool kAllowFormSubmit = false;

  // Ignore navigations with the CLIENT_REDIRECT qualifier on.
  constexpr bool kAllowClientRedirect = false;

  // We must never handle navigations started within a context menu.
  if (handle->WasStartedFromContextMenu())
    return content::NavigationThrottle::PROCEED;

  ui::PageTransition page_transition = handle->GetPageTransition();
  content::WebContents* web_contents = handle->GetWebContents();
  const GURL& url = handle->GetURL();
  if (ShouldIgnoreNavigation(page_transition, kAllowFormSubmit,
                             kAllowClientRedirect)) {
    if ((page_transition & ui::PAGE_TRANSITION_FORWARD_BACK) ||
        (page_transition & ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)) {
      // This enforces that whether we ignore the navigation or not, we make
      // sure that the user cannot copy/paste or type a url to reuse
      // ArcWebContentsData.
      MaybeRemoveComingFromArcFlag(web_contents, starting_url_, url);
    }
    return content::NavigationThrottle::PROCEED;
  }

  MaybeRemoveComingFromArcFlag(web_contents, starting_url_, url);

  if (!ShouldOverrideUrlLoading(starting_url_, url))
    return content::NavigationThrottle::PROCEED;

  if (arc_enabled_ &&
      arc::ArcNavigationThrottle::WillGetArcAppsForNavigation(
          handle,
          base::BindOnce(&AppsNavigationThrottle::OnDeferredNavigationProcessed,
                         weak_factory_.GetWeakPtr()))) {
    // Handling is now deferred to ArcNavigationThrottle, which asynchronously
    // queries ARC for apps, and runs OnDeferredNavigationProcessed() with an
    // action based on whether an acceptable app was found and user consent to
    // open received. We assume the UI is shown or a preferred app was found;
    // reset to false if we resume the navigation.
    ui_displayed_ = true;
    return content::NavigationThrottle::DEFER;
  }

  // We didn't query ARC, so proceed with the navigation and query if we have an
  // installed desktop PWA to handle the URL.
  if (IsDesktopPwasEnabled()) {
    std::vector<IntentPickerAppInfo> apps =
        FindPwaForUrl(web_contents, url, {});

    if (!apps.empty())
      ui_displayed_ = true;

    ShowIntentPickerBubbleForApps(web_contents, url, std::move(apps));
  }

  return content::NavigationThrottle::PROCEED;
}

void AppsNavigationThrottle::CloseTab() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::WebContents* web_contents = navigation_handle()->GetWebContents();
  if (web_contents)
    web_contents->ClosePage();
}

}  // namespace chromeos
