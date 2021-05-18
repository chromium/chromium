// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_NAVIGATION_THROTTLE_H_

#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/navigation_throttle.h"

class GURL;

namespace url_matcher {

class URLMatcher;

}

namespace enterprise_connectors {

class DeviceTrustService;

// DeviceTrustNavigationThrottle provides a simple way to start a handshake
// base on a list of allowed URLs.
// Handshake is an exchange between Chrome and an IdP. This start when Chrome
// visit one of the allowed list of URLs set in the policy
// `ContextAwareAccessSignalsAllowlist`. Chrome will add a HTTP header
// (X-Device-Trust: VerifiedAccess), when the IdP detect that header it should
// reply with a challenge from Verified Access. Chrome will take this challenge
// and build a challenge response that is sent back to the IdP using another
// HTTP header (X-Verified-Access-Challenge-Response).
class DeviceTrustNavigationThrottle : public content::NavigationThrottle {
 public:
  static std::unique_ptr<DeviceTrustNavigationThrottle> MaybeCreateThrottleFor(
      content::NavigationHandle* navigation_handle);

  using AttestationCallback = base::OnceCallback<void(const std::string&)>;

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

  // Not owned.
  DeviceTrustService* device_trust_service_;

  // Set `challege_response` into the header
  // `X-Verified-Access-Challenge-Response` of the redirection request to the
  // IdP and resume the navigation.
  void ReplyChallengeResponseAndResume(const std::string& challenge_response);

  // The URL matcher created from the ContextAwareAccessSignalsAllowlist policy.
  std::unique_ptr<url_matcher::URLMatcher> matcher_;

  PrefChangeRegistrar pref_change_registrar_;

  base::WeakPtrFactory<DeviceTrustNavigationThrottle> weak_ptr_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_NAVIGATION_THROTTLE_H_
