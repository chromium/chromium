// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PWC_PWC_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_PWC_PWC_NAVIGATION_THROTTLE_H_

#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationThrottleRegistry;
}  // namespace content

namespace pwc {

// Locks the main frame of a PrivilegedWebContents to the origins its policy
// allows (HTTPS-only). An off-allowlist main-frame navigation is cancelled at
// request start, at every redirect, and when it would commit without a URL
// loader. The last case matters because e.g. a subframe can navigate the main
// frame to about:blank (which inherits the subframe's, possibly off-allowlist,
// origin) without going through WillStartRequest(). Both the primary main frame
// and a prerendered main frame are covered. PrivilegedWebContents intends to
// disable prerendering altogether (via IsPrerender2Supported()), so the
// prerendered-main-frame coverage here is defense in depth for the case where
// prerendering is somehow enabled, and this throttle does not rely on that
// being disabled. Subframes and other frame trees are untouched. The throttle
// is only added for navigations in a PrivilegedWebContents.
class PwcNavigationThrottle : public content::NavigationThrottle {
 public:
  static void MaybeCreateAndAdd(content::NavigationThrottleRegistry& registry);

  explicit PwcNavigationThrottle(content::NavigationThrottleRegistry& registry);
  PwcNavigationThrottle(const PwcNavigationThrottle&) = delete;
  PwcNavigationThrottle& operator=(const PwcNavigationThrottle&) = delete;
  ~PwcNavigationThrottle() override;

  // content::NavigationThrottle:
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  ThrottleCheckResult WillCommitWithoutUrlLoader() override;
  const char* GetNameForLogging() override;

 private:
  ThrottleCheckResult CheckUrl();
};

}  // namespace pwc

#endif  // CHROME_BROWSER_PWC_PWC_NAVIGATION_THROTTLE_H_
