// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_PERSISTENCE_KEY_PERSISTENCE_DELEGATE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_PERSISTENCE_KEY_PERSISTENCE_DELEGATE_H_

#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/common/key_types.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/signing_key_pair.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace enterprise_connectors {

// Interface for classes that handle persistence of the key pair. There is an
// implementation for each platform.
class KeyPersistenceDelegate {
 public:
  using KeyTrustLevel =
      enterprise_management::BrowserPublicKeyUploadRequest::KeyTrustLevel;
  virtual ~KeyPersistenceDelegate() = default;

  // Validates that the current context has sufficient permissions to perform a
  // key rotation operation.
  virtual bool CheckRotationPermissions() = 0;

  // Stores the trust level and wrapped key in a platform specific location.
  // This method requires elevation since it writes to a location that is
  // shared by all OS users of the device.  Returns true on success.
  virtual bool StoreKeyPair(KeyTrustLevel trust_level,
                            std::vector<uint8_t> wrapped) = 0;

  // Loads the key from a platform specific location based on the key storage
  // `type`, by default the key in the permanent storage location is loaded.
  // Later this key is used to create a key pair. Returns a nullptr if the trust
  // level or wrapped bits could not be loaded. Otherwise returns a new hardware
  // generated signing key with a trust level of BPKUR::CHROME_BROWSER_HW_KEY if
  // available, or a new EC signing key pair with BPKUR::CHROME_BROWSER_OS_KEY
  // trust level is returned if available.
  virtual scoped_refptr<SigningKeyPair> LoadKeyPair(
      KeyStorageType type = KeyStorageType::kPermanent) = 0;

  // Creates a key pair in the temporary key storage location which is composed
  // of a hardware-backed signing key and trust level
  // BPKUR::CHROME_BROWSER_HW_KEY pair if available, Otherwise an EC signing key
  // pair with a and trust level BPKUR::CHROME_BROWSER_OS_KEY is created if
  // available. If neither are available, a nullptr is returned. This method
  // requires elevation since it writes to a location that is shared by all OS
  // users of the device.
  virtual scoped_refptr<SigningKeyPair> CreateKeyPair() = 0;

  // Moves the temporary signing key pair stored in the temporary key storage
  // location to the permanent key storage location after a successful key
  // upload. This method requires elevation since it writes to a location that
  // is shared by all OS users of the device.
  virtual bool PromoteTemporaryKeyPair() = 0;

  // Deletes the signing key in the key storage `type` location.
  virtual bool DeleteKeyPair(KeyStorageType type) = 0;

  // Deletes the signing key in the temporary key storage after a successful
  // key rotation. This method is only overridden in Mac platforms since signing
  // key rollback is handled in the StoreKeyPair method in Linux and Windows
  // platforms.
  virtual void CleanupTemporaryKeyData() {}
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_PERSISTENCE_KEY_PERSISTENCE_DELEGATE_H_
