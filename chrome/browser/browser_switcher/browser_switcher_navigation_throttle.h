// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_NAVIGATION_THROTTLE_H_

#include "content/public/browser/navigation_throttle.h"

namespace browser_switcher {

// Listens for navigation starts and redirects. On navigation start and
// redirect, may open an alternative browser and close the tab.
class BrowserSwitcherNavigationThrottle {
 public:
  BrowserSwitcherNavigationThrottle() = delete;
  BrowserSwitcherNavigationThrottle(const BrowserSwitcherNavigationThrottle&) =
      delete;
  BrowserSwitcherNavigationThrottle& operator=(
      const BrowserSwitcherNavigationThrottle&) = delete;

  // Creates a |NavigationThrottle| if needed for the navigation.
  static std::unique_ptr<content::NavigationThrottle> MaybeCreateThrottleFor(
      content::NavigationHandle* navigation);
};

}  // namespace browser_switcher

#endif  // CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_NAVIGATION_THROTTLE_H_
