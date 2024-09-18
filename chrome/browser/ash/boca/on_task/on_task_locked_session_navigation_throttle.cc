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
  // window that needs to be in locked mode, or if the navigation is occurring
  // outside the outermost main frame (such as subframes on the page so
  // resources can still load), or if it is a same document navigation (where we
  // are not navigating to a new page).
  if (!window_tracker || !window_tracker->browser()) {
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

content::NavigationThrottle::ThrottleCheckResult
OnTaskLockedSessionNavigationThrottle::CheckBlocklistFilter() {
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

  policy::URLBlocklist::URLBlocklistState blocklist_state =
      window_tracker->on_task_blocklist()->GetURLBlocklistState(url);
  if (blocklist_state ==
      policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST) {
    return content::NavigationThrottle::CANCEL;
  }

  if (blocklist_state ==
      policy::URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST) {
    window_tracker->on_task_blocklist()->MaybeSetURLRestrictionLevel(
        navigation_handle()->GetWebContents(),
        window_tracker->on_task_blocklist()->current_page_restriction_level());
    should_redirects_pass_ = true;
    return PROCEED;
  }
  return content::NavigationThrottle::CANCEL;
}

content::NavigationThrottle::ThrottleCheckResult
OnTaskLockedSessionNavigationThrottle::WillStartRequest() {
  return CheckBlocklistFilter();
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
  return CheckBlocklistFilter();
}

}  // namespace ash
