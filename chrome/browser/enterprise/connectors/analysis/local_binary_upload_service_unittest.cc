// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/local_binary_upload_service.h"

#include "base/functional/callback_helpers.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/connectors/analysis/analysis_settings.h"
#include "chrome/browser/enterprise/connectors/analysis/fake_content_analysis_sdk_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

using ::safe_browsing::BinaryUploadService;
using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SaveArg;

constexpr char kFakeUserActionId[] = "1234567890";

class MockRequest : public BinaryUploadService::Request {
 public:
  MockRequest(BinaryUploadService::ContentAnalysisCallback callback,
              LocalAnalysisSettings settings)
      : BinaryUploadService::Request(
            std::move(callback),
            CloudOrLocalAnalysisSettings(std::move(settings))) {}
  MOCK_METHOD1(GetRequestData, void(DataCallback));
};

class LocalBinaryUploadServiceTest : public testing::Test {
 public:
  LocalBinaryUploadServiceTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  std::unique_ptr<MockRequest> MakeRequest(
      BinaryUploadService::Result* scanning_result,
      ContentAnalysisResponse* scanning_response) {
    LocalAnalysisSettings settings;
    settings.local_path = "local_system_path";
    settings.user_specific = false;
    auto request = std::make_unique<NiceMock<MockRequest>>(
        base::BindOnce(
            [](BinaryUploadService::Result* target_result,
               ContentAnalysisResponse* target_response,
               BinaryUploadService::Result result,
               ContentAnalysisResponse response) {
              *target_result = result;
              *target_response = response;
            },
            scanning_result, scanning_response),
        settings);
    request->set_tab_title("tab_title");
    ON_CALL(*request, GetRequestData(_))
        .WillByDefault(
            Invoke([](BinaryUploadService::Request::DataCallback callback) {
              BinaryUploadService::Request::Data data;
              data.contents = "contents";
              data.size = data.contents.size();
              std::move(callback).Run(BinaryUploadService::Result::SUCCESS,
                                      std::move(data));
            }));
    return request;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  FakeContentAnalysisSdkManager fake_sdk_manager_;
};

TEST_F(LocalBinaryUploadServiceTest, ClientCreatedFromMaybeAcknowledge) {
  LocalBinaryUploadService lbus;

  LocalAnalysisSettings settings;
  settings.local_path = "local_system_path";
  settings.user_specific = false;
  content_analysis::sdk::Client::Config config{settings.local_path,
                                               settings.user_specific};

  auto ack = std::make_unique<safe_browsing::BinaryUploadService::Ack>(
      CloudOrLocalAnalysisSettings(std::move(settings)));
  lbus.MaybeAcknowledge(std::move(ack));

  EXPECT_TRUE(fake_sdk_manager_.HasClientForTesting(config));
}

TEST_F(LocalBinaryUploadServiceTest, ClientDestroyedWhenAckStatusIsAbnormal) {
  LocalBinaryUploadService lbus;

  fake_sdk_manager_.SetClientAckStatus(-1);

  LocalAnalysisSettings settings;
  settings.local_path = "local_system_path";
  settings.user_specific = false;
  content_analysis::sdk::Client::Config config{"local_system_path", false};

  auto ack = std::make_unique<safe_browsing::BinaryUploadService::Ack>(
      CloudOrLocalAnalysisSettings(std::move(settings)));
  lbus.MaybeAcknowledge(std::move(ack));

  EXPECT_TRUE(fake_sdk_manager_.HasClientForTesting(config));

  task_environment_.RunUntilIdle();

  EXPECT_FALSE(fake_sdk_manager_.HasClientForTesting(config));
}

TEST_F(LocalBinaryUploadServiceTest, UploadSucceeds) {
  LocalBinaryUploadService lbus;

  BinaryUploadService::Result result;
  ContentAnalysisResponse response;
  lbus.MaybeUploadForDeepScanning(MakeRequest(&result, &response));

  task_environment_.RunUntilIdle();

  EXPECT_EQ(result, BinaryUploadService::Result::SUCCESS);
}

TEST_F(LocalBinaryUploadServiceTest, UploadFailsWhenClientUnableToSend) {
  LocalBinaryUploadService lbus;

  fake_sdk_manager_.SetClientSendStatus(-1);

  BinaryUploadService::Result result;
  ContentAnalysisResponse response;
  lbus.MaybeUploadForDeepScanning(MakeRequest(&result, &response));

  task_environment_.FastForwardBy(base::Minutes(1));

  EXPECT_EQ(result, BinaryUploadService::Result::UPLOAD_FAILURE);
}

TEST_F(LocalBinaryUploadServiceTest,
       VerifyRequestTokenParityWhenUploadSucceeds) {
  LocalBinaryUploadService lbus;

  BinaryUploadService::Result result;
  ContentAnalysisResponse response;
  lbus.MaybeUploadForDeepScanning(MakeRequest(&result, &response));

  task_environment_.RunUntilIdle();

  FakeContentAnalysisSdkClient* fake_client_ptr =
      fake_sdk_manager_.GetFakeClient({"local_system_path", false});
  ASSERT_THAT(fake_client_ptr, NotNull());

  const content_analysis::sdk::ContentAnalysisRequest& sdk_request =
      fake_client_ptr->GetRequest();
  EXPECT_EQ(result, BinaryUploadService::Result::SUCCESS);
  EXPECT_TRUE(sdk_request.has_request_token());
  EXPECT_EQ(sdk_request.request_token(), response.request_token());
}

TEST_F(LocalBinaryUploadServiceTest, VerifyTabTitleIsSet) {
  LocalBinaryUploadService lbus;

  BinaryUploadService::Result result;
  ContentAnalysisResponse response;
  lbus.MaybeUploadForDeepScanning(MakeRequest(&result, &response));

  task_environment_.RunUntilIdle();

  FakeContentAnalysisSdkClient* fake_client_ptr =
      fake_sdk_manager_.GetFakeClient({"local_system_path", false});
  ASSERT_THAT(fake_client_ptr, NotNull());

  const content_analysis::sdk::ContentAnalysisRequest& sdk_request =
      fake_client_ptr->GetRequest();

  EXPECT_EQ(sdk_request.request_data().tab_title(), "tab_title");
}

TEST_F(LocalBinaryUploadServiceTest, SomeRequestsArePending) {
  LocalBinaryUploadService lbus;

  // Add one more request than the max number of concurrent active requests.
  // The remaining one should be pending.
  LocalAnalysisSettings settings;
  for (size_t i = 0; i < LocalBinaryUploadService::kMaxActiveCount + 1; ++i) {
    lbus.MaybeUploadForDeepScanning(
        std::make_unique<MockRequest>(base::DoNothing(), settings));
  }

  EXPECT_EQ(LocalBinaryUploadService::kMaxActiveCount,
            lbus.GetActiveRequestCountForTesting());
  EXPECT_EQ(1u, lbus.GetPendingRequestCountForTesting());
}

// Flaky on all platforms: http://crbug.com/1365018
TEST_F(LocalBinaryUploadServiceTest, DISABLED_PendingRequestsGetProcessed) {
  LocalBinaryUploadService lbus;

  content_analysis::sdk::ContentAnalysisResponse response;
  fake_sdk_manager_.SetClientSendResponse(response);

  BinaryUploadService::Result
      results[LocalBinaryUploadService::kMaxActiveCount + 1];
  ContentAnalysisResponse
      responses[LocalBinaryUploadService::kMaxActiveCount + 1];

  for (size_t i = 0; i < LocalBinaryUploadService::kMaxActiveCount + 1; ++i) {
    lbus.MaybeUploadForDeepScanning(MakeRequest(results + i, responses + i));
  }

  task_environment_.RunUntilIdle();

  EXPECT_EQ(0u, lbus.GetActiveRequestCountForTesting());
  EXPECT_EQ(0u, lbus.GetPendingRequestCountForTesting());

  for (auto result : results) {
    EXPECT_EQ(BinaryUploadService::Result::SUCCESS, result);
  }
}

TEST_F(LocalBinaryUploadServiceTest, AgentErrorMakesRequestPending) {
  LocalBinaryUploadService lbus;

  fake_sdk_manager_.SetClientSendStatus(-1);

  BinaryUploadService::Result result;
  ContentAnalysisResponse response;
  lbus.MaybeUploadForDeepScanning(MakeRequest(&result, &response));

  EXPECT_EQ(1u, lbus.GetActiveRequestCountForTesting());
  EXPECT_EQ(0u, lbus.GetPendingRequestCountForTesting());

  task_environment_.RunUntilIdle();

  EXPECT_EQ(0u, lbus.GetActiveRequestCountForTesting());
  EXPECT_EQ(1u, lbus.GetPendingRequestCountForTesting());
}

TEST_F(LocalBinaryUploadServiceTest, TimeoutWhileActive) {
  LocalBinaryUploadService lbus;

  BinaryUploadService::Result result;
  ContentAnalysisResponse response;
  lbus.MaybeUploadForDeepScanning(MakeRequest(&result, &response));

  EXPECT_EQ(1u, lbus.GetActiveRequestCountForTesting());
  EXPECT_EQ(0u, lbus.GetPendingRequestCountForTesting());

  const std::map<LocalBinaryUploadService::RequestKey,
                 LocalBinaryUploadService::RequestInfo>& actives =
      lbus.GetActiveRequestsForTesting();
  LocalBinaryUploadService::RequestKey key = actives.begin()->first;
  lbus.OnTimeoutForTesting(key);

  EXPECT_EQ(0u, lbus.GetActiveRequestCountForTesting());
  EXPECT_EQ(0u, lbus.GetPendingRequestCountForTesting());

  // The send should complete, but nothing should happen.
  task_environment_.FastForwardBy(base::Minutes(2));
  task_environment_.RunUntilIdle();
}

TEST_F(LocalBinaryUploadServiceTest, TimeoutWhilePending) {
  LocalBinaryUploadService lbus;

  fake_sdk_manager_.SetClientSendStatus(-1);

  BinaryUploadService::Result result;
  ContentAnalysisResponse response;
  lbus.MaybeUploadForDeepScanning(MakeRequest(&result, &response));

  task_environment_.RunUntilIdle();

  EXPECT_EQ(0u, lbus.GetActiveRequestCountForTesting());
  EXPECT_EQ(1u, lbus.GetPendingRequestCountForTesting());

  const std::vector<LocalBinaryUploadService::RequestInfo>& pendings =
      lbus.GetPendingRequestsForTesting();
  LocalBinaryUploadService::RequestKey key = pendings[0].request.get();
  lbus.OnTimeoutForTesting(key);

  EXPECT_EQ(0u, lbus.GetActiveRequestCountForTesting());
  EXPECT_EQ(0u, lbus.GetPendingRequestCountForTesting());

  // The send should complete, but nothing should happen.
  task_environment_.FastForwardBy(base::Minutes(2));
  task_environment_.RunUntilIdle();
}

TEST_F(LocalBinaryUploadServiceTest, OnConnectionRetryCompletesPending) {
  LocalBinaryUploadService lbus;

  fake_sdk_manager_.SetClientSendStatus(-1);

  BinaryUploadService::Result result;
  ContentAnalysisResponse response;
  lbus.MaybeUploadForDeepScanning(MakeRequest(&result, &response));

  task_environment_.RunUntilIdle();

  EXPECT_EQ(0u, lbus.GetActiveRequestCountForTesting());
  EXPECT_EQ(1u, lbus.GetPendingRequestCountForTesting());

  // The next time the code tries to connect to a client it succeeds.
  fake_sdk_manager_.SetClientSendStatus(0);

  task_environment_.FastForwardBy(base::Minutes(1));

  EXPECT_EQ(BinaryUploadService::Result::SUCCESS, result);
  EXPECT_EQ(0u, lbus.GetActiveRequestCountForTesting());
  EXPECT_EQ(0u, lbus.GetPendingRequestCountForTesting());
}

TEST_F(LocalBinaryUploadServiceTest, FailureAfterTooManyRetries) {
  LocalBinaryUploadService lbus;

  fake_sdk_manager_.SetClientSendStatus(-1);

  BinaryUploadService::Result result;
  ContentAnalysisResponse response;
  lbus.MaybeUploadForDeepScanning(MakeRequest(&result, &response));

  task_environment_.FastForwardBy(base::Minutes(1));

  EXPECT_EQ(BinaryUploadService::Result::UPLOAD_FAILURE, result);
  EXPECT_EQ(0u, lbus.GetActiveRequestCountForTesting());
  EXPECT_EQ(0u, lbus.GetPendingRequestCountForTesting());

  // New requests should fail immediately.
  result = BinaryUploadService::Result::UNKNOWN;
  lbus.MaybeUploadForDeepScanning(MakeRequest(&result, &response));
  EXPECT_EQ(BinaryUploadService::Result::UPLOAD_FAILURE, result);
}

TEST_F(LocalBinaryUploadServiceTest, CancelRequests) {
  LocalAnalysisSettings local;
  local.local_path = "local_system_path";
  local.user_specific = false;

  CloudOrLocalAnalysisSettings cloud_or_local(local);
  LocalBinaryUploadService lbus;

  // Add one more request than the max number of concurrent active requests.
  // The remaining one should be pending.
  LocalAnalysisSettings settings(local);
  for (size_t i = 0; i < LocalBinaryUploadService::kMaxActiveCount + 1; ++i) {
    auto request = std::make_unique<MockRequest>(base::DoNothing(), settings);
    request->set_user_action_id(kFakeUserActionId);
    lbus.MaybeUploadForDeepScanning(std::move(request));
  }

  EXPECT_EQ(LocalBinaryUploadService::kMaxActiveCount,
            lbus.GetActiveRequestCountForTesting());
  EXPECT_EQ(1u, lbus.GetPendingRequestCountForTesting());

  auto cr = std::make_unique<LocalBinaryUploadService::CancelRequests>(
      cloud_or_local);
  cr->set_user_action_id(kFakeUserActionId);
  lbus.MaybeCancelRequests(std::move(cr));

  EXPECT_EQ(0u, lbus.GetActiveRequestCountForTesting());
  EXPECT_EQ(0u, lbus.GetPendingRequestCountForTesting());

  task_environment_.RunUntilIdle();

  FakeContentAnalysisSdkClient* fake_client_ptr =
      fake_sdk_manager_.GetFakeClient({local.local_path, local.user_specific});
  EXPECT_EQ(kFakeUserActionId,
            fake_client_ptr->GetCancelRequests().user_action_id());
}

TEST_F(LocalBinaryUploadServiceTest,
       ClientDestroyedWhenCancelStatusIsAbnormal) {
  fake_sdk_manager_.SetClientCancelStatus(-1);

  LocalAnalysisSettings local;
  local.local_path = "local_system_path";
  local.user_specific = false;

  CloudOrLocalAnalysisSettings cloud_or_local(local);
  content_analysis::sdk::Client::Config config{local.local_path,
                                               local.user_specific};
  LocalBinaryUploadService lbus;

  auto cr = std::make_unique<LocalBinaryUploadService::CancelRequests>(
      cloud_or_local);
  cr->set_user_action_id("1234567890");
  lbus.MaybeCancelRequests(std::move(cr));

  EXPECT_TRUE(fake_sdk_manager_.HasClientForTesting(config));

  task_environment_.RunUntilIdle();

  EXPECT_FALSE(fake_sdk_manager_.HasClientForTesting(config));
}
}  // namespace enterprise_connectors
