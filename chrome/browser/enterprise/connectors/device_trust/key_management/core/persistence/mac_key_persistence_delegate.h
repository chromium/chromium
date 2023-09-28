// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_PERSISTENCE_MAC_KEY_PERSISTENCE_DELEGATE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_PERSISTENCE_MAC_KEY_PERSISTENCE_DELEGATE_H_

#include <memory>
#include <vector>

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/secure_enclave_client.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/key_persistence_delegate.h"

namespace enterprise_connectors {

class SigningKeyPair;

// Mac implementation of the KeyPersistenceDelegate interface. Mac currently
// only supports hardware generated Secure Enclave signing keys.
class MacKeyPersistenceDelegate : public KeyPersistenceDelegate {
 public:
  MacKeyPersistenceDelegate();
  ~MacKeyPersistenceDelegate() override;

  // KeyPersistenceDelegate:
  bool CheckRotationPermissions() override;
  bool StoreKeyPair(KeyPersistenceDelegate::KeyTrustLevel trust_level,
                    std::vector<uint8_t> wrapped) override;
  scoped_refptr<SigningKeyPair> LoadKeyPair(
      KeyStorageType type,
      LoadPersistedKeyResult* result) override;
  scoped_refptr<SigningKeyPair> CreateKeyPair() override;
  bool PromoteTemporaryKeyPair() override;
  bool DeleteKeyPair(KeyStorageType type) override;
  void CleanupTemporaryKeyData() override;

 private:
  std::unique_ptr<SecureEnclaveClient> client_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_PERSISTENCE_MAC_KEY_PERSISTENCE_DELEGATE_H_
