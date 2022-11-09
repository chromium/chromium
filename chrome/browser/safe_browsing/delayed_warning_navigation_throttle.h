// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DELAYED_WARNING_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_SAFE_BROWSING_DELAYED_WARNING_NAVIGATION_THROTTLE_H_

#include <memory>

#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace safe_browsing {

// A navigation throttle that detects downloads when a SafeBrowsing warning is
// delayed on the current page. SafeBrowsing Delayed Warnings experiment delays
// certain SafeBrowsing interstitials until a user interaction or other
// security moment such as a download or permission request occurs.
class DelayedWarningNavigationThrottle : public content::NavigationThrottle {
 public:
  explicit DelayedWarningNavigationThrottle(content::NavigationHandle* handle);
  ~DelayedWarningNavigationThrottle() override;

  static std::unique_ptr<DelayedWarningNavigationThrottle>
  MaybeCreateNavigationThrottle(content::NavigationHandle* navigation_handle);

  // content::NavigationThrottle:
  ThrottleCheckResult WillProcessResponse() override;
  const char* GetNameForLogging() override;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_DELAYED_WARNING_NAVIGATION_THROTTLE_H_
