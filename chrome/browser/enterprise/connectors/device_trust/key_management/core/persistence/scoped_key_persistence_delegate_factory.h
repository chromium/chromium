// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_PERSISTENCE_SCOPED_KEY_PERSISTENCE_DELEGATE_FACTORY_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_PERSISTENCE_SCOPED_KEY_PERSISTENCE_DELEGATE_FACTORY_H_

#include <stdint.h>

#include <vector>

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/key_persistence_delegate_factory.h"
#include "crypto/scoped_mock_unexportable_key_provider.h"

namespace enterprise_connectors {
namespace test {
class MockKeyPersistenceDelegate;

// Class used in tests to mock retrieval of TPM signing key pairs. Creating an
// instance of this will prevent tests from having to actually store signing
// key pairs (which requires an elevated process).
class ScopedKeyPersistenceDelegateFactory
    : public KeyPersistenceDelegateFactory {
 public:
  ScopedKeyPersistenceDelegateFactory();
  ~ScopedKeyPersistenceDelegateFactory() override;

  // Returns the TPM wrapped key.
  const std::vector<uint8_t>& wrapped_key() { return wrapped_key_; }

  // Returns a mocked instance which is already setup to mimic a TPM-backed
  // persistence delegate (with a provider and valid key).
  std::unique_ptr<MockKeyPersistenceDelegate> CreateMockedDelegate();

  // KeyPersistenceDelegateFactory:
  std::unique_ptr<KeyPersistenceDelegate> CreateKeyPersistenceDelegate()
      override;

 private:
  crypto::ScopedMockUnexportableKeyProvider scoped_key_provider_;
  std::vector<uint8_t> wrapped_key_;
};

}  // namespace test
}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_PERSISTENCE_SCOPED_KEY_PERSISTENCE_DELEGATE_FACTORY_H_
