// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PREFETCH_SEARCH_PREFETCH_SEARCH_PREFETCH_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_PRELOADING_PREFETCH_SEARCH_PREFETCH_SEARCH_PREFETCH_NAVIGATION_THROTTLE_H_

#include <stdint.h>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/navigation_throttle.h"

class SearchPrefetchService;

namespace content {
class NavigationThrottleRegistry;
}  // namespace content

// Marks `NavigationHandleUserData` when a navigation is served by search
// prefetch.
//
// While `//content` can access `content::NavigationHandle` from
// `FrameTreeNodeId` inside a `URLLoaderInterceptor` (via `FrameTreeNode`),
// `//chrome` cannot access it because `FrameTreeNode` is not exposed across the
// Content API boundary. As a result, `SearchPrefetchURLLoaderInterceptor`
// cannot directly mark `page_load_metrics::NavigationHandleUserData`. To bridge
// this gap:
//
// - `SearchPrefetchURLLoaderInterceptor` registers the navigation ID with
//   `SearchPrefetchService` upon intercepting a request.
// - This throttle checks `SearchPrefetchService::IsServingNavigation()` in
//   `WillProcessResponse()` to mark `NavigationHandleUserData`.
// - The tracked navigation ID is cleaned up in this throttle's destructor.
class SearchPrefetchNavigationThrottle : public content::NavigationThrottle {
 public:
  static void MaybeCreateAndAdd(content::NavigationThrottleRegistry& registry);

  explicit SearchPrefetchNavigationThrottle(
      content::NavigationThrottleRegistry& registry,
      base::WeakPtr<SearchPrefetchService> search_prefetch_service);
  ~SearchPrefetchNavigationThrottle() override;

  SearchPrefetchNavigationThrottle(const SearchPrefetchNavigationThrottle&) =
      delete;
  SearchPrefetchNavigationThrottle& operator=(
      const SearchPrefetchNavigationThrottle&) = delete;

  // content::NavigationThrottle:
  ThrottleCheckResult WillProcessResponse() override;
  const char* GetNameForLogging() override;

 private:
  base::WeakPtr<SearchPrefetchService> search_prefetch_service_;
  // Navigation ID cached at construction time.
  //
  // In `NavigationRequest`, `navigation_id_` is declared after
  // `throttle_registry_` and is destroyed before `throttle_registry_` is
  // destroyed. Accessing `navigation_handle()->GetNavigationId()` in
  // `~SearchPrefetchNavigationThrottle()` would result in a use-after-dtor
  // (MSan use-of-uninitialized-value).
  const int64_t navigation_id_;
};

#endif  // CHROME_BROWSER_PRELOADING_PREFETCH_SEARCH_PREFETCH_SEARCH_PREFETCH_NAVIGATION_THROTTLE_H_
