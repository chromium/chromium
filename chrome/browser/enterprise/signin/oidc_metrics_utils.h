// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_SIGNIN_OIDC_METRICS_UTILS_H_
#define CHROME_BROWSER_ENTERPRISE_SIGNIN_OIDC_METRICS_UTILS_H_

#include <optional>

#include "base/time/time.h"

// Steps of the OIDC profile enrolllment flow before profile registration. These
// values are persisted to logs and should not be renumbered. Please update
// the OidcInterceptionFunnelStep enum in enums.xml when adding a new step here.
enum class OidcInterceptionFunnelStep {
  kValidRedirectionCaptured = 0,
  kSuccessfulInfoParsed = 1,
  kEnrollmentStarted = 2,
  kConsetDialogShown = 3,
  kProfileRegistrationStarted = 4,
  kMaxValue = kProfileRegistrationStarted,
};

// Steps of the OIDC profile enrolllment flow after profile registration. These
// values are persisted to logs and should not be renumbered. Please update
// the OidcProfileCreationFunnelStep enum in enums.xml when adding a new step
// here.
enum class OidcProfileCreationFunnelStep {
  kProfileCreated = 0,
  kPolicyFetchStarted = 1,
  kAddingPrimaryAccount = 2,
  kMaxValue = kAddingPrimaryAccount,
};

// Outcomes of the OIDC profile enrolllment flow before profile registration.
// These values are persisted to logs and should not be renumbered. Please
// update the OidcInterceptionResult enum in enums.xml when adding a new value
// here.
enum class OidcInterceptionResult {
  kInterceptionInProgress = 0,
  kNoInterceptForCurrentProfile = 1,
  kInvalidUrlOrTokens = 2,
  kConsetDialogRejected = 3,
  kFailedToRegisterProfile = 4,
  kInvalidProfile = 5,
  kMaxValue = kInvalidProfile,
};

// Outcomes of the OIDC profile enrolllment flow after profile registration.
// These values are persisted to logs and should not be renumbered. Please
// update the OidcProfileCreationResult enum in enums.xml when adding a new
// value here.
enum class OidcProfileCreationResult {
  kEnrollmentSucceeded = 0,
  kSwitchedToExistingProfile = 1,
  kFailedToCreateProfile = 2,
  kFailedToFetchPolicy = 3,
  kFailedToAddPrimaryAccount = 4,
  kMismatchingProfileId = 5,
  kMaxValue = kMismatchingProfileId,
};

void RecordOidcInterceptionFunnelStep(OidcInterceptionFunnelStep step);

void RecordOidcInterceptionResult(OidcInterceptionResult result);

void RecordOidcProfileCreationFunnelStep(OidcProfileCreationFunnelStep step,
                                         bool is_dasher_based);

void RecordOidcProfileCreationResult(OidcProfileCreationResult result,
                                     bool is_dasher_based);

void RecordOidcEnrollmentRegistrationLatency(
    std::optional<bool> is_dasher_based,
    bool success,
    const base::TimeDelta latency);

void RecordOidcEnrollmentPolicyFetchLatency(bool is_dasher_based,
                                            bool success,
                                            const base::TimeDelta latency);

#endif  // CHROME_BROWSER_ENTERPRISE_SIGNIN_OIDC_METRICS_UTILS_H_
