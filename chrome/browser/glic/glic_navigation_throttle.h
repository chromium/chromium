// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_GLIC_GLIC_NAVIGATION_THROTTLE_H_

#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationThrottleRegistry;
}  // namespace content

namespace glic {

// A NavigationThrottle that intercepts navigations to
// https://gemini.google.com/glic/continue URLs and opens the Glic UI.
class GlicNavigationThrottle : public content::NavigationThrottle {
 public:
  static void MaybeCreateAndAdd(content::NavigationThrottleRegistry& registry);

  explicit GlicNavigationThrottle(
      content::NavigationThrottleRegistry& registry);
  GlicNavigationThrottle(const GlicNavigationThrottle&) = delete;
  GlicNavigationThrottle& operator=(const GlicNavigationThrottle&) = delete;
  ~GlicNavigationThrottle() override;

  // content::NavigationThrottle implementation:
  ThrottleCheckResult WillStartRequest() override;
  const char* GetNameForLogging() override;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_NAVIGATION_THROTTLE_H_
