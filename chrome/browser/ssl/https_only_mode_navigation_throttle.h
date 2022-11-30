// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_HTTPS_ONLY_MODE_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_SSL_HTTPS_ONLY_MODE_NAVIGATION_THROTTLE_H_

#include <memory>

#include "base/timer/timer.h"
#include "components/security_interstitials/content/security_blocking_page_factory.h"
#include "content/public/browser/navigation_throttle.h"
#include "url/gurl.h"

class PrefService;

// HttpsOnlyModeNavigationThrottle is responsible for observing HTTPS-Only Mode
// navigations that have been upgraded by HttpsOnlyModeUpgradeInterceptor,
// timing them out if they take too long, and handling failure by loading the
// HTTPS-Only Mode interstitial.
//
// Metadata about the navigation state (as it pertains to HTTPS-Only Mode)
// shared between HttpsOnlyModeUpgradeInterceptor and
// HttpsOnlyModeNavigationThrottle is stored in an HttpsOnlyModeTabHelper set
// as user-data on the WebContents in which the navigation occurs. (Such
// metadata might ordinarily be added to ChromeNavigationUIData, but the
// Interceptor only receives a clone of the data, so it can't be used as a
// channel between these classes.)
class HttpsOnlyModeNavigationThrottle : public content::NavigationThrottle {
 public:
  static std::unique_ptr<HttpsOnlyModeNavigationThrottle>
  MaybeCreateThrottleFor(
      content::NavigationHandle* handle,
      std::unique_ptr<SecurityBlockingPageFactory> blocking_page_factory,
      PrefService* prefs);

  HttpsOnlyModeNavigationThrottle(
      content::NavigationHandle* handle,
      std::unique_ptr<SecurityBlockingPageFactory> blocking_page_factory);
  ~HttpsOnlyModeNavigationThrottle() override;

  HttpsOnlyModeNavigationThrottle(const HttpsOnlyModeNavigationThrottle&) =
      delete;
  HttpsOnlyModeNavigationThrottle& operator=(
      const HttpsOnlyModeNavigationThrottle&) = delete;

  // content::NavigationThrottle:
  content::NavigationThrottle::ThrottleCheckResult WillRedirectRequest()
      override;
  content::NavigationThrottle::ThrottleCheckResult WillFailRequest() override;
  content::NavigationThrottle::ThrottleCheckResult WillProcessResponse()
      override;
  const char* GetNameForLogging() override;

  static void set_timeout_for_testing(int timeout_in_seconds);

 private:
  std::unique_ptr<SecurityBlockingPageFactory> blocking_page_factory_;
};

#endif  // CHROME_BROWSER_SSL_HTTPS_ONLY_MODE_NAVIGATION_THROTTLE_H_
