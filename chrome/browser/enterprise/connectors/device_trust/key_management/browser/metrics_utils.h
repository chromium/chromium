// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_METRICS_UTILS_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_METRICS_UTILS_H_

#include <optional>

#include "base/time/time.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/key_rotation_command.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/key_persistence_delegate.h"
#include "components/enterprise/browser/device_trust/device_trust_key_manager.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace enterprise_connectors {

// Contains all known key generation trust levels (generated via hardware or
// not). These values are persisted to logs and should not be renumbered. Please
// update the DTKeyTrustLevel enum in enums.xml when adding a new step here.
enum class DTKeyTrustLevel {
  kUnspecified = 0,
  kHw = 1,
  kOs = 2,
  kMaxValue = kOs,
};

// Contains all known key types, which represents the algorithms used to
// generate the Device Trust signing key. These values are persisted to logs and
// should not be renumbered. Please update the DTKeyType enum in enums.xml when
// adding a new step here.
enum class DTKeyType {
  kRsa = 0,
  kEc = 1,
  kMaxValue = kEc,
};

// Possible outcomes of the key rotation process. These
// values are persisted to logs and should not be renumbered. Please update
// the DTKeyRotationResult enum in enums.xml when adding a new step here.
enum class DTKeyRotationResult {
  kSucceeded = 0,
  kFailed = 1,
  kTimedOut = 2,
  kFailedKeyConflict = 3,
  kFailedOSRestriction = 4,
  kFailedInvalidPermissions = 5,
  kFailedInvalidInstallation = 6,
  kFailedInvalidDmTokenStorage = 7,
  kFailedInvalidDmToken = 8,
  kFailedInvalidManagementService = 9,
  kFailedInvalidDmServerUrl = 10,
  kFailedInvalidCommand = 11,
  kMaxValue = kFailedInvalidCommand,
};

// Possible client errors that can happen during key synchronization.
// Please update DTSynchronizationError enum in enums.xml when adding a new
// value here.
enum class DTSynchronizationError {
  kMissingKeyPair = 0,
  kInvalidDmToken = 1,
  kInvalidServerUrl = 2,
  kCannotBuildRequest = 3,
  kMaxValue = kCannotBuildRequest
};

// Logs the `key_metadata` trust level and type. If it is not defined
// (i.e. std::nullopt), nothing is logged. Also logs the `result` enum
// value.
void LogKeyLoadingResult(
    std::optional<DeviceTrustKeyManager::KeyMetadata> key_metadata,
    LoadPersistedKeyResult result);

// Logs the key rotation result based on the value of `status`. Also, if
// `had_nonce` is false, it will be logged as a key creation flow.
void LogKeyRotationResult(bool had_nonce, KeyRotationCommand::Status status);

// Logs the key synchronization `error`.
void LogSynchronizationError(DTSynchronizationError error);

// Logs the time it took for a key with `trust_level` to sign a payload, using
// `start_time`.
void LogSignatureLatency(
    enterprise_management::BrowserPublicKeyUploadRequest::KeyTrustLevel
        trust_level,
    base::TimeTicks start_time);

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_METRICS_UTILS_H_
