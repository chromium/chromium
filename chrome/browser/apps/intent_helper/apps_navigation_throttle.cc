// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/apps_navigation_throttle.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/optional.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/apps/intent_helper/intent_picker_auto_display_service.h"
#include "chrome/browser/apps/intent_helper/page_transition_util.h"
#include "chrome/browser/prerender/chrome_prerender_contents_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/intent_picker_tab_helper.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/web_applications/components/app_icon_manager.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/components/web_app_tab_helper_base.h"
#include "chrome/common/chrome_features.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/prerender/browser/prerender_contents.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/constants.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom.h"
#include "ui/gfx/image/image.h"
#include "url/origin.h"

namespace {

using ThrottleCheckResult = content::NavigationThrottle::ThrottleCheckResult;

// Returns true if |url| is a known and valid redirector that will redirect a
// navigation elsewhere.
bool IsGoogleRedirectorUrl(const GURL& url) {
  // This currently only check for redirectors on the "google" domain.
  if (!page_load_metrics::IsGoogleSearchHostname(url))
    return false;

  return url.path_piece() == "/url" && url.has_query();
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
      previous_url.SchemeIs(extensions::kExtensionScheme)) {
    return false;
  }

  // Skip URL redirectors that are intermediate pages redirecting towards a
  // final URL.
  if (IsGoogleRedirectorUrl(current_url))
    return false;

  return true;
}

GURL GetStartingGURL(content::NavigationHandle* navigation_handle) {
  // This helps us determine a reference GURL for the current NavigationHandle.
  // This is the order or preference: Referrer > LastCommittedURL >
  // InitiatorOrigin. InitiatorOrigin *should* only be used on very rare cases,
  // e.g. when the navigation goes from https: to http: on a new tab, thus
  // losing the other potential referrers.
  const GURL referrer_url = navigation_handle->GetReferrer().url;
  if (referrer_url.is_valid() && !referrer_url.is_empty())
    return referrer_url;

  const GURL last_committed_url =
      navigation_handle->GetWebContents()->GetLastCommittedURL();
  if (last_committed_url.is_valid() && !last_committed_url.is_empty())
    return last_committed_url;

  const auto& initiator_origin = navigation_handle->GetInitiatorOrigin();
  return initiator_origin.has_value() ? initiator_origin->GetURL() : GURL();
}

bool InAppBrowser(content::WebContents* web_contents) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  return !browser || browser->deprecated_is_app();
}

}  // namespace

namespace apps {

// static
const char AppsNavigationThrottle::kUseBrowserForLink[] = "use_browser";

// static
std::unique_ptr<content::NavigationThrottle>
AppsNavigationThrottle::MaybeCreate(content::NavigationHandle* handle) {
  if (!handle->IsInMainFrame())
    return nullptr;

  content::WebContents* web_contents = handle->GetWebContents();
  if (!CanCreate(web_contents))
    return nullptr;

  return std::make_unique<AppsNavigationThrottle>(handle);
}

// static
void AppsNavigationThrottle::ShowIntentPickerBubble(
    content::WebContents* web_contents,
    IntentPickerAutoDisplayService* ui_auto_display_service,
    const GURL& url) {
  std::vector<IntentPickerAppInfo> apps = FindPwaForUrl(web_contents, url, {});

  bool show_persistence_options = ShouldShowPersistenceOptions(apps);
  ShowIntentPickerBubbleForApps(
      web_contents, std::move(apps),
      /*show_stay_in_chrome=*/show_persistence_options,
      /*show_remember_selection=*/show_persistence_options,
      base::BindOnce(&OnIntentPickerClosed, web_contents,
                     ui_auto_display_service, url));
}

// static
void AppsNavigationThrottle::OnIntentPickerClosed(
    content::WebContents* web_contents,
    IntentPickerAutoDisplayService* ui_auto_display_service,
    const GURL& url,
    const std::string& launch_name,
    PickerEntryType entry_type,
    IntentPickerCloseReason close_reason,
    bool should_persist) {
  const bool should_launch_app =
      close_reason == IntentPickerCloseReason::OPEN_APP;
  switch (entry_type) {
    case PickerEntryType::kWeb:
      if (should_launch_app)
        web_app::ReparentWebContentsIntoAppBrowser(web_contents, launch_name);
      break;
    case PickerEntryType::kUnknown:
      // We reach here if the picker was closed without an app being chosen,
      // e.g. due to the tab being closed. Keep count of this scenario so we can
      // stop the UI from showing after 2+ dismissals.
      if (close_reason == IntentPickerCloseReason::DIALOG_DEACTIVATED) {
        if (ui_auto_display_service)
          ui_auto_display_service->IncrementCounter(url);
      }
      break;
    case PickerEntryType::kArc:
    case PickerEntryType::kDevice:
    case PickerEntryType::kMacNative:
      NOTREACHED();
  }
}

// static
bool AppsNavigationThrottle::IsGoogleRedirectorUrlForTesting(const GURL& url) {
  return IsGoogleRedirectorUrl(url);
}

// static
bool AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
    const GURL& previous_url,
    const GURL& current_url) {
  return ShouldOverrideUrlLoading(previous_url, current_url);
}

// static
void AppsNavigationThrottle::ShowIntentPickerBubbleForApps(
    content::WebContents* web_contents,
    std::vector<IntentPickerAppInfo> apps,
    bool show_stay_in_chrome,
    bool show_remember_selection,
    IntentPickerResponse callback) {
  if (apps.empty())
    return;

  // It should be safe to bind |web_contents| since closing the current tab will
  // close the intent picker and run the callback prior to the WebContents being
  // deallocated.
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser)
    return;

  IntentPickerTabHelper::SetShouldShowIcon(web_contents, true);
  browser->window()->ShowIntentPickerBubble(
      std::move(apps), show_stay_in_chrome, show_remember_selection,
      PageActionIconType::kIntentPicker, base::nullopt, std::move(callback));
}

AppsNavigationThrottle::AppsNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle),
      ui_displayed_(false),
      ui_auto_display_service_(
          IntentPickerAutoDisplayService::Get(Profile::FromBrowserContext(
              navigation_handle->GetWebContents()->GetBrowserContext()))),
      navigate_from_link_(false) {
  // |ui_auto_display_service_| can be null iff the call is coming from
  // IntentPickerView. Since the pointer to our service is never modified
  // (in case it is successfully created here) this check covers all the
  // non-static methods in this class.
  DCHECK(ui_auto_display_service_);
}

AppsNavigationThrottle::~AppsNavigationThrottle() = default;

const char* AppsNavigationThrottle::GetNameForLogging() {
  return "AppsNavigationThrottle";
}

ThrottleCheckResult AppsNavigationThrottle::WillStartRequest() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  starting_url_ = GetStartingGURL(navigation_handle());
  IntentPickerTabHelper::SetShouldShowIcon(
      navigation_handle()->GetWebContents(), false);
  return HandleRequest();
}

ThrottleCheckResult AppsNavigationThrottle::WillRedirectRequest() {
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
bool AppsNavigationThrottle::CanCreate(content::WebContents* web_contents) {
  // Do not create the throttle if no apps can be installed.
  // Do not create the throttle in incognito or for a prerender navigation.
  if (web_contents->GetBrowserContext()->IsOffTheRecord() ||
      prerender::ChromePrerenderContentsDelegate::FromWebContents(
          web_contents) != nullptr) {
    return false;
  }

  // Do not create the throttle if we are already in an app browser.
  // It is possible that the web contents is not inserted to tab strip
  // model at this stage (e.g. open url in new tab). So if we cannot
  // find a browser at this moment, skip the check and this will be handled
  // in later stage.
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (browser && browser->deprecated_is_app())
    return false;

  return true;
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
    case PickerAction::MAC_NATIVE_APP_PRESSED:
      return Platform::MAC_NATIVE;
    case PickerAction::ERROR_BEFORE_PICKER:
    case PickerAction::ERROR_AFTER_PICKER:
    case PickerAction::DIALOG_DEACTIVATED:
    case PickerAction::CHROME_PRESSED:
    case PickerAction::CHROME_PREFERRED_PRESSED:
      return Platform::CHROME;
    case PickerAction::DEVICE_PRESSED:
      return Platform::DEVICE;
    case PickerAction::PREFERRED_ACTIVITY_FOUND:
    case PickerAction::OBSOLETE_ALWAYS_PRESSED:
    case PickerAction::OBSOLETE_JUST_ONCE_PRESSED:
    case PickerAction::INVALID:
      break;
  }
  NOTREACHED();
  return Platform::ARC;
}

// static
AppsNavigationThrottle::PickerAction AppsNavigationThrottle::GetPickerAction(
    PickerEntryType entry_type,
    IntentPickerCloseReason close_reason,
    bool should_persist) {
  switch (close_reason) {
    case IntentPickerCloseReason::ERROR_BEFORE_PICKER:
      return PickerAction::ERROR_BEFORE_PICKER;
    case IntentPickerCloseReason::ERROR_AFTER_PICKER:
      return PickerAction::ERROR_AFTER_PICKER;
    case IntentPickerCloseReason::DIALOG_DEACTIVATED:
      return PickerAction::DIALOG_DEACTIVATED;
    case IntentPickerCloseReason::PREFERRED_APP_FOUND:
      return PickerAction::PREFERRED_ACTIVITY_FOUND;
    case IntentPickerCloseReason::STAY_IN_CHROME:
      return should_persist ? PickerAction::CHROME_PREFERRED_PRESSED
                            : PickerAction::CHROME_PRESSED;
    case IntentPickerCloseReason::OPEN_APP:
      switch (entry_type) {
        case PickerEntryType::kUnknown:
          NOTREACHED();
          return PickerAction::INVALID;
        case PickerEntryType::kArc:
          return should_persist ? PickerAction::ARC_APP_PREFERRED_PRESSED
                                : PickerAction::ARC_APP_PRESSED;
        case PickerEntryType::kWeb:
          return PickerAction::PWA_APP_PRESSED;
        case PickerEntryType::kDevice:
          return PickerAction::DEVICE_PRESSED;
        case PickerEntryType::kMacNative:
          return PickerAction::MAC_NATIVE_APP_PRESSED;
      }
  }

  NOTREACHED();
  return PickerAction::INVALID;
}

std::vector<IntentPickerAppInfo> AppsNavigationThrottle::FindAppsForUrl(
    content::WebContents* web_contents,
    const GURL& url,
    std::vector<IntentPickerAppInfo> apps) {
  return FindPwaForUrl(web_contents, url, std::move(apps));
}

// static
std::vector<IntentPickerAppInfo> AppsNavigationThrottle::FindPwaForUrl(
    content::WebContents* web_contents,
    const GURL& url,
    std::vector<IntentPickerAppInfo> apps) {
  // Check if the current URL has an installed desktop PWA, and add that to
  // the list of apps if it exists.
  Profile* const profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  base::Optional<web_app::AppId> app_id =
      web_app::FindInstalledAppWithUrlInScope(profile, url,
                                              /*window_only=*/true);
  if (!app_id)
    return apps;

  auto* const provider = web_app::WebAppProviderBase::GetProviderBase(profile);
  gfx::Image icon = gfx::Image::CreateFrom1xBitmap(
      provider->icon_manager().GetFavicon(*app_id));

  // Prefer the web and place apps of type PWA before apps of type ARC.
  // TODO(crbug.com/824598): deterministically sort this list.
  apps.emplace(apps.begin(), PickerEntryType::kWeb, icon, *app_id,
               provider->registrar().GetAppShortName(*app_id));

  return apps;
}

// static
void AppsNavigationThrottle::CloseOrGoBack(content::WebContents* web_contents) {
  DCHECK(web_contents);
  if (web_contents->GetController().CanGoBack())
    web_contents->GetController().GoBack();
  else
    web_contents->ClosePage();
}

// static
bool AppsNavigationThrottle::ContainsOnlyPwasAndMacApps(
    const std::vector<apps::IntentPickerAppInfo>& apps) {
  return std::all_of(apps.begin(), apps.end(),
                     [](const apps::IntentPickerAppInfo& app_info) {
                       return app_info.type == PickerEntryType::kWeb ||
                              app_info.type == PickerEntryType::kMacNative;
                     });
}

// static
bool AppsNavigationThrottle::ShouldShowPersistenceOptions(
    std::vector<apps::IntentPickerAppInfo>& apps) {
  // There is no support persistence for PWA so the selection should be hidden
  // if only PWAs are present.
  // TODO(crbug.com/826982): Provide the "Remember my choice" option when the
  // app registry can support persistence for PWAs.
  // This function is also used to hide the "Stay In Chrome" button when the
  // "Remember my choice" option is hidden such that the bubble is easy to
  // understand.
  // TODO(avi): When Chrome gains a UI for managing the persistence of PWAs,
  // reuse that UI for managing the persistent behavior of Universal Links.
  return !ContainsOnlyPwasAndMacApps(apps);
}

bool AppsNavigationThrottle::ShouldCancelNavigation(
    content::NavigationHandle* handle) {
  return false;
}

bool AppsNavigationThrottle::ShouldDeferNavigation(
    content::NavigationHandle* handle) {
  return false;
}

void AppsNavigationThrottle::ShowIntentPickerForApps(
    content::WebContents* web_contents,
    IntentPickerAutoDisplayService* ui_auto_display_service,
    const GURL& url,
    std::vector<IntentPickerAppInfo> apps,
    IntentPickerResponse callback) {
  if (apps.empty()) {
    IntentPickerTabHelper::SetShouldShowIcon(web_contents, false);
    ui_displayed_ = false;
    return;
  }
  IntentPickerTabHelper::SetShouldShowIcon(web_contents, true);
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser)
    return;
  const PickerShowState picker_show_state =
      GetPickerShowState(apps, web_contents, url);
  switch (picker_show_state) {
    case PickerShowState::kOmnibox:
      ui_displayed_ = false;
      break;
    case PickerShowState::kPopOut: {
      bool show_persistence_options = ShouldShowPersistenceOptions(apps);
      ShowIntentPickerBubbleForApps(
          web_contents, std::move(apps),
          /*show_stay_in_chrome=*/show_persistence_options,
          /*show_remember_selection=*/show_persistence_options,
          std::move(callback));
      break;
    }
    default:
      NOTREACHED();
  }
}

AppsNavigationThrottle::PickerShowState
AppsNavigationThrottle::GetPickerShowState(
    const std::vector<IntentPickerAppInfo>& apps_for_picker,
    content::WebContents* web_contents,
    const GURL& url) {
  return PickerShowState::kOmnibox;
}

IntentPickerResponse AppsNavigationThrottle::GetOnPickerClosedCallback(
    content::WebContents* web_contents,
    IntentPickerAutoDisplayService* ui_auto_display_service,
    const GURL& url) {
  return base::BindOnce(&OnIntentPickerClosed, web_contents,
                        ui_auto_display_service, url);
}

bool AppsNavigationThrottle::navigate_from_link() const {
  return navigate_from_link_;
}

bool AppsNavigationThrottle::ShouldAutoDisplayUi(
    const std::vector<apps::IntentPickerAppInfo>& apps_for_picker,
    content::WebContents* web_contents,
    const GURL& url) {
  if (apps_for_picker.empty())
    return false;

  if (InAppBrowser(web_contents))
    return false;

  if (!ShouldOverrideUrlLoading(starting_url_, url))
    return false;

  DCHECK(ui_auto_display_service_);
  return ui_auto_display_service_->ShouldAutoDisplayUi(url);
}

ThrottleCheckResult AppsNavigationThrottle::HandleRequest() {
  content::NavigationHandle* handle = navigation_handle();
  // If the navigation won't update the current document, don't check intent for
  // the navigation.
  if (handle->IsSameDocument())
    return content::NavigationThrottle::PROCEED;

  DCHECK(!ui_displayed_);

  navigate_from_link_ = false;

  // Always handle http(s) <form> submissions in Chrome for two reasons: 1) we
  // don't have a way to send POST data to ARC, and 2) intercepting http(s) form
  // submissions is not very important because such submissions are usually
  // done within the same domain. ShouldOverrideUrlLoading() below filters out
  // such submissions anyway.
  constexpr bool kAllowFormSubmit = false;

  // Ignore navigations with the CLIENT_REDIRECT qualifier on.
  constexpr bool kAllowClientRedirect = true;

  ui::PageTransition page_transition = handle->GetPageTransition();
  content::WebContents* web_contents = handle->GetWebContents();
  const GURL& url = handle->GetURL();
  if (!ShouldIgnoreNavigation(page_transition, kAllowFormSubmit,
                              kAllowClientRedirect) &&
      !handle->WasStartedFromContextMenu()) {
    navigate_from_link_ = true;
  }

  MaybeRemoveComingFromArcFlag(web_contents, starting_url_, url);

  base::Optional<ThrottleCheckResult> tab_strip_capture =
      CaptureExperimentalTabStripWebAppScopeNavigations(web_contents, handle);
  if (tab_strip_capture.has_value())
    return tab_strip_capture.value();

  // Do not pop up the intent picker bubble or automatically launch the app if
  // we shouldn't override url loading, or if we don't have a browser, or we are
  // already in an app browser.
  if (ShouldOverrideUrlLoading(starting_url_, url) &&
      !InAppBrowser(web_contents)) {
    // Handles apps that are automatically launched and the navigation needs to
    // be cancelled. This only applies on the new intent picker system, because
    // we don't need to defer the navigation to find out preferred app anymore.
    if (ShouldCancelNavigation(handle)) {
      return content::NavigationThrottle::CANCEL_AND_IGNORE;
    }

    if (ShouldDeferNavigation(handle)) {
      // Handling is now deferred to ArcIntentPickerAppFetcher, which
      // asynchronously queries ARC for apps, and runs
      // OnDeferredNavigationProcessed() with an action based on whether an
      // acceptable app was found and user consent to open received. We assume
      // the UI is shown or a preferred app was found; reset to false if we
      // resume the navigation.
      ui_displayed_ = true;
      return content::NavigationThrottle::DEFER;
    }
  }

  // We didn't query ARC, so proceed with the navigation and query if we have an
  // installed desktop PWA to handle the URL.
  std::vector<IntentPickerAppInfo> apps = FindAppsForUrl(web_contents, url, {});

  if (!apps.empty())
    ui_displayed_ = true;

  ShowIntentPickerForApps(
      web_contents, ui_auto_display_service_, url, std::move(apps),
      GetOnPickerClosedCallback(web_contents, ui_auto_display_service_, url));

  return content::NavigationThrottle::PROCEED;
}

base::Optional<ThrottleCheckResult>
AppsNavigationThrottle::CaptureExperimentalTabStripWebAppScopeNavigations(
    content::WebContents* web_contents,
    content::NavigationHandle* handle) const {
  if (!navigate_from_link())
    return base::nullopt;

  if (!base::FeatureList::IsEnabled(features::kDesktopPWAsTabStrip) ||
      !base::FeatureList::IsEnabled(
          features::kDesktopPWAsTabStripLinkCapturing)) {
    return base::nullopt;
  }

  Profile* const profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  web_app::WebAppProviderBase* provider =
      web_app::WebAppProviderBase::GetProviderBase(profile);
  if (!provider)
    return base::nullopt;

  base::Optional<web_app::AppId> app_id =
      provider->registrar().FindInstalledAppWithUrlInScope(
          handle->GetURL(), /*window_only=*/true);
  if (!app_id)
    return base::nullopt;

  if (!provider->registrar().IsInExperimentalTabbedWindowMode(*app_id))
    return base::nullopt;

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (web_app::AppBrowserController::IsForWebAppBrowser(browser, *app_id)) {
    // Already in the app window; navigation already captured.
    return base::nullopt;
  }

  // If |web_contents| hasn't loaded yet or has only loaded about:blank we
  // should reparent it into the app window to avoid leaving behind a blank tab.
  auto* tab_helper =
      web_app::WebAppTabHelperBase::FromWebContents(web_contents);
  if (tab_helper && !tab_helper->HasLoadedNonAboutBlankPage()) {
    web_app::ReparentWebContentsIntoAppBrowser(web_contents, *app_id);
    return content::NavigationThrottle::PROCEED;
  }

  apps::AppLaunchParams launch_params(
      *app_id, apps::mojom::LaunchContainer::kLaunchContainerWindow,
      WindowOpenDisposition::CURRENT_TAB,
      apps::mojom::AppLaunchSource::kSourceUrlHandler);
  launch_params.override_url = handle->GetURL();
  apps::AppServiceProxyFactory::GetForProfile(profile)
      ->BrowserAppLauncher()
      ->LaunchAppWithParams(launch_params);
  return content::NavigationThrottle::CANCEL_AND_IGNORE;
}

}  // namespace apps
