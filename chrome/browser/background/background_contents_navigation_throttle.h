// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BACKGROUND_BACKGROUND_CONTENTS_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_BACKGROUND_BACKGROUND_CONTENTS_NAVIGATION_THROTTLE_H_

#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationThrottleRegistry;
}  // namespace content

// NavigationThrottle for BackgroundContents that blocks any main-frame
// navigation or redirect to a URL outside the parent hosted app's web extent.
class BackgroundContentsNavigationThrottle
    : public content::NavigationThrottle {
 public:
  static void MaybeCreateAndAdd(content::NavigationThrottleRegistry& registry);

  explicit BackgroundContentsNavigationThrottle(
      content::NavigationThrottleRegistry& registry);
  BackgroundContentsNavigationThrottle(
      const BackgroundContentsNavigationThrottle&) = delete;
  BackgroundContentsNavigationThrottle& operator=(
      const BackgroundContentsNavigationThrottle&) = delete;
  ~BackgroundContentsNavigationThrottle() override;

  // content::NavigationThrottle:
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  const char* GetNameForLogging() override;

 private:
  ThrottleCheckResult WillStartOrRedirectRequest();
};

#endif  // CHROME_BROWSER_BACKGROUND_BACKGROUND_CONTENTS_NAVIGATION_THROTTLE_H_
