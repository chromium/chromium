// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_COMMON_COMMON_TYPES_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_COMMON_COMMON_TYPES_H_

#include <string>

#include "third_party/abseil-cpp/absl/types/optional.h"

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

// Enum representing all possible errors that may cause the generation of a
// challenge response to fail as part of the device identity attestation flow.
enum class DeviceTrustError {
  kUnknown = 0,
  kTimeout,
  kFailedToParseChallenge,
  kFailedToCreateResponse
};

// Used to convert an error `result` to a string. This function will return an
// empty string if `result` represents `kSuccess`.
const std::string AttestationResultToString(DTAttestationResult result);

// Used to convert `error` to a string representation.
const std::string DeviceTrustErrorToString(DeviceTrustError error);

// Response payload for the inline flow's attestation step, where the challenge
// response is created and signed.
struct AttestationResponse {
  std::string challenge_response{};
  DTAttestationResult result_code{};
};

// Top-level response payload to a request for a challenge response as part of
// a device identity attestation request.
struct DeviceTrustResponse {
  std::string challenge_response{};
  absl::optional<DeviceTrustError> error = absl::nullopt;
  absl::optional<DTAttestationResult> attestation_result = absl::nullopt;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_COMMON_COMMON_TYPES_H_
