// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/metrics_utils.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "crypto/signature_verifier.h"

using BPKUR = enterprise_management::BrowserPublicKeyUploadRequest;

namespace enterprise_connectors {

namespace {

constexpr char kLoadedKeyTrustLevelHistogram[] =
    "Enterprise.DeviceTrust.Key.TrustLevel";
constexpr char kLoadedKeyTypeHistogram[] = "Enterprise.DeviceTrust.Key.Type";
constexpr char kLoadPersistedKeyResultHistogram[] =
    "Enterprise.DeviceTrust.Key.LoadPersistedKeyResult";
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

std::string GetHistogramVariant(BPKUR::KeyTrustLevel trust_level) {
  switch (trust_level) {
    case BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED:
      static constexpr char kUnknown[] = "Unknown";
      return kUnknown;
    case BPKUR::CHROME_BROWSER_HW_KEY:
      static constexpr char kHardware[] = "Hardware";
      return kHardware;
    case BPKUR::CHROME_BROWSER_OS_KEY:
      static constexpr char kOS[] = "OS";
      return kOS;
  }
}

DTKeyRotationResult ResultFromStatus(KeyRotationCommand::Status status) {
  switch (status) {
    case KeyRotationCommand::Status::SUCCEEDED:
      return DTKeyRotationResult::kSucceeded;
    case KeyRotationCommand::Status::FAILED:
      return DTKeyRotationResult::kFailed;
    case KeyRotationCommand::Status::TIMED_OUT:
      return DTKeyRotationResult::kTimedOut;
    case KeyRotationCommand::Status::FAILED_KEY_CONFLICT:
      return DTKeyRotationResult::kFailedKeyConflict;
    case KeyRotationCommand::Status::FAILED_OS_RESTRICTION:
      return DTKeyRotationResult::kFailedOSRestriction;
    case KeyRotationCommand::Status::FAILED_INVALID_PERMISSIONS:
      return DTKeyRotationResult::kFailedInvalidPermissions;
    case KeyRotationCommand::Status::FAILED_INVALID_INSTALLATION:
      return DTKeyRotationResult::kFailedInvalidInstallation;
    case KeyRotationCommand::Status::FAILED_INVALID_DMTOKEN_STORAGE:
      return DTKeyRotationResult::kFailedInvalidDmTokenStorage;
    case KeyRotationCommand::Status::FAILED_INVALID_DMTOKEN:
      return DTKeyRotationResult::kFailedInvalidDmToken;
    case KeyRotationCommand::Status::FAILED_INVALID_MANAGEMENT_SERVICE:
      return DTKeyRotationResult::kFailedInvalidManagementService;
    case KeyRotationCommand::Status::FAILED_INVALID_DMSERVER_URL:
      return DTKeyRotationResult::kFailedInvalidDmServerUrl;
    case KeyRotationCommand::Status::FAILED_INVALID_COMMAND:
      return DTKeyRotationResult::kFailedInvalidCommand;
  }
}

}  // namespace

void LogKeyLoadingResult(
    std::optional<DeviceTrustKeyManager::KeyMetadata> key_metadata,
    LoadPersistedKeyResult result) {
  base::UmaHistogramEnumeration(kLoadPersistedKeyResultHistogram, result);

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

void LogSignatureLatency(BPKUR::KeyTrustLevel trust_level,
                         base::TimeTicks start_time) {
  static constexpr char kSigningLatencyHistogramFormat[] =
      "Enterprise.DeviceTrust.Key.Signing.Latency.%s";
  base::UmaHistogramTimes(
      base::StringPrintf(kSigningLatencyHistogramFormat,
                         GetHistogramVariant(trust_level).c_str()),
      base::TimeTicks::Now() - start_time);
}

}  // namespace enterprise_connectors
