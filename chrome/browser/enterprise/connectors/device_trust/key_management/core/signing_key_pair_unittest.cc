// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/signing_key_pair.h"

#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/ec_signing_key.h"
#include "crypto/scoped_mock_unexportable_key_provider.h"
#include "crypto/signature_verifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using BPKUP = enterprise_management::BrowserPublicKeyUploadResponse;
using BPKUR = enterprise_management::BrowserPublicKeyUploadRequest;

namespace enterprise_connectors {

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

std::unique_ptr<crypto::UnexportableSigningKey> GenerateSigningKey(
    BPKUR::KeyTrustLevel trust_level) {
  std::unique_ptr<crypto::UnexportableKeyProvider> provider;
  if (trust_level == BPKUR::CHROME_BROWSER_HW_KEY) {
    provider = crypto::GetUnexportableKeyProvider(/*config=*/{});
  } else {
    provider = std::make_unique<ECSigningKeyProvider>();
  }
  DCHECK(provider);
  auto acceptable_algorithms = {crypto::SignatureVerifier::ECDSA_SHA256};
  return provider->GenerateSigningKeySlowly(acceptable_algorithms);
}

}  // namespace

class SigningKeyPairTest : public testing::Test {
 protected:
  crypto::ScopedMockUnexportableKeyProvider scoped_key_provider_;
};

// Tests that the SigningKeyPair instance is correctly initialized with a
// hardware-backed SigningKeyPair if it was available.
TEST_F(SigningKeyPairTest, SigningKeyPairInstance_WithHwKey) {
  auto key_pair = base::MakeRefCounted<SigningKeyPair>(
      GenerateSigningKey(BPKUR::CHROME_BROWSER_HW_KEY),
      BPKUR::CHROME_BROWSER_HW_KEY);
  ValidateSigningKey(key_pair.get(), BPKUR::CHROME_BROWSER_HW_KEY);
}

// Tests that the SigningKeyPair instance is correctly initialized with a
// crypto::ECPrivateKey-backed SigningKeyPair if it was available.
TEST_F(SigningKeyPairTest, Create_WithECPrivateKey) {
  auto key_pair = base::MakeRefCounted<SigningKeyPair>(
      GenerateSigningKey(BPKUR::CHROME_BROWSER_OS_KEY),
      BPKUR::CHROME_BROWSER_OS_KEY);
  ValidateSigningKey(key_pair.get(), BPKUR::CHROME_BROWSER_OS_KEY);
}

}  // namespace enterprise_connectors
