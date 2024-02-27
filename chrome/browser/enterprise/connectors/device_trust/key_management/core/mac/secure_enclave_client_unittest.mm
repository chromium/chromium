// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/secure_enclave_client.h"

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>

#include <memory>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/metrics_util.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/mock_secure_enclave_helper.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/secure_enclave_helper.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/shared_command_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::apple::CFToNSPtrCast;
using base::apple::NSToCFPtrCast;
using testing::_;

namespace enterprise_connectors {

namespace {

constexpr char kPermanentStatusHistogramName[] =
    "Enterprise.DeviceTrust.Mac.SecureEnclaveOperation.Permanent";
constexpr char kTemporaryStatusHistogramName[] =
    "Enterprise.DeviceTrust.Mac.SecureEnclaveOperation.Temporary";

constexpr char kOSStatusHistogramPrefix[] =
    "Enterprise.DeviceTrust.Mac.KeychainOSStatus.";
constexpr char kKeychainOSStatusHistogramFormat[] =
    "Enterprise.DeviceTrust.Mac.KeychainOSStatus.%s.%s";

std::string GetOSStatusHistogramName(bool permanent_key,
                                     const std::string& operation) {
  return base::StringPrintf(kKeychainOSStatusHistogramFormat,
                            permanent_key ? "Permanent" : "Temporary",
                            operation.c_str());
}

}  // namespace

using test::MockSecureEnclaveHelper;

class SecureEnclaveClientTest : public testing::Test {
 protected:
  void SetUp() override {
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

  void VerifyQuery(CFDictionaryRef query, CFStringRef label) {
    EXPECT_TRUE(CFEqual(label, base::apple::GetValueFromDictionary<CFStringRef>(
                                   query, kSecAttrLabel)));
    EXPECT_TRUE(CFEqual(kSecAttrKeyTypeECSECPrimeRandom,
                        base::apple::GetValueFromDictionary<CFStringRef>(
                            query, kSecAttrKeyType)));
  }

  base::HistogramTester histogram_tester_;
  std::unique_ptr<SecureEnclaveClient> secure_enclave_client_;
  base::apple::ScopedCFTypeRef<SecKeyRef> test_key_;
  raw_ptr<MockSecureEnclaveHelper> mock_secure_enclave_helper_ = nullptr;
};

// Tests that the CreatePermanentKey method invokes both the SE helper's
// Delete and CreateSecureKey method and that the key attributes are set
// correctly.
TEST_F(SecureEnclaveClientTest, CreateKey_Success) {
  EXPECT_CALL(*mock_secure_enclave_helper_, Delete(_))
      .Times(1)
      .WillOnce([this](CFDictionaryRef query) {
        VerifyQuery(query, base::SysUTF8ToCFStringRef(
                               constants::kDeviceTrustSigningKeyLabel)
                               .get());
        return errSecSuccess;
      });

  EXPECT_CALL(*mock_secure_enclave_helper_, CreateSecureKey(_, _))
      .Times(1)
      .WillOnce([this](CFDictionaryRef attributes, OSStatus* status) {
        EXPECT_TRUE(CFEqual(
            base::SysUTF8ToCFStringRef(constants::kDeviceTrustSigningKeyLabel)
                .get(),
            base::apple::GetValueFromDictionary<CFStringRef>(attributes,
                                                             kSecAttrLabel)));
        EXPECT_TRUE(CFEqual(kSecAttrKeyTypeECSECPrimeRandom,
                            base::apple::GetValueFromDictionary<CFStringRef>(
                                attributes, kSecAttrKeyType)));
        EXPECT_TRUE(CFEqual(kSecAttrTokenIDSecureEnclave,
                            base::apple::GetValueFromDictionary<CFStringRef>(
                                attributes, kSecAttrTokenID)));
        EXPECT_TRUE(CFEqual(base::apple::NSToCFPtrCast(@256),
                            base::apple::GetValueFromDictionary<CFNumberRef>(
                                attributes, kSecAttrKeySizeInBits)));
        auto* private_key_attributes =
            base::apple::GetValueFromDictionary<CFDictionaryRef>(
                attributes, kSecPrivateKeyAttrs);
        EXPECT_TRUE(CFEqual(kCFBooleanTrue,
                            base::apple::GetValueFromDictionary<CFBooleanRef>(
                                private_key_attributes, kSecAttrIsPermanent)));

        *status = errSecSuccess;
        return test_key_;
      });
  EXPECT_EQ(secure_enclave_client_->CreatePermanentKey(), test_key_);

  // Should expect no create key failure metrics.
  histogram_tester_.ExpectTotalCount(kPermanentStatusHistogramName, 0);
  histogram_tester_.ExpectTotalCount(kTemporaryStatusHistogramName, 0);
  EXPECT_TRUE(
      histogram_tester_.GetTotalCountsForPrefix(kOSStatusHistogramPrefix)
          .empty());
}

// Tests that a create key failure metric is logged when the CreatePermanentKey
// method fails to create the permanent key.
TEST_F(SecureEnclaveClientTest, CreateKey_Failure) {
  EXPECT_CALL(*mock_secure_enclave_helper_, Delete(_))
      .Times(1)
      .WillOnce([this](CFDictionaryRef query) {
        VerifyQuery(query, base::SysUTF8ToCFStringRef(
                               constants::kDeviceTrustSigningKeyLabel)
                               .get());
        return errSecSuccess;
      });

  EXPECT_CALL(*mock_secure_enclave_helper_, CreateSecureKey(_, _))
      .Times(1)
      .WillOnce([](CFDictionaryRef attributes, OSStatus* status) {
        *status = errSecItemNotFound;
        return base::apple::ScopedCFTypeRef<SecKeyRef>();
      });
  EXPECT_FALSE(secure_enclave_client_->CreatePermanentKey());

  // Should expect one create key failure metric for the permanent key.
  histogram_tester_.ExpectUniqueSample(
      kPermanentStatusHistogramName,
      SecureEnclaveOperationStatus::kCreateSecureKeyFailed, 1);
  histogram_tester_.ExpectUniqueSample(GetOSStatusHistogramName(true, "Create"),
                                       errSecItemNotFound, 1);
  EXPECT_EQ(histogram_tester_.GetTotalCountsForPrefix(kOSStatusHistogramPrefix)
                .size(),
            1U);

  // Should expect no create key failure metric for the temporary key.
  histogram_tester_.ExpectTotalCount(kTemporaryStatusHistogramName, 0);
}

// Tests when the CopyStoredKey method invokes the SE helper's CopyKey method
// and a key is found using both a permanent and a temporary key type.
TEST_F(SecureEnclaveClientTest, CopyStoredKey_KeyFound) {
  EXPECT_CALL(*mock_secure_enclave_helper_, CopyKey(_, _))
      .Times(2)
      .WillRepeatedly([this](CFDictionaryRef query, OSStatus* status) {
        *status = errSecSuccess;
        return test_key_;
      });
  EXPECT_EQ(secure_enclave_client_->CopyStoredKey(
                SecureEnclaveClient::KeyType::kPermanent, nullptr),
            test_key_);
  EXPECT_EQ(secure_enclave_client_->CopyStoredKey(
                SecureEnclaveClient::KeyType::kTemporary, nullptr),
            test_key_);

  // Should expect no copy key failure metrics.
  histogram_tester_.ExpectTotalCount(kPermanentStatusHistogramName, 0);
  histogram_tester_.ExpectTotalCount(kTemporaryStatusHistogramName, 0);
  EXPECT_TRUE(
      histogram_tester_.GetTotalCountsForPrefix(kOSStatusHistogramPrefix)
          .empty());
}

// Tests when the CopyStoredKey method invokes the SE helper's CopyKey method
// and a key is not found using both a permanent and a temporary key type.
TEST_F(SecureEnclaveClientTest, CopyStoredKey_KeyNotFound) {
  EXPECT_CALL(*mock_secure_enclave_helper_, CopyKey(_, _))
      .Times(2)
      .WillRepeatedly([](CFDictionaryRef query, OSStatus* status) {
        *status = errSecItemNotFound;
        return base::apple::ScopedCFTypeRef<SecKeyRef>();
      });

  OSStatus error;
  EXPECT_FALSE(secure_enclave_client_->CopyStoredKey(
      SecureEnclaveClient::KeyType::kPermanent, &error));
  EXPECT_EQ(error, errSecItemNotFound);

  // Reset the error.
  error = errSecSuccess;

  EXPECT_FALSE(secure_enclave_client_->CopyStoredKey(
      SecureEnclaveClient::KeyType::kTemporary, &error));
  EXPECT_EQ(error, errSecItemNotFound);

  auto status = SecureEnclaveOperationStatus::
      kCopySecureKeyRefDataProtectionKeychainFailed;

  // Should expect one copy key reference failure metric for the permanent key.
  histogram_tester_.ExpectUniqueSample(kPermanentStatusHistogramName, status,
                                       1);

  // Should expect one copy key reference failure metric for the temporary key.
  histogram_tester_.ExpectUniqueSample(kTemporaryStatusHistogramName, status,
                                       1);

  histogram_tester_.ExpectUniqueSample(GetOSStatusHistogramName(true, "Copy"),
                                       errSecItemNotFound, 1);
  histogram_tester_.ExpectUniqueSample(GetOSStatusHistogramName(false, "Copy"),
                                       errSecItemNotFound, 1);
  EXPECT_EQ(histogram_tester_.GetTotalCountsForPrefix(kOSStatusHistogramPrefix)
                .size(),
            2U);
}

// Tests that the UpdateStoredKeyLabel method invokes the SE helper's
// Update method and that the key attributes and query are set correctly for
// the permanent key label being updated to the temporary key label.
TEST_F(SecureEnclaveClientTest,
       UpdateStoredKeyLabel_PermanentToTemporary_Success) {
  EXPECT_CALL(*mock_secure_enclave_helper_, Delete(_))
      .Times(1)
      .WillOnce([](CFDictionaryRef query) { return errSecSuccess; });

  EXPECT_CALL(*mock_secure_enclave_helper_, Update(_, _))
      .Times(1)
      .WillOnce([this](CFDictionaryRef query,
                       CFDictionaryRef attribute_to_update) {
        EXPECT_TRUE(CFEqual(base::SysUTF8ToCFStringRef(
                                constants::kTemporaryDeviceTrustSigningKeyLabel)
                                .get(),
                            base::apple::GetValueFromDictionary<CFStringRef>(
                                attribute_to_update, kSecAttrLabel)));
        VerifyQuery(query, base::SysUTF8ToCFStringRef(
                               constants::kDeviceTrustSigningKeyLabel)
                               .get());
        return errSecSuccess;
      });
  EXPECT_TRUE(secure_enclave_client_->UpdateStoredKeyLabel(
      SecureEnclaveClient::KeyType::kPermanent,
      SecureEnclaveClient::KeyType::kTemporary));

  // Should expect no update key failure metrics.
  histogram_tester_.ExpectTotalCount(kPermanentStatusHistogramName, 0);
  histogram_tester_.ExpectTotalCount(kTemporaryStatusHistogramName, 0);
  EXPECT_TRUE(
      histogram_tester_.GetTotalCountsForPrefix(kOSStatusHistogramPrefix)
          .empty());
}

// Tests that an update key failure metric is logged when the
// UpdateStoredKeyLabel method fails to update the permanent key to temporary
// key storage.
TEST_F(SecureEnclaveClientTest,
       UpdateStoredKeyLabel_PermanentToTemporary_Failure) {
  EXPECT_CALL(*mock_secure_enclave_helper_, Delete(_))
      .Times(1)
      .WillOnce([](CFDictionaryRef query) { return errSecSuccess; });

  EXPECT_CALL(*mock_secure_enclave_helper_, Update(_, _))
      .Times(1)
      .WillOnce([](CFDictionaryRef query, CFDictionaryRef attribute_to_update) {
        return errSecItemNotFound;
      });
  EXPECT_FALSE(secure_enclave_client_->UpdateStoredKeyLabel(
      SecureEnclaveClient::KeyType::kPermanent,
      SecureEnclaveClient::KeyType::kTemporary));

  auto status = SecureEnclaveOperationStatus::
      kUpdateSecureKeyLabelDataProtectionKeychainFailed;

  // Should expect an update failure metric for the permanent key.
  histogram_tester_.ExpectUniqueSample(kPermanentStatusHistogramName, status,
                                       1);
  histogram_tester_.ExpectUniqueSample(GetOSStatusHistogramName(true, "Update"),
                                       errSecItemNotFound, 1);
  EXPECT_EQ(histogram_tester_.GetTotalCountsForPrefix(kOSStatusHistogramPrefix)
                .size(),
            1U);

  // Should expect no update key failure metric for the temporary key.
  histogram_tester_.ExpectTotalCount(kTemporaryStatusHistogramName, 0);
}

// Tests that the UpdateStoredKeyLabel method invokes the SE helper's
// Update method and that the key attributes and query are set correctly for
// the temporary key label being updated to the permanent key label.
TEST_F(SecureEnclaveClientTest,
       UpdateStoredKeyLabel_TemporaryToPermanent_Success) {
  EXPECT_CALL(*mock_secure_enclave_helper_, Delete(_))
      .Times(1)
      .WillOnce([](CFDictionaryRef query) { return errSecSuccess; });

  EXPECT_CALL(*mock_secure_enclave_helper_, Update(_, _))
      .Times(1)
      .WillOnce([this](CFDictionaryRef query,
                       CFDictionaryRef attribute_to_update) {
        EXPECT_TRUE(CFEqual(
            base::SysUTF8ToCFStringRef(constants::kDeviceTrustSigningKeyLabel)
                .get(),
            base::apple::GetValueFromDictionary<CFStringRef>(
                attribute_to_update, kSecAttrLabel)));
        VerifyQuery(query, base::SysUTF8ToCFStringRef(
                               constants::kTemporaryDeviceTrustSigningKeyLabel)
                               .get());
        return errSecSuccess;
      });
  EXPECT_TRUE(secure_enclave_client_->UpdateStoredKeyLabel(
      SecureEnclaveClient::KeyType::kTemporary,
      SecureEnclaveClient::KeyType::kPermanent));

  // Should expect no update key failure metrics.
  histogram_tester_.ExpectTotalCount(kPermanentStatusHistogramName, 0);
  histogram_tester_.ExpectTotalCount(kTemporaryStatusHistogramName, 0);
  EXPECT_TRUE(
      histogram_tester_.GetTotalCountsForPrefix(kOSStatusHistogramPrefix)
          .empty());
}

// Tests that an update key failure metric is logged when the
// UpdateStoredKeyLabel method fails to update the temporary key to permanent
// key storage.
TEST_F(SecureEnclaveClientTest,
       UpdateStoredKeyLabel_TemporaryToPermanent_Failure) {
  EXPECT_CALL(*mock_secure_enclave_helper_, Delete(_))
      .Times(1)
      .WillOnce([](CFDictionaryRef query) { return errSecSuccess; });

  EXPECT_CALL(*mock_secure_enclave_helper_, Update(_, _))
      .Times(1)
      .WillOnce([](CFDictionaryRef query, CFDictionaryRef attribute_to_update) {
        return errSecItemNotFound;
      });
  EXPECT_FALSE(secure_enclave_client_->UpdateStoredKeyLabel(
      SecureEnclaveClient::KeyType::kTemporary,
      SecureEnclaveClient::KeyType::kPermanent));

  auto status = SecureEnclaveOperationStatus::
      kUpdateSecureKeyLabelDataProtectionKeychainFailed;

  // Should expect an update failure metric for the temporary key.
  histogram_tester_.ExpectUniqueSample(kTemporaryStatusHistogramName, status,
                                       1);
  histogram_tester_.ExpectUniqueSample(
      GetOSStatusHistogramName(false, "Update"), errSecItemNotFound, 1);
  EXPECT_EQ(histogram_tester_.GetTotalCountsForPrefix(kOSStatusHistogramPrefix)
                .size(),
            1U);

  // Should expect no update key failure metric for the permanent key.
  histogram_tester_.ExpectTotalCount(kPermanentStatusHistogramName, 0);
}

// Tests that the DeleteKey method invokes the SE helper's Delete method
// and that the key query is set correctly with the temporary key label.
TEST_F(SecureEnclaveClientTest, DeleteKey_TempKeyLabel_Success) {
  EXPECT_CALL(*mock_secure_enclave_helper_, Delete(_))
      .Times(1)
      .WillOnce([this](CFDictionaryRef query) {
        VerifyQuery(query, base::SysUTF8ToCFStringRef(
                               constants::kTemporaryDeviceTrustSigningKeyLabel)
                               .get());
        return errSecSuccess;
      });
  EXPECT_TRUE(secure_enclave_client_->DeleteKey(
      SecureEnclaveClient::KeyType::kTemporary));

  // Should expect no delete key failure metrics.
  histogram_tester_.ExpectTotalCount(kPermanentStatusHistogramName, 0);
  histogram_tester_.ExpectTotalCount(kTemporaryStatusHistogramName, 0);
  EXPECT_TRUE(
      histogram_tester_.GetTotalCountsForPrefix(kOSStatusHistogramPrefix)
          .empty());
}

// Tests that a delete key failure metric is logged when the DeleteKey method
// fails to delete the temporary key.
TEST_F(SecureEnclaveClientTest, DeleteKey_TempKeyLabel_Failure) {
  EXPECT_CALL(*mock_secure_enclave_helper_, Delete(_))
      .Times(1)
      .WillOnce([](CFDictionaryRef query) { return errSecItemNotFound; });
  EXPECT_FALSE(secure_enclave_client_->DeleteKey(
      SecureEnclaveClient::KeyType::kTemporary));

  auto status = SecureEnclaveOperationStatus::
      kDeleteSecureKeyDataProtectionKeychainFailed;

  // Should expect one delete key failure metric for the temporary key.
  histogram_tester_.ExpectUniqueSample(kTemporaryStatusHistogramName, status,
                                       1);
  histogram_tester_.ExpectUniqueSample(
      GetOSStatusHistogramName(false, "Delete"), errSecItemNotFound, 1);
  EXPECT_EQ(histogram_tester_.GetTotalCountsForPrefix(kOSStatusHistogramPrefix)
                .size(),
            1U);

  // Should expect no delete key failure metric for the permanent key.
  histogram_tester_.ExpectTotalCount(kPermanentStatusHistogramName, 0);
}

// Tests that the DeleteKey method invokes the SE helper's Delete method
// and that the key query is set correctly with the permanent key label.
TEST_F(SecureEnclaveClientTest, DeleteKey_PermanentKeyLabel_Success) {
  EXPECT_CALL(*mock_secure_enclave_helper_, Delete(_))
      .Times(1)
      .WillOnce([this](CFDictionaryRef query) {
        VerifyQuery(query, base::SysUTF8ToCFStringRef(
                               constants::kDeviceTrustSigningKeyLabel)
                               .get());
        return errSecSuccess;
      });
  EXPECT_TRUE(secure_enclave_client_->DeleteKey(
      SecureEnclaveClient::KeyType::kPermanent));

  // Should expect no delete key failure metrics.
  histogram_tester_.ExpectTotalCount(kPermanentStatusHistogramName, 0);
  histogram_tester_.ExpectTotalCount(kTemporaryStatusHistogramName, 0);
  EXPECT_TRUE(
      histogram_tester_.GetTotalCountsForPrefix(kOSStatusHistogramPrefix)
          .empty());
}

// Tests that a delete key failure metric is logged when the DeleteKey method
// fails to delete the permanent key.
TEST_F(SecureEnclaveClientTest, DeleteKey_PermanentKeyLabel_Failure) {
  EXPECT_CALL(*mock_secure_enclave_helper_, Delete(_))
      .Times(1)
      .WillOnce([](CFDictionaryRef query) { return errSecItemNotFound; });
  EXPECT_FALSE(secure_enclave_client_->DeleteKey(
      SecureEnclaveClient::KeyType::kPermanent));

  auto status = SecureEnclaveOperationStatus::
      kDeleteSecureKeyDataProtectionKeychainFailed;

  // Should expect one delete key failure metric for the permanent key.
  histogram_tester_.ExpectUniqueSample(kPermanentStatusHistogramName, status,
                                       1);
  histogram_tester_.ExpectUniqueSample(GetOSStatusHistogramName(true, "Delete"),
                                       errSecItemNotFound, 1);
  EXPECT_EQ(histogram_tester_.GetTotalCountsForPrefix(kOSStatusHistogramPrefix)
                .size(),
            1U);

  // Should expect no delete key failure metric for the temporary key.
  histogram_tester_.ExpectTotalCount(kTemporaryStatusHistogramName, 0);
}

// Tests that the ExportPublicKey method successfully creates the public key
// data and stores it in output.
TEST_F(SecureEnclaveClientTest, ExportPublicKey) {
  std::vector<uint8_t> output;
  OSStatus error;
  EXPECT_TRUE(
      secure_enclave_client_->ExportPublicKey(test_key_.get(), output, &error));
  EXPECT_TRUE(output.size() > 0);
}

// Tests that the SignDataWithKey method successfully creates a signature
// and stores it in output.
TEST_F(SecureEnclaveClientTest, SignDataWithKey) {
  std::vector<uint8_t> output;
  std::string data = "test_string";
  OSStatus error;
  EXPECT_TRUE(secure_enclave_client_->SignDataWithKey(
      test_key_.get(), base::as_bytes(base::make_span(data)), output, &error));
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
