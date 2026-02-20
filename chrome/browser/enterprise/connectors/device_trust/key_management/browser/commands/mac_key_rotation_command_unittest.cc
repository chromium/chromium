// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/mac_key_rotation_command.h"

#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/enterprise/connectors/device_trust/common/device_trust_constants.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_features.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/common/key_types.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/mock_secure_enclave_client.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/mock_key_network_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/mock_key_persistence_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/scoped_key_persistence_delegate_factory.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/key_rotation_manager.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/metrics_util.h"
#include "components/enterprise/client_certificates/core/cloud_management_delegate.h"
#include "components/enterprise/client_certificates/core/mock_cloud_management_delegate.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::ElementsAre;
using testing::InSequence;
using testing::Pair;
using testing::Return;

namespace enterprise_connectors {

using test::MockKeyNetworkDelegate;
using test::MockKeyPersistenceDelegate;
using test::MockSecureEnclaveClient;
using test::ScopedKeyPersistenceDelegateFactory;
using HttpResponseCode =
    enterprise_connectors::test::MockKeyNetworkDelegate::HttpResponseCode;

namespace {

// Add a couple of seconds to the exact timeout time.
const base::TimeDelta kTimeoutTime =
    timeouts::kHandshakeTimeout + base::Seconds(2);

constexpr char kNonce[] = "nonce";
constexpr char kFakeDMToken[] = "fake-browser-dm-token";

constexpr HttpResponseCode kSuccessCode = 200;
constexpr HttpResponseCode kFailureCode = 400;
constexpr HttpResponseCode kKeyConflictCode = 409;

constexpr char kRotateStatusHistogram[] =
    "Enterprise.DeviceTrust.RotateSigningKey.NoNonce.Status";
constexpr char kUploadCodeHistogram[] =
    "Enterprise.DeviceTrust.RotateSigningKey.NoNonce.UploadCode";

constexpr char kHistogramPrefix[] = "Enterprise.DeviceTrust.RotateSigningKey";

}  // namespace

class MacKeyRotationCommandTest : public testing::Test {
 protected:
  MacKeyRotationCommandTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void SetUp() override {
    auto mock_secure_enclave_client =
        std::make_unique<MockSecureEnclaveClient>();
    mock_secure_enclave_client_ = mock_secure_enclave_client.get();
    SecureEnclaveClient::SetInstanceForTesting(
        std::move(mock_secure_enclave_client));

    params_.nonce = kNonce;

    auto mock_persistence_delegate = scoped_factory_.CreateMockedECDelegate();
    mock_persistence_delegate_ = mock_persistence_delegate.get();

    auto mock_cloud_delegate =
        std::make_unique<enterprise_attestation::MockCloudManagementDelegate>();
    mock_cloud_delegate_ = mock_cloud_delegate.get();
    KeyRotationManager::SetForTesting(KeyRotationManager::CreateForTesting(
        std::move(mock_persistence_delegate)));

    rotation_command_ = base::WrapUnique(
        new MacKeyRotationCommand(std::move(mock_cloud_delegate)));
  }

  void FastForwardBeyondTimeout() {
    task_environment_.FastForwardBy(kTimeoutTime + base::Seconds(2));
    task_environment_.RunUntilIdle();
  }

  void SetUpDmToken(std::string dm_token = kFakeDMToken) {
    EXPECT_CALL(*mock_cloud_delegate_, GetDMToken())
        .WillRepeatedly(Return(dm_token));
  }

  void PostUploadSetup(HttpResponseCode response_code) {
    auto new_persistence_delegate = scoped_factory_.CreateMockedECDelegate();
    mock_persistence_delegate_ = new_persistence_delegate.get();
    KeyRotationManager::SetForTesting(KeyRotationManager::CreateForTesting(
        std::move(new_persistence_delegate)));

    if (response_code == kSuccessCode) {
      EXPECT_CALL(*mock_persistence_delegate_, CleanupTemporaryKeyData());
    } else {
      EXPECT_CALL(*mock_persistence_delegate_, StoreKeyPair(_, _));
    }
  }

  void UploadPublicKey(HttpResponseCode response_code) {
    policy::DMServerJobResult result;
    result.response_code = response_code;

    EXPECT_CALL(*mock_cloud_delegate_, UploadBrowserPublicKey(_, _))
        .WillOnce(
            [response_code, result, this](
                const enterprise_management::DeviceManagementRequest& request,
                base::OnceCallback<void(policy::DMServerJobResult)> callback) {
              this->PostUploadSetup(response_code);
              std::move(callback).Run(result);
            });
  }

  void VerifyHistograms(RotationStatus status) {
    histogram_tester_->ExpectUniqueSample(kRotateStatusHistogram, status, 1);
    EXPECT_THAT(histogram_tester_->GetTotalCountsForPrefix(kHistogramPrefix),
                ElementsAre(Pair(kRotateStatusHistogram, 1)));
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  std::unique_ptr<MacKeyRotationCommand> rotation_command_;
  raw_ptr<MockSecureEnclaveClient, DanglingUntriaged>
      mock_secure_enclave_client_ = nullptr;
  raw_ptr<MockKeyNetworkDelegate, DanglingUntriaged> mock_network_delegate_ =
      nullptr;
  raw_ptr<enterprise_attestation::MockCloudManagementDelegate,
          DanglingUntriaged>
      mock_cloud_delegate_ = nullptr;
  raw_ptr<MockKeyPersistenceDelegate, DanglingUntriaged>
      mock_persistence_delegate_ = nullptr;

  std::unique_ptr<base::HistogramTester> histogram_tester_;

  test::ScopedKeyPersistenceDelegateFactory scoped_factory_;
  KeyRotationCommand::Params params_;
};

// Tests a failed key rotation due to the secure enclave not being supported.
TEST_F(MacKeyRotationCommandTest, RotateFailure_SecureEnclaveUnsupported) {
  EXPECT_CALL(*mock_secure_enclave_client_, VerifySecureEnclaveSupported())
      .WillOnce(Return(false));

  base::test::TestFuture<KeyRotationCommand::Status> future;
  rotation_command_->Trigger(params_, future.GetCallback());
  EXPECT_EQ(KeyRotationCommand::Status::FAILED_OS_RESTRICTION, future.Get());
}

// Tests a failed key rotation due to failure creating a new signing key pair.
TEST_F(MacKeyRotationCommandTest, RotateFailure_CreateKeyFailure) {
  EXPECT_CALL(*mock_secure_enclave_client_, VerifySecureEnclaveSupported())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_persistence_delegate_, CreateKeyPair()).WillOnce([]() {
    return nullptr;
  });
  SetUpDmToken();

  base::test::TestFuture<KeyRotationCommand::Status> future;
  rotation_command_->Trigger(params_, future.GetCallback());
  EXPECT_EQ(KeyRotationCommand::Status::FAILED, future.Get());

  VerifyHistograms(RotationStatus::FAILURE_CANNOT_GENERATE_NEW_KEY);
}

// Tests a failed key rotation due to a store key failure.
TEST_F(MacKeyRotationCommandTest, RotateFailure_StoreKeyFailure) {
  EXPECT_CALL(*mock_secure_enclave_client_, VerifySecureEnclaveSupported())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_persistence_delegate_, CreateKeyPair());
  EXPECT_CALL(*mock_persistence_delegate_, StoreKeyPair(_, _))
      .WillOnce(Return(false));
  SetUpDmToken();

  base::test::TestFuture<KeyRotationCommand::Status> future;
  rotation_command_->Trigger(params_, future.GetCallback());
  EXPECT_EQ(KeyRotationCommand::Status::FAILED, future.Get());

  VerifyHistograms(RotationStatus::FAILURE_CANNOT_STORE_KEY);
}

// Tests a failed key rotation when uploading the key to the dm server
// fails due to a key conflict failure.
TEST_F(MacKeyRotationCommandTest, RotateFailure_KeyConflict) {
  InSequence s;
  EXPECT_CALL(*mock_secure_enclave_client_, VerifySecureEnclaveSupported())
      .WillOnce(Return(true));
  SetUpDmToken();
  EXPECT_CALL(*mock_persistence_delegate_, CreateKeyPair());
  EXPECT_CALL(*mock_persistence_delegate_, StoreKeyPair(_, _))
      .WillOnce(Return(true));
  UploadPublicKey(kKeyConflictCode);

  base::test::TestFuture<KeyRotationCommand::Status> future;
  rotation_command_->Trigger(params_, future.GetCallback());
  EXPECT_EQ(KeyRotationCommand::Status::FAILED_KEY_CONFLICT, future.Get());
}

// Tests a failed key rotation due to a failure sending the key to the dm
// server.
TEST_F(MacKeyRotationCommandTest, RotateFailure_UploadKeyFailure) {
  InSequence s;
  EXPECT_CALL(*mock_secure_enclave_client_, VerifySecureEnclaveSupported())
      .WillOnce(Return(true));
  SetUpDmToken();
  EXPECT_CALL(*mock_persistence_delegate_, CreateKeyPair());
  EXPECT_CALL(*mock_persistence_delegate_, StoreKeyPair(_, _))
      .WillOnce(Return(true));
  UploadPublicKey(kFailureCode);

  base::test::TestFuture<KeyRotationCommand::Status> future;
  rotation_command_->Trigger(params_, future.GetCallback());
  EXPECT_EQ(KeyRotationCommand::Status::FAILED, future.Get());
}

TEST_F(MacKeyRotationCommandTest, RotateFailure_EmptyDmToken) {
  SetUpDmToken("");
  EXPECT_CALL(*mock_secure_enclave_client_, VerifySecureEnclaveSupported())
      .WillOnce(Return(true));

  base::test::TestFuture<KeyRotationCommand::Status> future;
  rotation_command_->Trigger(params_, future.GetCallback());
  EXPECT_EQ(KeyRotationCommand::Status::FAILED, future.Get());

  VerifyHistograms(RotationStatus::FAILURE_INVALID_DMTOKEN);
}

TEST_F(MacKeyRotationCommandTest, RotateFailure_LongDmToken) {
  // Create a DM token that has 5000 characters.
  std::string long_dm_token(5000, 'a');
  SetUpDmToken(long_dm_token);
  EXPECT_CALL(*mock_secure_enclave_client_, VerifySecureEnclaveSupported())
      .WillOnce(Return(true));

  base::test::TestFuture<KeyRotationCommand::Status> future;
  rotation_command_->Trigger(params_, future.GetCallback());
  EXPECT_EQ(KeyRotationCommand::Status::FAILED, future.Get());

  VerifyHistograms(RotationStatus::FAILURE_INVALID_DMTOKEN);
}

// Tests when the key rotation is successful.
TEST_F(MacKeyRotationCommandTest, Rotate_Success) {
  EXPECT_CALL(*mock_secure_enclave_client_, VerifySecureEnclaveSupported())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_persistence_delegate_, CreateKeyPair());
  EXPECT_CALL(*mock_persistence_delegate_, StoreKeyPair(_, _))
      .WillOnce(Return(true));
  SetUpDmToken();
  UploadPublicKey(kSuccessCode);

  base::test::TestFuture<KeyRotationCommand::Status> future;
  rotation_command_->Trigger(params_, future.GetCallback());
  EXPECT_EQ(KeyRotationCommand::Status::SUCCEEDED, future.Get());

  // Advancing beyond timeout time doesn't cause any crashes.
  FastForwardBeyondTimeout();

  histogram_tester_->ExpectUniqueSample(kRotateStatusHistogram,
                                        RotationStatus::SUCCESS, 1);
  histogram_tester_->ExpectUniqueSample(kUploadCodeHistogram, kSuccessCode, 1);
  // Make sure no other histograms were logged.
  EXPECT_THAT(histogram_tester_->GetTotalCountsForPrefix(kHistogramPrefix),
              ElementsAre(Pair(kRotateStatusHistogram, 1),
                          Pair(kUploadCodeHistogram, 1)));
}

// Tests what happens when the key rotation succeeds beyond the timeout limit
// before the command object is destroyed.
TEST_F(MacKeyRotationCommandTest, Rotate_Timeout_ReturnBeforeDestruction) {
  EXPECT_CALL(*mock_secure_enclave_client_, VerifySecureEnclaveSupported())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_persistence_delegate_, CreateKeyPair());
  EXPECT_CALL(*mock_persistence_delegate_, StoreKeyPair(_, _))
      .WillOnce(Return(true));
  SetUpDmToken();

  base::OnceCallback<void(policy::DMServerJobResult)> captured_callback;
  EXPECT_CALL(*mock_cloud_delegate_, UploadBrowserPublicKey(_, _))
      .WillOnce(
          [&captured_callback](
              const enterprise_management::DeviceManagementRequest& request,
              base::OnceCallback<void(policy::DMServerJobResult)> callback) {
            captured_callback = std::move(callback);
          });

  base::test::TestFuture<KeyRotationCommand::Status> future;
  rotation_command_->Trigger(params_, future.GetCallback());

  FastForwardBeyondTimeout();

  EXPECT_EQ(KeyRotationCommand::Status::TIMED_OUT, future.Get());

  policy::DMServerJobResult result;
  result.response_code = kSuccessCode;
  std::move(captured_callback).Run(result);

  // Make sure the callback runs before exiting the test.
  task_environment_.RunUntilIdle();
}

// Tests what happens when the key rotation succeeds beyond the timeout limit
// after the command object is destroyed.
TEST_F(MacKeyRotationCommandTest, Rotate_Timeout_ReturnAfterDestruction) {
  EXPECT_CALL(*mock_secure_enclave_client_, VerifySecureEnclaveSupported())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_persistence_delegate_, CreateKeyPair());
  EXPECT_CALL(*mock_persistence_delegate_, StoreKeyPair(_, _))
      .WillOnce(Return(true));
  SetUpDmToken();

  base::OnceCallback<void(policy::DMServerJobResult)> captured_callback;
  EXPECT_CALL(*mock_cloud_delegate_, UploadBrowserPublicKey(_, _))
      .WillOnce(
          [&captured_callback](
              const enterprise_management::DeviceManagementRequest& request,
              base::OnceCallback<void(policy::DMServerJobResult)> callback) {
            captured_callback = std::move(callback);
          });

  base::test::TestFuture<KeyRotationCommand::Status> future;
  rotation_command_->Trigger(params_, future.GetCallback());

  FastForwardBeyondTimeout();

  EXPECT_EQ(KeyRotationCommand::Status::TIMED_OUT, future.Get());

  rotation_command_.reset();

  policy::DMServerJobResult result;
  result.response_code = kSuccessCode;
  std::move(captured_callback).Run(result);

  // Make sure the callback runs before exiting the test.
  task_environment_.RunUntilIdle();
}

}  // namespace enterprise_connectors
