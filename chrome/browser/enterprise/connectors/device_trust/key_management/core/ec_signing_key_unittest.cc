// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/ec_signing_key.h"

#include <memory>

#include "base/containers/span.h"
#include "crypto/signature_verifier.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

class ECSigningKeyTest : public testing::Test {
 public:
  ECSigningKeyTest() {
    auto acceptable_algorithms = {crypto::SignatureVerifier::ECDSA_SHA256};
    key_ = provider_.GenerateSigningKeySlowly(acceptable_algorithms);
  }

  crypto::UnexportableSigningKey* key() { return key_.get(); }

  bool Verify(crypto::SignatureVerifier::SignatureAlgorithm algo,
              base::span<const uint8_t> pubkey,
              base::span<const uint8_t> signature,
              const std::string& data) {
    crypto::SignatureVerifier verifier;
    verifier.VerifyInit(algo, signature, pubkey);
    verifier.VerifyUpdate(base::as_bytes(base::make_span(data)));
    return verifier.VerifyFinal();
  }

  ECSigningKeyProvider* provider() { return &provider_; }

 private:
  ECSigningKeyProvider provider_;
  std::unique_ptr<crypto::UnexportableSigningKey> key_;
};

TEST_F(ECSigningKeyTest, Sign) {
  auto pubkey = key()->GetSubjectPublicKeyInfo();
  ASSERT_NE(0u, pubkey.size());

  const std::string data("data to be sign");

  // Make sure that signatures generated with the key can be verified.
  auto signature = key()->SignSlowly(base::as_bytes(base::make_span(data)));
  ASSERT_TRUE(signature);
  ASSERT_NE(0u, signature->size());
  ASSERT_TRUE(Verify(key()->Algorithm(), pubkey, *signature, data));
}

TEST_F(ECSigningKeyTest, SameAlgoAfterWrapping) {
  auto algo_orig = key()->Algorithm();

  std::unique_ptr<crypto::UnexportableSigningKey> key2 =
      provider()->FromWrappedSigningKeySlowly(key()->GetWrappedKey());
  auto algo_wrapped = key2->Algorithm();

  ASSERT_EQ(algo_orig, algo_wrapped);
}

TEST_F(ECSigningKeyTest, SamePubKeyAfterWrapping) {
  auto pubkey_orig = key()->GetSubjectPublicKeyInfo();

  std::unique_ptr<crypto::UnexportableSigningKey> key2 =
      provider()->FromWrappedSigningKeySlowly(key()->GetWrappedKey());
  auto pubkey_wrapped = key2->GetSubjectPublicKeyInfo();

  ASSERT_EQ(pubkey_orig, pubkey_wrapped);
}

TEST_F(ECSigningKeyTest, WrapAndSign) {
  // Get pubkey from original key.
  auto pubkey = key()->GetSubjectPublicKeyInfo();
  ASSERT_NE(0u, pubkey.size());

  // Make sure that when wrapped and unwrapped, the signature can be verified
  // with the original pubkey.
  std::unique_ptr<crypto::UnexportableSigningKey> key2 =
      provider()->FromWrappedSigningKeySlowly(key()->GetWrappedKey());
  const std::string data("data to be sign");
  auto signature = key2->SignSlowly(base::as_bytes(base::make_span(data)));
  ASSERT_TRUE(signature);
  ASSERT_NE(0u, signature->size());
  ASSERT_TRUE(Verify(key2->Algorithm(), pubkey, *signature, data));
}

}  // namespace enterprise_connectors
