// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NAVIGATION_THROTTLE_H_

#include "base/memory/weak_ptr.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationThrottleRegistry;
}

// Throttles navigations in tabs under the control of a DevTools Protocol (CDP)
// session
class DevToolsNavigationThrottle : public content::NavigationThrottle {
 public:
  static void MaybeCreateAndAdd(content::NavigationThrottleRegistry& registry);

  explicit DevToolsNavigationThrottle(
      content::NavigationThrottleRegistry& registry);

  DevToolsNavigationThrottle(const DevToolsNavigationThrottle&) = delete;
  DevToolsNavigationThrottle& operator=(const DevToolsNavigationThrottle&) =
      delete;
  DevToolsNavigationThrottle(DevToolsNavigationThrottle&&) = delete;
  DevToolsNavigationThrottle& operator=(DevToolsNavigationThrottle&&) = delete;

  ~DevToolsNavigationThrottle() override;

  // content::NavigationThrottle implementation:
  content::NavigationThrottle::ThrottleCheckResult WillStartRequest() override;
  content::NavigationThrottle::ThrottleCheckResult WillRedirectRequest()
      override;
  const char* GetNameForLogging() override;

 private:
  content::NavigationThrottle::ThrottleCheckResult WillStartOrRedirectRequest();
  void OnGatingDecision(bool is_allowed);

  base::WeakPtrFactory<DevToolsNavigationThrottle> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NAVIGATION_THROTTLE_H_
