// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_WEBSTORE_CHROME_WEB_STORE_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_ENTERPRISE_WEBSTORE_CHROME_WEB_STORE_NAVIGATION_THROTTLE_H_

#include "content/public/browser/navigation_throttle.h"

// This throttle checks if the navigation is to the Chrome Web Store. If so,
// sets a DM token and client ID in the HTTP request headers if available.
namespace enterprise_webstore {
class ChromeWebStoreNavigationThrottle : public content::NavigationThrottle {
 public:
  explicit ChromeWebStoreNavigationThrottle(
      content::NavigationThrottleRegistry& registry);
  ChromeWebStoreNavigationThrottle(const ChromeWebStoreNavigationThrottle&) =
      delete;
  ChromeWebStoreNavigationThrottle& operator=(
      const ChromeWebStoreNavigationThrottle&) = delete;
  ~ChromeWebStoreNavigationThrottle() override;

  // content::NavigationThrottle implementation:
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  const char* GetNameForLogging() override;

 private:
  ThrottleCheckResult MaybeAppendHeaders();
};

}  // namespace enterprise_webstore

#endif  // CHROME_BROWSER_ENTERPRISE_WEBSTORE_CHROME_WEB_STORE_NAVIGATION_THROTTLE_H_
