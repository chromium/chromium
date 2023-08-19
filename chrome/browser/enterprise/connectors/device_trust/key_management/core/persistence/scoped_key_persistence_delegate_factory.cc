// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/scoped_key_persistence_delegate_factory.h"

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
  auto provider = crypto::GetUnexportableKeyProvider();
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
      .WillByDefault(testing::DoAll(
          testing::Invoke([&side_effect]() { side_effect.Run(); }),
          testing::Invoke([]() {
            return base::MakeRefCounted<SigningKeyPair>(
                GenerateHardwareSigningKey(), BPKUR::CHROME_BROWSER_HW_KEY);
          })));
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
      .WillByDefault(testing::Invoke([]() {
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

}  // namespace test
}  // namespace enterprise_connectors
