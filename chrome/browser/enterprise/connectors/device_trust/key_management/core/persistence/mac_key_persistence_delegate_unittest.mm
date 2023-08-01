// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/mac_key_persistence_delegate.h"

#import <Foundation/Foundation.h>
#import <Security/Security.h>

#include <string>
#include <utility>

#include "base/apple/bridging.h"
#include "base/containers/span.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/mock_secure_enclave_client.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/secure_enclave_signing_key.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/shared_command_constants.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/signing_key_pair.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

using BPKUR = enterprise_management::BrowserPublicKeyUploadRequest;
using ::testing::_;
using ::testing::InSequence;

namespace enterprise_connectors {

using test::MockSecureEnclaveClient;

class MacKeyPersistenceDelegateTest : public testing::Test {
 public:
  MacKeyPersistenceDelegateTest() {
    SetMockClient();
    persistence_delegate_ = std::make_unique<MacKeyPersistenceDelegate>();
  }

 protected:
  void SetMockClient() {
    auto mock_secure_enclave_client =
        std::make_unique<MockSecureEnclaveClient>();
    mock_secure_enclave_client_ = mock_secure_enclave_client.get();
    SecureEnclaveClient::SetInstanceForTesting(
        std::move(mock_secure_enclave_client));
  }

  std::vector<uint8_t> CreateWrappedKeyLabel(std::string label) {
    std::vector<uint8_t> output;
    output.assign(label.begin(), label.end());
    return output;
  }

  // Creates a test key.
  base::ScopedCFTypeRef<SecKeyRef> CreateTestKey() {
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
    return base::ScopedCFTypeRef<SecKeyRef>(
        SecKeyCreateRandomKey(test_attributes, nullptr));
  }

  std::unique_ptr<MacKeyPersistenceDelegate> persistence_delegate_;
  raw_ptr<MockSecureEnclaveClient, DanglingUntriaged>
      mock_secure_enclave_client_ = nullptr;
};

// Tests that storing a key with an OS key trust level invokes the clients'
// UpdateStoredKeyLabel method when the wrapped data is correct.
TEST_F(MacKeyPersistenceDelegateTest, StoreKeyPair_OSKey_Success) {
  InSequence s;
  EXPECT_CALL(*mock_secure_enclave_client_, UpdateStoredKeyLabel(_, _))
      .Times(0);
  EXPECT_TRUE(persistence_delegate_->StoreKeyPair(
      BPKUR::CHROME_BROWSER_OS_KEY,
      CreateWrappedKeyLabel(constants::kDeviceTrustSigningKeyLabel)));

  // Correct wrapped data consists of the wrapped temporary key label.
  EXPECT_CALL(*mock_secure_enclave_client_, UpdateStoredKeyLabel(_, _))
      .WillOnce([](SecureEnclaveClient::KeyType current_key_type,
                   SecureEnclaveClient::KeyType new_key_type) {
        EXPECT_EQ(SecureEnclaveClient::KeyType::kTemporary, current_key_type);
        EXPECT_EQ(SecureEnclaveClient::KeyType::kPermanent, new_key_type);
        return true;
      });
  EXPECT_TRUE(persistence_delegate_->StoreKeyPair(
      BPKUR::CHROME_BROWSER_OS_KEY,
      CreateWrappedKeyLabel(constants::kTemporaryDeviceTrustSigningKeyLabel)));
}

// Tests that storing a key with an OS key trust level fails when the clients'
// UpdateStoredKeyLabel method returns false.
TEST_F(MacKeyPersistenceDelegateTest, StoreKeyPair_OSKey_Failure) {
  EXPECT_CALL(*mock_secure_enclave_client_, UpdateStoredKeyLabel(_, _))
      .WillOnce([](SecureEnclaveClient::KeyType current_key_type,
                   SecureEnclaveClient::KeyType new_key_type) {
        EXPECT_EQ(SecureEnclaveClient::KeyType::kTemporary, current_key_type);
        EXPECT_EQ(SecureEnclaveClient::KeyType::kPermanent, new_key_type);
        return false;
      });
  EXPECT_FALSE(persistence_delegate_->StoreKeyPair(
      BPKUR::CHROME_BROWSER_OS_KEY,
      CreateWrappedKeyLabel(constants::kTemporaryDeviceTrustSigningKeyLabel)));
}

// Tests that storing a key with an unspecified trust level invokes the clients'
// DeleteKey method with the correct key type.
TEST_F(MacKeyPersistenceDelegateTest, StoreKeyPair_UnspecifiedKey_Success) {
  InSequence s;

  EXPECT_CALL(*mock_secure_enclave_client_, DeleteKey(_))
      .WillOnce([](SecureEnclaveClient::KeyType current_key_type) {
        EXPECT_EQ(SecureEnclaveClient::KeyType::kPermanent, current_key_type);
        return true;
      });
  EXPECT_TRUE(persistence_delegate_->StoreKeyPair(
      BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED, std::vector<uint8_t>()));
}

// Tests that storing a hardware generated key invokes the clients'
// UpdateStoredKeyLabel method when the wrapped data is correct.
TEST_F(MacKeyPersistenceDelegateTest, StoreKeyPair_HardwareKey_Success) {
  InSequence s;

  EXPECT_CALL(*mock_secure_enclave_client_, UpdateStoredKeyLabel(_, _))
      .Times(0);
  EXPECT_TRUE(persistence_delegate_->StoreKeyPair(
      BPKUR::CHROME_BROWSER_HW_KEY,
      CreateWrappedKeyLabel(constants::kDeviceTrustSigningKeyLabel)));

  // Correct wrapped data consists of the wrapped temporary key label.
  EXPECT_CALL(*mock_secure_enclave_client_, UpdateStoredKeyLabel(_, _))
      .WillOnce([](SecureEnclaveClient::KeyType current_key_type,
                   SecureEnclaveClient::KeyType new_key_type) {
        EXPECT_EQ(SecureEnclaveClient::KeyType::kTemporary, current_key_type);
        EXPECT_EQ(SecureEnclaveClient::KeyType::kPermanent, new_key_type);
        return true;
      });
  EXPECT_TRUE(persistence_delegate_->StoreKeyPair(
      BPKUR::CHROME_BROWSER_HW_KEY,
      CreateWrappedKeyLabel(constants::kTemporaryDeviceTrustSigningKeyLabel)));
}

// Tests that storing a hardware generated key fails when the clients'
// UpdateStoredKeyLabel method returns false.
TEST_F(MacKeyPersistenceDelegateTest, StoreKeyPair_HardwareKey_Failure) {
  EXPECT_CALL(*mock_secure_enclave_client_, UpdateStoredKeyLabel(_, _))
      .WillOnce([](SecureEnclaveClient::KeyType current_key_type,
                   SecureEnclaveClient::KeyType new_key_type) {
        EXPECT_EQ(SecureEnclaveClient::KeyType::kTemporary, current_key_type);
        EXPECT_EQ(SecureEnclaveClient::KeyType::kPermanent, new_key_type);
        return false;
      });
  EXPECT_FALSE(persistence_delegate_->StoreKeyPair(
      BPKUR::CHROME_BROWSER_HW_KEY,
      CreateWrappedKeyLabel(constants::kTemporaryDeviceTrustSigningKeyLabel)));
}

// Tests loading a key pair when no key previously existed.
TEST_F(MacKeyPersistenceDelegateTest, LoadKeyPair_NoKey) {
  InSequence s;

  EXPECT_CALL(*mock_secure_enclave_client_, GetStoredKeyLabel(_, _))
      .WillOnce(
          [](SecureEnclaveClient::KeyType type, std::vector<uint8_t>& output) {
            EXPECT_EQ(SecureEnclaveClient::KeyType::kPermanent, type);
            return false;
          });
  EXPECT_FALSE(persistence_delegate_->LoadKeyPair());

  EXPECT_CALL(*mock_secure_enclave_client_, GetStoredKeyLabel(_, _))
      .WillOnce(
          [](SecureEnclaveClient::KeyType type, std::vector<uint8_t>& output) {
            EXPECT_EQ(SecureEnclaveClient::KeyType::kPermanent, type);
            return true;
          });
  EXPECT_FALSE(persistence_delegate_->LoadKeyPair());
}

// Tests loading a key pair when a key previously existed.
TEST_F(MacKeyPersistenceDelegateTest, LoadKeyPair_Key) {
  EXPECT_CALL(*mock_secure_enclave_client_, GetStoredKeyLabel(_, _))
      .WillOnce(
          [](SecureEnclaveClient::KeyType type, std::vector<uint8_t>& output) {
            EXPECT_EQ(SecureEnclaveClient::KeyType::kPermanent, type);
            std::string label = constants::kDeviceTrustSigningKeyLabel;
            output.assign(label.begin(), label.end());
            return true;
          });

  SetMockClient();
  EXPECT_CALL(*mock_secure_enclave_client_, CopyStoredKey(_))
      .WillOnce([this](SecureEnclaveClient::KeyType type) {
        EXPECT_EQ(SecureEnclaveClient::KeyType::kPermanent, type);
        return CreateTestKey();
      });

  auto key_pair = persistence_delegate_->LoadKeyPair();
  EXPECT_EQ(BPKUR::CHROME_BROWSER_HW_KEY, key_pair->trust_level());
  EXPECT_TRUE(key_pair->key());
}

// Tests a failure to create a new key pair.
TEST_F(MacKeyPersistenceDelegateTest, CreateKeyPair__EmptySigningKey) {
  EXPECT_CALL(*mock_secure_enclave_client_, UpdateStoredKeyLabel(_, _))
      .WillOnce([](SecureEnclaveClient::KeyType current_key_type,
                   SecureEnclaveClient::KeyType new_key_type) {
        EXPECT_EQ(SecureEnclaveClient::KeyType::kPermanent, current_key_type);
        EXPECT_EQ(SecureEnclaveClient::KeyType::kTemporary, new_key_type);
        return true;
      });

  SetMockClient();
  EXPECT_CALL(*mock_secure_enclave_client_, CreatePermanentKey())
      .WillOnce([]() { return base::ScopedCFTypeRef<SecKeyRef>(); });
  EXPECT_FALSE(persistence_delegate_->CreateKeyPair());
}

// Tests a successful call to create a new key pair.
TEST_F(MacKeyPersistenceDelegateTest, CreateKeyPair_Success) {
  EXPECT_CALL(*mock_secure_enclave_client_, UpdateStoredKeyLabel(_, _))
      .WillOnce([](SecureEnclaveClient::KeyType current_key_type,
                   SecureEnclaveClient::KeyType new_key_type) {
        EXPECT_EQ(SecureEnclaveClient::KeyType::kPermanent, current_key_type);
        EXPECT_EQ(SecureEnclaveClient::KeyType::kTemporary, new_key_type);
        return true;
      });

  SetMockClient();
  EXPECT_CALL(*mock_secure_enclave_client_, CreatePermanentKey())
      .WillOnce([this]() { return CreateTestKey(); });
  auto key_pair = persistence_delegate_->CreateKeyPair();
  EXPECT_EQ(BPKUR::CHROME_BROWSER_HW_KEY, key_pair->trust_level());
  EXPECT_TRUE(key_pair->key());
}

// Tests that the CleanupTemporaryKeyData correctly invokes the clients'
// DeleteKey method with the correct key type.
TEST_F(MacKeyPersistenceDelegateTest, CleanupTemporaryKeyData) {
  EXPECT_CALL(*mock_secure_enclave_client_, DeleteKey(_))
      .WillOnce([](SecureEnclaveClient::KeyType key_type) {
        EXPECT_EQ(SecureEnclaveClient::KeyType::kTemporary, key_type);
        return true;
      });
  persistence_delegate_->CleanupTemporaryKeyData();
}

}  // namespace enterprise_connectors
