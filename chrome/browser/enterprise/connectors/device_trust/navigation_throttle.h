// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_NAVIGATION_THROTTLE_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "content/public/browser/navigation_throttle.h"

namespace device_signals {
class UserPermissionService;
}  // namespace device_signals

class ConsentRequester;

namespace enterprise_connectors {

class DeviceTrustService;
struct DeviceTrustResponse;

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
  // Create a navigation throttle for the given navigation if device trust is
  // enabled.  Returns nullptr if no throttling should be done.
  static std::unique_ptr<DeviceTrustNavigationThrottle> MaybeCreateThrottleFor(
      content::NavigationHandle* navigation_handle);

  DeviceTrustNavigationThrottle(
      DeviceTrustService* device_trust_service,
      device_signals::UserPermissionService* user_permission_service,
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
  content::NavigationThrottle::ThrottleCheckResult MayTriggerConsentDialog();
  content::NavigationThrottle::ThrottleCheckResult AddHeadersIfNeeded();

  // Not owned, profile-keyed service.
  const raw_ptr<DeviceTrustService> device_trust_service_;

  // Not owned, profile-keyed service.
  const raw_ptr<device_signals::UserPermissionService> user_permission_service_;

  // Resumes the navigation by setting a value into the header
  // `X-Verified-Access-Challenge-Response` of the redirection request to the
  // IdP and resume the navigation. That value is determined by the properties
  // of `dt_response` which, when in success cases, contains a valid response
  // string. `start_time` is used to measure the latency of the end-to-end flow.
  void ReplyChallengeResponseAndResume(base::TimeTicks start_time,
                                       const DeviceTrustResponse& dt_response);

  // Invoked when generation of the challenge response timed out.
  void OnResponseTimedOut(base::TimeTicks start_time);

  // Callback function for when kDeviceSignalsConsentReceived is updated
  // to true.
  void OnConsentPrefUpdated();

  std::unique_ptr<ConsentRequester> consent_requester_;

  // Only set to true when a challenge response (or timeout) resumed the
  // throttled navigation.
  bool is_resumed_{false};

  base::WeakPtrFactory<DeviceTrustNavigationThrottle> weak_ptr_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_NAVIGATION_THROTTLE_H_
