// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_NAVIGATION_THROTTLE_H_

#include "base/callback_list.h"
#include "base/values.h"
#include "content/public/browser/navigation_throttle.h"

class GURL;

namespace url_matcher {

class URLMatcher;

}

namespace enterprise_connectors {

class DeviceTrustService;

// DeviceTrustNavigationThrottle provides a simple way to start a handshake
// between Chrome and an origin based on a list of trusted URLs set in the
// `ContextAwareAccessSignalsAllowlist` policy.
//
// The handshake begins when the user visits a trusted URL. Chrome
// adds the (X-Device-Trust: VerifiedAccess) HTTP header to the request.
// When the origin detects this header it responds with a 302 redirect that
// includes a Verified Access challenge in the X-Verified-Access-Challenge HTTP
// header. Chrome uses the challenge to build a challenge response that is sent
// back to the origin via the X-Verified-Access-Challenge-Response HTTP header.
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

  void OnTrustedUrlPatternsChanged(const base::ListValue*);

  content::NavigationThrottle::ThrottleCheckResult AddHeadersIfNeeded();

  // Not owned.
  DeviceTrustService* device_trust_service_;

  // Set `challege_response` into the header
  // `X-Verified-Access-Challenge-Response` of the redirection request to the
  // IdP and resume the navigation.
  void ReplyChallengeResponseAndResume(const std::string& challenge_response);

  // The URL matcher created from the ContextAwareAccessSignalsAllowlist policy.
  std::unique_ptr<url_matcher::URLMatcher> matcher_;

  // Subscription for trusted URL pattern changes.
  base::CallbackListSubscription subscription_;

  base::WeakPtrFactory<DeviceTrustNavigationThrottle> weak_ptr_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_NAVIGATION_THROTTLE_H_
