// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_COMMON_KEY_TYPES_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_COMMON_KEY_TYPES_H_

namespace enterprise_connectors {

// Represents the possible key storage types for the Device Trust connector.
enum class KeyStorageType {
  // A temporary key used during the rotation process. After a successful
  // key upload operation, the temporary key will be made permanent.
  kTemporary,

  // The key to use for signing operations.
  kPermanent,
};

// Possible results for the key synchronization operation.
enum class KeySyncResult {
  kConflictServerError,  // Server responds with a 409 error.
  kUnknownServerError,   // Server responds with a 5xx error.
  kUnknownFailure,
  kSuccess,
  kMaxValue = kSuccess,
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_COMMON_KEY_TYPES_H_
