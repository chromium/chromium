// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_PERSISTENCE_SCOPED_KEY_PERSISTENCE_DELEGATE_FACTORY_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_PERSISTENCE_SCOPED_KEY_PERSISTENCE_DELEGATE_FACTORY_H_

#include <stdint.h>

#include <map>
#include <utility>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/key_persistence_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/key_persistence_delegate_factory.h"
#include "crypto/scoped_mock_unexportable_key_provider.h"

namespace enterprise_connectors {
namespace test {
class MockKeyPersistenceDelegate;

// Class used in tests to mock retrieval of hardware signing key pairs. Creating
// an instance of this will prevent tests from having to actually store signing
// key pairs (which requires an elevated process).
class ScopedKeyPersistenceDelegateFactory
    : public KeyPersistenceDelegateFactory {
 public:
  ScopedKeyPersistenceDelegateFactory();
  ~ScopedKeyPersistenceDelegateFactory() override;

  const std::vector<uint8_t>& hw_wrapped_key() { return hw_wrapped_key_; }
  const std::vector<uint8_t>& ec_wrapped_key() { return ec_wrapped_key_; }

  // Returns a mocked instance which is already setup to mimic a hardware-backed
  // persistence delegate (with a provider and valid key).
  std::unique_ptr<MockKeyPersistenceDelegate> CreateMockedHardwareDelegate();

  // Returns a mocked instance which is already setup to mimic a hardware-backed
  // persistence delegate (with a provider and valid key). The mock will invoke
  // `side_effect` before returning the key value in LoadKeyPair.
  std::unique_ptr<MockKeyPersistenceDelegate>
  CreateMockedHardwareDelegateWithLoadingSideEffect(
      base::RepeatingClosure& side_effect);

  // Returns a mocked instance which is already setup to mimic an EC-backed
  // persistence delegate (with a provider and valid key).
  std::unique_ptr<MockKeyPersistenceDelegate> CreateMockedECDelegate();

  // KeyPersistenceDelegateFactory:
  std::unique_ptr<KeyPersistenceDelegate> CreateKeyPersistenceDelegate()
      override;

  // Function used to inject a mocked instance to dependencies using the
  // factory statically. i.e. if the thing under test is calling:
  // KeyPersistenceDelegateFactory::GetInstance()
  //   ->CreateKeyPersistenceDelegate();
  void set_next_instance(
      std::unique_ptr<KeyPersistenceDelegate> next_instance) {
    next_instance_ = std::move(next_instance);
  }

 private:
  crypto::ScopedMockUnexportableKeyProvider scoped_key_provider_;
  std::vector<uint8_t> hw_wrapped_key_;
  std::vector<uint8_t> ec_wrapped_key_;

  base::RepeatingClosure do_nothing_ = base::DoNothing();

  // Next instance to be returned by `CreateKeyPersistenceDelegate`. Typically
  // set by tests to mock a persistence delegate fetched statically.
  std::unique_ptr<KeyPersistenceDelegate> next_instance_;
};

// Class used in tests to mock storing and retrieval of hardware signing key
// pairs. Creating an instance of this will make tests use this class' own
// in-memory storage for signing keys instead of the actual storage (registry,
// file system etc.)
class ScopedInMemoryKeyPersistenceDelegateFactory
    : public KeyPersistenceDelegateFactory,
      public KeyPersistenceDelegate {
 public:
  ScopedInMemoryKeyPersistenceDelegateFactory();
  ~ScopedInMemoryKeyPersistenceDelegateFactory() override;

  // The ScopedInMemoryKeyPersistenceDelegateFactory that creates
  // KeyPersistenceDelegate using this function must outlive it.
  // KeyPersistenceDelegateFactory:
  std::unique_ptr<KeyPersistenceDelegate> CreateKeyPersistenceDelegate()
      override;

  // KeyPersistenceDelegate:
  bool CheckRotationPermissions() override;
  bool StoreKeyPair(KeyTrustLevel trust_level,
                    std::vector<uint8_t> wrapped) override;
  scoped_refptr<SigningKeyPair> LoadKeyPair(
      KeyStorageType type,
      LoadPersistedKeyResult* result) override;
  scoped_refptr<SigningKeyPair> CreateKeyPair() override;
  bool PromoteTemporaryKeyPair() override;
  bool DeleteKeyPair(KeyStorageType type) override;

 private:
  std::map<KeyStorageType, std::pair<KeyTrustLevel, std::vector<uint8_t>>>
      key_map_;
};

// A KeyPersistenceDelegate that forwards all calls to another delegate.
class KeyPersistenceDelegateStub : public KeyPersistenceDelegate {
 public:
  // `delegate` must outlive the KeyPersistenceDelegateStub object (this).
  explicit KeyPersistenceDelegateStub(KeyPersistenceDelegate& delegate)
      : delegate_(delegate) {}

  // KeyPersistenceDelegate:
  bool CheckRotationPermissions() override;
  bool StoreKeyPair(KeyTrustLevel trust_level,
                    std::vector<uint8_t> wrapped) override;
  scoped_refptr<SigningKeyPair> LoadKeyPair(
      KeyStorageType type,
      LoadPersistedKeyResult* result) override;
  scoped_refptr<SigningKeyPair> CreateKeyPair() override;
  bool PromoteTemporaryKeyPair() override;
  bool DeleteKeyPair(KeyStorageType type) override;

 private:
  const raw_ref<KeyPersistenceDelegate> delegate_;
};

}  // namespace test
}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_PERSISTENCE_SCOPED_KEY_PERSISTENCE_DELEGATE_FACTORY_H_
