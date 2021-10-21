// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/scoped_key_persistence_delegate_factory.h"

#include "base/check.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/mock_key_persistence_delegate.h"
#include "crypto/signature_verifier.h"
#include "crypto/unexportable_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using BPKUR = enterprise_management::BrowserPublicKeyUploadRequest;

namespace enterprise_connectors {
namespace test {

ScopedKeyPersistenceDelegateFactory::ScopedKeyPersistenceDelegateFactory() {
  // Generating and forcing the usage of a mocked TPM wrapped key to allow
  // tests to skip having to rotate/store the key itself.
  auto provider = crypto::GetUnexportableKeyProvider();
  DCHECK(provider);
  const crypto::SignatureVerifier::SignatureAlgorithm algorithms[] = {
      crypto::SignatureVerifier::ECDSA_SHA256};
  auto signing_key = provider->GenerateSigningKeySlowly(algorithms);
  DCHECK(signing_key);
  wrapped_key_ = signing_key->GetWrappedKey();

  KeyPersistenceDelegateFactory::SetInstanceForTesting(this);
}

ScopedKeyPersistenceDelegateFactory::~ScopedKeyPersistenceDelegateFactory() {
  KeyPersistenceDelegateFactory::ClearInstanceForTesting();
}

std::unique_ptr<MockKeyPersistenceDelegate>
ScopedKeyPersistenceDelegateFactory::CreateMockedDelegate() {
  auto mocked_delegate = std::make_unique<MockKeyPersistenceDelegate>();
  ON_CALL(*mocked_delegate.get(), LoadKeyPair)
      .WillByDefault(testing::Return(KeyPersistenceDelegate::KeyInfo(
          BPKUR::CHROME_BROWSER_TPM_KEY, wrapped_key_)));
  ON_CALL(*mocked_delegate.get(), GetTpmBackedKeyProvider)
      .WillByDefault(testing::Invoke([]() {
        // This is mocked via crypto::ScopedMockUnexportableKeyProvider.
        return crypto::GetUnexportableKeyProvider();
      }));
  return mocked_delegate;
}

std::unique_ptr<KeyPersistenceDelegate>
ScopedKeyPersistenceDelegateFactory::CreateKeyPersistenceDelegate() {
  return CreateMockedDelegate();
}

}  // namespace test
}  // namespace enterprise_connectors
