// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_PERSISTENCE_KEY_PERSISTENCE_DELEGATE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_PERSISTENCE_KEY_PERSISTENCE_DELEGATE_H_

#include <memory>
#include <vector>

#include "components/policy/proto/device_management_backend.pb.h"
#include "crypto/unexportable_key.h"

namespace enterprise_connectors {

// Interface for classes that handle persistence of the key pair. There is an
// implementation for each platform.
class KeyPersistenceDelegate {
 public:
  using KeyTrustLevel =
      enterprise_management::BrowserPublicKeyUploadRequest::KeyTrustLevel;
  using KeyInfo = std::pair<KeyTrustLevel, std::vector<uint8_t>>;
  virtual ~KeyPersistenceDelegate() = default;

  // Validates that the current context has sufficient permissions to perform a
  // key rotation operation.
  virtual bool CheckRotationPermissions() = 0;

  // Stores the trust level and wrapped key in a platform specific location.
  // This method requires elevation since it writes to a location that is
  // shared by all OS users of the device.  Returns true on success.
  virtual bool StoreKeyPair(KeyTrustLevel trust_level,
                            std::vector<uint8_t> wrapped) = 0;

  // Loads the key from a platform specific location.  Returns
  // BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED and an empty vector if the trust level
  // or wrapped bits could not be loaded.
  virtual KeyInfo LoadKeyPair() = 0;

  // Returns the TPM-backed signing key provider for the platform if available.
  virtual std::unique_ptr<crypto::UnexportableKeyProvider>
  GetTpmBackedKeyProvider() = 0;

 protected:
  // Returns an invalid key info.
  KeyInfo invalid_key_info() {
    return {enterprise_management::BrowserPublicKeyUploadRequest::
                KEY_TRUST_LEVEL_UNSPECIFIED,
            std::vector<uint8_t>()};
  }
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_PERSISTENCE_KEY_PERSISTENCE_DELEGATE_H_
