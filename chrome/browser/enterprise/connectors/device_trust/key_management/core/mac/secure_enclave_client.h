// Copyright 2022 The Chromium Authors
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
#include "third_party/abseil-cpp/absl/types/optional.h"

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

  // Returns the key type from the `wrapped_key_label` if the label matches
  // any of the supported key labels. Otherwise a nullptr is returned.
  static absl::optional<SecureEnclaveClient::KeyType> GetTypeFromWrappedKey(
      base::span<const uint8_t> wrapped_key_label);

  // Creates a new Secure Enclave private key with a permanent key label.
  virtual base::ScopedCFTypeRef<SecKeyRef> CreatePermanentKey() = 0;

  // Queries for the secure key using its label determined by the key `type`.
  // Returns the secure key reference or a nullptr if no key was found.
  virtual base::ScopedCFTypeRef<SecKeyRef> CopyStoredKey(KeyType type) = 0;

  // Deletes any key stored in `new_key_type` and updates the private key
  // storage in `current_key_type` to `new_key_type` and modifies the key label
  // to reflect this change.
  virtual bool UpdateStoredKeyLabel(KeyType current_key_type,
                                    KeyType new_key_type) = 0;

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

  // Verifies whether the Secure Enclave is supported for the device.
  // Returns true if the Secure Enclave is supported and false otherwise.
  virtual bool VerifySecureEnclaveSupported() = 0;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_MAC_SECURE_ENCLAVE_CLIENT_H_
