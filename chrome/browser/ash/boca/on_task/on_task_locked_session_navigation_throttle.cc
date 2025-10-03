// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/on_task/on_task_locked_session_navigation_throttle.h"

#include <memory>

#include "ash/shell.h"
#include "ash/webui/boca_ui/url_constants.h"
#include "chrome/browser/ash/boca/on_task/on_task_locked_session_window_tracker.h"
#include "chrome/browser/login_detection/login_detection_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chromeos/ash/components/boca/boca_role_util.h"
#include "chromeos/ash/components/boca/on_task/on_task_blocklist.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "components/google/core/common/google_util.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/extension_urls.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace ash {  // namespace
namespace {

using ::boca::LockedNavigationOptions;

// Returns whether all the given query parameters are found in the URL.
bool DoAllQueryParamsExist(const std::set<std::string>& request_params,
                           const GURL& url) {
  if (!url.has_query()) {
    return false;
  }
  for (const auto& param : request_params) {
    std::string param_value;
    if (!net::GetValueForKeyInQuery(url, param, &param_value)) {
      return false;
    }
  }
  return true;
}

// Returns whether the url is the start of an Oauth login.
bool IsOauthLoginStart(const GURL& url) {
  return DoAllQueryParamsExist(login_detection::GetOAuthLoginStartQueryParams(),
                               url);
}

// Returns whether the url is the completion of an Oauth login.
bool IsOauthLoginComplete(const GURL& url) {
  return DoAllQueryParamsExist(
      login_detection::GetOAuthLoginCompleteQueryParams(), url);
}

bool IsChromeWebStoreURL(const GURL& url) {
  return (url.GetHost() == extension_urls::GetWebstoreLaunchURL().GetHost()) ||
         (url.GetHost() == extension_urls::GetNewWebstoreLaunchURL().GetHost());
}

bool IsBocaAppHostURL(const GURL& url) {
  return (url.SchemeIs(content::kChromeUIUntrustedScheme) &&
          url.GetHost() == boca::kChromeBocaAppHost);
}

}  // namespace

OnTaskLockedSessionNavigationThrottle::OnTaskLockedSessionNavigationThrottle(
    content::NavigationThrottleRegistry& registry)
    : content::NavigationThrottle(registry) {}

OnTaskLockedSessionNavigationThrottle::
    ~OnTaskLockedSessionNavigationThrottle() = default;

const char* OnTaskLockedSessionNavigationThrottle::GetNameForLogging() {
  return "OnTaskLockedSessionNavigationThrottle";
}

// static
void OnTaskLockedSessionNavigationThrottle::MaybeCreateAndAdd(
    content::NavigationThrottleRegistry& registry) {
  content::NavigationHandle& handle = registry.GetNavigationHandle();
  if (!ash::boca_util::IsEnabled(
          ash::BrowserContextHelper::Get()->GetUserByBrowserContext(
              handle.GetWebContents()->GetBrowserContext()))) {
    return;
  }

  LockedSessionWindowTracker* const window_tracker =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(
          handle.GetWebContents()->GetBrowserContext());
  // We do not need to create the throttle when we are not currently observing a
  // window that needs to be in locked mode, or if the navigation throttle is
  // not ready to start (where we are adding new tabs), or if the navigation is
  // occurring outside the outermost main frame (such as subframes on the page
  // so resources can still load), or if it is a same document navigation (where
  // we are not navigating to a new page).
  if (!window_tracker || !window_tracker->browser() ||
      !window_tracker->can_start_navigation_throttle()) {
    return;
  }

  if (!handle.IsInOutermostMainFrame()) {
    return;
  }

  if (handle.IsSameDocument()) {
    return;
  }

  Browser* const content_browser =
      LockedSessionWindowTracker::GetBrowserWithTab(handle.GetWebContents());

  // Ensure we only apply the nav throttle on OnTask SWA navigations.
  if (content_browser && (content_browser != window_tracker->browser() &&
                          !content_browser->is_type_app_popup())) {
    return;
  }
  window_tracker->ObserveWebContents(handle.GetWebContents());
  registry.AddThrottle(
      base::WrapUnique(new OnTaskLockedSessionNavigationThrottle(registry)));
}

void OnTaskLockedSessionNavigationThrottle::MaybeShowBlockedURLToast() {
  // Display the toast when the navigation is user-initiated. Note that
  // `HasUserGesture` does not capture browser-initiated navigations. The
  // negation of `IsRendererInitiated` tells us whether the navigation is
  // browser-generated.
  if (navigation_handle()->HasUserGesture() ||
      !navigation_handle()->IsRendererInitiated()) {
    LockedSessionWindowTracker* const window_tracker =
        LockedSessionWindowTrackerFactory::GetForBrowserContext(
            navigation_handle()->GetWebContents()->GetBrowserContext());

    // TODO: b/377767192 - Add tests to for scenarios regarding tab browser
    // instance changes
    if (window_tracker && !IsOutsideOnTaskAppNavigation()) {
      window_tracker->ShowURLBlockedToast();
    }
  }
}

bool OnTaskLockedSessionNavigationThrottle::MaybeProceedForOneLevelDeep(
    content::WebContents* tab,
    const GURL& url) {
  LockedSessionWindowTracker* const window_tracker =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(
          navigation_handle()->GetWebContents()->GetBrowserContext());
  if (!window_tracker) {
    return false;
  }

  // Google search sometimes redirects to the captcha page. We let this
  // navigation proceed by default.
  if (google_util::IsGoogleDomainUrl(
          url, google_util::SubdomainPermission::DISALLOW_SUBDOMAIN,
          google_util::PortPermission::ALLOW_NON_STANDARD_PORTS) &&
      url.path().starts_with("/sorry/")) {
    return true;
  }

  OnTaskBlocklist* const on_task_blocklist =
      window_tracker->on_task_blocklist();
  if (!on_task_blocklist->CanPerformOneLevelNavigation(tab)) {
    return false;
  }
  on_task_blocklist->MaybeSetURLRestrictionLevel(
      navigation_handle()->GetWebContents(), url,
      LockedNavigationOptions::BLOCK_NAVIGATION);
  return true;
}

bool OnTaskLockedSessionNavigationThrottle::
    ShouldBlockSensitiveUrlNavigation() {
  // Block download urls, files, urls via post request (form submission being
  // an exception), blob urls, non-boca app chrome urls, and other local
  // schemes.
  const GURL& url = navigation_handle()->GetURL();
  return (navigation_handle()->IsDownload() ||
          (navigation_handle()->GetRequestMethod() !=
               net::HttpRequestHeaders::kGetMethod &&
           !navigation_handle()->IsFormSubmission()) ||
          (!url.SchemeIsHTTPOrHTTPS() && !IsBocaAppHostURL(url)) ||
          IsChromeWebStoreURL(url));
}

bool OnTaskLockedSessionNavigationThrottle::IsOutsideOnTaskAppNavigation() {
  // TODO(b/377347487): Add test for Navigations that happen outside the OnTask
  // SWA but attach the tab to the OnTask SWA subsequently.
  Browser* const content_browser =
      LockedSessionWindowTracker::GetBrowserWithTab(
          navigation_handle()->GetWebContents());
  LockedSessionWindowTracker* const window_tracker =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(
          navigation_handle()->GetWebContents()->GetBrowserContext());
  // Handle the case where the creation of the tab is in the OnTask app
  // context, but is moved to a different browser right after (such as open link
  // in chrome window context menu).
  if (!content_browser || (content_browser != window_tracker->browser() &&
                           !content_browser->is_type_app_popup())) {
    return true;
  }
  return false;
}

content::NavigationThrottle::ThrottleCheckResult
OnTaskLockedSessionNavigationThrottle::CheckRestrictions() {
  // If there is a client side redirect, let those through.
  if (navigation_handle()->GetNavigationEntry() &&
      (navigation_handle()->GetNavigationEntry()->GetTransitionType() &
       ui::PageTransition::PAGE_TRANSITION_CLIENT_REDIRECT)) {
    return PROCEED;
  }
  LockedSessionWindowTracker* const window_tracker =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(
          navigation_handle()->GetWebContents()->GetBrowserContext());
  Browser* const content_browser =
      LockedSessionWindowTracker::GetBrowserWithTab(
          navigation_handle()->GetWebContents());

  if (IsOutsideOnTaskAppNavigation()) {
    return PROCEED;
  }

  if (ShouldBlockSensitiveUrlNavigation() &&
      !window_tracker->oauth_in_progress()) {
    MaybeShowBlockedURLToast();
    return CANCEL;
  }

  // Allow redirects triggered as separate navigation requests to go through so
  // they do not count towards the 1LD quota. We do not extend this to other
  // navigation restrictions to prevent users from circumventing said
  // restrictions.
  OnTaskBlocklist* const on_task_blocklist =
      window_tracker->on_task_blocklist();
  if (navigation_handle()->GetRedirectChain().size() > 1 &&
      on_task_blocklist->IsCurrentRestrictionOneLevelDeep()) {
    return PROCEED;
  }
  const GURL& url = navigation_handle()->GetURL();

  // There is no nav restriction associated with the home tab so the blocklist
  // may enforce nav restrictions based on the previous active tab. We allow all
  // requests to the home URL to go through for now.
  // TODO(crbug.com/413468168) - Associate a nav restriction with the home tab.
  if (IsBocaAppHostURL(url)) {
    return PROCEED;
  }

  // Checks if the query is the end of an OAuth login. If so, then we want
  // to let these pass.
  if (IsOauthLoginComplete(url)) {
    should_redirects_pass_ = true;
    return PROCEED;
  }

  // Checks if the query is the start of an OAuth login. If so, then we want
  // to let these pass.
  if (IsOauthLoginStart(url)) {
    window_tracker->set_oauth_in_progress(true);
    // Set `should_redirects_pass_` to true in case the Oauth login flow happens
    // in the main tab and not in a popup window. This ensures that we are still
    // letting Oauth login flows to proceed.
    should_redirects_pass_ = true;
    return PROCEED;
  }

  // If the navigation is taking place in a popup and isn't recognized as an
  // OAuth navigation, still give it a chance to finish. If by the end
  //  of the navigation we haven't determined that it is an OAuth login flow,
  //  the window_tracker will close the popup.
  if (content_browser && content_browser->is_type_app_popup() &&
      !window_tracker->CanOpenNewPopup()) {
    return PROCEED;
  }

  // This is a page reload, let the navigation pass since if we were able to get
  // to this page, then it was already filtered. This is so that one level deep
  // navigation can still reload the current page even though we have already
  // navigated one level deeper into the page.
  // Note: this throttle allows reloads that redirect to a different URL; if
  // that URL needs to be blocked by another blocklist, such as the one imposed
  // by the device admin panel, this would be enforced by a different
  // NavigationThrottle.
  if (on_task_blocklist->IsCurrentRestrictionOneLevelDeep() &&
      navigation_handle()->GetReloadType() != content::ReloadType::NONE &&
      navigation_handle()->GetWebContents()->GetLastCommittedURL().is_valid()) {
    should_redirects_pass_ = true;
    return PROCEED;
  }

  // Check for history navigations via the back and forward shortcuts or via
  // the context menu. Back needs to be explicitly allowed to go back in the
  // case this was a one level deep navigation and we do not want to block
  // the navigation from going back.
  if (on_task_blocklist->IsCurrentRestrictionOneLevelDeep() &&
      navigation_handle()->GetNavigationEntry() &&
      navigation_handle()->GetNavigationEntry()->GetTransitionType() &
          ui::PageTransition::PAGE_TRANSITION_FORWARD_BACK) {
    content::NavigationController& controller =
        navigation_handle()->GetWebContents()->GetController();
    int current_index = controller.GetLastCommittedEntryIndex();
    int pending_index = controller.GetPendingEntryIndex();
    if (pending_index < current_index) {
      should_redirects_pass_ = true;
      return PROCEED;
    }
  }

  policy::URLBlocklist::URLBlocklistState blocklist_state =
      on_task_blocklist->GetURLBlocklistState(url);
  if (blocklist_state ==
      policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST) {
    MaybeShowBlockedURLToast();
    return content::NavigationThrottle::CANCEL;
  }

  if (blocklist_state ==
      policy::URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST) {
    // If this navigation occurs on a tab restricted to one level deep
    // navigations, it will only be allowed if the tab hasn't performed a one
    // level deep navigation yet, which is true if the tab's last committed URL
    // hasn't changed from when the restrictions were enabled. Navigations in
    // newly opened tabs, such as when ctrl-clicking a link, also count as
    // navigating one level deep. For those cases, restrict the new tab to the
    // exact URL for subsequent navigations. The exact URL matching will occur
    // in `on_task_blocklist->CanPerformOneLevelNavigation()`.
    if (on_task_blocklist->current_page_restriction_level() ==
        LockedNavigationOptions::LIMITED_NAVIGATION) {
      if (!MaybeProceedForOneLevelDeep(on_task_blocklist->previous_tab(),
                                       url)) {
        MaybeShowBlockedURLToast();
        return content::NavigationThrottle::CANCEL;
      }
    } else if (on_task_blocklist->current_page_restriction_level() ==
               LockedNavigationOptions::
                   SAME_DOMAIN_OPEN_OTHER_DOMAIN_LIMITED_NAVIGATION) {
      // We need to account for several scenarios here, because a navigation
      // needs to be allowed if it is within the same domain as the original URL
      // in the parent tab, but conditionally allowed to go one level deep (1LD)
      // if it has not already (in the same parent tab or a child tab). Domain
      // checks happen against the original URL of the parent tab, but 1LD
      // checks need to cover three possibilities:
      // 1. Navigation on the same parent tab.
      // 2. Navigation on a new child tab (spawned through ctrl+click for
      // instance).
      // 3. Navigation on a pre-existing child tab that may or may not have
      // already met the 1LD requirement.
      GURL source_url = window_tracker->browser()
                            ->tab_strip_model()
                            ->GetActiveWebContents()
                            ->GetLastCommittedURL();
      const SessionID original_tab_id = sessions::SessionTabHelper::IdForTab(
          on_task_blocklist->previous_tab());
      if (on_task_blocklist->one_level_deep_original_url().contains(
              original_tab_id)) {
        source_url =
            on_task_blocklist->one_level_deep_original_url()[original_tab_id];
      }
      if (source_url.is_valid()) {
        if (OnTaskBlocklist::IsURLInDomain(url, source_url)) {
          // Same domain navigation.
          on_task_blocklist->MaybeSetURLRestrictionLevel(
              navigation_handle()->GetWebContents(), url,
              LockedNavigationOptions::
                  SAME_DOMAIN_OPEN_OTHER_DOMAIN_LIMITED_NAVIGATION);
        } else if (on_task_blocklist->IsParentTab(
                       navigation_handle()->GetWebContents()) &&
                   !MaybeProceedForOneLevelDeep(
                       navigation_handle()->GetWebContents(), url)) {
          // Cannot go 1LD on the same parent tab.
          MaybeShowBlockedURLToast();
          return content::NavigationThrottle::CANCEL;
        } else if (const SessionID nav_tab_id =
                       sessions::SessionTabHelper::IdForTab(
                           navigation_handle()->GetWebContents());
                   on_task_blocklist->child_tab_to_nav_filters().contains(
                       nav_tab_id) &&
                   on_task_blocklist->child_tab_to_nav_filters()[nav_tab_id] ==
                       LockedNavigationOptions::BLOCK_NAVIGATION) {
          // Cannot go 1LD on a pre-existing child tab.
          MaybeShowBlockedURLToast();
          return content::NavigationThrottle::CANCEL;
        } else if (!MaybeProceedForOneLevelDeep(
                       on_task_blocklist->previous_tab(), url)) {
          // Disallowed 1LD navigation on a new child tab.
          MaybeShowBlockedURLToast();
          return content::NavigationThrottle::CANCEL;
        }
      }
    } else {
      // Set the restrictions for this new url if possible with the parent tab's
      // restrictions. This will be skipped if the tab which this
      // navigation is occurring in is already set.
      on_task_blocklist->MaybeSetURLRestrictionLevel(
          navigation_handle()->GetWebContents(), url,
          on_task_blocklist->current_page_restriction_level());
    }
    should_redirects_pass_ = true;
    return PROCEED;
  }
  MaybeShowBlockedURLToast();
  return content::NavigationThrottle::CANCEL;
}

content::NavigationThrottle::ThrottleCheckResult
OnTaskLockedSessionNavigationThrottle::WillStartRequest() {
  return CheckRestrictions();
}

content::NavigationThrottle::ThrottleCheckResult
OnTaskLockedSessionNavigationThrottle::WillProcessResponse() {
  LockedSessionWindowTracker* const window_tracker =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(
          navigation_handle()->GetWebContents()->GetBrowserContext());

  // This check is needed other SWA launches during unlocked that needs to
  // process navigation responses.
  // TODO: b/377767192 - Add tests to for scenarios regarding tab browser
  // instance changes

  if (IsOutsideOnTaskAppNavigation() || should_redirects_pass_) {
    return PROCEED;
  }
  if (ShouldBlockSensitiveUrlNavigation() &&
      !window_tracker->oauth_in_progress()) {
    MaybeShowBlockedURLToast();
    return CANCEL;
  }
  return CheckRestrictions();
}

content::NavigationThrottle::ThrottleCheckResult
OnTaskLockedSessionNavigationThrottle::WillRedirectRequest() {
  Browser* const content_browser =
      LockedSessionWindowTracker::GetBrowserWithTab(
          navigation_handle()->GetWebContents());
  LockedSessionWindowTracker* const window_tracker =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(
          navigation_handle()->GetWebContents()->GetBrowserContext());
  if (content_browser && content_browser->is_type_app_popup()) {
    // After the Oauth flow is completed, we let the `OnBrowserClosing`
    // observer from the `window_tracker` to set `oauth_in_progress` to be false
    // since a request may have returned with the auth code and marked as
    // complete, it may take some time for the redirect from oauth login flow to
    // the landing page to happen. If this is marked as false early, the window
    // may close before a redirect happens. We should let the `OnBrowserClosing`
    // call happen first. Similarly, since we aren't setting the bool for OnTask
    // for the popup browser, it can autoclose, so we can rely on that to reset
    // our `oauth_in_progress` flag.
    if (IsOauthLoginComplete(navigation_handle()->GetURL())) {
      return PROCEED;
    }

    // Checks to see if the popup window is an OAuth login. If the Oauth login
    // flow is just started, we tell the window tracker that it is in progress
    // to avoid closing the OAuth login window before it has been completed.
    if (IsOauthLoginStart(navigation_handle()->GetURL())) {
      window_tracker->set_oauth_in_progress(true);
      return PROCEED;
    }
    if (window_tracker->oauth_in_progress()) {
      return content::NavigationThrottle::PROCEED;
    }
    MaybeShowBlockedURLToast();
    return content::NavigationThrottle::CANCEL;
  }

  if (should_redirects_pass_) {
    return PROCEED;
  }
  // This catch all case is to catch navigations where we identify a case where
  // we should not always pass all redirects (such as blob schemes or page
  // reload in case of server redirects).
  return CheckRestrictions();
}

}  // namespace ash
