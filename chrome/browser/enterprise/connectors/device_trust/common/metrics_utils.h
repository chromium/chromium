// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_COMMON_METRICS_UTILS_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_COMMON_METRICS_UTILS_H_

#include "base/time/time.h"

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

// Various possible outcomes to the attestation step in the overarching Device
// Trust connector attestation flow. These values are persisted to logs and
// should not be renumbered. Please update the DTAttestationResult enum in
// enums.xml when adding a new value here.
enum class DTAttestationResult {
  kMissingCoreSignals = 0,
  kMissingSigningKey = 1,
  kBadChallengeFormat = 2,
  kBadChallengeSource = 3,
  kFailedToSerializeKeyInfo = 4,
  kFailedToGenerateResponse = 5,
  kFailedToSignResponse = 6,
  kFailedToSerializeResponse = 7,
  kEmptySerializedResponse = 8,
  kSuccess = 9,
  kMaxValue = kSuccess,
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

void LogAttestationResult(DTAttestationResult result);

void LogAttestationResponseLatency(base::TimeTicks start_time, bool success);

#if BUILDFLAG(IS_CHROMEOS_ASH)
void LogOrigin(DTOrigin origin);

void LogEnrollmentStatus();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_COMMON_METRICS_UTILS_H_
