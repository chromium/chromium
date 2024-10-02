// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/on_task/on_task_locked_session_navigation_throttle.h"

#include <memory>

#include "ash/shell.h"
#include "chrome/browser/ash/boca/on_task/on_task_locked_session_window_tracker.h"
#include "chrome/browser/login_detection/login_detection_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chromeos/ash/components/boca/on_task/on_task_blocklist.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "net/base/url_util.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace ash {  // namespace
namespace {
using RestrictionLevel = OnTaskBlocklist::RestrictionLevel;

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

}  // namespace

OnTaskLockedSessionNavigationThrottle::OnTaskLockedSessionNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle) {}

OnTaskLockedSessionNavigationThrottle::
    ~OnTaskLockedSessionNavigationThrottle() = default;

const char* OnTaskLockedSessionNavigationThrottle::GetNameForLogging() {
  return "OnTaskLockedSessionNavigationThrottle";
}

// static
std::unique_ptr<content::NavigationThrottle>
OnTaskLockedSessionNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* handle) {
  LockedSessionWindowTracker* const window_tracker =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(
          handle->GetWebContents()->GetBrowserContext());
  // We do not need to create the throttle when we are not currently observing a
  // window that needs to be in locked mode, or if the navigation throttle is
  // not ready to start (where we are adding new tabs), or if the navigation is
  // occurring outside the outermost main frame (such as subframes on the page
  // so resources can still load), or if it is a same document navigation (where
  // we are not navigating to a new page).
  if (!window_tracker || !window_tracker->browser() ||
      !window_tracker->can_start_navigation_throttle()) {
    return nullptr;
  }

  if (!handle->IsInOutermostMainFrame()) {
    return nullptr;
  }

  if (handle->IsSameDocument()) {
    return nullptr;
  }

  Browser* const content_browser =
      LockedSessionWindowTracker::GetBrowserWithTab(handle->GetWebContents());
  if (content_browser && content_browser != window_tracker->browser() &&
      !content_browser->is_type_app_popup()) {
    return nullptr;
  }
  window_tracker->ObserveWebContents(handle->GetWebContents());
  return base::WrapUnique(new OnTaskLockedSessionNavigationThrottle(handle));
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
  OnTaskBlocklist* const on_task_blocklist =
      window_tracker->on_task_blocklist();
  if (!on_task_blocklist->CanPerformOneLevelNavigation(tab)) {
    return false;
  }
  on_task_blocklist->MaybeSetURLRestrictionLevel(
      navigation_handle()->GetWebContents(), url,
      OnTaskBlocklist::RestrictionLevel::kLimitedNavigation);
  return true;
}

content::NavigationThrottle::ThrottleCheckResult
OnTaskLockedSessionNavigationThrottle::CheckRestrictions() {
  const GURL& url = navigation_handle()->GetURL();

  // Checks if the query is the end of an OAuth login. If so, then we want
  // to let these pass.
  if (IsOauthLoginComplete(navigation_handle()->GetURL())) {
    should_redirects_pass_ = true;
    return PROCEED;
  }

  LockedSessionWindowTracker* const window_tracker =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(
          navigation_handle()->GetWebContents()->GetBrowserContext());

  // Checks if the query is the start of an OAuth login. If so, then we want
  // to let these pass.
  if (IsOauthLoginStart(navigation_handle()->GetURL())) {
    window_tracker->set_oauth_in_progress(true);
    // Set `should_redirects_pass_` to true in case the Oauth login flow happens
    // in the main tab and not in a popup window. This ensures that we are still
    // letting Oauth login flows to proceed.
    should_redirects_pass_ = true;
    return PROCEED;
  }
  Browser* const content_browser =
      LockedSessionWindowTracker::GetBrowserWithTab(
          navigation_handle()->GetWebContents());

  // If the navigation is taking place in a popup and isn't recognized as an
  // OAuth navigation, still give it a chance to finish. If by the end
  //  of the navigation we haven't determined that it is an OAuth login flow,
  //  the window_tracker will close the popup.
  if (content_browser && content_browser->is_type_app_popup() &&
      !window_tracker->CanOpenNewPopup()) {
    window_tracker->set_oauth_in_progress(false);
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
  if (window_tracker->on_task_blocklist()->IsCurrentRestrictionOneLevelDeep() &&
      navigation_handle()->GetReloadType() != content::ReloadType::NONE) {
    should_redirects_pass_ = true;
    return PROCEED;
  }

  // Check for history navigations via the back and forward shortcuts or via the
  // context menu. Back needs to be explicitly allowed to go back in the case
  // this was a one level deep navigation and we do not want to block the
  // navigation from going back.
  if (window_tracker->on_task_blocklist()->IsCurrentRestrictionOneLevelDeep() &&
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

  OnTaskBlocklist* const on_task_blocklist =
      window_tracker->on_task_blocklist();

  policy::URLBlocklist::URLBlocklistState blocklist_state =
      on_task_blocklist->GetURLBlocklistState(url);
  if (blocklist_state ==
      policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST) {
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
        RestrictionLevel::kOneLevelDeepNavigation) {
      if (!MaybeProceedForOneLevelDeep(on_task_blocklist->previous_tab(),
                                       url)) {
        return content::NavigationThrottle::CANCEL;
      }
    } else if (on_task_blocklist->current_page_restriction_level() ==
               RestrictionLevel::kDomainAndOneLevelDeepNavigation) {
      // Similar conditions as the above, but we first check if it's the same
      // domain first before checking the one level deep case since we allow
      // same domain navigations as well.

      // We pick the initiator origin if available in case we want to check if
      // the current url we are attempting to check matches the domain of the
      // initial url for the tab. For example if we have the initiator origin as
      // google.com and the last committed url is en.google.com, we want to
      // check the domain with google.com instead.
      const GURL& source_url =
          navigation_handle()->GetInitiatorOrigin()
              ? navigation_handle()->GetInitiatorOrigin()->GetURL()
              : window_tracker->browser()
                    ->tab_strip_model()
                    ->GetActiveWebContents()
                    ->GetLastCommittedURL();
      if (source_url.is_valid()) {
        if (url.DomainIs(source_url.host())) {
          on_task_blocklist->MaybeSetURLRestrictionLevel(
              navigation_handle()->GetWebContents(), url,
              RestrictionLevel::kDomainAndOneLevelDeepNavigation);
        } else {
          if (!MaybeProceedForOneLevelDeep(
                  navigation_handle()->GetWebContents(), url)) {
            return content::NavigationThrottle::CANCEL;
          }
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
  return content::NavigationThrottle::CANCEL;
}

content::NavigationThrottle::ThrottleCheckResult
OnTaskLockedSessionNavigationThrottle::WillStartRequest() {
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
