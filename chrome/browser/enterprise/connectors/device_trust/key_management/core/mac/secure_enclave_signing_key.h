// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_MAC_SECURE_ENCLAVE_SIGNING_KEY_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_MAC_SECURE_ENCLAVE_SIGNING_KEY_H_

#include <Security/Security.h>

#include <memory>
#include <optional>

#include "base/containers/span.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/secure_enclave_client.h"
#include "crypto/unexportable_key.h"

namespace enterprise_connectors {

// A key provider, inspired by crypto::UnexportableKeyProvider, for providing
// keys protected by the Secure Enclave.
class SecureEnclaveSigningKeyProvider {
 public:
  // Represents a loaded version of the "permanent" key.
  SecureEnclaveSigningKeyProvider();

  ~SecureEnclaveSigningKeyProvider();

  // Creates a new key pair backed by the Secure Enclave. In case of failure,
  // will return nullptr.
  std::unique_ptr<crypto::UnexportableSigningKey> GenerateSigningKeySlowly();

  // Tries to load an existing key pair of `key_type`. In case of failure, will
  // populate `error` with the error code and return nullptr.
  std::unique_ptr<crypto::UnexportableSigningKey> LoadStoredSigningKeySlowly(
      SecureEnclaveClient::KeyType key_type,
      OSStatus* error);
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_MAC_SECURE_ENCLAVE_SIGNING_KEY_H_
