// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_cloud_state_keys_uploader.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/policy/dm_token_storage.h"
#include "chrome/browser/chromeos/policy/server_backed_state_keys_broker.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "components/policy/core/common/cloud/mock_device_management_service.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace policy {
namespace {

const char kClientId[] = "fake-client-id";

std::string CreateDevicePolicyData(const std::string& policy_value) {
  em::PolicyData policy_data;
  policy_data.set_policy_type(dm_protocol::kChromeDevicePolicyType);
  policy_data.set_policy_value(policy_value);
  return policy_data.SerializeAsString();
}

em::DeviceManagementResponse GetPolicyResponse() {
  em::DeviceManagementResponse policy_response;
  policy_response.mutable_policy_response()->add_responses()->set_policy_data(
      CreateDevicePolicyData("fake-policy-data"));
  return policy_response;
}

class FakeDMTokenStorage : public DMTokenStorageBase {
 public:
  explicit FakeDMTokenStorage(const std::string& dm_token,
                              base::TimeDelta delay_response)
      : dm_token_(dm_token), delay_response_(delay_response) {}
  void StoreDMToken(const std::string& dm_token,
                    StoreCallback callback) override {
    dm_token_ = dm_token;
    std::move(callback).Run(true);
  }
  void RetrieveDMToken(RetrieveCallback callback) override {
    if (!delay_response_.is_zero()) {
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE, base::BindOnce(std::move(callback), dm_token_),
          delay_response_);
    } else {
      std::move(callback).Run(dm_token_);
    }
  }

 private:
  std::string dm_token_;
  base::TimeDelta delay_response_;
};

}  // namespace

class DeviceCloudStateKeysUploaderTest : public testing::Test {
 public:
  DeviceCloudStateKeysUploaderTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        client_id_(kClientId),
        broker_(&fake_session_manager_client_) {}

  void SetUp() override {
    service_.ScheduleInitialization(0);
    base::RunLoop().RunUntilIdle();
    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &url_loader_factory_);
  }

  void TestStatus(bool expect_success, bool success) {
    EXPECT_EQ(expect_success, success);
    run_loop_.Quit();
  }

  // Set expectations for a success of the state keys uploading task.
  void ExpectSuccess(bool expect_success) {
    uploader_->SetStatusCallbackForTesting(
        base::BindOnce(&DeviceCloudStateKeysUploaderTest::TestStatus,
                       base::Unretained(this), expect_success));
  }

  void InitUploader(
      const std::string& dm_token,
      base::TimeDelta dm_token_retrieval_delay = base::TimeDelta()) {
    uploader_ = std::make_unique<DeviceCloudStateKeysUploader>(
        client_id_, &service_, &broker_, shared_url_loader_factory_,
        std::make_unique<FakeDMTokenStorage>(dm_token,
                                             dm_token_retrieval_delay));
    uploader_->Init();
  }

  void SetStateKeys(int count) {
    std::vector<std::string> state_keys;
    for (int i = 0; i < count; ++i)
      state_keys.push_back(std::string(1, '1' + i));
    fake_session_manager_client_.set_server_backed_state_keys(state_keys);
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::RunLoop run_loop_;
  std::string client_id_;
  chromeos::FakeSessionManagerClient fake_session_manager_client_;
  ServerBackedStateKeysBroker broker_;
  testing::StrictMock<MockDeviceManagementService> service_;
  std::unique_ptr<DeviceCloudStateKeysUploader> uploader_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  network::TestURLLoaderFactory url_loader_factory_;
};

// Expects to successfully upload state keys.
TEST_F(DeviceCloudStateKeysUploaderTest, UploadStateKeys) {
  SetStateKeys(1);
  InitUploader("test-dm-token");

  DeviceManagementService::JobConfiguration::JobType job_type;
  em::DeviceManagementResponse policy_response = GetPolicyResponse();
  EXPECT_CALL(service_, StartJob)
      .WillOnce(DoAll(
          service_.CaptureJobType(&job_type),
          service_.StartJobAsync(net::OK, DeviceManagementService::kSuccess,
                                 policy_response)));
  ExpectSuccess(true);
  run_loop_.Run();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type);
  EXPECT_THAT(uploader_->state_keys_to_upload(), testing::ElementsAre("1"));
}

// Expects to successfully upload state keys with unrealistically delayed DM
// Token retrieval, so another state keys update happen before DM Token is
// available.
TEST_F(DeviceCloudStateKeysUploaderTest,
       UploadStateKeysWithDelayedDMTokenRetrieval) {
  SetStateKeys(1);
  InitUploader("test-dm-token",
               ServerBackedStateKeysBroker::GetPollIntervalForTesting() * 2);

  task_environment_.FastForwardBy(
      ServerBackedStateKeysBroker::GetPollIntervalForTesting() / 2);

  // Expect state keys to be uploaded, but client not registered yet
  // (DM Token retrieval still pending).
  EXPECT_THAT(uploader_->state_keys_to_upload(), testing::ElementsAre("1"));
  EXPECT_FALSE(uploader_->IsClientRegistered());

  // Set new state keys and fast-foward time so state keys update is triggered
  // during DM Token retrieval (which should be still pending).
  SetStateKeys(2);
  task_environment_.FastForwardBy(
      ServerBackedStateKeysBroker::GetPollIntervalForTesting());
  EXPECT_THAT(uploader_->state_keys_to_upload(),
              testing::ElementsAre("1", "2"));
  EXPECT_FALSE(uploader_->IsClientRegistered());

  // Expect DM Token retrieval and registration to be completed
  // and a successful state keys upload (via policy fetch).
  DeviceManagementService::JobConfiguration::JobType job_type;
  em::DeviceManagementResponse policy_response = GetPolicyResponse();
  EXPECT_CALL(service_, StartJob)
      .WillOnce(DoAll(
          service_.CaptureJobType(&job_type),
          service_.StartJobAsync(net::OK, DeviceManagementService::kSuccess,
                                 policy_response)));
  ExpectSuccess(true);
  run_loop_.Run();
  EXPECT_TRUE(uploader_->IsClientRegistered());
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type);
}

// Expects to trigger state keys uploads after state key updates multiple times.
TEST_F(DeviceCloudStateKeysUploaderTest, UploadStateKeysMultipleTimes) {
  SetStateKeys(1);
  InitUploader("test-dm-token");

  DeviceManagementService::JobConfiguration::JobType job_type;
  em::DeviceManagementResponse policy_response = GetPolicyResponse();
  EXPECT_CALL(service_, StartJob)
      .Times(3)
      .WillRepeatedly(DoAll(
          service_.CaptureJobType(&job_type),
          service_.StartJobAsync(net::OK, DeviceManagementService::kSuccess,
                                 policy_response)));
  ExpectSuccess(true);
  run_loop_.Run();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type);
  EXPECT_THAT(uploader_->state_keys_to_upload(), testing::ElementsAre("1"));

  // Set new state keys and fast-forward time to trigger state keys update.
  ExpectSuccess(true);
  SetStateKeys(2);
  task_environment_.FastForwardBy(
      ServerBackedStateKeysBroker::GetPollIntervalForTesting());
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type);
  EXPECT_THAT(uploader_->state_keys_to_upload(),
              testing::ElementsAre("1", "2"));

  // Set new state keys and fast-forward time to trigger state keys update.
  ExpectSuccess(true);
  SetStateKeys(3);
  task_environment_.FastForwardBy(
      ServerBackedStateKeysBroker::GetPollIntervalForTesting());
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type);
  EXPECT_THAT(uploader_->state_keys_to_upload(),
              testing::ElementsAre("1", "2", "3"));
}

// Expects to fail when uploading state keys with empty DM Token.
TEST_F(DeviceCloudStateKeysUploaderTest, UploadStateKeysEmptyTokenFailure) {
  SetStateKeys(1);
  InitUploader(/*dm_token=*/"");
  ExpectSuccess(false);
  run_loop_.Run();
  EXPECT_FALSE(uploader_->IsClientRegistered());
  EXPECT_THAT(uploader_->state_keys_to_upload(), testing::ElementsAre("1"));
}

// Expect to fail when uploading state keys without initializing them first.
TEST_F(DeviceCloudStateKeysUploaderTest, UploadStateKeysEmptyStateKeysFailure) {
  InitUploader("test-dm-token");
  ExpectSuccess(false);
  run_loop_.Run();
  EXPECT_FALSE(uploader_->IsClientRegistered());
  EXPECT_THAT(uploader_->state_keys_to_upload(), testing::IsEmpty());
}

}  // namespace policy
