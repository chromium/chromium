// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_AIM_ELIGIBILITY_REFRESH_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_AUTOCOMPLETE_AIM_ELIGIBILITY_REFRESH_NAVIGATION_THROTTLE_H_

#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/navigation_throttle_registry.h"

namespace content {
class WebContents;
}  // namespace content

// A lightweight navigation throttle that refreshes AIM eligibility status on
// primary main frame AIM navigations.
class AimEligibilityRefreshNavigationThrottle
    : public content::NavigationThrottle {
 public:
  using ThrottleCheckResult = content::NavigationThrottle::ThrottleCheckResult;

  explicit AimEligibilityRefreshNavigationThrottle(
      content::NavigationThrottleRegistry& registry);
  AimEligibilityRefreshNavigationThrottle(
      const AimEligibilityRefreshNavigationThrottle&) = delete;
  AimEligibilityRefreshNavigationThrottle& operator=(
      const AimEligibilityRefreshNavigationThrottle&) = delete;
  ~AimEligibilityRefreshNavigationThrottle() override;

  // content::NavigationThrottle:
  const char* GetNameForLogging() override;
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;

  // Adds the navigation throttle to primary main frame navigations.
  static void MaybeCreateAndAdd(content::NavigationThrottleRegistry& registry);

 private:
  // A helper to handle both normal navigations and redirects.
  ThrottleCheckResult ProcessNavigation();
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_AIM_ELIGIBILITY_REFRESH_NAVIGATION_THROTTLE_H_
