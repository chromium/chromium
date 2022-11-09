// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/metrics_utils.h"

#include "base/metrics/histogram_functions.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "crypto/signature_verifier.h"

using BPKUR = enterprise_management::BrowserPublicKeyUploadRequest;

namespace enterprise_connectors {

namespace {

constexpr char kLoadedKeyTrustLevelHistogram[] =
    "Enterprise.DeviceTrust.Key.TrustLevel";
constexpr char kLoadedKeyTypeHistogram[] = "Enterprise.DeviceTrust.Key.Type";
constexpr char kKeyCreationResultHistogram[] =
    "Enterprise.DeviceTrust.Key.CreationResult";
constexpr char kKeyRotationResultHistogram[] =
    "Enterprise.DeviceTrust.Key.RotationResult";

DTKeyTrustLevel ConvertTrustLevel(BPKUR::KeyTrustLevel trust_level) {
  switch (trust_level) {
    case BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED:
      return DTKeyTrustLevel::kUnspecified;
    case BPKUR::CHROME_BROWSER_HW_KEY:
      return DTKeyTrustLevel::kHw;
    case BPKUR::CHROME_BROWSER_OS_KEY:
      return DTKeyTrustLevel::kOs;
  }
}

DTKeyType AlgorithmToType(
    crypto::SignatureVerifier::SignatureAlgorithm algorithm) {
  switch (algorithm) {
    case crypto::SignatureVerifier::RSA_PKCS1_SHA1:
    case crypto::SignatureVerifier::RSA_PKCS1_SHA256:
    case crypto::SignatureVerifier::RSA_PSS_SHA256:
      return DTKeyType::kRsa;
    case crypto::SignatureVerifier::ECDSA_SHA256:
      return DTKeyType::kEc;
  }
}

DTKeyRotationResult ResultFromStatus(KeyRotationCommand::Status status) {
  switch (status) {
    case KeyRotationCommand::Status::SUCCEEDED:
      return DTKeyRotationResult::kSucceeded;
    case KeyRotationCommand::Status::FAILED:
      return DTKeyRotationResult::kFailed;
    case KeyRotationCommand::Status::FAILED_KEY_CONFLICT:
      return DTKeyRotationResult::kFailedKeyConflict;
    case KeyRotationCommand::Status::FAILED_OS_RESTRICTION:
      return DTKeyRotationResult::kFailedOSRestriction;
    case KeyRotationCommand::Status::TIMED_OUT:
      return DTKeyRotationResult::kTimedOut;
  }
}

}  // namespace

void LogKeyLoadingResult(
    absl::optional<DeviceTrustKeyManager::KeyMetadata> key_metadata) {
  if (!key_metadata.has_value()) {
    return;
  }

  base::UmaHistogramEnumeration(kLoadedKeyTypeHistogram,
                                AlgorithmToType(key_metadata->algorithm));
  base::UmaHistogramEnumeration(kLoadedKeyTrustLevelHistogram,
                                ConvertTrustLevel(key_metadata->trust_level));
}

void LogKeyRotationResult(bool had_nonce, KeyRotationCommand::Status status) {
  base::UmaHistogramEnumeration(
      had_nonce ? kKeyRotationResultHistogram : kKeyCreationResultHistogram,
      ResultFromStatus(status));
}

void LogSynchronizationError(DTSynchronizationError error) {
  static constexpr char kSynchronizationErrorHistogram[] =
      "Enterprise.DeviceTrust.SyncSigningKey.ClientError";
  base::UmaHistogramEnumeration(kSynchronizationErrorHistogram, error);
}

}  // namespace enterprise_connectors
