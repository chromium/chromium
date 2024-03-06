// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/scoped_key_persistence_delegate_factory.h"

#include <utility>

#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/ec_signing_key.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/mock_key_persistence_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/signing_key_pair.h"
#include "crypto/signature_verifier.h"
#include "crypto/unexportable_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using BPKUR = enterprise_management::BrowserPublicKeyUploadRequest;

namespace enterprise_connectors {
namespace test {

namespace {

std::unique_ptr<crypto::UnexportableSigningKey> GenerateHardwareSigningKey() {
  auto provider = crypto::GetUnexportableKeyProvider(/*config=*/{});
  DCHECK(provider);
  auto acceptable_algorithms = {crypto::SignatureVerifier::ECDSA_SHA256};
  auto signing_key = provider->GenerateSigningKeySlowly(acceptable_algorithms);
  DCHECK(signing_key);
  return signing_key;
}

std::unique_ptr<crypto::UnexportableSigningKey> GenerateECSigningKey() {
  auto ec_key_provider = std::make_unique<ECSigningKeyProvider>();
  auto acceptable_algorithms = {crypto::SignatureVerifier::ECDSA_SHA256};
  return ec_key_provider->GenerateSigningKeySlowly(acceptable_algorithms);
}

}  // namespace

ScopedKeyPersistenceDelegateFactory::ScopedKeyPersistenceDelegateFactory() {
  KeyPersistenceDelegateFactory::SetInstanceForTesting(this);
}

ScopedKeyPersistenceDelegateFactory::~ScopedKeyPersistenceDelegateFactory() {
  KeyPersistenceDelegateFactory::ClearInstanceForTesting();
}

std::unique_ptr<MockKeyPersistenceDelegate>
ScopedKeyPersistenceDelegateFactory::CreateMockedHardwareDelegate() {
  return CreateMockedHardwareDelegateWithLoadingSideEffect(do_nothing_);
}

std::unique_ptr<MockKeyPersistenceDelegate>
ScopedKeyPersistenceDelegateFactory::
    CreateMockedHardwareDelegateWithLoadingSideEffect(
        base::RepeatingClosure& side_effect) {
  if (hw_wrapped_key_.empty()) {
    hw_wrapped_key_ = GenerateHardwareSigningKey()->GetWrappedKey();
  }

  auto mocked_delegate = std::make_unique<MockKeyPersistenceDelegate>();
  ON_CALL(*mocked_delegate.get(), LoadKeyPair)
      .WillByDefault(testing::Invoke(
          [&side_effect](KeyStorageType type, LoadPersistedKeyResult* result) {
            side_effect.Run();
            if (result) {
              *result = LoadPersistedKeyResult::kSuccess;
            }
            return base::MakeRefCounted<SigningKeyPair>(
                GenerateHardwareSigningKey(), BPKUR::CHROME_BROWSER_HW_KEY);
          }));
  ON_CALL(*mocked_delegate.get(), CreateKeyPair)
      .WillByDefault(testing::Invoke([]() {
        return base::MakeRefCounted<SigningKeyPair>(
            GenerateHardwareSigningKey(), BPKUR::CHROME_BROWSER_HW_KEY);
      }));
  return mocked_delegate;
}

std::unique_ptr<MockKeyPersistenceDelegate>
ScopedKeyPersistenceDelegateFactory::CreateMockedECDelegate() {
  if (ec_wrapped_key_.empty()) {
    ec_wrapped_key_ = GenerateECSigningKey()->GetWrappedKey();
  }

  auto mocked_delegate = std::make_unique<MockKeyPersistenceDelegate>();
  ON_CALL(*mocked_delegate.get(), LoadKeyPair)
      .WillByDefault(testing::Invoke([](KeyStorageType type,
                                        LoadPersistedKeyResult* result) {
        if (result) {
          *result = LoadPersistedKeyResult::kSuccess;
        }
        return base::MakeRefCounted<SigningKeyPair>(
            GenerateECSigningKey(), BPKUR::CHROME_BROWSER_OS_KEY);
      }));
  ON_CALL(*mocked_delegate.get(), CreateKeyPair)
      .WillByDefault(testing::Invoke([]() {
        return base::MakeRefCounted<SigningKeyPair>(
            GenerateECSigningKey(), BPKUR::CHROME_BROWSER_OS_KEY);
      }));
  return mocked_delegate;
}

std::unique_ptr<KeyPersistenceDelegate>
ScopedKeyPersistenceDelegateFactory::CreateKeyPersistenceDelegate() {
  if (next_instance_) {
    return std::move(next_instance_);
  }

  // If no mock instance was set, simply default to a mocked hardware delegate.
  return CreateMockedHardwareDelegate();
}

ScopedInMemoryKeyPersistenceDelegateFactory::
    ScopedInMemoryKeyPersistenceDelegateFactory() {
  KeyPersistenceDelegateFactory::SetInstanceForTesting(this);
}

ScopedInMemoryKeyPersistenceDelegateFactory::
    ~ScopedInMemoryKeyPersistenceDelegateFactory() {
  KeyPersistenceDelegateFactory::ClearInstanceForTesting();
}

std::unique_ptr<KeyPersistenceDelegate>
ScopedInMemoryKeyPersistenceDelegateFactory::CreateKeyPersistenceDelegate() {
  return std::make_unique<KeyPersistenceDelegateStub>(*this);
}

// We may want a method to change this to test no permission case?
bool ScopedInMemoryKeyPersistenceDelegateFactory::CheckRotationPermissions() {
  return true;
}

bool ScopedInMemoryKeyPersistenceDelegateFactory::StoreKeyPair(
    KeyTrustLevel trust_level,
    std::vector<uint8_t> wrapped) {
  if (trust_level == BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED) {
    // Remove key
    DCHECK_EQ(wrapped.size(), 0u);
    key_map_.erase(KeyStorageType::kPermanent);
    return true;
  }

  key_map_[KeyStorageType::kPermanent] =
      std::make_pair(trust_level, std::move(wrapped));
  return true;
}

scoped_refptr<SigningKeyPair>
ScopedInMemoryKeyPersistenceDelegateFactory::LoadKeyPair(
    KeyStorageType type,
    LoadPersistedKeyResult* result) {
  auto it = key_map_.find(type);
  if (it == key_map_.end()) {
    return ReturnLoadKeyError(LoadPersistedKeyResult::kNotFound, result);
  }

  const std::vector<uint8_t>& wrapped_key = it->second.second;
  auto provider = std::make_unique<ECSigningKeyProvider>();
  auto signing_key = provider->FromWrappedSigningKeySlowly(wrapped_key);

  if (!signing_key) {
    return ReturnLoadKeyError(LoadPersistedKeyResult::kMalformedKey, result);
  }

  if (result) {
    *result = LoadPersistedKeyResult::kSuccess;
  }
  KeyTrustLevel trust_level = it->second.first;
  return base::MakeRefCounted<SigningKeyPair>(std::move(signing_key),
                                              trust_level);
}

scoped_refptr<SigningKeyPair>
ScopedInMemoryKeyPersistenceDelegateFactory::CreateKeyPair() {
  auto algorithm = {crypto::SignatureVerifier::ECDSA_SHA256};
  auto signing_key =
      std::make_unique<ECSigningKeyProvider>()->GenerateSigningKeySlowly(
          algorithm);

  if (!signing_key) {
    return nullptr;
  }
  return base::MakeRefCounted<SigningKeyPair>(std::move(signing_key),
                                              BPKUR::CHROME_BROWSER_OS_KEY);
}

bool ScopedInMemoryKeyPersistenceDelegateFactory::PromoteTemporaryKeyPair() {
  // TODO(b/290068551): Implement this method.
  return true;
}

bool ScopedInMemoryKeyPersistenceDelegateFactory::DeleteKeyPair(
    KeyStorageType type) {
  // TODO(b/290068551): Implement this method.
  return true;
}

bool KeyPersistenceDelegateStub::CheckRotationPermissions() {
  return delegate_->CheckRotationPermissions();
}

bool KeyPersistenceDelegateStub::StoreKeyPair(KeyTrustLevel trust_level,
                                              std::vector<uint8_t> wrapped) {
  return delegate_->StoreKeyPair(trust_level, wrapped);
}

scoped_refptr<SigningKeyPair> KeyPersistenceDelegateStub::LoadKeyPair(
    KeyStorageType type,
    LoadPersistedKeyResult* result) {
  return delegate_->LoadKeyPair(type, result);
}

scoped_refptr<SigningKeyPair> KeyPersistenceDelegateStub::CreateKeyPair() {
  return delegate_->CreateKeyPair();
}

bool KeyPersistenceDelegateStub::PromoteTemporaryKeyPair() {
  return delegate_->PromoteTemporaryKeyPair();
}

bool KeyPersistenceDelegateStub::DeleteKeyPair(KeyStorageType type) {
  return delegate_->DeleteKeyPair(type);
}

}  // namespace test
}  // namespace enterprise_connectors
