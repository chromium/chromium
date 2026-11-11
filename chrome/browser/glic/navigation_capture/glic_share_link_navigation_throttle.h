// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_NAVIGATION_CAPTURE_GLIC_SHARE_LINK_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_GLIC_NAVIGATION_CAPTURE_GLIC_SHARE_LINK_NAVIGATION_THROTTLE_H_

#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationThrottleRegistry;
}  // namespace content

namespace glic {

class GlicShareLinkNavigationThrottle : public content::NavigationThrottle {
 public:
  static void MaybeCreateAndAdd(content::NavigationThrottleRegistry& registry);

  explicit GlicShareLinkNavigationThrottle(
      content::NavigationThrottleRegistry& registry);
  GlicShareLinkNavigationThrottle(const GlicShareLinkNavigationThrottle&) =
      delete;
  GlicShareLinkNavigationThrottle& operator=(
      const GlicShareLinkNavigationThrottle&) = delete;
  ~GlicShareLinkNavigationThrottle() override;

  // content::NavigationThrottle:
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  const char* GetNameForLogging() override;

 protected:
  virtual void InvokeGlic();

 private:
  ThrottleCheckResult HandleRequest();
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_NAVIGATION_CAPTURE_GLIC_SHARE_LINK_NAVIGATION_THROTTLE_H_
