// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_MAC_METRICS_UTIL_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_MAC_METRICS_UTIL_H_

#include <Security/Security.h>

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/secure_enclave_client.h"

namespace enterprise_connectors {

// Status of the Secure Enclave operations used to create and
// modify the Secure Enclave key during the key rotation. This must be kept in
// sync with the DTSecureEnclaveOperationStatus UMA enum.
enum class SecureEnclaveOperationStatus {
  kCreateSecureKeyFailed,
  kCopySecureKeyRefFailed,
  kCopySecureKeyRefDataProtectionKeychainFailed,
  kDeleteSecureKeyFailed,
  kDeleteSecureKeyDataProtectionKeychainFailed,
  kUpdateSecureKeyLabelFailed,
  kUpdateSecureKeyLabelDataProtectionKeychainFailed,
  kMaxValue = kUpdateSecureKeyLabelDataProtectionKeychainFailed,
};

// Enum for the operation being performed on the Device Trust key pair. This is
// used for recording the key operation status.
enum class KeychainOperation {
  kCreate = 0,
  kCopy = 1,
  kDelete = 2,
  kUpdate = 3,
  kExportPublicKey = 4,
  kSignPayload = 5,
};

// Logs UMA metrics for the Keychain `operation` failing with `error_code` for
// the given key `type`.
void RecordKeyOperationStatus(KeychainOperation operation,
                              SecureEnclaveClient::KeyType type,
                              OSStatus error_code);

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_MAC_METRICS_UTIL_H_
