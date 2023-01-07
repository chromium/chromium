// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/mac_key_rotation_command.h"

#include <string>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/mock_secure_enclave_client.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/mock_key_network_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/mock_key_persistence_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/scoped_key_persistence_delegate_factory.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/key_rotation_manager.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/metrics_util.h"
#include "chrome/browser/enterprise/connectors/device_trust/prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::InSequence;
using testing::Invoke;
using testing::Return;
namespace enterprise_connectors {

using test::MockKeyNetworkDelegate;
using test::MockKeyPersistenceDelegate;
using test::MockSecureEnclaveClient;
using test::ScopedKeyPersistenceDelegateFactory;
using HttpResponseCode =
    enterprise_connectors::test::MockKeyNetworkDelegate::HttpResponseCode;

namespace {

constexpr char kNonce[] = "nonce";
constexpr char kFakeDMToken[] = "fake-browser-dm-token";
constexpr char kFakeDmServerUrl[] =
    "https://m.google.com/"
    "management_service?retry=false&agent=Chrome+1.2.3(456)&apptype=Chrome&"
    "critical=true&deviceid=fake-client-id&devicetype=2&platform=Test%7CUnit%"
    "7C1.2.3&request=browser_public_key_upload";
constexpr char kInvalidDmServerUrl[] =
    "https://example.com/"
    "management_service?retry=false&agent=Chrome+1.2.3(456)&apptype=Chrome&"
    "critical=true&deviceid=fake-client-id&devicetype=2&platform=Test%7CUnit%"
    "7C1.2.3&request=browser_public_key_upload";
constexpr HttpResponseCode kSuccessCode = 200;
constexpr HttpResponseCode kFailureCode = 400;
constexpr HttpResponseCode kKeyConflictCode = 409;

}  // namespace

class MacKeyRotationCommandTest : public testing::Test {
 protected:
  MacKeyRotationCommandTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}

  void SetUp() override {
    auto mock_secure_enclave_client =
        std::make_unique<MockSecureEnclaveClient>();
    mock_secure_enclave_client_ = mock_secure_enclave_client.get();
    SecureEnclaveClient::SetInstanceForTesting(
        std::move(mock_secure_enclave_client));

    params.dm_token = kFakeDMToken;
    params.dm_server_url = kFakeDmServerUrl;
    params.nonce = kNonce;

    auto mock_network_delegate = std::make_unique<MockKeyNetworkDelegate>();
    auto mock_persistence_delegate = scoped_factory_.CreateMockedECDelegate();

    mock_network_delegate_ = mock_network_delegate.get();
    mock_persistence_delegate_ = mock_persistence_delegate.get();
    EXPECT_CALL(*mock_persistence_delegate_, LoadKeyPair());

    RegisterDeviceTrustConnectorLocalPrefs(local_prefs_.registry());

    rotation_command_ = absl::WrapUnique(
        new MacKeyRotationCommand(test_shared_loader_factory_, &local_prefs_));

    KeyRotationManager::SetForTesting(KeyRotationManager::CreateForTesting(
        std::move(mock_network_delegate),
        std::move(mock_persistence_delegate)));
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  std::unique_ptr<MacKeyRotationCommand> rotation_command_;
  MockSecureEnclaveClient* mock_secure_enclave_client_ = nullptr;
  MockKeyNetworkDelegate* mock_network_delegate_ = nullptr;
  MockKeyPersistenceDelegate* mock_persistence_delegate_ = nullptr;
  test::ScopedKeyPersistenceDelegateFactory scoped_factory_;
  KeyRotationCommand::Params params;
  TestingPrefServiceSimple local_prefs_;
};

// Tests a failed key rotation due to the secure enclave not being supported.
TEST_F(MacKeyRotationCommandTest, RotateFailure_SecureEnclaveUnsupported) {
  EXPECT_CALL(*mock_secure_enclave_client_, VerifySecureEnclaveSupported())
      .WillOnce(Return(false));

  base::test::TestFuture<KeyRotationCommand::Status> future;
  rotation_command_->Trigger(params, future.GetCallback());
  EXPECT_EQ(KeyRotationCommand::Status::FAILED_OS_RESTRICTION, future.Get());
  EXPECT_TRUE(local_prefs_.GetBoolean(kDeviceTrustDisableKeyCreationPref));
}

// Tests a failed key rotation due to an invalid command to rotate.
TEST_F(MacKeyRotationCommandTest, RotateFailure_InvalidCommand) {
  InSequence s;
  EXPECT_CALL(*mock_secure_enclave_client_, VerifySecureEnclaveSupported())
      .WillOnce(Return(true));

  params.dm_server_url = kInvalidDmServerUrl;
  base::test::TestFuture<KeyRotationCommand::Status> future;
  rotation_command_->Trigger(params, future.GetCallback());
  EXPECT_EQ(KeyRotationCommand::Status::FAILED, future.Get());
  EXPECT_FALSE(local_prefs_.GetBoolean(kDeviceTrustDisableKeyCreationPref));
}

// Tests a failed key rotation due to failure creating a new signing key pair.
TEST_F(MacKeyRotationCommandTest, RotateFailure_CreateKeyFailure) {
  InSequence s;
  EXPECT_CALL(*mock_secure_enclave_client_, VerifySecureEnclaveSupported())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_persistence_delegate_, CheckRotationPermissions())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_persistence_delegate_, CreateKeyPair())
      .WillOnce(Invoke([]() { return nullptr; }));

  base::test::TestFuture<KeyRotationCommand::Status> future;
  rotation_command_->Trigger(params, future.GetCallback());
  EXPECT_EQ(KeyRotationCommand::Status::FAILED, future.Get());
  EXPECT_FALSE(local_prefs_.GetBoolean(kDeviceTrustDisableKeyCreationPref));
}

// Tests a failed key rotation due to a store key failure.
TEST_F(MacKeyRotationCommandTest, RotateFailure_StoreKeyFailure) {
  InSequence s;
  EXPECT_CALL(*mock_secure_enclave_client_, VerifySecureEnclaveSupported())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_persistence_delegate_, CheckRotationPermissions())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_persistence_delegate_, CreateKeyPair());
  EXPECT_CALL(*mock_persistence_delegate_, StoreKeyPair(_, _))
      .WillOnce(Return(false));

  base::test::TestFuture<KeyRotationCommand::Status> future;
  rotation_command_->Trigger(params, future.GetCallback());
  EXPECT_EQ(KeyRotationCommand::Status::FAILED, future.Get());
  EXPECT_FALSE(local_prefs_.GetBoolean(kDeviceTrustDisableKeyCreationPref));
}

// Tests a failed key rotation when uploading a the key to the dm server
// fails due to a key conflict failure.
TEST_F(MacKeyRotationCommandTest, RotateFailure_KeyConflict) {
  InSequence s;
  EXPECT_CALL(*mock_secure_enclave_client_, VerifySecureEnclaveSupported())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_persistence_delegate_, CheckRotationPermissions())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_persistence_delegate_, CreateKeyPair());
  EXPECT_CALL(*mock_persistence_delegate_, StoreKeyPair(_, _))
      .WillOnce(Return(true));
  EXPECT_CALL(
      *mock_network_delegate_,
      SendPublicKeyToDmServer(GURL(kFakeDmServerUrl), kFakeDMToken, _, _))
      .WillOnce(Invoke([](const GURL& url, const std::string& dm_token,
                          const std::string& body,
                          base::OnceCallback<void(int)> callback) {
        std::move(callback).Run(kKeyConflictCode);
      }));
  EXPECT_CALL(*mock_persistence_delegate_, StoreKeyPair(_, _))
      .WillOnce(Return(true));

  base::test::TestFuture<KeyRotationCommand::Status> future;
  rotation_command_->Trigger(params, future.GetCallback());
  EXPECT_EQ(KeyRotationCommand::Status::FAILED_KEY_CONFLICT, future.Get());
  EXPECT_TRUE(local_prefs_.GetBoolean(kDeviceTrustDisableKeyCreationPref));
}

// Tests a failed key rotation due to a failure sending the key to the dm
// server.
TEST_F(MacKeyRotationCommandTest, RotateFailure_UploadKeyFailure) {
  InSequence s;
  EXPECT_CALL(*mock_secure_enclave_client_, VerifySecureEnclaveSupported())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_persistence_delegate_, CheckRotationPermissions())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_persistence_delegate_, CreateKeyPair());
  EXPECT_CALL(*mock_persistence_delegate_, StoreKeyPair(_, _))
      .WillOnce(Return(true));
  EXPECT_CALL(
      *mock_network_delegate_,
      SendPublicKeyToDmServer(GURL(kFakeDmServerUrl), kFakeDMToken, _, _))
      .WillOnce(Invoke([](const GURL& url, const std::string& dm_token,
                          const std::string& body,
                          base::OnceCallback<void(int)> callback) {
        std::move(callback).Run(kFailureCode);
      }));
  EXPECT_CALL(*mock_persistence_delegate_, StoreKeyPair(_, _))
      .WillOnce(Return(true));

  base::test::TestFuture<KeyRotationCommand::Status> future;
  rotation_command_->Trigger(params, future.GetCallback());
  EXPECT_EQ(KeyRotationCommand::Status::FAILED, future.Get());
  EXPECT_FALSE(local_prefs_.GetBoolean(kDeviceTrustDisableKeyCreationPref));
}

// Tests when the key rotation is successful.
TEST_F(MacKeyRotationCommandTest, Rotate_Success) {
  InSequence s;
  EXPECT_CALL(*mock_secure_enclave_client_, VerifySecureEnclaveSupported())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_persistence_delegate_, CheckRotationPermissions())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_persistence_delegate_, CreateKeyPair());
  EXPECT_CALL(*mock_persistence_delegate_, StoreKeyPair(_, _))
      .WillOnce(Return(true));
  EXPECT_CALL(
      *mock_network_delegate_,
      SendPublicKeyToDmServer(GURL(kFakeDmServerUrl), kFakeDMToken, _, _))
      .WillOnce(Invoke([](const GURL& url, const std::string& dm_token,
                          const std::string& body,
                          base::OnceCallback<void(int)> callback) {
        std::move(callback).Run(kSuccessCode);
      }));

  base::test::TestFuture<KeyRotationCommand::Status> future;
  rotation_command_->Trigger(params, future.GetCallback());
  EXPECT_EQ(KeyRotationCommand::Status::SUCCEEDED, future.Get());
  EXPECT_FALSE(local_prefs_.GetBoolean(kDeviceTrustDisableKeyCreationPref));
}

}  // namespace enterprise_connectors
