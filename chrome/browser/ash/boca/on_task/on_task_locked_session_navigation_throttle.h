// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOCA_ON_TASK_ON_TASK_LOCKED_SESSION_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_ASH_BOCA_ON_TASK_ON_TASK_LOCKED_SESSION_NAVIGATION_THROTTLE_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/boca/on_task/locked_session_window_tracker_factory.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"

namespace ash {

// A navigation throttle that helps OnTask SWA lock down navigations based on
// imposed restrictions.
class OnTaskLockedSessionNavigationThrottle
    : public content::NavigationThrottle {
 public:
  // Returns a navigation throttle when the navigation is happening inside
  // a tabbed web app and the tabbed web app has a pinned home tab.
  static std::unique_ptr<content::NavigationThrottle> MaybeCreateThrottleFor(
      content::NavigationHandle* handle);

  ~OnTaskLockedSessionNavigationThrottle() override;

  // content::NavigationThrottle:
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  const char* GetNameForLogging() override;

 private:
  explicit OnTaskLockedSessionNavigationThrottle(
      content::NavigationHandle* handle);

  // Checks that the restriction is applied to the navigation whether it
  // happens via redirect or when the navigation is first started.
  ThrottleCheckResult CheckRestrictions();

  // Checks to see if we can proceed during a one level deep navigation for the
  // url. When there is a new tab that is created as a result of
  // this navigation, we will set that tab's restrictions to
  // `kLimitedNavigation`.
  bool MaybeProceedForOneLevelDeep(content::WebContents* tab, const GURL& url);

  // `should_redirects_pass` allows url redirects to go through without going
  // through the blocklist checks. This should only be flipped to true after the
  // navigation request is started and the url is allowed to pass. This is used
  // to help ensure we allow urls to pass when the restriction is set to one
  // level deep, but is also used when we need to allow redirects to go
  // through when the initial checks are passed, but redirects would otherwise
  // fail.
  bool should_redirects_pass_ = false;

  base::WeakPtrFactory<OnTaskLockedSessionNavigationThrottle>
      weak_pointer_factory_{this};
};

}  // namespace ash
#endif  // CHROME_BROWSER_ASH_BOCA_ON_TASK_ON_TASK_LOCKED_SESSION_NAVIGATION_THROTTLE_H_
