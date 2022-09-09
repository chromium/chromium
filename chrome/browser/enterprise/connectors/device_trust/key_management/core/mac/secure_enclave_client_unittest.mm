// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/secure_enclave_client.h"

#include <Security/Security.h>

#include <memory>

#include "base/containers/span.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/metrics_util.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/mock_secure_enclave_helper.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/secure_enclave_helper.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/shared_command_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace enterprise_connectors {

namespace {

constexpr char kPermanentStatusHistogramName[] =
    "Enterprise.DeviceTrust.Mac.SecureEnclaveOperation.Permanent";
constexpr char kTemporaryStatusHistogramName[] =
    "Enterprise.DeviceTrust.Mac.SecureEnclaveOperation.Temporary";

}  // namespace

using test::MockSecureEnclaveHelper;

class SecureEnclaveClientTest : public testing::Test {
 protected:
  void SetUp() override {
    if (@available(macOS 10.15, *))
      data_protection_keychain_ = true;

    auto mock_secure_enclave_helper =
        std::make_unique<MockSecureEnclaveHelper>();
    mock_secure_enclave_helper_ = mock_secure_enclave_helper.get();
    SecureEnclaveHelper::SetInstanceForTesting(
        std::move(mock_secure_enclave_helper));
    secure_enclave_client_ = SecureEnclaveClient::Create();
    CreateAndSetTestKey();
  }

  // Creates a test key.
  void CreateAndSetTestKey() {
    base::ScopedCFTypeRef<CFMutableDictionaryRef> test_attributes(
        CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                  &kCFTypeDictionaryKeyCallBacks,
                                  &kCFTypeDictionaryValueCallBacks));
    CFDictionarySetValue(test_attributes, kSecAttrLabel,
                         base::SysUTF8ToCFStringRef("fake-label"));
    CFDictionarySetValue(test_attributes, kSecAttrKeyType,
                         kSecAttrKeyTypeECSECPrimeRandom);
    CFDictionarySetValue(test_attributes, kSecAttrKeySizeInBits, @256);
    base::ScopedCFTypeRef<CFMutableDictionaryRef> private_key_params(
        CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                  &kCFTypeDictionaryKeyCallBacks,
                                  &kCFTypeDictionaryValueCallBacks));
    CFDictionarySetValue(private_key_params, kSecAttrIsPermanent, @NO);
    CFDictionarySetValue(test_attributes, kSecPrivateKeyAttrs,
                         private_key_params);
    test_key_ = base::ScopedCFTypeRef<SecKeyRef>(
        SecKeyCreateRandomKey(test_attributes, nullptr));
  }

  void VerifyQuery(CFDictionaryRef query, CFStringRef label) {
    EXPECT_TRUE(CFEqual(label, base::mac::GetValueFromDictionary<CFStringRef>(
                                   query, kSecAttrLabel)));
    EXPECT_TRUE(CFEqual(kSecAttrKeyTypeECSECPrimeRandom,
                        base::mac::GetValueFromDictionary<CFStringRef>(
                            query, kSecAttrKeyType)));
  }

  MockSecureEnclaveHelper* mock_secure_enclave_helper_ = nullptr;
  std::unique_ptr<SecureEnclaveClient> secure_enclave_client_;
  base::ScopedCFTypeRef<SecKeyRef> test_key_;
  bool data_protection_keychain_ = false;
};

// Tests that the CreatePermanentKey method invokes both the SE helper's
// Delete and CreateSecureKey method and that the key attributes are set
// correctly.
TEST_F(SecureEnclaveClientTest, CreateKey_Success) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*mock_secure_enclave_helper_, Delete(_))
      .Times(1)
      .WillOnce([this](CFDictionaryRef query) {
        VerifyQuery(query, base::SysUTF8ToCFStringRef(
                               constants::kDeviceTrustSigningKeyLabel));
        return true;
      });

  EXPECT_CALL(*mock_secure_enclave_helper_, CreateSecureKey(_))
      .Times(1)
      .WillOnce([this](CFDictionaryRef attributes) {
        EXPECT_TRUE(CFEqual(
            base::SysUTF8ToCFStringRef(constants::kDeviceTrustSigningKeyLabel),
            base::mac::GetValueFromDictionary<CFStringRef>(attributes,
                                                           kSecAttrLabel)));
        EXPECT_TRUE(CFEqual(kSecAttrKeyTypeECSECPrimeRandom,
                            base::mac::GetValueFromDictionary<CFStringRef>(
                                attributes, kSecAttrKeyType)));
        EXPECT_TRUE(CFEqual(kSecAttrTokenIDSecureEnclave,
                            base::mac::GetValueFromDictionary<CFStringRef>(
                                attributes, kSecAttrTokenID)));
        EXPECT_TRUE(
            CFEqual(@256, base::mac::GetValueFromDictionary<CFNumberRef>(
                              attributes, kSecAttrKeySizeInBits)));
        auto* private_key_attributes =
            base::mac::GetValueFromDictionary<CFDictionaryRef>(
                attributes, kSecPrivateKeyAttrs);
        EXPECT_TRUE(
            CFEqual(@YES, base::mac::GetValueFromDictionary<CFBooleanRef>(
                              private_key_attributes, kSecAttrIsPermanent)));
        return test_key_;
      });
  EXPECT_EQ(secure_enclave_client_->CreatePermanentKey(), test_key_);

  // Should expect no create key failure metrics.
  histogram_tester.ExpectTotalCount(kPermanentStatusHistogramName, 0);
  histogram_tester.ExpectTotalCount(kTemporaryStatusHistogramName, 0);
}

// Tests that a create key failure metric is logged when the CreatePermanentKey
// method fails to create the permanent key.
TEST_F(SecureEnclaveClientTest, CreateKey_Failure) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*mock_secure_enclave_helper_, Delete(_))
      .Times(1)
      .WillOnce([this](CFDictionaryRef query) {
        VerifyQuery(query, base::SysUTF8ToCFStringRef(
                               constants::kDeviceTrustSigningKeyLabel));
        return true;
      });

  EXPECT_CALL(*mock_secure_enclave_helper_, CreateSecureKey(_))
      .Times(1)
      .WillOnce([](CFDictionaryRef attributes) {
        return base::ScopedCFTypeRef<SecKeyRef>();
      });
  EXPECT_FALSE(secure_enclave_client_->CreatePermanentKey());

  // Should expect one create key failure metric for the permanent key.
  histogram_tester.ExpectUniqueSample(
      kPermanentStatusHistogramName,
      SecureEnclaveOperationStatus::kCreateSecureKeyFailed, 1);

  // Should expect no create key failure metric for the temporary key.
  histogram_tester.ExpectTotalCount(kTemporaryStatusHistogramName, 0);
}

// Tests when the CopyStoredKey method invokes the SE helper's CopyKey method
// and a key is found using both a permanent and a temporary key type.
TEST_F(SecureEnclaveClientTest, CopyStoredKey_KeyFound) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*mock_secure_enclave_helper_, CopyKey(_))
      .Times(2)
      .WillRepeatedly([this](CFDictionaryRef query) { return test_key_; });
  EXPECT_EQ(secure_enclave_client_->CopyStoredKey(
                SecureEnclaveClient::KeyType::kPermanent),
            test_key_);
  EXPECT_EQ(secure_enclave_client_->CopyStoredKey(
                SecureEnclaveClient::KeyType::kTemporary),
            test_key_);

  // Should expect no copy key failure metrics.
  histogram_tester.ExpectTotalCount(kPermanentStatusHistogramName, 0);
  histogram_tester.ExpectTotalCount(kTemporaryStatusHistogramName, 0);
}

// Tests when the CopyStoredKey method invokes the SE helper's CopyKey method
// and a key is not found using both a permanent and a temporary key type.
TEST_F(SecureEnclaveClientTest, CopyStoredKey_KeyNotFound) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*mock_secure_enclave_helper_, CopyKey(_))
      .Times(2)
      .WillRepeatedly([](CFDictionaryRef query) {
        return base::ScopedCFTypeRef<SecKeyRef>();
      });
  EXPECT_FALSE(secure_enclave_client_->CopyStoredKey(
      SecureEnclaveClient::KeyType::kPermanent));
  EXPECT_FALSE(secure_enclave_client_->CopyStoredKey(
      SecureEnclaveClient::KeyType::kTemporary));

  auto status = data_protection_keychain_
                    ? SecureEnclaveOperationStatus::
                          kCopySecureKeyRefDataProtectionKeychainFailed
                    : SecureEnclaveOperationStatus::kCopySecureKeyRefFailed;

  // Should expect one copy key reference failure metric for the permanent key.
  histogram_tester.ExpectUniqueSample(kPermanentStatusHistogramName, status, 1);

  // Should expect one copy key reference failure metric for the temporary key.
  histogram_tester.ExpectUniqueSample(kTemporaryStatusHistogramName, status, 1);
}

// Tests that the UpdateStoredKeyLabel method invokes the SE helper's
// Update method and that the key attributes and query are set correctly for
// the permanent key label being updated to the temporary key label.
TEST_F(SecureEnclaveClientTest,
       UpdateStoredKeyLabel_PermanentToTemporary_Success) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*mock_secure_enclave_helper_, Delete(_))
      .Times(1)
      .WillOnce([](CFDictionaryRef query) { return true; });

  EXPECT_CALL(*mock_secure_enclave_helper_, Update(_, _))
      .Times(1)
      .WillOnce(
          [this](CFDictionaryRef query, CFDictionaryRef attribute_to_update) {
            EXPECT_TRUE(
                CFEqual(base::SysUTF8ToCFStringRef(
                            constants::kTemporaryDeviceTrustSigningKeyLabel),
                        base::mac::GetValueFromDictionary<CFStringRef>(
                            attribute_to_update, kSecAttrLabel)));
            VerifyQuery(query, base::SysUTF8ToCFStringRef(
                                   constants::kDeviceTrustSigningKeyLabel));
            return true;
          });
  EXPECT_TRUE(secure_enclave_client_->UpdateStoredKeyLabel(
      SecureEnclaveClient::KeyType::kPermanent,
      SecureEnclaveClient::KeyType::kTemporary));

  // Should expect no update key failure metrics.
  histogram_tester.ExpectTotalCount(kPermanentStatusHistogramName, 0);
  histogram_tester.ExpectTotalCount(kTemporaryStatusHistogramName, 0);
}

// Tests that an update key failure metric is logged when the
// UpdateStoredKeyLabel method fails to update the permanent key to temporary
// key storage.
TEST_F(SecureEnclaveClientTest,
       UpdateStoredKeyLabel_PermanentToTemporary_Failure) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*mock_secure_enclave_helper_, Delete(_))
      .Times(1)
      .WillOnce([](CFDictionaryRef query) { return true; });

  EXPECT_CALL(*mock_secure_enclave_helper_, Update(_, _))
      .Times(1)
      .WillOnce([](CFDictionaryRef query, CFDictionaryRef attribute_to_update) {
        return false;
      });
  EXPECT_FALSE(secure_enclave_client_->UpdateStoredKeyLabel(
      SecureEnclaveClient::KeyType::kPermanent,
      SecureEnclaveClient::KeyType::kTemporary));

  auto status = data_protection_keychain_
                    ? SecureEnclaveOperationStatus::
                          kUpdateSecureKeyLabelDataProtectionKeychainFailed
                    : SecureEnclaveOperationStatus::kUpdateSecureKeyLabelFailed;

  // Should expect an update failure metric for the permanent key.
  histogram_tester.ExpectUniqueSample(kPermanentStatusHistogramName, status, 1);

  // Should expect no update key failure metric for the temporary key.
  histogram_tester.ExpectTotalCount(kTemporaryStatusHistogramName, 0);
}

// Tests that the UpdateStoredKeyLabel method invokes the SE helper's
// Update method and that the key attributes and query are set correctly for
// the temporary key label being updated to the permanent key label.
TEST_F(SecureEnclaveClientTest,
       UpdateStoredKeyLabel_TemporaryToPermanent_Success) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*mock_secure_enclave_helper_, Delete(_))
      .Times(1)
      .WillOnce([](CFDictionaryRef query) { return true; });

  EXPECT_CALL(*mock_secure_enclave_helper_, Update(_, _))
      .Times(1)
      .WillOnce([this](CFDictionaryRef query,
                       CFDictionaryRef attribute_to_update) {
        EXPECT_TRUE(CFEqual(
            base::SysUTF8ToCFStringRef(constants::kDeviceTrustSigningKeyLabel),
            base::mac::GetValueFromDictionary<CFStringRef>(attribute_to_update,
                                                           kSecAttrLabel)));
        VerifyQuery(query,
                    base::SysUTF8ToCFStringRef(
                        constants::kTemporaryDeviceTrustSigningKeyLabel));
        return true;
      });
  EXPECT_TRUE(secure_enclave_client_->UpdateStoredKeyLabel(
      SecureEnclaveClient::KeyType::kTemporary,
      SecureEnclaveClient::KeyType::kPermanent));

  // Should expect no update key failure metrics.
  histogram_tester.ExpectTotalCount(kPermanentStatusHistogramName, 0);
  histogram_tester.ExpectTotalCount(kTemporaryStatusHistogramName, 0);
}

// Tests that an update key failure metric is logged when the
// UpdateStoredKeyLabel method fails to update the temporary key to permanent
// key storage.
TEST_F(SecureEnclaveClientTest,
       UpdateStoredKeyLabel_TemporaryToPermanent_Failure) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*mock_secure_enclave_helper_, Delete(_))
      .Times(1)
      .WillOnce([](CFDictionaryRef query) { return true; });

  EXPECT_CALL(*mock_secure_enclave_helper_, Update(_, _))
      .Times(1)
      .WillOnce([](CFDictionaryRef query, CFDictionaryRef attribute_to_update) {
        return false;
      });
  EXPECT_FALSE(secure_enclave_client_->UpdateStoredKeyLabel(
      SecureEnclaveClient::KeyType::kTemporary,
      SecureEnclaveClient::KeyType::kPermanent));

  auto status = data_protection_keychain_
                    ? SecureEnclaveOperationStatus::
                          kUpdateSecureKeyLabelDataProtectionKeychainFailed
                    : SecureEnclaveOperationStatus::kUpdateSecureKeyLabelFailed;

  // Should expect an update failure metric for the temporary key.
  histogram_tester.ExpectUniqueSample(kTemporaryStatusHistogramName, status, 1);

  // Should expect no update key failure metric for the permanent key.
  histogram_tester.ExpectTotalCount(kPermanentStatusHistogramName, 0);
}

// Tests that the DeleteKey method invokes the SE helper's Delete method
// and that the key query is set correctly with the temporary key label.
TEST_F(SecureEnclaveClientTest, DeleteKey_TempKeyLabel_Success) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*mock_secure_enclave_helper_, Delete(_))
      .Times(1)
      .WillOnce([this](CFDictionaryRef query) {
        VerifyQuery(query,
                    base::SysUTF8ToCFStringRef(
                        constants::kTemporaryDeviceTrustSigningKeyLabel));
        return true;
      });
  EXPECT_TRUE(secure_enclave_client_->DeleteKey(
      SecureEnclaveClient::KeyType::kTemporary));

  // Should expect no delete key failure metrics.
  histogram_tester.ExpectTotalCount(kPermanentStatusHistogramName, 0);
  histogram_tester.ExpectTotalCount(kTemporaryStatusHistogramName, 0);
}

// Tests that a delete key failure metric is logged when the DeleteKey method
// fails to delete the temporary key.
TEST_F(SecureEnclaveClientTest, DeleteKey_TempKeyLabel_Failure) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*mock_secure_enclave_helper_, Delete(_))
      .Times(1)
      .WillOnce([](CFDictionaryRef query) { return false; });
  EXPECT_FALSE(secure_enclave_client_->DeleteKey(
      SecureEnclaveClient::KeyType::kTemporary));

  auto status = data_protection_keychain_
                    ? SecureEnclaveOperationStatus::
                          kDeleteSecureKeyDataProtectionKeychainFailed
                    : SecureEnclaveOperationStatus::kDeleteSecureKeyFailed;

  // Should expect one delete key failure metric for the temporary key.
  histogram_tester.ExpectUniqueSample(kTemporaryStatusHistogramName, status, 1);

  // Should expect no delete key failure metric for the permanent key.
  histogram_tester.ExpectTotalCount(kPermanentStatusHistogramName, 0);
}

// Tests that the DeleteKey method invokes the SE helper's Delete method
// and that the key query is set correctly with the permanent key label.
TEST_F(SecureEnclaveClientTest, DeleteKey_PermanentKeyLabel_Success) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*mock_secure_enclave_helper_, Delete(_))
      .Times(1)
      .WillOnce([this](CFDictionaryRef query) {
        VerifyQuery(query, base::SysUTF8ToCFStringRef(
                               constants::kDeviceTrustSigningKeyLabel));
        return true;
      });
  EXPECT_TRUE(secure_enclave_client_->DeleteKey(
      SecureEnclaveClient::KeyType::kPermanent));

  // Should expect no delete key failure metrics.
  histogram_tester.ExpectTotalCount(kPermanentStatusHistogramName, 0);
  histogram_tester.ExpectTotalCount(kTemporaryStatusHistogramName, 0);
}

// Tests that a delete key failure metric is logged when the DeleteKey method
// fails to delete the permanent key.
TEST_F(SecureEnclaveClientTest, DeleteKey_PermanentKeyLabel_Failure) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*mock_secure_enclave_helper_, Delete(_))
      .Times(1)
      .WillOnce([](CFDictionaryRef query) { return false; });
  EXPECT_FALSE(secure_enclave_client_->DeleteKey(
      SecureEnclaveClient::KeyType::kPermanent));

  auto status = data_protection_keychain_
                    ? SecureEnclaveOperationStatus::
                          kDeleteSecureKeyDataProtectionKeychainFailed
                    : SecureEnclaveOperationStatus::kDeleteSecureKeyFailed;

  // Should expect one delete key failure metric for the permanent key.
  histogram_tester.ExpectUniqueSample(kPermanentStatusHistogramName, status, 1);

  // Should expect no delete key failure metric for the temporary key.
  histogram_tester.ExpectTotalCount(kTemporaryStatusHistogramName, 0);
}

// Tests that the GetStoredKeyLabel method invokes the SE helper's CopyKey
// method, and that the query and output is correct for a temporary key.
TEST_F(SecureEnclaveClientTest, GetStoredKeyLabel_TempKeyLabelFound) {
  base::HistogramTester histogram_tester;

  std::vector<uint8_t> output;
  std::string temp_label = constants::kTemporaryDeviceTrustSigningKeyLabel;
  EXPECT_CALL(*mock_secure_enclave_helper_, CopyKey(_))
      .Times(1)
      .WillOnce([this, &temp_label](CFDictionaryRef query) {
        VerifyQuery(query, base::SysUTF8ToCFStringRef(temp_label));
        return test_key_;
      });

  EXPECT_TRUE(secure_enclave_client_->GetStoredKeyLabel(
      SecureEnclaveClient::KeyType::kTemporary, output));
  std::vector<uint8_t> expected_output;
  expected_output.assign(temp_label.begin(), temp_label.end());
  EXPECT_EQ(expected_output, output);

  // Should expect no copy key failure metrics.
  histogram_tester.ExpectTotalCount(kPermanentStatusHistogramName, 0);
  histogram_tester.ExpectTotalCount(kTemporaryStatusHistogramName, 0);
}

// Tests that the GetStoredKeyLabel method invokes the SE helper's CopyKey
// method and that the query search returns false. Also tests that a copy key
// failure metric is logged.
TEST_F(SecureEnclaveClientTest, GetStoredKeyLabel_TemporaryKeyNotFound) {
  base::HistogramTester histogram_tester;

  std::vector<uint8_t> output;
  EXPECT_CALL(*mock_secure_enclave_helper_, CopyKey(_))
      .Times(1)
      .WillOnce([](CFDictionaryRef query) {
        return base::ScopedCFTypeRef<SecKeyRef>();
      });
  EXPECT_FALSE(secure_enclave_client_->GetStoredKeyLabel(
      SecureEnclaveClient::KeyType::kTemporary, output));
  std::vector<uint8_t> expected_output;
  EXPECT_EQ(expected_output, output);

  auto status = data_protection_keychain_
                    ? SecureEnclaveOperationStatus::
                          kCopySecureKeyRefDataProtectionKeychainFailed
                    : SecureEnclaveOperationStatus::kCopySecureKeyRefFailed;

  // Should expect one copy key failure metric for the temporary key.
  histogram_tester.ExpectUniqueSample(kTemporaryStatusHistogramName, status, 1);

  // Should expect no copy key failure metric for the permanent key.
  histogram_tester.ExpectTotalCount(kPermanentStatusHistogramName, 0);
}

// Tests that the GetStoredKeyLabel method invokes the SE helper's CopyKey
// method and that the query and output is correct for a permanent key.
TEST_F(SecureEnclaveClientTest, GetStoredKeyLabel_PermanentKeyLabelFound) {
  base::HistogramTester histogram_tester;

  std::vector<uint8_t> output;
  std::string permanent_label = constants::kDeviceTrustSigningKeyLabel;
  EXPECT_CALL(*mock_secure_enclave_helper_, CopyKey(_))
      .Times(1)
      .WillOnce([this, &permanent_label](CFDictionaryRef query) {
        VerifyQuery(query, base::SysUTF8ToCFStringRef(permanent_label));
        return test_key_;
      });
  EXPECT_TRUE(secure_enclave_client_->GetStoredKeyLabel(
      SecureEnclaveClient::KeyType::kPermanent, output));
  std::vector<uint8_t> expected_output;
  expected_output.assign(permanent_label.begin(), permanent_label.end());
  EXPECT_EQ(expected_output, output);

  // Should expect no copy key failure metrics.
  histogram_tester.ExpectTotalCount(kPermanentStatusHistogramName, 0);
  histogram_tester.ExpectTotalCount(kTemporaryStatusHistogramName, 0);
}

// Tests that the GetStoredKeyLabel method invokes the SE helper's CopyKey
// method and that the query search returns false. Also tests that a copy key
// failure metric is logged.
TEST_F(SecureEnclaveClientTest, GetStoredKeyLabel_PermanentKeyNotFound) {
  base::HistogramTester histogram_tester;

  std::vector<uint8_t> output;
  EXPECT_CALL(*mock_secure_enclave_helper_, CopyKey(_))
      .Times(1)
      .WillOnce([](CFDictionaryRef query) {
        return base::ScopedCFTypeRef<SecKeyRef>();
      });
  EXPECT_FALSE(secure_enclave_client_->GetStoredKeyLabel(
      SecureEnclaveClient::KeyType::kPermanent, output));
  std::vector<uint8_t> expected_output;
  EXPECT_EQ(expected_output, output);

  auto status = data_protection_keychain_
                    ? SecureEnclaveOperationStatus::
                          kCopySecureKeyRefDataProtectionKeychainFailed
                    : SecureEnclaveOperationStatus::kCopySecureKeyRefFailed;

  // Should expect one copy key failure metric for the permanent key.
  histogram_tester.ExpectUniqueSample(kPermanentStatusHistogramName, status, 1);

  // Should expect no copy key failure metric for the temporary key.
  histogram_tester.ExpectTotalCount(kTemporaryStatusHistogramName, 0);
}

// Tests that the ExportPublicKey method successfully creates the public key
// data and stores it in output.
TEST_F(SecureEnclaveClientTest, ExportPublicKey) {
  std::vector<uint8_t> output;
  EXPECT_TRUE(secure_enclave_client_->ExportPublicKey(test_key_, output));
  EXPECT_TRUE(output.size() > 0);
}

// Tests that the SignDataWithKey method successfully creates a signature
// and stores it in output.
TEST_F(SecureEnclaveClientTest, SignDataWithKey) {
  std::vector<uint8_t> output;
  std::string data = "test_string";
  EXPECT_TRUE(secure_enclave_client_->SignDataWithKey(
      test_key_, base::as_bytes(base::make_span(data)), output));
  EXPECT_TRUE(output.size() > 0);
}

// Tests that the VerifySecureEnclaveSupported method invokes the SE helper's
// IsSecureEnclaveSupported method.
TEST_F(SecureEnclaveClientTest, VerifySecureEnclaveSupported) {
  EXPECT_CALL(*mock_secure_enclave_helper_, IsSecureEnclaveSupported())
      .Times(1)
      .WillOnce([]() { return true; });
  EXPECT_TRUE(secure_enclave_client_->VerifySecureEnclaveSupported());
}

}  // namespace enterprise_connectors
