// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_WEB_TIME_LIMIT_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_WEB_TIME_LIMIT_NAVIGATION_THROTTLE_H_

#include <memory>

#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace chromeos {

// Throttles disallowed navigation for WebContents when time limit has
// been reached. It blocks the navigation and loads the web time limit error
// page in the blocked WebContents.
class WebTimeLimitNavigationThrottle : public content::NavigationThrottle {
 public:
  static std::unique_ptr<WebTimeLimitNavigationThrottle> MaybeCreateThrottleFor(
      content::NavigationHandle* navigation_handle);

  ~WebTimeLimitNavigationThrottle() override;

  // Delete copy constructor and copy assignment operator.
  WebTimeLimitNavigationThrottle(const WebTimeLimitNavigationThrottle&) =
      delete;
  WebTimeLimitNavigationThrottle& operator=(
      const WebTimeLimitNavigationThrottle&) = delete;

  // content::NavigationThrottle:
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  ThrottleCheckResult WillProcessResponse() override;
  const char* GetNameForLogging() override;

 private:
  explicit WebTimeLimitNavigationThrottle(
      content::NavigationHandle* navigation_handle);

  ThrottleCheckResult WillStartOrRedirectRequest();
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_WEB_TIME_LIMIT_NAVIGATION_THROTTLE_H_
