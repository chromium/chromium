// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_MAC_METRICS_UTIL_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_MAC_METRICS_UTIL_H_

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

// Metrics for the Secure Enclave key `operation` with the
// key`type` describing where the key is stored (i.e permanent or temporary key
// storage).
void RecordKeyOperationStatus(SecureEnclaveOperationStatus operation,
                              SecureEnclaveClient::KeyType type);

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_MAC_METRICS_UTIL_H_
