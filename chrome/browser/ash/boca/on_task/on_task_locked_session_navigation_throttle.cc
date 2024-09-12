// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/on_task/on_task_locked_session_navigation_throttle.h"

#include <memory>

#include "ash/shell.h"
#include "chrome/browser/ash/boca/on_task/on_task_locked_session_window_tracker.h"
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
  if (content_browser && content_browser != window_tracker->browser()) {
    return nullptr;
  }
  window_tracker->ObserveWebContents(handle->GetWebContents());
  return base::WrapUnique(new OnTaskLockedSessionNavigationThrottle(handle));
}

content::NavigationThrottle::ThrottleCheckResult
OnTaskLockedSessionNavigationThrottle::CheckBlocklistFilter() {
  const GURL& url = navigation_handle()->GetURL();

  LockedSessionWindowTracker* const window_tracker =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(
          navigation_handle()->GetWebContents()->GetBrowserContext());
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
  if (should_redirects_pass_) {
    return PROCEED;
  }
  // This catch all case is to catch navigations where we identify a case where
  // we should not always pass all redirects (such as blob schemes or page
  // reload in case of server redirects).
  return CheckBlocklistFilter();
}

}  // namespace ash
