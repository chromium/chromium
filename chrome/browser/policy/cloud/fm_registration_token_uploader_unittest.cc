// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/fm_registration_token_uploader.h"

#include "base/test/protobuf_matchers.h"
#include "base/test/task_environment.h"
#include "components/invalidation/invalidation_listener.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::test::EqualsProto;
using ::testing::_;
using ::testing::Return;
using ::testing::WithArg;

namespace policy {

namespace {

const char kFakeRegistrationToken[] = "fake_registration_token";
const char kFakeDMToken[] = "fake_dm_token";
const int kExpectedProtocolVersion = 1;
const base::Time kFakeTokenEndOfLife = base::Time::Now();

class MockInvalidationListener : public invalidation::InvalidationListener {
 public:
  MOCK_METHOD(void, AddObserver, (Observer * handler), (override));
  MOCK_METHOD(bool, HasObserver, (const Observer* handler), (const, override));
  MOCK_METHOD(void, RemoveObserver, (const Observer* handler), (override));

  MOCK_METHOD(void,
              Start,
              (invalidation::RegistrationTokenHandler * handler),
              (override));
  MOCK_METHOD(void,
              SetRegistrationUploadStatus,
              (RegistrationTokenUploadStatus status),
              (override));
  MOCK_METHOD(void, Shutdown, (), (override));
};
}  // namespace

class FmRegistrationTokenUploaderTest : public testing::Test {
 public:
  FmRegistrationTokenUploaderTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        core_(dm_protocol::kChromeDevicePolicyType,
              std::string(),
              &mock_store_,
              task_environment_.GetMainThreadTaskRunner(),
              network::TestNetworkConnectionTracker::CreateGetter()) {}

 protected:
  void SetRegistrationTokenUploadState(MockCloudPolicyClient& client,
                                       CloudPolicyClient::Result result) {
    ON_CALL(client, UploadFmRegistrationToken(_, _))
        .WillByDefault(
            WithArg<1>([result](CloudPolicyClient::ResultCallback callback) {
              std::move(callback).Run(result);
            }));
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  testing::NiceMock<MockInvalidationListener> mock_invalidation_listener_;
  testing::NiceMock<MockCloudPolicyStore> mock_store_;
  CloudPolicyCore core_;
};

TEST_F(FmRegistrationTokenUploaderTest,
       OnRegistrationTokenReceivedSuccessfullyUploads) {
  auto client = std::make_unique<MockCloudPolicyClient>();
  MockCloudPolicyClient* client_ptr = client.get();
  // Make registration token upload request successful.
  SetRegistrationTokenUploadState(
      *client_ptr,
      CloudPolicyClient::Result(DeviceManagementStatus::DM_STATUS_SUCCESS));
  // Register and connect cloud policy client.
  client_ptr->SetDMToken(kFakeDMToken);
  core_.Connect(std::move(client));
  FmRegistrationTokenUploader uploader(PolicyInvalidationScope::kDevice,
                                       &mock_invalidation_listener_, &core_);
  // Expect that `UploadFmRegistrationToken()` will be called with correct
  // arguments.
  enterprise_management::FmRegistrationTokenUploadRequest request;
  request.set_token(kFakeRegistrationToken);
  request.set_protocol_version(kExpectedProtocolVersion);
  request.set_token_type(
      enterprise_management::FmRegistrationTokenUploadRequest::DEVICE);
  request.set_expiration_timestamp_ms(
      kFakeTokenEndOfLife.InMillisecondsSinceUnixEpoch());
  EXPECT_CALL(*client_ptr, UploadFmRegistrationToken(EqualsProto(request), _));
  // Expect successful token upload.
  EXPECT_CALL(
      mock_invalidation_listener_,
      SetRegistrationUploadStatus(
          MockInvalidationListener::RegistrationTokenUploadStatus::kSucceeded));

  uploader.OnRegistrationTokenReceived(kFakeRegistrationToken,
                                       kFakeTokenEndOfLife);
}

TEST_F(FmRegistrationTokenUploaderTest,
       OnRegistrationTokenReceivedSuccessfullyUploadsOnConnectedCore) {
  auto client = std::make_unique<MockCloudPolicyClient>();
  MockCloudPolicyClient* client_ptr = client.get();
  // Make registration token upload request successful.
  SetRegistrationTokenUploadState(
      *client_ptr,
      CloudPolicyClient::Result(DeviceManagementStatus::DM_STATUS_SUCCESS));
  // Register cloud policy client.
  client_ptr->SetDMToken(kFakeDMToken);
  FmRegistrationTokenUploader uploader(PolicyInvalidationScope::kDevice,
                                       &mock_invalidation_listener_, &core_);
  // Simulate first token upload that should not finish, due to
  // disconnected cloud policy core. This should subscribe to cloud policy core
  // events.
  EXPECT_CALL(*client_ptr, UploadFmRegistrationToken(_, _)).Times(0);
  uploader.OnRegistrationTokenReceived(kFakeRegistrationToken,
                                       kFakeTokenEndOfLife);
  // Expect that `UploadFmRegistrationToken()` will be called with correct
  // arguments when cloud policy core is connected.
  enterprise_management::FmRegistrationTokenUploadRequest request;
  request.set_token(kFakeRegistrationToken);
  request.set_protocol_version(kExpectedProtocolVersion);
  request.set_token_type(
      enterprise_management::FmRegistrationTokenUploadRequest::DEVICE);
  request.set_expiration_timestamp_ms(
      kFakeTokenEndOfLife.InMillisecondsSinceUnixEpoch());
  EXPECT_CALL(*client_ptr, UploadFmRegistrationToken(EqualsProto(request), _));
  EXPECT_CALL(
      mock_invalidation_listener_,
      SetRegistrationUploadStatus(
          MockInvalidationListener::RegistrationTokenUploadStatus::kSucceeded));

  core_.Connect(std::move(client));
  task_environment_.FastForwardBy(base::Minutes(1));
  // Notify about connected core twice to ensure that internally observers
  // are cleaned up properly, and `SetRegistrationUploadStatus` is indeed only
  // called once.
  core_.Disconnect();
  client = std::make_unique<policy::MockCloudPolicyClient>();
  client->SetDMToken(kFakeDMToken);
  core_.Connect(std::move(client));
  task_environment_.FastForwardBy(base::Minutes(1));
}

TEST_F(FmRegistrationTokenUploaderTest,
       OnRegistrationTokenReceivedSuccessfullyUploadsOnRegisteredClient) {
  auto client = std::make_unique<policy::MockCloudPolicyClient>();
  MockCloudPolicyClient* client_ptr = client.get();
  // Make registration token upload request successful.
  SetRegistrationTokenUploadState(
      *client,
      CloudPolicyClient::Result(DeviceManagementStatus::DM_STATUS_SUCCESS));
  core_.Connect(std::move(client));
  FmRegistrationTokenUploader uploader(PolicyInvalidationScope::kDevice,
                                       &mock_invalidation_listener_, &core_);
  // Simulate first token upload that should not finish, due to
  // unregistered cloud policy client. This should subscribe to cloud policy
  // client events.
  EXPECT_CALL(*client_ptr, UploadFmRegistrationToken(_, _)).Times(0);
  uploader.OnRegistrationTokenReceived(kFakeRegistrationToken,
                                       kFakeTokenEndOfLife);
  // Expect that `UploadFmRegistrationToken()` will be called with correct
  // arguments when cloud policy client is registered.
  enterprise_management::FmRegistrationTokenUploadRequest request;
  request.set_token(kFakeRegistrationToken);
  request.set_protocol_version(kExpectedProtocolVersion);
  request.set_token_type(
      enterprise_management::FmRegistrationTokenUploadRequest::DEVICE);
  request.set_expiration_timestamp_ms(
      kFakeTokenEndOfLife.InMillisecondsSinceUnixEpoch());
  EXPECT_CALL(*client_ptr, UploadFmRegistrationToken(EqualsProto(request), _));
  EXPECT_CALL(
      mock_invalidation_listener_,
      SetRegistrationUploadStatus(
          MockInvalidationListener::RegistrationTokenUploadStatus::kSucceeded));

  client_ptr->SetDMToken(kFakeDMToken);
  client_ptr->NotifyRegistrationStateChanged();
  task_environment_.FastForwardBy(base::Minutes(1));
  // Notify about registration twice to ensure that internally observers
  // clean up properly, and `SetRegistrationUploadStatus` is indeed only
  // called once.
  client_ptr->NotifyRegistrationStateChanged();
  task_environment_.FastForwardBy(base::Minutes(1));
}

TEST_F(FmRegistrationTokenUploaderTest,
       FailedUploadToDmServerRestartsWithCorrectBackoff) {
  auto client = std::make_unique<policy::MockCloudPolicyClient>();
  MockCloudPolicyClient* client_ptr = client.get();
  // Fail registration token upload request.
  SetRegistrationTokenUploadState(
      *client, CloudPolicyClient::Result(
                   DeviceManagementStatus::DM_STATUS_REQUEST_FAILED));
  // Register and connect cloud policy client.
  client_ptr->SetDMToken(kFakeDMToken);
  core_.Connect(std::move(client));
  FmRegistrationTokenUploader uploader(PolicyInvalidationScope::kDevice,
                                       &mock_invalidation_listener_, &core_);
  // Simulate first token upload that should not finish, due to failed request.
  EXPECT_CALL(
      mock_invalidation_listener_,
      SetRegistrationUploadStatus(
          MockInvalidationListener::RegistrationTokenUploadStatus::kFailed));
  uploader.OnRegistrationTokenReceived(kFakeRegistrationToken,
                                       kFakeTokenEndOfLife);
  // Make next registration token upload requests successful.
  SetRegistrationTokenUploadState(
      *client_ptr,
      CloudPolicyClient::Result(DeviceManagementStatus::DM_STATUS_SUCCESS));
  // Expect that `UploadFmRegistrationToken()` will be called with correct
  // params on retry request.
  enterprise_management::FmRegistrationTokenUploadRequest request;
  request.set_token(kFakeRegistrationToken);
  request.set_protocol_version(kExpectedProtocolVersion);
  request.set_token_type(
      enterprise_management::FmRegistrationTokenUploadRequest::DEVICE);
  request.set_expiration_timestamp_ms(
      kFakeTokenEndOfLife.InMillisecondsSinceUnixEpoch());
  EXPECT_CALL(*client_ptr, UploadFmRegistrationToken(EqualsProto(request), _));
  EXPECT_CALL(
      mock_invalidation_listener_,
      SetRegistrationUploadStatus(
          MockInvalidationListener::RegistrationTokenUploadStatus::kSucceeded));

  task_environment_.FastForwardBy(base::Minutes(1));
}

}  // namespace policy
