// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_PERSISTENCE_KEY_PERSISTENCE_DELEGATE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_PERSISTENCE_KEY_PERSISTENCE_DELEGATE_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/common/key_types.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/signing_key_pair.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace enterprise_connectors {

// Different outcomes that can happen from attempting to load a key from
// persistence. Mapped to DTLoadPersistedKeyResult in enums.xml, do not
// change ordering.
enum class LoadPersistedKeyResult {
  // Key was loaded successfully.
  kSuccess = 0,

  // Key was not found.
  kNotFound = 1,

  // Something is malformed/missing from the storage, the key
  // cannot be loaded into memory.
  kMalformedKey = 2,

  // An unknown error occurred, can be retried.
  kUnknown = 3,

  kMaxValue = kUnknown
};

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
  // This method may require elevation since it could write to a location that
  // is shared by all OS users of the device.  Returns true on success.
  virtual bool StoreKeyPair(KeyTrustLevel trust_level,
                            std::vector<uint8_t> wrapped) = 0;

  // Loads the key from a platform specific location based on the key storage
  // `type`. Returns a nullptr if the trust
  // level or wrapped bits could not be loaded. Will set `result` with a value
  // representing whether the operation was successful, or with a specific error
  // if it wasn't.
  virtual scoped_refptr<SigningKeyPair> LoadKeyPair(
      KeyStorageType type,
      LoadPersistedKeyResult* result) = 0;

  // Creates a key pair in the temporary key storage location which is composed
  // of a hardware-backed signing key and trust level
  // BPKUR::CHROME_BROWSER_HW_KEY pair if available, Otherwise an EC signing key
  // pair with a and trust level BPKUR::CHROME_BROWSER_OS_KEY is created if
  // available. If neither are available, a nullptr is returned. This method
  // may require elevation depending on the key type and platform.
  virtual scoped_refptr<SigningKeyPair> CreateKeyPair() = 0;

  // Moves the stored temporary signing key pair stored to the permanent key
  // storage location after a successful key upload. This method may require
  // elevation since it could write to a location that is shared by all OS users
  // of the device.
  virtual bool PromoteTemporaryKeyPair() = 0;

  // Deletes the signing key in the key storage `type` location.
  virtual bool DeleteKeyPair(KeyStorageType type) = 0;

  // Deletes the signing key in the temporary key storage after a successful
  // key rotation. This method is only overridden in Mac platforms since signing
  // key rollback is handled in the StoreKeyPair method in Linux and Windows
  // platforms.
  virtual void CleanupTemporaryKeyData();

 protected:
  // Utility function for more easily returning load key errors. `result` is the
  // actual error, and `out_result` is the out parameter in which to set the
  // result. Will always return a nullptr.
  scoped_refptr<SigningKeyPair> ReturnLoadKeyError(
      LoadPersistedKeyResult result,
      LoadPersistedKeyResult* out_result);
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_PERSISTENCE_KEY_PERSISTENCE_DELEGATE_H_
