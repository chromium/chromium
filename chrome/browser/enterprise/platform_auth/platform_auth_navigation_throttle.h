// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_PLATFORM_AUTH_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_PLATFORM_AUTH_NAVIGATION_THROTTLE_H_

#include "content/public/browser/navigation_throttle.h"
#include "net/http/http_request_headers.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace net {
class HttpRequestHeaders;
}  // namespace net

namespace enterprise_auth {
class PlatformAuthNavigationThrottle : public content::NavigationThrottle {
 public:
  static std::unique_ptr<PlatformAuthNavigationThrottle> MaybeCreateThrottleFor(
      content::NavigationHandle* navigation_handle);

  explicit PlatformAuthNavigationThrottle(
      content::NavigationHandle* navigation_handle);
  PlatformAuthNavigationThrottle(const PlatformAuthNavigationThrottle&) =
      delete;
  PlatformAuthNavigationThrottle& operator=(
      const PlatformAuthNavigationThrottle&) = delete;
  ~PlatformAuthNavigationThrottle() override;

  // content::NavigationThrottle implementation:
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  const char* GetNameForLogging() override;

 private:
  ThrottleCheckResult FetchHeaders();
  void FetchHeadersCallback(net::HttpRequestHeaders auth_headers);

  // Since `PlatformAuthProviderManager` can return data synchronously and
  // asynchronously, these variables ensure that the throttle doesn't:
  // - Defer if `FetchHeadersCallback` has already ran
  // - Resume if `FetchHeadersCallback` runs before the throttle is deferred
  bool fetch_headers_callback_ran_ = false;
  bool is_deferred_ = false;

  // Track which headers were attached to remove them from the request on
  // redirects.
  std::vector<std::string> attached_headers_;

  base::WeakPtrFactory<PlatformAuthNavigationThrottle> weak_ptr_factory_{this};
};
}  // namespace enterprise_auth

#endif  // CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_PLATFORM_AUTH_NAVIGATION_THROTTLE_H_
