// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PRERENDER_DSE_PREWARM_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_PRELOADING_PRERENDER_DSE_PREWARM_NAVIGATION_THROTTLE_H_

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/preloading/prerender/search_prewarm_progress_service.h"
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
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  const char* GetNameForLogging() override;

  void OnSearchPrewarmFinished();

 private:
  ThrottleCheckResult CheckNoRaceWithDSEPrewarm();

  GURL dse_url_;

  // Prewarm page loading status tracker to throttle the concurrent requests to
  // search.
  base::WeakPtr<SearchPrewarmProgressService> prewarm_progress_service_;

  base::CallbackListSubscription prewarm_finished_subscription_;

  bool is_deferring_ = false;

  base::WeakPtrFactory<DSEPrewarmNavigationThrottle> weak_factory_{this};
};

#endif  // CHROME_BROWSER_PRELOADING_PRERENDER_DSE_PREWARM_NAVIGATION_THROTTLE_H_
