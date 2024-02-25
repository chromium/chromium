// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_PERSISTENCE_WIN_KEY_PERSISTENCE_DELEGATE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_PERSISTENCE_WIN_KEY_PERSISTENCE_DELEGATE_H_

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/key_persistence_delegate.h"

#include <memory>
#include <vector>

#include "base/win/registry.h"
#include "crypto/signature_verifier.h"

namespace enterprise_connectors {

class SigningKeyPair;

// Windows implementation of the KeyPersistenceDelegate interface.
class WinKeyPersistenceDelegate : public KeyPersistenceDelegate {
 public:
  ~WinKeyPersistenceDelegate() override;

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
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_PERSISTENCE_WIN_KEY_PERSISTENCE_DELEGATE_H_
