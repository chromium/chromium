// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_PERSISTENCE_MOCK_KEY_PERSISTENCE_DELEGATE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_PERSISTENCE_MOCK_KEY_PERSISTENCE_DELEGATE_H_

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/key_persistence_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/signing_key_pair.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace enterprise_connectors {
namespace test {

// Mock implementation of the KeyPersistenceDelegate interface.
class MockKeyPersistenceDelegate : public KeyPersistenceDelegate {
 public:
  MockKeyPersistenceDelegate();
  ~MockKeyPersistenceDelegate() override;

  // KeyPersistenceDelegate:
  MOCK_METHOD(bool, CheckRotationPermissions, (), (override));
  MOCK_METHOD(bool,
              StoreKeyPair,
              (KeyPersistenceDelegate::KeyTrustLevel, std::vector<uint8_t>),
              (override));
  MOCK_METHOD(scoped_refptr<enterprise_connectors::SigningKeyPair>,
              LoadKeyPair,
              (KeyStorageType, LoadPersistedKeyResult*),
              (override));
  MOCK_METHOD(scoped_refptr<enterprise_connectors::SigningKeyPair>,
              CreateKeyPair,
              (),
              (override));
  MOCK_METHOD(void, CleanupTemporaryKeyData, (), (override));
  MOCK_METHOD(bool, PromoteTemporaryKeyPair, (), (override));
  MOCK_METHOD(bool, DeleteKeyPair, (KeyStorageType), (override));
};

}  // namespace test
}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_PERSISTENCE_MOCK_KEY_PERSISTENCE_DELEGATE_H_
