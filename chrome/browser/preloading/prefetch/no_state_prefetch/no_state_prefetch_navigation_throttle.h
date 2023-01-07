// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PREFETCH_NO_STATE_PREFETCH_NO_STATE_PREFETCH_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_PRELOADING_PREFETCH_NO_STATE_PREFETCH_NO_STATE_PREFETCH_NAVIGATION_THROTTLE_H_

#include "content/public/browser/navigation_throttle.h"

namespace prerender {

// A navigation throttle that ensures NoState Prefetch navigations stay same
// origin when created with the same origin type.
class NoStatePrefetchNavigationThrottle : public content::NavigationThrottle {
 public:
  explicit NoStatePrefetchNavigationThrottle(
      content::NavigationHandle* navigation_handle);
  ~NoStatePrefetchNavigationThrottle() override;

  NoStatePrefetchNavigationThrottle(const NoStatePrefetchNavigationThrottle&) =
      delete;
  NoStatePrefetchNavigationThrottle& operator=(
      const NoStatePrefetchNavigationThrottle&) = delete;

  // content::NavigationThrottle:
  content::NavigationThrottle::ThrottleCheckResult WillRedirectRequest()
      override;
  content::NavigationThrottle::ThrottleCheckResult WillStartRequest() override;
  const char* GetNameForLogging() override;

  // Creates a navigation throttle when NoState Prefetch contents are limited to
  // same origin.
  static std::unique_ptr<NavigationThrottle> MaybeCreateThrottleFor(
      content::NavigationHandle* navigation_handle);

 private:
  // Called by |WillRedirectRequest()| and |WillStartRequest()|. Cancels
  // navigations that are cross origin from the initator origin.
  content::NavigationThrottle::ThrottleCheckResult CancelIfCrossOrigin();
};

}  // namespace prerender

#endif  // CHROME_BROWSER_PRELOADING_PREFETCH_NO_STATE_PREFETCH_NO_STATE_PREFETCH_NAVIGATION_THROTTLE_H_
