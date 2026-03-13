// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PRERENDER_DSE_PREWARM_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_PRELOADING_PRERENDER_DSE_PREWARM_NAVIGATION_THROTTLE_H_

#include "base/memory/weak_ptr.h"
#include "content/public/browser/navigation_throttle.h"
#include "url/gurl.h"

namespace content {
class NavigationThrottleRegistry;
}  // namespace content

// DSEPrewarmNavigationThrottle defers user navigations to the search origin
// when a DSE prewarm request is ongoing. This prevents race conditions where
// the prewarm request might override cookies set by other requests.
class DSEPrewarmNavigationThrottle : public content::NavigationThrottle {
 public:
  static void MaybeCreateAndAdd(content::NavigationThrottleRegistry& registry);

  explicit DSEPrewarmNavigationThrottle(
      content::NavigationThrottleRegistry& registry);
  ~DSEPrewarmNavigationThrottle() override;

  DSEPrewarmNavigationThrottle(const DSEPrewarmNavigationThrottle&) = delete;
  DSEPrewarmNavigationThrottle& operator=(const DSEPrewarmNavigationThrottle&) =
      delete;

  // content::NavigationThrottle:
  // TODO(crbug.com/485414743): Add WillRedirectRequest handling.
  ThrottleCheckResult WillStartRequest() override;
  const char* GetNameForLogging() override;

 private:
  void OnSearchPrewarmFinished();

  GURL dse_url_;

  base::WeakPtrFactory<DSEPrewarmNavigationThrottle> weak_factory_{this};
};

#endif  // CHROME_BROWSER_PRELOADING_PRERENDER_DSE_PREWARM_NAVIGATION_THROTTLE_H_
