// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_COMMON_METRICS_UTILS_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_COMMON_METRICS_UTILS_H_

#include <set>

#include "base/time/time.h"
#include "chrome/browser/enterprise/connectors/device_trust/common/common_types.h"

namespace enterprise_connectors {

// Various funnel steps of the Device Trust connector attestation flow. These
// values are persisted to logs and should not be renumbered. Please update
// the DTAttestationFunnelStep enum in enums.xml when adding a new step here.
enum class DTAttestationFunnelStep {
  kAttestationFlowStarted = 0,
  kChallengeReceived = 1,
  kSignalsCollected = 2,
  kChallengeResponseSent = 3,
  kMaxValue = kChallengeResponseSent,
};

// Top-level result of a complete Device Trust handshake flow between the
// browser and a server. Please update the DTHandshakeResult in enums.xml when
// adding new enum values.
enum class DTHandshakeResult {
  kSuccess = 0,
  kUnknown = 1,
  kTimeout = 2,
  kFailedToParseChallenge = 3,
  kFailedToCreateResponse = 4,
  kMaxValue = kFailedToCreateResponse
};

// Policy levels that are enabled for the Device Trust connector which determine
// the result of the challenge response in the attestation flow. These values
// are persisted to logs and should not be renumbered. Please update the
// DTAttestationPolicyLevel enum in enums.xml when adding a new step here.
enum class DTAttestationPolicyLevel {
  kNone = 0,
  kUnknown = 1,
  kBrowser = 2,
  kUser = 3,
  kUserAndBrowser = 4,
  kMaxValue = kUserAndBrowser
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Possible origins of the Device Trust connector attestation flow on ChromeOS.
// These values are persisted to logs and should not be renumbered. Please
// update the DTOrigins enum in enums.xml when adding a new step here.
enum class DTOrigin {
  kInSession = 0,
  kLoginScreen = 1,
  kMaxValue = kLoginScreen,
};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void LogAttestationFunnelStep(DTAttestationFunnelStep step);

void LogAttestationPolicyLevel(const std::set<DTCPolicyLevel>& levels);

void LogAttestationResult(DTAttestationResult result);

void LogDeviceTrustResponse(const DeviceTrustResponse& response,
                            base::TimeTicks start_time);

#if BUILDFLAG(IS_CHROMEOS_ASH)
void LogOrigin(DTOrigin origin);

void LogEnrollmentStatus();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_COMMON_METRICS_UTILS_H_
