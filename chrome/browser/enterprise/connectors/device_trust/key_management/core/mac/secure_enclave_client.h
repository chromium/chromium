// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_MAC_SECURE_ENCLAVE_CLIENT_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_MAC_SECURE_ENCLAVE_CLIENT_H_

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/mac/scoped_cftyperef.h"

namespace enterprise_connectors {

// Wrapper for the object in charge of interacting with the SecureEnclaveHelper
// and the Secure Enclave for key management.
class SecureEnclaveClient {
 public:
  virtual ~SecureEnclaveClient() = default;

  enum class KeyType {
    // A temporary key used during the rotation process. After a successful
    // key upload operation, the temporary key will be made permanent.
    kTemporary,

    // The key to use for signing operations.
    kPermanent,
  };

  static void SetInstanceForTesting(
      std::unique_ptr<SecureEnclaveClient> client);

  static std::unique_ptr<SecureEnclaveClient> Create();

  // Creates a new Secure Enclave private key with a temporary key label.
  virtual base::ScopedCFTypeRef<SecKeyRef> CreateTemporaryKey() = 0;

  // Updates the private key label from the temporary key label to the
  // non-temporary label.
  virtual bool MoveTemporaryKeyToPermanent() = 0;

  // Queries for the secure key using its label determined by the key `type`
  // and deletes it from the secure enclave.
  virtual bool DeleteKey(KeyType type) = 0;

  // Queries for the secure key using its label determined by the key `type`
  // and stores the key label in `output`.
  virtual bool GetStoredKeyLabel(KeyType type,
                                 std::vector<uint8_t>& output) = 0;

  // Creates the public key using the secure private `key` and SPKI encodes
  // this public key for key upload. The encoded key is stored in `output`.
  virtual bool ExportPublicKey(SecKeyRef key, std::vector<uint8_t>& output) = 0;

  // Creates the signature of `data` using the secure `key`. This signature is
  // stored in `output`.
  virtual bool SignDataWithKey(SecKeyRef key,
                               base::span<const uint8_t> data,
                               std::vector<uint8_t>& output) = 0;

  // Verifies whether the keychain is currently unlocked. Returns true
  // if it is unlocked and false otherwise.
  virtual bool VerifyKeychainUnlocked() = 0;

  // Verifies whether the Secure Enclave is supported for the device.
  // Returns true if the Secure Enclave is supported and false otherwise.
  virtual bool VerifySecureEnclaveSupported() = 0;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_MAC_SECURE_ENCLAVE_CLIENT_H_
