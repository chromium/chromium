// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_NAVIGATION_THROTTLE_H_

#include "chrome/browser/enterprise/connectors/device_trust/device_trust_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/url_matcher/url_matcher.h"
#include "content/public/browser/navigation_throttle.h"

class GURL;

namespace enterprise_connectors {

// DeviceTrustNavigationThrottle provides a simple way to start a handshake
// base on a list of allowed URLs.
class DeviceTrustNavigationThrottle : public content::NavigationThrottle {
 public:
  static std::unique_ptr<DeviceTrustNavigationThrottle> MaybeCreateThrottleFor(
      content::NavigationHandle* navigation_handle);

  explicit DeviceTrustNavigationThrottle(
      content::NavigationHandle* navigation_handle);
  DeviceTrustNavigationThrottle(const DeviceTrustNavigationThrottle&) = delete;
  DeviceTrustNavigationThrottle& operator=(
      const DeviceTrustNavigationThrottle&) = delete;
  ~DeviceTrustNavigationThrottle() override;

  // NavigationThrottle overrides.
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  const char* GetNameForLogging() override;

 private:
  ThrottleCheckResult GetUrlThrottleResult(const GURL& url);

  void OnPolicyUpdate();

  content::NavigationThrottle::ThrottleCheckResult AddHeadersIfNeeded();

  DeviceTrustService* device_trust_service_;
  // The URL matcher created from the ContextAwareAccessSignalsAllowlist policy.
  std::unique_ptr<url_matcher::URLMatcher> matcher_;

  PrefChangeRegistrar pref_change_registrar_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_NAVIGATION_THROTTLE_H_
