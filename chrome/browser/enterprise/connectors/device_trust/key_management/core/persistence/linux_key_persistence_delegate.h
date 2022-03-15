// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_PERSISTENCE_LINUX_KEY_PERSISTENCE_DELEGATE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_PERSISTENCE_LINUX_KEY_PERSISTENCE_DELEGATE_H_

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/key_persistence_delegate.h"

namespace enterprise_connectors {

// Linux implementation of the KeyPersistenceDelegate interface.
class LinuxKeyPersistenceDelegate : public KeyPersistenceDelegate {
 public:
  ~LinuxKeyPersistenceDelegate() override;

  // KeyPersistenceDelegate:
  bool CheckRotationPermissions() override;
  bool StoreKeyPair(KeyPersistenceDelegate::KeyTrustLevel trust_level,
                    std::vector<uint8_t> wrapped) override;
  KeyPersistenceDelegate::KeyInfo LoadKeyPair() override;
  std::unique_ptr<crypto::UnexportableKeyProvider> GetTpmBackedKeyProvider()
      override;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_PERSISTENCE_LINUX_KEY_PERSISTENCE_DELEGATE_H_
