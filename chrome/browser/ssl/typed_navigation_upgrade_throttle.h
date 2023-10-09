// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_TYPED_NAVIGATION_UPGRADE_THROTTLE_H_
#define CHROME_BROWSER_SSL_TYPED_NAVIGATION_UPGRADE_THROTTLE_H_

#include <memory>

#include "base/timer/elapsed_timer.h"
#include "base/timer/timer.h"
#include "content/public/browser/navigation_throttle.h"
#include "url/gurl.h"

namespace content {
class NavigationHandle;
}

// Responsible for observing navigations that were typed in the omnibox
// and defaulted to HTTPS scheme and falling back to HTTP version when needed.
class TypedNavigationUpgradeThrottle : public content::NavigationThrottle {
 public:
  static std::unique_ptr<content::NavigationThrottle> MaybeCreateThrottleFor(
      content::NavigationHandle* handle);

  static bool IsNavigationUsingHttpsAsDefaultScheme(
      content::NavigationHandle* handle);

  ~TypedNavigationUpgradeThrottle() override;

  // content::NavigationThrottle:
  content::NavigationThrottle::ThrottleCheckResult WillStartRequest() override;
  content::NavigationThrottle::ThrottleCheckResult WillFailRequest() override;
  content::NavigationThrottle::ThrottleCheckResult WillRedirectRequest()
      override;
  content::NavigationThrottle::ThrottleCheckResult WillProcessResponse()
      override;
  const char* GetNameForLogging() override;

  // Sets the port used by the embedded https server. This is used to determine
  // the correct port while upgrading URLs to https if the original URL has a
  // non-default port.
  static void SetHttpsPortForTesting(int https_port_for_testing);
  // Sets the port used by the embedded http server. This is used to determine
  // the correct port while falling back to http if the upgraded https URL has a
  // non-default port.
  static void SetHttpPortForTesting(int http_port_for_testing);
  static int GetHttpsPortForTesting();

 private:
  explicit TypedNavigationUpgradeThrottle(content::NavigationHandle* handle);

  TypedNavigationUpgradeThrottle(const TypedNavigationUpgradeThrottle&) =
      delete;
  TypedNavigationUpgradeThrottle& operator=(
      const TypedNavigationUpgradeThrottle&) = delete;

  void OnHttpsLoadTimeout();

  // Initiates a new navigation to the HTTP version of the original navigation's
  // URL. If |stop_navigation| is true, also stops any pending navigation in the
  // current WebContents.
  void FallbackToHttp(bool stop_navigation);

  const GURL http_url_;
  base::OneShotTimer timer_;
};

#endif  // CHROME_BROWSER_SSL_TYPED_NAVIGATION_UPGRADE_THROTTLE_H_
