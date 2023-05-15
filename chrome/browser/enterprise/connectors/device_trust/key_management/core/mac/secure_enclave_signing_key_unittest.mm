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
#include "base/containers/span.h"
#include "base/mac/scoped_cftyperef.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/mock_secure_enclave_client.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/secure_enclave_client.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/shared_command_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
    base::ScopedCFTypeRef<CFMutableDictionaryRef> test_attributes(
        CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                  &kCFTypeDictionaryKeyCallBacks,
                                  &kCFTypeDictionaryValueCallBacks));
    CFDictionarySetValue(test_attributes, kSecAttrLabel, CFSTR("fake-label"));
    CFDictionarySetValue(test_attributes, kSecAttrKeyType,
                         kSecAttrKeyTypeECSECPrimeRandom);
    CFDictionarySetValue(test_attributes, kSecAttrKeySizeInBits,
                         base::apple::NSToCFPtrCast(@256));
    base::ScopedCFTypeRef<CFMutableDictionaryRef> private_key_params(
        CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                  &kCFTypeDictionaryKeyCallBacks,
                                  &kCFTypeDictionaryValueCallBacks));
    CFDictionarySetValue(private_key_params, kSecAttrIsPermanent,
                         kCFBooleanFalse);
    CFDictionarySetValue(test_attributes, kSecPrivateKeyAttrs,
                         private_key_params);
    test_key_ = base::ScopedCFTypeRef<SecKeyRef>(
        SecKeyCreateRandomKey(test_attributes, nullptr));
  }

  // Sets the unexportable key using the test key.
  void SetUnexportableKey() {
    auto provider = SecureEnclaveSigningKeyProvider(
        SecureEnclaveClient::KeyType::kPermanent);
    EXPECT_CALL(*mock_secure_enclave_client_, CreatePermanentKey())
        .Times(1)
        .WillOnce([this]() { return test_key_; });
    auto acceptable_algorithms = {crypto::SignatureVerifier::ECDSA_SHA256};
    key_ = provider.GenerateSigningKeySlowly(acceptable_algorithms);
  }

  MockSecureEnclaveClient* mock_secure_enclave_client_ = nullptr;
  std::unique_ptr<crypto::UnexportableSigningKey> key_;
  base::ScopedCFTypeRef<SecKeyRef> test_key_;
};

// Tests that the GenerateSigningKeySlowly method invokes the SE client's
// CreatePermanentKey method only when the provider is a permanent key provider.
TEST_F(SecureEnclaveSigningKeyTest, GenerateSigningKeySlowly) {
  auto acceptable_algorithms = {crypto::SignatureVerifier::ECDSA_SHA256};

  InSequence s;

  auto provider =
      SecureEnclaveSigningKeyProvider(SecureEnclaveClient::KeyType::kPermanent);

  EXPECT_CALL(*mock_secure_enclave_client_, CreatePermanentKey())
      .Times(1)
      .WillOnce([this]() { return test_key_; });
  auto unexportable_key =
      provider.GenerateSigningKeySlowly(acceptable_algorithms);
  EXPECT_TRUE(unexportable_key);
  EXPECT_EQ(crypto::SignatureVerifier::ECDSA_SHA256,
            unexportable_key->Algorithm());

  provider =
      SecureEnclaveSigningKeyProvider(SecureEnclaveClient::KeyType::kTemporary);
  EXPECT_CALL(*mock_secure_enclave_client_, CreatePermanentKey()).Times(0);
  unexportable_key = provider.GenerateSigningKeySlowly(acceptable_algorithms);
  EXPECT_FALSE(unexportable_key);
}

// Tests that the FromWrappedSigningKeySlowly returns a nullptr when the wrapped
// key label is empty.
TEST_F(SecureEnclaveSigningKeyTest,
       FromWrappedSigningKeySlowly_EmptyWrappedKey) {
  auto permanent_key_provider = SecureEnclaveSigningKeyProvider(
      (SecureEnclaveClient::KeyType::kPermanent));
  base::span<const uint8_t> empty_span;
  EXPECT_FALSE(permanent_key_provider.FromWrappedSigningKeySlowly(empty_span));
}

// Tests that the FromWrappedSigningKeySlowly returns a nullptr when the wrapped
// key label is invalid and does not match the provider key type.
TEST_F(SecureEnclaveSigningKeyTest,
       FromWrappedSigningKeySlowly_InvalidWrappedKeyLabel) {
  auto permanent_key_provider = SecureEnclaveSigningKeyProvider(
      (SecureEnclaveClient::KeyType::kPermanent));
  EXPECT_FALSE(permanent_key_provider.FromWrappedSigningKeySlowly(
      base::as_bytes(base::make_span(
          std::string(constants::kTemporaryDeviceTrustSigningKeyLabel)))));

  auto temp_key_provider = SecureEnclaveSigningKeyProvider(
      (SecureEnclaveClient::KeyType::kTemporary));
  EXPECT_FALSE(temp_key_provider.FromWrappedSigningKeySlowly(base::as_bytes(
      base::make_span(std::string(constants::kDeviceTrustSigningKeyLabel)))));
}

// Tests that the FromWrappedSigningKeySlowly invokes the SE client's
// CopyStoredKey method only when the wrapped key label matches the key
// provider type and the provider is a permanent key provider.
TEST_F(SecureEnclaveSigningKeyTest,
       FromWrappedSigningKeySlowly_PermanentKeyProvider) {
  auto permanent_key_provider = SecureEnclaveSigningKeyProvider(
      (SecureEnclaveClient::KeyType::kPermanent));
  std::unique_ptr<crypto::UnexportableSigningKey> unexportable_key;

  InSequence s;

  // Permanent provider key type with a wrapped permanent key label.
  EXPECT_CALL(*mock_secure_enclave_client_, CopyStoredKey(_))
      .Times(1)
      .WillOnce([this](SecureEnclaveClient::KeyType type) {
        EXPECT_EQ(SecureEnclaveClient::KeyType::kPermanent, type);
        return test_key_;
      });
  unexportable_key = permanent_key_provider.FromWrappedSigningKeySlowly(
      base::as_bytes(base::make_span(
          std::string(constants::kDeviceTrustSigningKeyLabel))));
  EXPECT_TRUE(unexportable_key);
  EXPECT_EQ(crypto::SignatureVerifier::ECDSA_SHA256,
            unexportable_key->Algorithm());

  // Permanent key provider with a wrapped temporary key label.
  EXPECT_CALL(*mock_secure_enclave_client_, CopyStoredKey(_)).Times(0);
  unexportable_key = permanent_key_provider.FromWrappedSigningKeySlowly(
      base::as_bytes(base::make_span(
          std::string(constants::kTemporaryDeviceTrustSigningKeyLabel))));
  EXPECT_FALSE(unexportable_key);
}

// Tests that the FromWrappedSigningKeySlowly invokes the SE client's
// CopyStoredKey method only when the wrapped key label matches the key
// provider type and the provider is a temporary key provider.
TEST_F(SecureEnclaveSigningKeyTest,
       FromWrappedSigningKeySlowly_TemporaryKeyProvider) {
  auto temp_key_provider = SecureEnclaveSigningKeyProvider(
      (SecureEnclaveClient::KeyType::kTemporary));
  std::unique_ptr<crypto::UnexportableSigningKey> unexportable_key;

  InSequence s;

  // Temporary key provider with a wrapped temporary key label.
  EXPECT_CALL(*mock_secure_enclave_client_, CopyStoredKey(_))
      .Times(1)
      .WillOnce([this](SecureEnclaveClient::KeyType type) {
        EXPECT_EQ(SecureEnclaveClient::KeyType::kTemporary, type);
        return test_key_;
      });
  unexportable_key = temp_key_provider.FromWrappedSigningKeySlowly(
      base::as_bytes(base::make_span(
          std::string(constants::kTemporaryDeviceTrustSigningKeyLabel))));
  EXPECT_TRUE(unexportable_key);
  EXPECT_EQ(crypto::SignatureVerifier::ECDSA_SHA256,
            unexportable_key->Algorithm());

  // Temporary provider key type with a wrapped permanent key label.
  EXPECT_CALL(*mock_secure_enclave_client_, CopyStoredKey(_)).Times(0);
  unexportable_key = temp_key_provider.FromWrappedSigningKeySlowly(
      base::as_bytes(base::make_span(
          std::string(constants::kDeviceTrustSigningKeyLabel))));
  EXPECT_FALSE(unexportable_key);
}

// Tests that the GetSubjectPublicKeyInfo method invokes the SE client's
// ExportPublicKey method and that the public key information gotten from
// this method is correct.
TEST_F(SecureEnclaveSigningKeyTest, GetSubjectPublicKeyInfo) {
  SetUnexportableKey();
  std::string test_data = "data";
  EXPECT_CALL(*mock_secure_enclave_client_, ExportPublicKey(_, _))
      .WillOnce([&test_data](SecKeyRef key, std::vector<uint8_t>& output) {
        output.assign(test_data.begin(), test_data.end());
        return true;
      });

  EXPECT_EQ(std::vector<uint8_t>(test_data.begin(), test_data.end()),
            key_->GetSubjectPublicKeyInfo());
}

// Tests that the GetWrappedKey method invokes the SE client's
// GetStoredKeyLabel method and that the wrapped key label is the permanent
// key label since the SecureEnclaveSigningKey is currently a permanent key.
TEST_F(SecureEnclaveSigningKeyTest, GetWrappedKey) {
  SetUnexportableKey();
  EXPECT_CALL(*mock_secure_enclave_client_,
              GetStoredKeyLabel(SecureEnclaveClient::KeyType::kPermanent, _))
      .WillOnce(
          [](SecureEnclaveClient::KeyType type, std::vector<uint8_t>& output) {
            std::string label =
                (type == SecureEnclaveClient::KeyType::kTemporary)
                    ? constants::kTemporaryDeviceTrustSigningKeyLabel
                    : constants::kDeviceTrustSigningKeyLabel;
            output.assign(label.begin(), label.end());
            return true;
          });
  auto wrapped = key_->GetWrappedKey();
  EXPECT_EQ(constants::kDeviceTrustSigningKeyLabel,
            std::string(wrapped.begin(), wrapped.end()));
}

// Tests that the SignSlowly method invokes the SE client's SignDataWithKey
// method and that the signature is correct.
TEST_F(SecureEnclaveSigningKeyTest, SignSlowly) {
  SetUnexportableKey();
  std::string test_data = "data";
  EXPECT_CALL(*mock_secure_enclave_client_, SignDataWithKey(_, _, _))
      .Times(1)
      .WillOnce([&test_data](SecKeyRef key, base::span<const uint8_t> data,
                             std::vector<uint8_t>& output) {
        output.assign(test_data.begin(), test_data.end());
        return true;
      });
  EXPECT_EQ(
      std::vector<uint8_t>(test_data.begin(), test_data.end()),
      key_->SignSlowly(base::as_bytes(base::make_span("data to be sign"))));
}

}  // namespace enterprise_connectors
