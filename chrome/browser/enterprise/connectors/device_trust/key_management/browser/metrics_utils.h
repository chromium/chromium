// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_METRICS_UTILS_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_METRICS_UTILS_H_

#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/key_rotation_command.h"
#include "components/enterprise/browser/device_trust/device_trust_key_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
  kMaxValue = kTimedOut,
};

// Logs the `key_metadata` trust level and type. If it is not defined
// (i.e. absl::nullopt), nothing is logged.
void LogKeyLoadingResult(
    absl::optional<DeviceTrustKeyManager::KeyMetadata> key_metadata);

// Logs the key rotation result based on the value of `status`. Also, if
// `had_nonce` is false, it will be logged as a key creation flow.
void LogKeyRotationResult(bool had_nonce, KeyRotationCommand::Status status);

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_METRICS_UTILS_H_
