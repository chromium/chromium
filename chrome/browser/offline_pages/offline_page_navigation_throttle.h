// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_OFFLINE_PAGE_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_OFFLINE_PAGES_OFFLINE_PAGE_NAVIGATION_THROTTLE_H_

#include <memory>

#include "content/public/browser/navigation_throttle.h"

namespace offline_pages {

extern const char kOfflinePagesDidNavigationThrottleCancelNavigation[];

// OfflinePageNavigationThrottle cancels any request coming from the renderer
// that include the "X-Chrome-offline" header.
// TODO(crbug.com/40054839): Remove this class once OfflinePageHeader has been
// refactored.
class OfflinePageNavigationThrottle : public content::NavigationThrottle {
 public:
  explicit OfflinePageNavigationThrottle(
      content::NavigationHandle* navigation_handle);
  ~OfflinePageNavigationThrottle() override;

  OfflinePageNavigationThrottle(const OfflinePageNavigationThrottle&) = delete;
  OfflinePageNavigationThrottle& operator=(
      const OfflinePageNavigationThrottle&) = delete;

  // content::NavigationThrottle:
  ThrottleCheckResult WillStartRequest() override;
  const char* GetNameForLogging() override;

  static std::unique_ptr<NavigationThrottle> MaybeCreateThrottleFor(
      content::NavigationHandle* navigation_handle);
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_OFFLINE_PAGE_NAVIGATION_THROTTLE_H_
