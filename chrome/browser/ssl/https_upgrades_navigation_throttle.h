// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_HTTPS_UPGRADES_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_SSL_HTTPS_UPGRADES_NAVIGATION_THROTTLE_H_

#include <memory>

#include "components/security_interstitials/content/security_blocking_page_factory.h"
#include "content/public/browser/navigation_throttle.h"

class PrefService;

// HttpsUpgradesNavigationThrottle is responsible for observing HTTPS-First Mode
// navigations that have been upgraded by HttpsUpgradesInterceptor, timing them
// out if they take too long, and catching fallback navigations to HTTP and
// triggering the HTTPS-First Mode interstitial.
//
// Metadata about the navigation state (as it pertains to HTTPS-First Mode)
// shared between HttpsUpgradesInterceptor and HttpsUpgradesNavigationThrottle
// is stored in an HttpsOnlyModeTabHelper set as user-data on the WebContents in
// which the navigation occurs. (Such metadata might ordinarily be added to
// ChromeNavigationUIData, but the Interceptor only receives a clone of the
// data, so it can't be used as a channel between these classes.)
class HttpsUpgradesNavigationThrottle : public content::NavigationThrottle {
 public:
  static std::unique_ptr<HttpsUpgradesNavigationThrottle>
  MaybeCreateThrottleFor(
      content::NavigationHandle* handle,
      std::unique_ptr<SecurityBlockingPageFactory> blocking_page_factory,
      PrefService* prefs);

  HttpsUpgradesNavigationThrottle(
      content::NavigationHandle* handle,
      std::unique_ptr<SecurityBlockingPageFactory> blocking_page_factory,
      bool http_interstitial_enabled);
  ~HttpsUpgradesNavigationThrottle() override;

  HttpsUpgradesNavigationThrottle(const HttpsUpgradesNavigationThrottle&) =
      delete;
  HttpsUpgradesNavigationThrottle& operator=(
      const HttpsUpgradesNavigationThrottle&) = delete;

  // content::NavigationThrottle:
  content::NavigationThrottle::ThrottleCheckResult WillStartRequest() override;
  content::NavigationThrottle::ThrottleCheckResult WillRedirectRequest()
      override;
  content::NavigationThrottle::ThrottleCheckResult WillFailRequest() override;
  content::NavigationThrottle::ThrottleCheckResult WillProcessResponse()
      override;
  const char* GetNameForLogging() override;

  static void set_timeout_for_testing(int timeout_in_seconds);

 private:
  std::unique_ptr<SecurityBlockingPageFactory> blocking_page_factory_;

  // Whether the throttle should trigger the interstitial warning before
  // navigating to the HTTP fallback URL.
  bool http_interstitial_enabled_;
};

#endif  // CHROME_BROWSER_SSL_HTTPS_UPGRADES_NAVIGATION_THROTTLE_H_
