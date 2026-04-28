// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OMNIBOX_GEOLOCATION_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_OMNIBOX_GEOLOCATION_NAVIGATION_THROTTLE_H_

#include <memory>

#include "content/public/browser/navigation_throttle.h"

class GeolocationNavigationThrottle : public content::NavigationThrottle {
 public:
  static std::unique_ptr<content::NavigationThrottle> MaybeCreateThrottleFor(
      content::NavigationThrottleRegistry& registry);

  explicit GeolocationNavigationThrottle(
      content::NavigationThrottleRegistry& registry);
  ~GeolocationNavigationThrottle() override;

  GeolocationNavigationThrottle(const GeolocationNavigationThrottle&) = delete;
  GeolocationNavigationThrottle& operator=(
      const GeolocationNavigationThrottle&) = delete;

  // content::NavigationThrottle:
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  const char* GetNameForLogging() override;

 private:
  ThrottleCheckResult ProcessNavigation();
};
#endif  // CHROME_BROWSER_OMNIBOX_GEOLOCATION_NAVIGATION_THROTTLE_H_
