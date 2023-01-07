// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_navigation_throttle.h"

#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_contents.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace prerender {

NoStatePrefetchNavigationThrottle::NoStatePrefetchNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle) {}

NoStatePrefetchNavigationThrottle::~NoStatePrefetchNavigationThrottle() =
    default;

content::NavigationThrottle::ThrottleCheckResult
NoStatePrefetchNavigationThrottle::WillRedirectRequest() {
  return CancelIfCrossOrigin();
}

content::NavigationThrottle::ThrottleCheckResult
NoStatePrefetchNavigationThrottle::WillStartRequest() {
  return CancelIfCrossOrigin();
}

content::NavigationThrottle::ThrottleCheckResult
NoStatePrefetchNavigationThrottle::CancelIfCrossOrigin() {
  GURL prefetch_url = navigation_handle()->GetURL();
  if (!prefetch_url.SchemeIsHTTPOrHTTPS()) {
    return {CANCEL, net::ERR_BLOCKED_BY_CLIENT};
  }
  DCHECK(navigation_handle()->GetInitiatorOrigin());
  if (!navigation_handle()->GetInitiatorOrigin()->IsSameOriginWith(
          prefetch_url)) {
    return {CANCEL, net::ERR_BLOCKED_BY_CLIENT};
  }
  return PROCEED;
}

const char* NoStatePrefetchNavigationThrottle::GetNameForLogging() {
  return "NoStatePrefetchNavigationThrottle";
}

// static
std::unique_ptr<content::NavigationThrottle>
NoStatePrefetchNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* navigation_handle) {
  prerender::NoStatePrefetchManager* no_state_prefetch_manager =
      prerender::NoStatePrefetchManagerFactory::GetForBrowserContext(
          navigation_handle->GetWebContents()->GetBrowserContext());
  if (no_state_prefetch_manager) {
    auto* no_state_prefetch_contents =
        no_state_prefetch_manager->GetNoStatePrefetchContents(
            navigation_handle->GetWebContents());
    if (no_state_prefetch_contents && no_state_prefetch_contents->origin() ==
                                          ORIGIN_SAME_ORIGIN_SPECULATION) {
      return std::make_unique<NoStatePrefetchNavigationThrottle>(
          navigation_handle);
    }
  }
  return nullptr;
}

}  // namespace prerender
