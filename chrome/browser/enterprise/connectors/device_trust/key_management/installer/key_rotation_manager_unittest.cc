// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/key_rotation_manager.h"

#include <memory>
#include <string>
#include <utility>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/key_network_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/mock_key_network_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/key_persistence_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/mock_key_persistence_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/scoped_key_persistence_delegate_factory.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/signing_key_pair.h"
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
  base::test::TaskEnvironment task_environment_;
};

// Tests a success key rotation flow when a hardware key and hardware key
// provider are available.
TEST_P(KeyRotationManagerTest, Rotate_Hw_WithKey) {
  base::HistogramTester histogram_tester;

  // The factory creates instances backed by fake hardware keys.
  auto mock_persistence_delegate =
      scoped_factory_.CreateMockedHardwareDelegate();
  auto original_key_wrapped = scoped_factory_.hw_wrapped_key();

  // The mocked delegate is already set-up to return a working hardware key
  // and provider.
  EXPECT_CALL(*mock_persistence_delegate, LoadKeyPair());
  EXPECT_CALL(*mock_persistence_delegate, CheckRotationPermissions())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_persistence_delegate, CreateKeyPair()).Times(1);
  EXPECT_CALL(
      *mock_persistence_delegate,
      StoreKeyPair(BPKUR::CHROME_BROWSER_HW_KEY, Not(original_key_wrapped)))
      .WillOnce(Return(true));

  GURL dm_server_url(kDmServerUrl);
  auto mock_network_delegate = std::make_unique<MockKeyNetworkDelegate>();
  std::string captured_body;
  EXPECT_CALL(*mock_network_delegate,
              SendPublicKeyToDmServer(dm_server_url, kDmToken, _, _))
      .WillOnce(
          Invoke([&captured_body](const GURL& url, const std::string& dm_token,
                                  const std::string& body,
                                  base::OnceCallback<void(int)> callback) {
            captured_body = body;
            std::move(callback).Run(kSuccessCode);
          }));
  EXPECT_CALL(*mock_persistence_delegate, CleanupTemporaryKeyData());

  auto manager = KeyRotationManager::CreateForTesting(
      std::move(mock_network_delegate), std::move(mock_persistence_delegate));

  base::test::TestFuture<bool> future;
  manager->Rotate(dm_server_url, kDmToken, nonce(), future.GetCallback());
  EXPECT_TRUE(future.Get());

  // Validate body.
  enterprise_management::DeviceManagementRequest request;
  ASSERT_TRUE(request.ParseFromString(captured_body));
  auto upload_key_request = request.browser_public_key_upload_request();
  EXPECT_EQ(BPKUR::EC_KEY, upload_key_request.key_type());
  EXPECT_EQ(BPKUR::CHROME_BROWSER_HW_KEY, upload_key_request.key_trust_level());
  EXPECT_FALSE(upload_key_request.public_key().empty());
  EXPECT_FALSE(upload_key_request.signature().empty());

  // Should expect one successful attempt to rotate a key.
  histogram_tester.ExpectUniqueSample(status_histogram_name(),
                                      RotationStatus::SUCCESS, 1);
  histogram_tester.ExpectTotalCount(opposite_status_histogram_name(), 0);
  histogram_tester.ExpectUniqueSample(http_code_histogram_name(), kSuccessCode,
                                      1);
}

// Tests a success key rotation flow when hardware key provider is available,
// but no previous key was created.
TEST_P(KeyRotationManagerTest, Rotate_Hw_NoKey) {
  base::HistogramTester histogram_tester;

  // The factory creates instances backed by fake hardware keys.
  auto mock_persistence_delegate =
      scoped_factory_.CreateMockedHardwareDelegate();

  // The mocked delegate is already set-up to return a working hardware key
  // and provider. Force it to not return a key.
  EXPECT_CALL(*mock_persistence_delegate, LoadKeyPair()).WillOnce(Invoke([]() {
    return nullptr;
  }));
  EXPECT_CALL(*mock_persistence_delegate, CheckRotationPermissions())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_persistence_delegate, CreateKeyPair());
  EXPECT_CALL(*mock_persistence_delegate,
              StoreKeyPair(BPKUR::CHROME_BROWSER_HW_KEY, _))
      .WillOnce(Return(true));

  GURL dm_server_url(kDmServerUrl);
  auto mock_network_delegate = std::make_unique<MockKeyNetworkDelegate>();
  EXPECT_CALL(*mock_network_delegate,
              SendPublicKeyToDmServer(dm_server_url, kDmToken, _, _))
      .WillOnce(Invoke([](const GURL& url, const std::string& dm_token,
                          const std::string& body,
                          base::OnceCallback<void(int)> callback) {
        std::move(callback).Run(kSuccessCode);
      }));
  EXPECT_CALL(*mock_persistence_delegate, CleanupTemporaryKeyData());

  auto manager = KeyRotationManager::CreateForTesting(
      std::move(mock_network_delegate), std::move(mock_persistence_delegate));

  base::test::TestFuture<bool> future;
  manager->Rotate(dm_server_url, kDmToken, nonce(), future.GetCallback());
  EXPECT_TRUE(future.Get());

  // Should expect one successful attempt to rotate a key.
  histogram_tester.ExpectUniqueSample(status_histogram_name(),
                                      RotationStatus::SUCCESS, 1);
  histogram_tester.ExpectTotalCount(opposite_status_histogram_name(), 0);
}

// Tests a success key rotation flow when a hardware key provider is not
// available and no key previously existed.
TEST_P(KeyRotationManagerTest, Rotate_NoHw_NoKey) {
  base::HistogramTester histogram_tester;

  // The factory creates instances backed by fake EC keys.
  auto mock_persistence_delegate = scoped_factory_.CreateMockedECDelegate();

  EXPECT_CALL(*mock_persistence_delegate, LoadKeyPair()).WillOnce(Invoke([]() {
    return nullptr;
  }));
  EXPECT_CALL(*mock_persistence_delegate, CheckRotationPermissions())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_persistence_delegate, CreateKeyPair());

  EXPECT_CALL(*mock_persistence_delegate,
              StoreKeyPair(BPKUR::CHROME_BROWSER_OS_KEY, _))
      .WillOnce(Return(true));

  GURL dm_server_url(kDmServerUrl);
  auto mock_network_delegate = std::make_unique<MockKeyNetworkDelegate>();
  EXPECT_CALL(*mock_network_delegate,
              SendPublicKeyToDmServer(dm_server_url, kDmToken, _, _))
      .WillOnce(Invoke([](const GURL& url, const std::string& dm_token,
                          const std::string& body,
                          base::OnceCallback<void(int)> callback) {
        std::move(callback).Run(kSuccessCode);
      }));
  EXPECT_CALL(*mock_persistence_delegate, CleanupTemporaryKeyData());

  auto manager = KeyRotationManager::CreateForTesting(
      std::move(mock_network_delegate), std::move(mock_persistence_delegate));

  base::test::TestFuture<bool> future;
  manager->Rotate(dm_server_url, kDmToken, nonce(), future.GetCallback());
  EXPECT_TRUE(future.Get());

  // Should expect one successful attempt to rotate a key.
  histogram_tester.ExpectUniqueSample(status_histogram_name(),
                                      RotationStatus::SUCCESS, 1);
  histogram_tester.ExpectTotalCount(opposite_status_histogram_name(), 0);
}

// Tests a failed key rotation flow when no key previously existed and creating
// a new key pair fails.
TEST_P(KeyRotationManagerTest, Rotate_NoKey_CreateKeyPairFails) {
  base::HistogramTester histogram_tester;

  // The factory creates instances backed by fake EC keys.
  auto mock_persistence_delegate = scoped_factory_.CreateMockedECDelegate();

  EXPECT_CALL(*mock_persistence_delegate, LoadKeyPair()).WillOnce(Invoke([]() {
    return nullptr;
  }));
  EXPECT_CALL(*mock_persistence_delegate, CheckRotationPermissions())
      .WillOnce(Return(true));

  EXPECT_CALL(*mock_persistence_delegate, CreateKeyPair())
      .WillOnce(Invoke([]() { return nullptr; }));

  EXPECT_CALL(*mock_persistence_delegate,
              StoreKeyPair(BPKUR::CHROME_BROWSER_OS_KEY, _))
      .Times(0);

  GURL dm_server_url(kDmServerUrl);
  auto mock_network_delegate = std::make_unique<MockKeyNetworkDelegate>();
  EXPECT_CALL(*mock_network_delegate,
              SendPublicKeyToDmServer(dm_server_url, kDmToken, _, _))
      .Times(0);

  auto manager = KeyRotationManager::CreateForTesting(
      std::move(mock_network_delegate), std::move(mock_persistence_delegate));

  base::test::TestFuture<bool> future;
  manager->Rotate(dm_server_url, kDmToken, nonce(), future.GetCallback());
  EXPECT_FALSE(future.Get());

  // Should expect one failed attempt to rotate a key on first try.
  histogram_tester.ExpectUniqueSample(
      status_histogram_name(), RotationStatus::FAILURE_CANNOT_GENERATE_NEW_KEY,
      1);
  histogram_tester.ExpectTotalCount(opposite_status_histogram_name(), 0);
}

// Tests a failed key rotation flow when a key previously existed and creating a
// new key pair fails.
TEST_P(KeyRotationManagerTest, Rotate_Key_CreateKeyPairFails) {
  base::HistogramTester histogram_tester;

  // The factory creates instances backed by fake EC keys.
  auto mock_persistence_delegate = scoped_factory_.CreateMockedECDelegate();

  EXPECT_CALL(*mock_persistence_delegate, LoadKeyPair());
  EXPECT_CALL(*mock_persistence_delegate, CheckRotationPermissions())
      .WillOnce(Return(true));

  EXPECT_CALL(*mock_persistence_delegate, CreateKeyPair())
      .WillOnce(Invoke([]() { return nullptr; }));

  EXPECT_CALL(*mock_persistence_delegate,
              StoreKeyPair(BPKUR::CHROME_BROWSER_OS_KEY, _))
      .Times(0);

  GURL dm_server_url(kDmServerUrl);
  auto mock_network_delegate = std::make_unique<MockKeyNetworkDelegate>();
  EXPECT_CALL(*mock_network_delegate,
              SendPublicKeyToDmServer(dm_server_url, kDmToken, _, _))
      .Times(0);

  auto manager = KeyRotationManager::CreateForTesting(
      std::move(mock_network_delegate), std::move(mock_persistence_delegate));

  base::test::TestFuture<bool> future;
  manager->Rotate(dm_server_url, kDmToken, nonce(), future.GetCallback());
  EXPECT_FALSE(future.Get());

  // Should expect one failed attempt to rotate a key on first try.
  histogram_tester.ExpectUniqueSample(
      status_histogram_name(), RotationStatus::FAILURE_CANNOT_GENERATE_NEW_KEY,
      1);
  histogram_tester.ExpectTotalCount(opposite_status_histogram_name(), 0);
}

// Tests a failed key rotation flow when a hardware key provider is available
// and no key previously existed and the network request permanetly failed.
// Also, in this case the registry should be cleared.
TEST_P(KeyRotationManagerTest,
       Rotate_Hw_WithoutKey_NetworkFails_ClearRegistry) {
  base::HistogramTester histogram_tester;

  // The factory creates instances backed by fake hardware keys.
  auto mock_persistence_delegate =
      scoped_factory_.CreateMockedHardwareDelegate();

  InSequence s;

  // The mocked delegate is already set-up to return a working hardware key
  // and provider. Force it to not return a key.
  EXPECT_CALL(*mock_persistence_delegate, LoadKeyPair()).WillOnce(Invoke([]() {
    return nullptr;
  }));

  EXPECT_CALL(*mock_persistence_delegate, CheckRotationPermissions())
      .WillOnce(Return(true));

  EXPECT_CALL(*mock_persistence_delegate, CreateKeyPair());
  EXPECT_CALL(*mock_persistence_delegate,
              StoreKeyPair(BPKUR::CHROME_BROWSER_HW_KEY, _))
      .WillOnce(Return(true));

  GURL dm_server_url(kDmServerUrl);
  auto mock_network_delegate = std::make_unique<MockKeyNetworkDelegate>();
  EXPECT_CALL(*mock_network_delegate,
              SendPublicKeyToDmServer(dm_server_url, kDmToken, _, _))
      .WillOnce(Invoke([](const GURL& url, const std::string& dm_token,
                          const std::string& body,
                          base::OnceCallback<void(int)> callback) {
        std::move(callback).Run(kHardFailureCode);
      }));

  EXPECT_CALL(
      *mock_persistence_delegate,
      StoreKeyPair(BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED, std::vector<uint8_t>()))
      .WillOnce(Return(true));

  auto manager = KeyRotationManager::CreateForTesting(
      std::move(mock_network_delegate), std::move(mock_persistence_delegate));

  base::test::TestFuture<bool> future;
  manager->Rotate(dm_server_url, kDmToken, nonce(), future.GetCallback());
  EXPECT_FALSE(future.Get());

  // Should expect one failed attempt to rotate a key on first try.
  histogram_tester.ExpectUniqueSample(
      status_histogram_name(), RotationStatus::FAILURE_CANNOT_UPLOAD_KEY, 1);
  histogram_tester.ExpectTotalCount(opposite_status_histogram_name(), 0);
  histogram_tester.ExpectUniqueSample(http_code_histogram_name(),
                                      kHardFailureCode, 1);
}

// Tests a failed key rotation flow when a hardware key provider is available
// and no key previously existed and the network request transiently
// fails. Also, in this case the registry should be cleared.
TEST_P(KeyRotationManagerTest,
       Rotate_Hw_WithoutKey_ExhaustedNetworkFails_ClearRegistry) {
  base::HistogramTester histogram_tester;

  // The factory creates instances backed by fake hardware keys.
  auto mock_persistence_delegate =
      scoped_factory_.CreateMockedHardwareDelegate();

  InSequence s;

  // The mocked delegate is already set-up to return a working hardware key
  // and provider. Force it to not return a key.
  EXPECT_CALL(*mock_persistence_delegate, LoadKeyPair()).WillOnce(Invoke([]() {
    return nullptr;
  }));

  EXPECT_CALL(*mock_persistence_delegate, CheckRotationPermissions())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_persistence_delegate, CreateKeyPair());
  EXPECT_CALL(*mock_persistence_delegate,
              StoreKeyPair(BPKUR::CHROME_BROWSER_HW_KEY, _))
      .WillOnce(Return(true));

  GURL dm_server_url(kDmServerUrl);
  auto mock_network_delegate = std::make_unique<MockKeyNetworkDelegate>();
  EXPECT_CALL(*mock_network_delegate,
              SendPublicKeyToDmServer(dm_server_url, kDmToken, _, _))
      .WillRepeatedly(Invoke([](const GURL& url, const std::string& dm_token,
                                const std::string& body,
                                base::OnceCallback<void(int)> callback) {
        std::move(callback).Run(kTransientFailureCode);
      }));

  EXPECT_CALL(
      *mock_persistence_delegate,
      StoreKeyPair(BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED, std::vector<uint8_t>()))
      .WillOnce(Return(true));

  auto manager = KeyRotationManager::CreateForTesting(
      std::move(mock_network_delegate), std::move(mock_persistence_delegate));

  base::test::TestFuture<bool> future;
  manager->Rotate(dm_server_url, kDmToken, nonce(), future.GetCallback());
  EXPECT_FALSE(future.Get());

  // Should expect one failed attempt to rotate a key with max tries.
  histogram_tester.ExpectUniqueSample(
      status_histogram_name(),
      RotationStatus::FAILURE_CANNOT_UPLOAD_KEY_TRIES_EXHAUSTED, 1);
  histogram_tester.ExpectTotalCount(opposite_status_histogram_name(), 0);
  histogram_tester.ExpectUniqueSample(http_code_histogram_name(),
                                      kTransientFailureCode, 1);
}

// Tests a success key rotation flow when a hardware key provider is not
// available and a key previously existed.
TEST_P(KeyRotationManagerTest, Rotate_NoHw_WithKey) {
  base::HistogramTester histogram_tester;

  auto mock_persistence_delegate = scoped_factory_.CreateMockedECDelegate();

  EXPECT_CALL(*mock_persistence_delegate, LoadKeyPair());
  EXPECT_CALL(*mock_persistence_delegate, CheckRotationPermissions())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_persistence_delegate, CreateKeyPair());
  EXPECT_CALL(*mock_persistence_delegate,
              StoreKeyPair(BPKUR::CHROME_BROWSER_OS_KEY, _))
      .WillOnce(Return(true));

  GURL dm_server_url(kDmServerUrl);
  auto mock_network_delegate = std::make_unique<MockKeyNetworkDelegate>();
  EXPECT_CALL(*mock_network_delegate,
              SendPublicKeyToDmServer(dm_server_url, kDmToken, _, _))
      .WillOnce(Invoke([](const GURL& url, const std::string& dm_token,
                          const std::string& body,
                          base::OnceCallback<void(int)> callback) {
        std::move(callback).Run(kSuccessCode);
      }));
  EXPECT_CALL(*mock_persistence_delegate, CleanupTemporaryKeyData());

  auto manager = KeyRotationManager::CreateForTesting(
      std::move(mock_network_delegate), std::move(mock_persistence_delegate));

  base::test::TestFuture<bool> future;
  manager->Rotate(dm_server_url, kDmToken, nonce(), future.GetCallback());
  EXPECT_TRUE(future.Get());

  // Should expect one successful attempt to rotate a key.
  histogram_tester.ExpectUniqueSample(status_histogram_name(),
                                      RotationStatus::SUCCESS, 1);
  histogram_tester.ExpectTotalCount(opposite_status_histogram_name(), 0);
}

// Tests a failed key rotation flow when a hardware key provider is not
// available and a key previously existed, but storing the new key locally
// failed.
TEST_P(KeyRotationManagerTest, Rotate_NoHw_WithKey_StoreFailed) {
  base::HistogramTester histogram_tester;

  auto mock_persistence_delegate = scoped_factory_.CreateMockedECDelegate();
  auto original_key_wrapped = scoped_factory_.ec_wrapped_key();

  EXPECT_CALL(*mock_persistence_delegate, LoadKeyPair());
  EXPECT_CALL(*mock_persistence_delegate, CheckRotationPermissions())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_persistence_delegate, CreateKeyPair());
  EXPECT_CALL(
      *mock_persistence_delegate,
      StoreKeyPair(BPKUR::CHROME_BROWSER_OS_KEY, Not(original_key_wrapped)))
      .WillOnce(Return(false));

  auto mock_network_delegate =
      std::make_unique<testing::StrictMock<MockKeyNetworkDelegate>>();

  auto manager = KeyRotationManager::CreateForTesting(
      std::move(mock_network_delegate), std::move(mock_persistence_delegate));

  GURL dm_server_url(kDmServerUrl);

  base::test::TestFuture<bool> future;
  manager->Rotate(dm_server_url, kDmToken, nonce(), future.GetCallback());
  EXPECT_FALSE(future.Get());

  // Should expect one failed attempt to rotate a key.
  histogram_tester.ExpectUniqueSample(
      status_histogram_name(), RotationStatus::FAILURE_CANNOT_STORE_KEY, 1);
  histogram_tester.ExpectTotalCount(opposite_status_histogram_name(), 0);
}

// Tests a key rotation flow where the network request fails and the subsequent
// attempt to restore the old key also fails.
TEST_P(KeyRotationManagerTest, Rotate_NoHw_WithKey_NetworkFails_RestoreFails) {
  base::HistogramTester histogram_tester;

  auto mock_persistence_delegate = scoped_factory_.CreateMockedECDelegate();
  auto original_key_wrapped = scoped_factory_.ec_wrapped_key();
  InSequence s;

  EXPECT_CALL(*mock_persistence_delegate, LoadKeyPair());
  EXPECT_CALL(*mock_persistence_delegate, CheckRotationPermissions())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_persistence_delegate, CreateKeyPair());
  EXPECT_CALL(
      *mock_persistence_delegate,
      StoreKeyPair(BPKUR::CHROME_BROWSER_OS_KEY, Not(original_key_wrapped)))
      .WillOnce(Return(true));  // Store of new key fails.

  GURL dm_server_url(kDmServerUrl);
  auto mock_network_delegate = std::make_unique<MockKeyNetworkDelegate>();
  EXPECT_CALL(*mock_network_delegate,
              SendPublicKeyToDmServer(dm_server_url, kDmToken, _, _))
      .WillOnce(Invoke([](const GURL& url, const std::string& dm_token,
                          const std::string& body,
                          base::OnceCallback<void(int)> callback) {
        std::move(callback).Run(kHardFailureCode);
      }));

  EXPECT_CALL(*mock_persistence_delegate,
              StoreKeyPair(BPKUR::CHROME_BROWSER_OS_KEY,
                           Not(original_key_wrapped)))
      .WillOnce(Return(false));  // Restore of old key fails.
  auto manager = KeyRotationManager::CreateForTesting(
      std::move(mock_network_delegate), std::move(mock_persistence_delegate));

  base::test::TestFuture<bool> future;
  manager->Rotate(dm_server_url, kDmToken, nonce(), future.GetCallback());
  EXPECT_FALSE(future.Get());

  // Should expect one failed attempt to rotate a key on first try.
  histogram_tester.ExpectUniqueSample(
      status_histogram_name(),
      RotationStatus::FAILURE_CANNOT_UPLOAD_KEY_RESTORE_FAILED, 1);
  histogram_tester.ExpectTotalCount(opposite_status_histogram_name(), 0);
}

// Tests a failed key rotation flow when a hardware key provider is not
// available and a key previously existed, and the network request transiently
// fails. Also, in this case, the original key should be stored back.
TEST_P(KeyRotationManagerTest, Rotate_NoHw_WithKey_ExhaustedNetworkFailure) {
  base::HistogramTester histogram_tester;

  auto mock_persistence_delegate = scoped_factory_.CreateMockedECDelegate();
  auto original_key_wrapped = scoped_factory_.ec_wrapped_key();

  InSequence s;

  EXPECT_CALL(*mock_persistence_delegate, LoadKeyPair());
  EXPECT_CALL(*mock_persistence_delegate, CheckRotationPermissions())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_persistence_delegate, CreateKeyPair());
  EXPECT_CALL(
      *mock_persistence_delegate,
      StoreKeyPair(BPKUR::CHROME_BROWSER_OS_KEY, Not(original_key_wrapped)))
      .WillOnce(Return(true));

  GURL dm_server_url(kDmServerUrl);
  auto mock_network_delegate = std::make_unique<MockKeyNetworkDelegate>();
  EXPECT_CALL(*mock_network_delegate,
              SendPublicKeyToDmServer(dm_server_url, kDmToken, _, _))
      .WillRepeatedly(Invoke([](const GURL& url, const std::string& dm_token,
                                const std::string& body,
                                base::OnceCallback<void(int)> callback) {
        std::move(callback).Run(kTransientFailureCode);
      }));

  EXPECT_CALL(
      *mock_persistence_delegate,
      StoreKeyPair(BPKUR::CHROME_BROWSER_OS_KEY, Not(original_key_wrapped)))
      .WillOnce(Return(true));

  auto manager = KeyRotationManager::CreateForTesting(
      std::move(mock_network_delegate), std::move(mock_persistence_delegate));

  base::test::TestFuture<bool> future;
  manager->Rotate(dm_server_url, kDmToken, nonce(), future.GetCallback());
  EXPECT_FALSE(future.Get());

  // Should expect one failed attempt to rotate a key with max tries.
  histogram_tester.ExpectUniqueSample(
      status_histogram_name(),
      RotationStatus::FAILURE_CANNOT_UPLOAD_KEY_TRIES_EXHAUSTED, 1);
  histogram_tester.ExpectTotalCount(opposite_status_histogram_name(), 0);
}

// Tests a success key rotation flow when incorrect permissions were set
// on the signing key file.
TEST_P(KeyRotationManagerTest, Rotate_StoreFailed_InvalidFilePermissions) {
  base::HistogramTester histogram_tester;

  auto mock_persistence_delegate =
      std::make_unique<MockKeyPersistenceDelegate>();
  EXPECT_CALL(*mock_persistence_delegate, CheckRotationPermissions())
      .WillOnce(Return(false));
  EXPECT_CALL(*mock_persistence_delegate, LoadKeyPair());
  EXPECT_CALL(*mock_persistence_delegate, CreateKeyPair()).Times(0);
  EXPECT_CALL(*mock_persistence_delegate,
              StoreKeyPair(BPKUR::CHROME_BROWSER_OS_KEY, _))
      .Times(0);

  GURL dm_server_url(kDmServerUrl);
  auto mock_network_delegate = std::make_unique<MockKeyNetworkDelegate>();
  EXPECT_CALL(*mock_network_delegate,
              SendPublicKeyToDmServer(dm_server_url, kDmToken, _, _))
      .Times(0);

  auto manager = KeyRotationManager::CreateForTesting(
      std::move(mock_network_delegate), std::move(mock_persistence_delegate));

  base::test::TestFuture<bool> future;
  manager->Rotate(dm_server_url, kDmToken, nonce(), future.GetCallback());
  EXPECT_FALSE(future.Get());

  // Should expect one successful attempt to rotate a key.
  histogram_tester.ExpectUniqueSample(
      status_histogram_name(),
      RotationStatus::FAILURE_INCORRECT_FILE_PERMISSIONS, 1);
  histogram_tester.ExpectTotalCount(opposite_status_histogram_name(), 0);
}

INSTANTIATE_TEST_SUITE_P(, KeyRotationManagerTest, testing::Bool());

}  // namespace enterprise_connectors
