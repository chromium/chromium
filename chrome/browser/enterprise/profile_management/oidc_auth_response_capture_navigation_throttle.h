// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_PROFILE_MANAGEMENT_OIDC_AUTH_RESPONSE_CAPTURE_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_ENTERPRISE_PROFILE_MANAGEMENT_OIDC_AUTH_RESPONSE_CAPTURE_NAVIGATION_THROTTLE_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "components/url_matcher/url_matcher.h"
#include "content/public/browser/navigation_throttle.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace profile_management {

// This throttle looks for redirection from Oidc authentications to the hard
// coded host `chromeprofiletoken`. It will capture the redirection and try to
// create or switch to a managed profile using the tokens from the auth
// response. The workflow is currently experimental and not productionized.
class OidcAuthResponseCaptureNavigationThrottle
    : public content::NavigationThrottle {
 public:
  // Create a navigation throttle for the given navigation if Oidc
  // authentication based enrollment is enabled. Returns nullptr if no
  // throttling should be done.
  static std::unique_ptr<OidcAuthResponseCaptureNavigationThrottle>
  MaybeCreateThrottleFor(content::NavigationHandle* navigation_handle);

  explicit OidcAuthResponseCaptureNavigationThrottle(
      content::NavigationHandle* navigation_handle);

  OidcAuthResponseCaptureNavigationThrottle(
      const OidcAuthResponseCaptureNavigationThrottle&) = delete;
  OidcAuthResponseCaptureNavigationThrottle& operator=(
      const OidcAuthResponseCaptureNavigationThrottle&) = delete;
  ~OidcAuthResponseCaptureNavigationThrottle() override;

  // content::NavigationThrottle implementation:
  ThrottleCheckResult WillRedirectRequest() override;
  ThrottleCheckResult WillProcessResponse() override;

  const char* GetNameForLogging() override;

  // Method to get a new URL matcher instead of the usual static one for
  // testing, due the feature flag value may have changed in different cases.
  static std::unique_ptr<url_matcher::URLMatcher>
  GetOidcEnrollmentUrlMatcherForTesting();

 private:
  ThrottleCheckResult AttemptToTriggerInterception();

  // Starts OIDC registration and profile creation process if the response is
  // valid.
  void RegisterWithOidcTokens(ProfileManagementOidcTokens tokens,
                              data_decoder::DataDecoder::ValueOrError result);

  bool interception_triggered_ = false;
  base::WeakPtrFactory<OidcAuthResponseCaptureNavigationThrottle>
      weak_ptr_factory_{this};
};

}  // namespace profile_management

#endif  // CHROME_BROWSER_ENTERPRISE_PROFILE_MANAGEMENT_OIDC_AUTH_RESPONSE_CAPTURE_NAVIGATION_THROTTLE_H_
