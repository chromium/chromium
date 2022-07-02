// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/scoped_key_persistence_delegate_factory.h"

#include "base/check.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/ec_signing_key.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/mock_key_persistence_delegate.h"
#include "crypto/signature_verifier.h"
#include "crypto/unexportable_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using BPKUR = enterprise_management::BrowserPublicKeyUploadRequest;

namespace enterprise_connectors {
namespace test {

namespace {

std::vector<uint8_t> GenerateHardwareWrapped() {
  auto provider = crypto::GetUnexportableKeyProvider();
  DCHECK(provider);
  auto acceptable_algorithms = {crypto::SignatureVerifier::ECDSA_SHA256};
  auto signing_key = provider->GenerateSigningKeySlowly(acceptable_algorithms);
  DCHECK(signing_key);
  return signing_key->GetWrappedKey();
}

std::vector<uint8_t> GenerateECWrapped() {
  ECSigningKeyProvider ec_key_provider;
  auto acceptable_algorithms = {crypto::SignatureVerifier::ECDSA_SHA256};
  auto signing_key =
      ec_key_provider.GenerateSigningKeySlowly(acceptable_algorithms);
  return signing_key->GetWrappedKey();
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
    hw_wrapped_key_ = GenerateHardwareWrapped();
  }

  auto mocked_delegate = std::make_unique<MockKeyPersistenceDelegate>();
  ON_CALL(*mocked_delegate.get(), LoadKeyPair)
      .WillByDefault(testing::DoAll(
          testing::Invoke([&side_effect]() { side_effect.Run(); }),
          testing::Return(KeyPersistenceDelegate::KeyInfo(
              BPKUR::CHROME_BROWSER_HW_KEY, hw_wrapped_key_))));
  ON_CALL(*mocked_delegate.get(), GetUnexportableKeyProvider)
      .WillByDefault(testing::Invoke([]() {
        // This is mocked via crypto::ScopedMockUnexportableKeyProvider.
        return crypto::GetUnexportableKeyProvider();
      }));
  return mocked_delegate;
}

std::unique_ptr<MockKeyPersistenceDelegate>
ScopedKeyPersistenceDelegateFactory::CreateMockedECDelegate() {
  if (ec_wrapped_key_.empty()) {
    ec_wrapped_key_ = GenerateECWrapped();
  }

  auto mocked_delegate = std::make_unique<MockKeyPersistenceDelegate>();
  ON_CALL(*mocked_delegate.get(), LoadKeyPair)
      .WillByDefault(testing::Return(KeyPersistenceDelegate::KeyInfo(
          BPKUR::CHROME_BROWSER_OS_KEY, ec_wrapped_key_)));
  ON_CALL(*mocked_delegate.get(), GetUnexportableKeyProvider)
      .WillByDefault(testing::Invoke([]() { return nullptr; }));
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
