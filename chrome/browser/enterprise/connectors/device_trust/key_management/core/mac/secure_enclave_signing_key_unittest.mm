// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/secure_enclave_signing_key.h"

#import <Foundation/Foundation.h>
#import <Security/Security.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/apple/bridging.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/mock_secure_enclave_client.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/secure_enclave_client.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/shared_command_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::apple::CFToNSPtrCast;
using base::apple::NSToCFPtrCast;
using testing::_;
using ::testing::InSequence;

namespace enterprise_connectors {

using test::MockSecureEnclaveClient;

class SecureEnclaveSigningKeyTest : public testing::Test {
 public:
  SecureEnclaveSigningKeyTest() {
    CreateTestKey();
    auto mock_secure_enclave_client =
        std::make_unique<MockSecureEnclaveClient>();
    mock_secure_enclave_client_ = mock_secure_enclave_client.get();
    SecureEnclaveClient::SetInstanceForTesting(
        std::move(mock_secure_enclave_client));
  }

 protected:
  // Creates a test key.
  void CreateTestKey() {
    NSDictionary* test_attributes = @{
      CFToNSPtrCast(kSecAttrLabel) : @"fake-label",
      CFToNSPtrCast(kSecAttrKeyType) :
          CFToNSPtrCast(kSecAttrKeyTypeECSECPrimeRandom),
      CFToNSPtrCast(kSecAttrKeySizeInBits) : @256,
      CFToNSPtrCast(kSecPrivateKeyAttrs) :
          @{CFToNSPtrCast(kSecAttrIsPermanent) : @NO}
    };

    test_key_.reset(
        SecKeyCreateRandomKey(NSToCFPtrCast(test_attributes), nullptr));
  }

  // Sets the unexportable key using the test key.
  void SetUnexportableKey() {
    EXPECT_CALL(*mock_secure_enclave_client_, CreatePermanentKey())
        .Times(1)
        .WillOnce([this]() { return test_key_; });
    key_ = provider_.GenerateSigningKeySlowly();
  }

  raw_ptr<MockSecureEnclaveClient, DanglingUntriaged>
      mock_secure_enclave_client_ = nullptr;
  SecureEnclaveSigningKeyProvider provider_;
  std::unique_ptr<crypto::UnexportableSigningKey> key_;
  base::apple::ScopedCFTypeRef<SecKeyRef> test_key_;
};

// Tests that the GenerateSigningKeySlowly method invokes the SE client's
// CreatePermanentKey method to create a permanent key.
TEST_F(SecureEnclaveSigningKeyTest, GenerateSigningKeySlowly) {
  key_.reset();
  SetUnexportableKey();
  ASSERT_TRUE(key_);
  EXPECT_EQ(key_->Algorithm(), crypto::SignatureVerifier::ECDSA_SHA256);
  EXPECT_TRUE(key_->GetSecKeyRef());
}

// Tests that the LoadStoredSigningKeySlowly invokes the SE client's
// CopyStoredKey method with the permanent key type.
TEST_F(SecureEnclaveSigningKeyTest,
       LoadStoredSigningKeySlowly_PermanentKeyType) {
  EXPECT_CALL(*mock_secure_enclave_client_,
              CopyStoredKey(SecureEnclaveClient::KeyType::kPermanent, _))
      .Times(1)
      .WillOnce([this](SecureEnclaveClient::KeyType type, OSStatus* error) {
        return test_key_;
      });

  OSStatus error;
  auto unexportable_key = provider_.LoadStoredSigningKeySlowly(
      SecureEnclaveClient::KeyType::kPermanent, &error);
  ASSERT_TRUE(unexportable_key);
  EXPECT_EQ(unexportable_key->Algorithm(),
            crypto::SignatureVerifier::ECDSA_SHA256);

  auto wrapped = unexportable_key->GetWrappedKey();
  EXPECT_EQ(std::string(wrapped.begin(), wrapped.end()),
            constants::kDeviceTrustSigningKeyLabel);
}

// Tests the LoadStoredSigningKeySlowly function when a key cannot be found.
TEST_F(SecureEnclaveSigningKeyTest,
       LoadStoredSigningKeySlowly_PermanentKeyType_NotFound) {
  EXPECT_CALL(*mock_secure_enclave_client_,
              CopyStoredKey(SecureEnclaveClient::KeyType::kPermanent, _))
      .Times(1)
      .WillOnce([](SecureEnclaveClient::KeyType type, OSStatus* error) {
        *error = errSecItemNotFound;
        return base::apple::ScopedCFTypeRef<SecKeyRef>(nullptr);
      });

  OSStatus error;
  EXPECT_FALSE(provider_.LoadStoredSigningKeySlowly(
      SecureEnclaveClient::KeyType::kPermanent, &error));
  EXPECT_EQ(error, errSecItemNotFound);
}

// Tests that the LoadStoredSigningKeySlowly invokes the SE client's
// CopyStoredKey method with the temporary key type.
TEST_F(SecureEnclaveSigningKeyTest,
       LoadStoredSigningKeySlowly_TemporaryKeyType) {
  EXPECT_CALL(*mock_secure_enclave_client_,
              CopyStoredKey(SecureEnclaveClient::KeyType::kTemporary, _))
      .Times(1)
      .WillOnce([this](SecureEnclaveClient::KeyType type, OSStatus* error) {
        return test_key_;
      });

  OSStatus error;
  auto unexportable_key = provider_.LoadStoredSigningKeySlowly(
      SecureEnclaveClient::KeyType::kTemporary, &error);
  ASSERT_TRUE(unexportable_key);
  EXPECT_EQ(unexportable_key->Algorithm(),
            crypto::SignatureVerifier::ECDSA_SHA256);

  auto wrapped = unexportable_key->GetWrappedKey();
  EXPECT_EQ(std::string(wrapped.begin(), wrapped.end()),
            constants::kTemporaryDeviceTrustSigningKeyLabel);
}

// Tests that the GetSubjectPublicKeyInfo method invokes the SE client's
// ExportPublicKey method and that the public key information gotten from
// this method is correct.
TEST_F(SecureEnclaveSigningKeyTest, GetSubjectPublicKeyInfo) {
  SetUnexportableKey();
  std::string test_data = "data";
  EXPECT_CALL(*mock_secure_enclave_client_, ExportPublicKey(_, _, _))
      .WillOnce([&test_data](SecKeyRef key, std::vector<uint8_t>& output,
                             OSStatus* error) {
        output.assign(test_data.begin(), test_data.end());
        return true;
      });

  EXPECT_EQ(std::vector<uint8_t>(test_data.begin(), test_data.end()),
            key_->GetSubjectPublicKeyInfo());
}

// Tests that the SignSlowly method invokes the SE client's SignDataWithKey
// method and that the signature is correct.
TEST_F(SecureEnclaveSigningKeyTest, SignSlowly) {
  SetUnexportableKey();
  std::string test_data = "data";
  EXPECT_CALL(*mock_secure_enclave_client_, SignDataWithKey(_, _, _, _))
      .Times(1)
      .WillOnce([&test_data](SecKeyRef key, base::span<const uint8_t> data,
                             std::vector<uint8_t>& output, OSStatus* error) {
        output.assign(test_data.begin(), test_data.end());
        return true;
      });
  EXPECT_EQ(
      std::vector<uint8_t>(test_data.begin(), test_data.end()),
      key_->SignSlowly(base::as_bytes(base::make_span("data to be sign"))));
}

}  // namespace enterprise_connectors
