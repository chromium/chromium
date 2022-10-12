// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_COMMON_COMMON_TYPES_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_COMMON_COMMON_TYPES_H_

#include <string>

namespace enterprise_connectors {

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
  kFailedToSerializeSignals = 10,
  kMaxValue = kFailedToSerializeSignals,
};

struct AttestationResponse {
  std::string challenge_response{};
  DTAttestationResult result_code{};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_COMMON_COMMON_TYPES_H_
