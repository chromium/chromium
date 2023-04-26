// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/common/common_types.h"

#include "chrome/browser/enterprise/connectors/device_trust/common/device_trust_constants.h"

namespace enterprise_connectors {

const std::string AttestationErrorToString(DTAttestationResult result) {
  switch (result) {
    case DTAttestationResult::kMissingCoreSignals:
      return errors::kMissingCoreSignals;
    case DTAttestationResult::kMissingSigningKey:
      return errors::kMissingSigningKey;
    case DTAttestationResult::kBadChallengeFormat:
      return errors::kBadChallengeFormat;
    case DTAttestationResult::kBadChallengeSource:
      return errors::kBadChallengeSource;
    case DTAttestationResult::kFailedToSerializeKeyInfo:
      return errors::kFailedToSerializeKeyInfo;
    case DTAttestationResult::kFailedToGenerateResponse:
      return errors::kFailedToGenerateResponse;
    case DTAttestationResult::kFailedToSignResponse:
      return errors::kFailedToSignResponse;
    case DTAttestationResult::kFailedToSerializeResponse:
      return errors::kFailedToSerializeResponse;
    case DTAttestationResult::kEmptySerializedResponse:
      return errors::kEmptySerializedResponse;
    case DTAttestationResult::kFailedToSerializeSignals:
      return errors::kFailedToSerializeSignals;
    case DTAttestationResult::kSuccess:
    case DTAttestationResult::kSuccessNoSignature:
      return std::string();
  }
}

bool IsSuccessAttestationResult(DTAttestationResult result) {
  return result == DTAttestationResult::kSuccess ||
         result == DTAttestationResult::kSuccessNoSignature;
}

const std::string DeviceTrustErrorToString(DeviceTrustError error) {
  switch (error) {
    case DeviceTrustError::kUnknown:
      return errors::kUnknown;
    case DeviceTrustError::kTimeout:
      return errors::kTimeout;
    case DeviceTrustError::kFailedToParseChallenge:
      return errors::kFailedToParseChallenge;
    case DeviceTrustError::kFailedToCreateResponse:
      return errors::kFailedToCreateResponse;
  }
}

}  // namespace enterprise_connectors
