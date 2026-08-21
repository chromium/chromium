// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_GUEST_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_GUEST_NAVIGATION_THROTTLE_H_

#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationThrottleRegistry;
}  // namespace content

namespace glic {

class GlicGuestNavigationThrottle : public content::NavigationThrottle {
 public:
  static void MaybeCreateAndAdd(content::NavigationThrottleRegistry& registry);

  explicit GlicGuestNavigationThrottle(
      content::NavigationThrottleRegistry& registry);
  ~GlicGuestNavigationThrottle() override;
  GlicGuestNavigationThrottle(const GlicGuestNavigationThrottle&) = delete;
  GlicGuestNavigationThrottle& operator=(const GlicGuestNavigationThrottle&) =
      delete;

  // content::NavigationThrottle:
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  const char* GetNameForLogging() override;

 private:
  ThrottleCheckResult HandleRequest();
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_GUEST_NAVIGATION_THROTTLE_H_
