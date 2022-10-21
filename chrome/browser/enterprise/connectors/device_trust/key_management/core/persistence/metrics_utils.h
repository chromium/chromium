// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_PERSISTENCE_METRICS_UTILS_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_PERSISTENCE_METRICS_UTILS_H_

namespace enterprise_connectors {

// Possible errors of the persistence delegate operations. This must be kept in
// sync with the DTKeyPersistenceError UMA enum.
enum class KeyPersistenceError {
  kLockPersistenceStorageFailed,
  kOpenPersistenceStorageFailed,
  kReadPersistenceStorageFailed,
  kWritePersistenceStorageFailed,
  kRetrievePersistenceStoragePermissionsFailed,
  kInvalidPermissionsForPersistenceStorage,
  kJsonFormatSigningKeyPairFailed,
  kDeleteKeyPairFailed,
  kInvalidTrustLevel,
  kInvalidSigningKeyPairFormat,
  kKeyPairMissingTrustLevel,
  kKeyPairMissingSigningKey,
  kInvalidSigningKey,
  kFailureDecodingSigningKey,
  kCreateSigningKeyFromWrappedFailed,
  kGenerateOSSigningKeyFailed,
  kGenerateHardwareSigningKeyFailed,
  kMaxValue = kGenerateHardwareSigningKeyFailed,
};

// Possible operations of the persistence delegates.
enum class KeyPersistenceOperation {
  kCheckPermissions,
  kStoreKeyPair,
  kLoadKeyPair,
  kCreateKeyPair,
  kMaxValue = kCreateKeyPair,
};

// Records any `error` encountered during the key persistence `operation`.
void RecordError(KeyPersistenceOperation operation, KeyPersistenceError error);

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_PERSISTENCE_METRICS_UTILS_H_
