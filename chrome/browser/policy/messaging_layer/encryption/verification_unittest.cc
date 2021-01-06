// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/encryption/verification.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"

using ::testing::Eq;
using ::testing::HasSubstr;

namespace reporting {
namespace {

class VerificationTest : public ::testing::Test {
 protected:
  VerificationTest() = default;
  void SetUp() override {
    // Generate key pair
    ED25519_keypair(public_key_, private_key_);
  }

  uint8_t public_key_[ED25519_PUBLIC_KEY_LEN];
  uint8_t private_key_[ED25519_PRIVATE_KEY_LEN];
};

TEST_F(VerificationTest, SignAndVerify) {
  static constexpr char message[] = "ABCDEF 012345";
  // Sign a message.
  uint8_t signature[ED25519_SIGNATURE_LEN];
  ASSERT_THAT(ED25519_sign(signature, reinterpret_cast<const uint8_t*>(message),
                           strlen(message), private_key_),
              Eq(1));

  // Verify the signature.
  SignatureVerifier verifier(std::string(
      reinterpret_cast<const char*>(public_key_), ED25519_PUBLIC_KEY_LEN));
  EXPECT_OK(
      verifier.Verify(std::string(message, strlen(message)),
                      std::string(reinterpret_cast<const char*>(signature),
                                  ED25519_SIGNATURE_LEN)));
}

TEST_F(VerificationTest, SignAndFailBadSignature) {
  static constexpr char message[] = "ABCDEF 012345";
  // Sign a message.
  uint8_t signature[ED25519_SIGNATURE_LEN];
  ASSERT_THAT(ED25519_sign(signature, reinterpret_cast<const uint8_t*>(message),
                           strlen(message), private_key_),
              Eq(1));

  // Verify the signature - wrong length.
  SignatureVerifier verifier(std::string(
      reinterpret_cast<const char*>(public_key_), ED25519_PUBLIC_KEY_LEN));
  Status status =
      verifier.Verify(std::string(message, strlen(message)),
                      std::string(reinterpret_cast<const char*>(signature),
                                  ED25519_SIGNATURE_LEN - 1));
  EXPECT_THAT(status.code(), Eq(error::FAILED_PRECONDITION));
  EXPECT_THAT(status.message(), HasSubstr("Wrong signature size"));

  // Verify the signature - mismatch.
  signature[0] = ~signature[0];
  status = verifier.Verify(std::string(message, strlen(message)),
                           std::string(reinterpret_cast<const char*>(signature),
                                       ED25519_SIGNATURE_LEN));
  EXPECT_THAT(status.code(), Eq(error::INVALID_ARGUMENT));
  EXPECT_THAT(status.message(), HasSubstr("Verification failed"));
}

TEST_F(VerificationTest, SignAndFailBadPublicKey) {
  static constexpr char message[] = "ABCDEF 012345";
  // Sign a message.
  uint8_t signature[ED25519_SIGNATURE_LEN];
  ASSERT_THAT(ED25519_sign(signature, reinterpret_cast<const uint8_t*>(message),
                           strlen(message), private_key_),
              Eq(1));

  // Verify the public key - wrong length.
  SignatureVerifier verifier(std::string(
      reinterpret_cast<const char*>(public_key_), ED25519_PUBLIC_KEY_LEN - 1));
  Status status =
      verifier.Verify(std::string(message, strlen(message)),
                      std::string(reinterpret_cast<const char*>(signature),
                                  ED25519_SIGNATURE_LEN));
  EXPECT_THAT(status.code(), Eq(error::FAILED_PRECONDITION));
  EXPECT_THAT(status.message(), HasSubstr("Wrong public key size"));

  // Verify the public key - mismatch.
  public_key_[0] = ~public_key_[0];
  SignatureVerifier verifier2(std::string(
      reinterpret_cast<const char*>(public_key_), ED25519_PUBLIC_KEY_LEN));
  status =
      verifier2.Verify(std::string(message, strlen(message)),
                       std::string(reinterpret_cast<const char*>(signature),
                                   ED25519_SIGNATURE_LEN));
  EXPECT_THAT(status.code(), Eq(error::INVALID_ARGUMENT));
  EXPECT_THAT(status.message(), HasSubstr("Verification failed"));
}

}  // namespace
}  // namespace reporting
