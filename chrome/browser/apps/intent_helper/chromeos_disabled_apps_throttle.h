// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_INTENT_HELPER_CHROMEOS_DISABLED_APPS_THROTTLE_H_
#define CHROME_BROWSER_APPS_INTENT_HELPER_CHROMEOS_DISABLED_APPS_THROTTLE_H_

#include <memory>

#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace apps {

// Allows cancelling a navigation to a disabled app & showing the disabled app
// page.
class ChromeOsDisabledAppsThrottle : public content::NavigationThrottle {
 public:
  using ThrottleCheckResult = content::NavigationThrottle::ThrottleCheckResult;

  // Possibly creates a navigation throttle that checks if the given url is
  // part of a disabled app.
  static std::unique_ptr<content::NavigationThrottle> MaybeCreate(
      content::NavigationHandle* handle);

  ChromeOsDisabledAppsThrottle(const ChromeOsDisabledAppsThrottle&) = delete;
  ChromeOsDisabledAppsThrottle& operator=(const ChromeOsDisabledAppsThrottle&) =
      delete;
  ~ChromeOsDisabledAppsThrottle() override;

  // content::NavigationHandle overrides
  const char* GetNameForLogging() override;
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;

 private:
  explicit ChromeOsDisabledAppsThrottle(
      content::NavigationHandle* navigation_handle);
  ThrottleCheckResult HandleRequest();
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_INTENT_HELPER_CHROMEOS_DISABLED_APPS_THROTTLE_H_
