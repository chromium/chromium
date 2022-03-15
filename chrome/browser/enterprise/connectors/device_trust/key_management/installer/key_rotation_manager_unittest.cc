// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/key_rotation_manager.h"

#include <memory>
#include <string>
#include <utility>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/ec_signing_key.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/key_network_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/mock_key_network_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/key_persistence_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/mock_key_persistence_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/scoped_key_persistence_delegate_factory.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/metrics_util.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "crypto/unexportable_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

using BPKUP = enterprise_management::BrowserPublicKeyUploadResponse;
using BPKUR = enterprise_management::BrowserPublicKeyUploadRequest;
using testing::_;
using testing::ByMove;
using testing::InSequence;
using testing::Invoke;
using testing::Not;
using testing::Return;
using testing::StrictMock;

namespace enterprise_connectors {

using test::MockKeyNetworkDelegate;
using test::MockKeyPersistenceDelegate;
using HttpResponseCode = KeyNetworkDelegate::HttpResponseCode;

namespace {

const char kDmServerUrl[] = "dmserver.example.com";
const char kDmToken[] = "dm_token";

constexpr HttpResponseCode kSuccessCode = 200;
constexpr HttpResponseCode kHardFailureCode = 400;
constexpr HttpResponseCode kTransientFailureCode = 500;

KeyPersistenceDelegate::KeyInfo CreateEmptyKeyPair() {
  return {BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED, std::vector<uint8_t>()};
}

}  // namespace

// Tests KeyRotationManager with and without a nonce. The most significant
// effect of this is with UMA: recording will happen to either one histogram
// or another.  status_histogram_name() return the name of the histogram that
// should be recorded to and opposite_status_histogram_name() is the name of
// the histogram that should not.
class KeyRotationManagerTest : public testing::Test,
                               public ::testing::WithParamInterface<bool> {
 protected:
  bool use_nonce() const { return GetParam(); }
  std::string nonce() const { return use_nonce() ? "nonce" : std::string(); }
  const char* status_histogram_name() const {
    return use_nonce()
               ? "Enterprise.DeviceTrust.RotateSigningKey.WithNonce.Status"
               : "Enterprise.DeviceTrust.RotateSigningKey.NoNonce.Status";
  }
  const char* opposite_status_histogram_name() const {
    return use_nonce()
               ? "Enterprise.DeviceTrust.RotateSigningKey.NoNonce.Status"
               : "Enterprise.DeviceTrust.RotateSigningKey.WithNonce.Status";
  }

  const char* http_code_histogram_name() const {
    return use_nonce()
               ? "Enterprise.DeviceTrust.RotateSigningKey.WithNonce.UploadCode"
               : "Enterprise.DeviceTrust.RotateSigningKey.NoNonce.UploadCode";
  }

  test::ScopedKeyPersistenceDelegateFactory scoped_factory_;
};

// Tests a success key rotation flow when a TPM key and TPM key provider are
// available.
TEST_P(KeyRotationManagerTest, RotateWithAdminRights_Tpm_WithKey) {
  base::HistogramTester histogram_tester;

  // The factory creates instances backed by fake TPM keys.
  auto mock_persistence_delegate = scoped_factory_.CreateMockedTpmDelegate();
  auto original_key_wrapped = scoped_factory_.tpm_wrapped_key();

  // The mocked delegate is already set-up to return a working TPM key and
  // provider.
  EXPECT_CALL(*mock_persistence_delegate, LoadKeyPair());
  EXPECT_CALL(*mock_persistence_delegate, CheckRotationPermissions())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_persistence_delegate, GetTpmBackedKeyProvider()).Times(2);
  EXPECT_CALL(
      *mock_persistence_delegate,
      StoreKeyPair(BPKUR::CHROME_BROWSER_TPM_KEY, Not(original_key_wrapped)))
      .WillOnce(Return(true));

  GURL dm_server_url(kDmServerUrl);
  auto mock_network_delegate = std::make_unique<MockKeyNetworkDelegate>();
  std::string captured_body;
  EXPECT_CALL(*mock_network_delegate,
              SendPublicKeyToDmServerSync(dm_server_url, kDmToken, _))
      .WillOnce(
          Invoke([&captured_body](const GURL& url, const std::string& dm_token,
                                  const std::string& body) {
            captured_body = body;
            return kSuccessCode;
          }));

  auto manager = KeyRotationManager::CreateForTesting(
      std::move(mock_network_delegate), std::move(mock_persistence_delegate));

  EXPECT_TRUE(manager->RotateWithAdminRights(dm_server_url, kDmToken, nonce()));

  // Validate body.
  enterprise_management::DeviceManagementRequest request;
  ASSERT_TRUE(request.ParseFromString(captured_body));
  auto upload_key_request = request.browser_public_key_upload_request();
  EXPECT_EQ(BPKUR::EC_KEY, upload_key_request.key_type());
  EXPECT_EQ(BPKUR::CHROME_BROWSER_TPM_KEY,
            upload_key_request.key_trust_level());
  EXPECT_FALSE(upload_key_request.public_key().empty());
  EXPECT_FALSE(upload_key_request.signature().empty());

  // Should expect one successful attempt to rotate a key.
  histogram_tester.ExpectUniqueSample(status_histogram_name(),
                                      RotationStatus::SUCCESS, 1);
  histogram_tester.ExpectTotalCount(opposite_status_histogram_name(), 0);
  histogram_tester.ExpectUniqueSample(http_code_histogram_name(), kSuccessCode,
                                      1);
}

// Tests a success key rotation flow when TPM key provider is available, but
// no previous key was created.
TEST_P(KeyRotationManagerTest, RotateWithAdminRights_Tpm_NoKey) {
  base::HistogramTester histogram_tester;

  // The factory creates instances backed by fake TPM keys.
  auto mock_persistence_delegate = scoped_factory_.CreateMockedTpmDelegate();

  // The mocked delegate is already set-up to return a working TPM key and
  // provider. Force it to not return a key.
  EXPECT_CALL(*mock_persistence_delegate, LoadKeyPair())
      .WillOnce(Return(CreateEmptyKeyPair()));
  EXPECT_CALL(*mock_persistence_delegate, CheckRotationPermissions())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_persistence_delegate, GetTpmBackedKeyProvider());
  EXPECT_CALL(*mock_persistence_delegate,
              StoreKeyPair(BPKUR::CHROME_BROWSER_TPM_KEY, _))
      .WillOnce(Return(true));

  GURL dm_server_url(kDmServerUrl);
  auto mock_network_delegate = std::make_unique<MockKeyNetworkDelegate>();
  EXPECT_CALL(*mock_network_delegate,
              SendPublicKeyToDmServerSync(dm_server_url, kDmToken, _))
      .WillOnce(Return(kSuccessCode));

  auto manager = KeyRotationManager::CreateForTesting(
      std::move(mock_network_delegate), std::move(mock_persistence_delegate));

  EXPECT_TRUE(manager->RotateWithAdminRights(dm_server_url, kDmToken, nonce()));
  // Should expect one successful attempt to rotate a key.
  histogram_tester.ExpectUniqueSample(status_histogram_name(),
                                      RotationStatus::SUCCESS, 1);
  histogram_tester.ExpectTotalCount(opposite_status_histogram_name(), 0);
}

// Tests a success key rotation flow when a TPM key provider is not available
// and no key previously existed.
TEST_P(KeyRotationManagerTest, RotateWithAdminRights_NoTpm_NoKey) {
  base::HistogramTester histogram_tester;

  auto mock_persistence_delegate =
      std::make_unique<MockKeyPersistenceDelegate>();
  EXPECT_CALL(*mock_persistence_delegate, LoadKeyPair())
      .WillOnce(Return(CreateEmptyKeyPair()));
  EXPECT_CALL(*mock_persistence_delegate, CheckRotationPermissions())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_persistence_delegate, GetTpmBackedKeyProvider());
  EXPECT_CALL(*mock_persistence_delegate,
              StoreKeyPair(BPKUR::CHROME_BROWSER_OS_KEY, _))
      .WillOnce(Return(true));

  GURL dm_server_url(kDmServerUrl);
  auto mock_network_delegate = std::make_unique<MockKeyNetworkDelegate>();
  EXPECT_CALL(*mock_network_delegate,
              SendPublicKeyToDmServerSync(dm_server_url, kDmToken, _))
      .WillOnce(Return(kSuccessCode));

  auto manager = KeyRotationManager::CreateForTesting(
      std::move(mock_network_delegate), std::move(mock_persistence_delegate));

  EXPECT_TRUE(manager->RotateWithAdminRights(dm_server_url, kDmToken, nonce()));

  // Should expect one successful attempt to rotate a key.
  histogram_tester.ExpectUniqueSample(status_histogram_name(),
                                      RotationStatus::SUCCESS, 1);
  histogram_tester.ExpectTotalCount(opposite_status_histogram_name(), 0);
}

// Tests a failed key rotation flow when a TPM key provider is available
// and no key previously existed and the network request permanetly failed.
// Also, in this case the registry should be cleared.
TEST_P(KeyRotationManagerTest,
       RotateWithAdminRights_Tpm_WithoutKey_NetworkFails_ClearRegistry) {
  base::HistogramTester histogram_tester;

  // The factory creates instances backed by fake TPM keys.
  auto mock_persistence_delegate = scoped_factory_.CreateMockedTpmDelegate();

  InSequence s;

  // The mocked delegate is already set-up to return a working TPM key and
  // provider. Force it to not return a key.
  EXPECT_CALL(*mock_persistence_delegate, LoadKeyPair())
      .WillOnce(Return(CreateEmptyKeyPair()));

  EXPECT_CALL(*mock_persistence_delegate, CheckRotationPermissions())
      .WillOnce(Return(true));

  EXPECT_CALL(*mock_persistence_delegate, GetTpmBackedKeyProvider());
  EXPECT_CALL(*mock_persistence_delegate,
              StoreKeyPair(BPKUR::CHROME_BROWSER_TPM_KEY, _))
      .WillOnce(Return(true));

  GURL dm_server_url(kDmServerUrl);
  auto mock_network_delegate = std::make_unique<MockKeyNetworkDelegate>();
  EXPECT_CALL(*mock_network_delegate,
              SendPublicKeyToDmServerSync(dm_server_url, kDmToken, _))
      .WillOnce(Return(kHardFailureCode));

  EXPECT_CALL(
      *mock_persistence_delegate,
      StoreKeyPair(BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED, std::vector<uint8_t>()))
      .WillOnce(Return(true));

  auto manager = KeyRotationManager::CreateForTesting(
      std::move(mock_network_delegate), std::move(mock_persistence_delegate));

  EXPECT_FALSE(
      manager->RotateWithAdminRights(dm_server_url, kDmToken, nonce()));

  //   Should expect one failed attempt to rotate a key on first try.
  histogram_tester.ExpectUniqueSample(
      status_histogram_name(), RotationStatus::FAILURE_CANNOT_UPLOAD_KEY, 1);
  histogram_tester.ExpectTotalCount(opposite_status_histogram_name(), 0);
  histogram_tester.ExpectUniqueSample(http_code_histogram_name(),
                                      kHardFailureCode, 1);
}

// Tests a failed key rotation flow when a TPM key provider is available
// and no key previously existed and the network request transiently
// fails. Also, in this case the registry should be cleared.
TEST_P(
    KeyRotationManagerTest,
    RotateWithAdminRights_Tpm_WithoutKey_ExhaustedNetworkFails_ClearRegistry) {
  base::HistogramTester histogram_tester;

  // The factory creates instances backed by fake TPM keys.
  auto mock_persistence_delegate = scoped_factory_.CreateMockedTpmDelegate();

  InSequence s;

  // The mocked delegate is already set-up to return a working TPM key and
  // provider. Force it to not return a key.
  EXPECT_CALL(*mock_persistence_delegate, LoadKeyPair())
      .WillOnce(Return(CreateEmptyKeyPair()));

  EXPECT_CALL(*mock_persistence_delegate, CheckRotationPermissions())
      .WillOnce(Return(true));

  EXPECT_CALL(*mock_persistence_delegate, GetTpmBackedKeyProvider());
  EXPECT_CALL(*mock_persistence_delegate,
              StoreKeyPair(BPKUR::CHROME_BROWSER_TPM_KEY, _))
      .WillOnce(Return(true));

  GURL dm_server_url(kDmServerUrl);
  auto mock_network_delegate = std::make_unique<MockKeyNetworkDelegate>();
  EXPECT_CALL(*mock_network_delegate,
              SendPublicKeyToDmServerSync(dm_server_url, kDmToken, _))
      .WillRepeatedly(Return(kTransientFailureCode));

  EXPECT_CALL(
      *mock_persistence_delegate,
      StoreKeyPair(BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED, std::vector<uint8_t>()))
      .WillOnce(Return(true));

  auto manager = KeyRotationManager::CreateForTesting(
      std::move(mock_network_delegate), std::move(mock_persistence_delegate));

  EXPECT_FALSE(
      manager->RotateWithAdminRights(dm_server_url, kDmToken, nonce()));

  // Should expect one failed attempt to rotate a key with max tries.
  histogram_tester.ExpectUniqueSample(
      status_histogram_name(),
      RotationStatus::FAILURE_CANNOT_UPLOAD_KEY_TRIES_EXHAUSTED, 1);
  histogram_tester.ExpectTotalCount(opposite_status_histogram_name(), 0);
  histogram_tester.ExpectUniqueSample(http_code_histogram_name(),
                                      kTransientFailureCode, 1);
}

// Tests a success key rotation flow when a TPM key provider is not available
// and a key previously existed.
TEST_P(KeyRotationManagerTest, RotateWithAdminRights_NoTpm_WithKey) {
  base::HistogramTester histogram_tester;

  auto mock_persistence_delegate = scoped_factory_.CreateMockedECDelegate();

  EXPECT_CALL(*mock_persistence_delegate, LoadKeyPair());
  EXPECT_CALL(*mock_persistence_delegate, CheckRotationPermissions())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_persistence_delegate, GetTpmBackedKeyProvider());
  EXPECT_CALL(*mock_persistence_delegate,
              StoreKeyPair(BPKUR::CHROME_BROWSER_OS_KEY, _))
      .WillOnce(Return(true));

  GURL dm_server_url(kDmServerUrl);
  auto mock_network_delegate = std::make_unique<MockKeyNetworkDelegate>();
  EXPECT_CALL(*mock_network_delegate,
              SendPublicKeyToDmServerSync(dm_server_url, kDmToken, _))
      .WillOnce(Return(kSuccessCode));

  auto manager = KeyRotationManager::CreateForTesting(
      std::move(mock_network_delegate), std::move(mock_persistence_delegate));

  EXPECT_TRUE(manager->RotateWithAdminRights(dm_server_url, kDmToken, nonce()));

  // Should expect one successful attempt to rotate a key.
  histogram_tester.ExpectUniqueSample(status_histogram_name(),
                                      RotationStatus::SUCCESS, 1);
  histogram_tester.ExpectTotalCount(opposite_status_histogram_name(), 0);
}

// Tests a failed key rotation flow when a TPM key provider is not available
// and a key previously existed, but storing the new key locally failed.
TEST_P(KeyRotationManagerTest,
       RotateWithAdminRights_NoTpm_WithKey_StoreFailed) {
  base::HistogramTester histogram_tester;

  auto mock_persistence_delegate = scoped_factory_.CreateMockedECDelegate();
  auto original_key_wrapped = scoped_factory_.ec_wrapped_key();

  EXPECT_CALL(*mock_persistence_delegate, LoadKeyPair());
  EXPECT_CALL(*mock_persistence_delegate, CheckRotationPermissions())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_persistence_delegate, GetTpmBackedKeyProvider());
  EXPECT_CALL(
      *mock_persistence_delegate,
      StoreKeyPair(BPKUR::CHROME_BROWSER_OS_KEY, Not(original_key_wrapped)))
      .WillOnce(Return(false));

  auto mock_network_delegate =
      std::make_unique<testing::StrictMock<MockKeyNetworkDelegate>>();

  auto manager = KeyRotationManager::CreateForTesting(
      std::move(mock_network_delegate), std::move(mock_persistence_delegate));

  GURL dm_server_url(kDmServerUrl);
  EXPECT_FALSE(
      manager->RotateWithAdminRights(dm_server_url, kDmToken, nonce()));

  // Should expect one failed attempt to rotate a key.
  histogram_tester.ExpectUniqueSample(
      status_histogram_name(), RotationStatus::FAILURE_CANNOT_STORE_KEY, 1);
  histogram_tester.ExpectTotalCount(opposite_status_histogram_name(), 0);
}

// Tests a key rotation flow where the network request fails and the subsequent
// attempt to restore the old key also fails.
TEST_P(KeyRotationManagerTest,
       RotateWithAdminRights_NoTpm_WithKey_NetworkFails_RestoreFails) {
  base::HistogramTester histogram_tester;

  auto mock_persistence_delegate = scoped_factory_.CreateMockedECDelegate();
  auto original_key_wrapped = scoped_factory_.ec_wrapped_key();

  EXPECT_CALL(*mock_persistence_delegate, LoadKeyPair());
  EXPECT_CALL(*mock_persistence_delegate, CheckRotationPermissions())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_persistence_delegate, GetTpmBackedKeyProvider());
  EXPECT_CALL(
      *mock_persistence_delegate,
      StoreKeyPair(BPKUR::CHROME_BROWSER_OS_KEY, Not(original_key_wrapped)))
      .WillOnce(Return(true));  // Store of new key fails.
  EXPECT_CALL(*mock_persistence_delegate,
              StoreKeyPair(BPKUR::CHROME_BROWSER_OS_KEY, original_key_wrapped))
      .WillOnce(Return(false));  // Restore of old key fails.

  GURL dm_server_url(kDmServerUrl);
  auto mock_network_delegate = std::make_unique<MockKeyNetworkDelegate>();
  EXPECT_CALL(*mock_network_delegate,
              SendPublicKeyToDmServerSync(dm_server_url, kDmToken, _))
      .WillOnce(Return(kHardFailureCode));

  auto manager = KeyRotationManager::CreateForTesting(
      std::move(mock_network_delegate), std::move(mock_persistence_delegate));

  EXPECT_FALSE(
      manager->RotateWithAdminRights(dm_server_url, kDmToken, nonce()));

  // Should expect one failed attempt to rotate a key on first try.
  histogram_tester.ExpectUniqueSample(
      status_histogram_name(),
      RotationStatus::FAILURE_CANNOT_UPLOAD_KEY_RESTORE_FAILED, 1);
  histogram_tester.ExpectTotalCount(opposite_status_histogram_name(), 0);
}

// Tests a failed key rotation flow when a TPM key provider is not available
// and a key previously existed, and the network request transiently fails.
// Also, in this case, the original key should be stored back.
TEST_P(KeyRotationManagerTest,
       RotateWithAdminRights_NoTpm_WithKey_ExhaustedNetworkFailure) {
  base::HistogramTester histogram_tester;

  auto mock_persistence_delegate = scoped_factory_.CreateMockedECDelegate();
  auto original_key_wrapped = scoped_factory_.ec_wrapped_key();

  InSequence s;

  EXPECT_CALL(*mock_persistence_delegate, LoadKeyPair());
  EXPECT_CALL(*mock_persistence_delegate, CheckRotationPermissions())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_persistence_delegate, GetTpmBackedKeyProvider());
  EXPECT_CALL(
      *mock_persistence_delegate,
      StoreKeyPair(BPKUR::CHROME_BROWSER_OS_KEY, Not(original_key_wrapped)))
      .WillOnce(Return(true));

  GURL dm_server_url(kDmServerUrl);
  auto mock_network_delegate = std::make_unique<MockKeyNetworkDelegate>();
  EXPECT_CALL(*mock_network_delegate,
              SendPublicKeyToDmServerSync(dm_server_url, kDmToken, _))
      .WillRepeatedly(Return(kTransientFailureCode));

  EXPECT_CALL(*mock_persistence_delegate,
              StoreKeyPair(BPKUR::CHROME_BROWSER_OS_KEY, original_key_wrapped))
      .WillOnce(Return(true));

  auto manager = KeyRotationManager::CreateForTesting(
      std::move(mock_network_delegate), std::move(mock_persistence_delegate));

  EXPECT_FALSE(
      manager->RotateWithAdminRights(dm_server_url, kDmToken, nonce()));

  // Should expect one failed attempt to rotate a key with max tries.
  histogram_tester.ExpectUniqueSample(
      status_histogram_name(),
      RotationStatus::FAILURE_CANNOT_UPLOAD_KEY_TRIES_EXHAUSTED, 1);
  histogram_tester.ExpectTotalCount(opposite_status_histogram_name(), 0);
}

// Tests a success key rotation flow when incorrect permissions were set
// on the signing key file.
TEST_P(KeyRotationManagerTest,
       RotateWithAdminRights_StoreFailed_InvalidFilePermissions) {
  base::HistogramTester histogram_tester;

  auto mock_persistence_delegate =
      std::make_unique<MockKeyPersistenceDelegate>();
  EXPECT_CALL(*mock_persistence_delegate, CheckRotationPermissions())
      .WillOnce(Return(false));
  EXPECT_CALL(*mock_persistence_delegate, LoadKeyPair());
  EXPECT_CALL(*mock_persistence_delegate, GetTpmBackedKeyProvider()).Times(0);
  EXPECT_CALL(*mock_persistence_delegate,
              StoreKeyPair(BPKUR::CHROME_BROWSER_OS_KEY, _))
      .Times(0);

  GURL dm_server_url(kDmServerUrl);
  auto mock_network_delegate = std::make_unique<MockKeyNetworkDelegate>();
  EXPECT_CALL(*mock_network_delegate,
              SendPublicKeyToDmServerSync(dm_server_url, kDmToken, _))
      .Times(0);

  auto manager = KeyRotationManager::CreateForTesting(
      std::move(mock_network_delegate), std::move(mock_persistence_delegate));

  EXPECT_FALSE(
      manager->RotateWithAdminRights(dm_server_url, kDmToken, nonce()));

  // Should expect one successful attempt to rotate a key.
  histogram_tester.ExpectUniqueSample(
      status_histogram_name(),
      RotationStatus::FAILURE_INCORRECT_FILE_PERMISSIONS, 1);
  histogram_tester.ExpectTotalCount(opposite_status_histogram_name(), 0);
}

INSTANTIATE_TEST_SUITE_P(, KeyRotationManagerTest, testing::Bool());

}  // namespace enterprise_connectors
