// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/signing_key_util.h"

#include <vector>

#include "base/containers/span.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/common/key_types.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/mock_key_persistence_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/scoped_key_persistence_delegate_factory.h"
#include "crypto/unexportable_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using BPKUR = enterprise_management::BrowserPublicKeyUploadRequest;

namespace enterprise_connectors {

using test::MockKeyPersistenceDelegate;

namespace {

void ValidateSigningKey(SigningKeyPair* key_pair,
                        BPKUR::KeyTrustLevel expected_trust_level) {
  ASSERT_TRUE(key_pair);

  EXPECT_EQ(expected_trust_level, key_pair->trust_level());
  ASSERT_TRUE(key_pair->key());

  // Extract a pubkey should work.
  std::vector<uint8_t> pubkey = key_pair->key()->GetSubjectPublicKeyInfo();
  ASSERT_GT(pubkey.size(), 0u);

  // Signing should work.
  auto signed_data = key_pair->key()->SignSlowly(
      base::as_bytes(base::make_span("data to sign")));
  ASSERT_TRUE(signed_data.has_value());
  ASSERT_GT(signed_data->size(), 0u);
}

}  // namespace

class SigningKeyUtilTest : public testing::Test {
 protected:
  void StartTest(BPKUR::KeyTrustLevel trust_level) {
    std::unique_ptr<MockKeyPersistenceDelegate> mocked_delegate;
    if (trust_level == BPKUR::CHROME_BROWSER_HW_KEY) {
      mocked_delegate = factory_.CreateMockedHardwareDelegate();
    } else {
      mocked_delegate = factory_.CreateMockedECDelegate();
    }

    auto* mock_delegate_ptr = mocked_delegate.get();
    factory_.set_next_instance(std::move(mocked_delegate));

    EXPECT_CALL(*mock_delegate_ptr, LoadKeyPair(KeyStorageType::kPermanent, _));

    auto loaded_key = LoadPersistedKey();
    EXPECT_EQ(loaded_key.result, LoadPersistedKeyResult::kSuccess);
    ValidateSigningKey(loaded_key.key_pair.get(), trust_level);
  }

  test::ScopedKeyPersistenceDelegateFactory factory_;
};

// Tests that the LoadPersistedKey method correctly loads a hardware-backed
// SigningKeyPair when the persistence delegate is a hardware delegate.
TEST_F(SigningKeyUtilTest, LoadPersistedKey_WithHWPrivateKey) {
  StartTest(BPKUR::CHROME_BROWSER_HW_KEY);
}

// Tests that the LoadPersistedKey method correctly loads a
// crypto::ECPrivateKeySigningKeyPair when the persistence delegate is an EC
// delegate.
TEST_F(SigningKeyUtilTest, LoadPersistedKey_WithECPrivateKey) {
  StartTest(BPKUR::CHROME_BROWSER_OS_KEY);
}

}  // namespace enterprise_connectors
