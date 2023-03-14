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

// A subclass of LocalBinaryUploadService which avoids using
// SystemSignalsServiceHost and does not do any binary verification.
//
// By default this class works in auto-verify mode, which means that
// agents are always considered verified.  This simplifies writing tests.
// Test related specifically to the verification process don't use
// auto-verify mode.
class FakeLocalBinaryUploadService : public LocalBinaryUploadService {
 public:
  explicit FakeLocalBinaryUploadService(Profile* profile,
                                        bool auto_verify = true)
      : LocalBinaryUploadService(profile), auto_verify_(auto_verify) {}

  // Finish a call to StartAgentVerification() with the given information.
  void FinishAgentVerification(
      content_analysis::sdk::Client::Config config,
      const base::span<const char* const> subject_names,
      const std::vector<device_signals::FileSystemItem>& items) {
    OnFileSystemSignals(config, subject_names, items);
  }

 private:
  void StartAgentVerification(
      const content_analysis::sdk::Client::Config& config,
      const base::span<const char* const> subject_names) override {
    if (auto_verify_) {
      GetAgentVerifiedMapForTesting()[config] = true;
    }
  }

  bool IsAgentVerified(
      const content_analysis::sdk::Client::Config& config) override {
    return auto_verify_ || LocalBinaryUploadService::IsAgentVerified(config);
  }

  bool auto_verify_;
};

class LocalBinaryUploadServiceTest : public testing::Test {
 public:
  LocalBinaryUploadServiceTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  std::unique_ptr<MockRequest> MakeRequest(
      const content_analysis::sdk::Client::Config& config,
      BinaryUploadService::Result* scanning_result,
      ContentAnalysisResponse* scanning_response) {
    LocalAnalysisSettings settings;
    settings.local_path = config.name;
    settings.user_specific = config.user_specific;
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
  content_analysis::sdk::Client::Config config{"local_system_path", false};
  FakeLocalBinaryUploadService lbus(&profile_);

  LocalAnalysisSettings settings;
  settings.local_path = config.name;
  settings.user_specific = config.user_specific;

  auto ack = std::make_unique<safe_browsing::BinaryUploadService::Ack>(
      CloudOrLocalAnalysisSettings(std::move(settings)));
  lbus.MaybeAcknowledge(std::move(ack));

  EXPECT_TRUE(fake_sdk_manager_.HasClientForTesting(config));
}

TEST_F(LocalBinaryUploadServiceTest, ClientDestroyedWhenAckStatusIsAbnormal) {
  content_analysis::sdk::Client::Config config{"local_system_path", false};
  FakeLocalBinaryUploadService lbus(&profile_);

  fake_sdk_manager_.SetClientAckStatus(-1);

  LocalAnalysisSettings settings;
  settings.local_path = config.name;
  settings.user_specific = config.user_specific;

  auto ack = std::make_unique<safe_browsing::BinaryUploadService::Ack>(
      CloudOrLocalAnalysisSettings(std::move(settings)));
  lbus.MaybeAcknowledge(std::move(ack));

  EXPECT_TRUE(fake_sdk_manager_.HasClientForTesting(config));

  task_environment_.RunUntilIdle();

  EXPECT_FALSE(fake_sdk_manager_.HasClientForTesting(config));
}

TEST_F(LocalBinaryUploadServiceTest, UploadSucceeds) {
  content_analysis::sdk::Client::Config config{"local_system_path", false};
  FakeLocalBinaryUploadService lbus(&profile_);

  BinaryUploadService::Result result;
  ContentAnalysisResponse response;
  lbus.MaybeUploadForDeepScanning(MakeRequest(config, &result, &response));

  task_environment_.RunUntilIdle();

  EXPECT_EQ(result, BinaryUploadService::Result::SUCCESS);
}

TEST_F(LocalBinaryUploadServiceTest, UploadFailsWhenClientUnableToSend) {
  content_analysis::sdk::Client::Config config{"local_system_path", false};
  FakeLocalBinaryUploadService lbus(&profile_);

  fake_sdk_manager_.SetClientSendStatus(-1);

  BinaryUploadService::Result result;
  ContentAnalysisResponse response;
  lbus.MaybeUploadForDeepScanning(MakeRequest(config, &result, &response));

  task_environment_.FastForwardBy(base::Minutes(1));

  EXPECT_EQ(result, BinaryUploadService::Result::UPLOAD_FAILURE);
}

TEST_F(LocalBinaryUploadServiceTest,
       VerifyRequestTokenParityWhenUploadSucceeds) {
  content_analysis::sdk::Client::Config config{"local_system_path", false};
  FakeLocalBinaryUploadService lbus(&profile_);

  BinaryUploadService::Result result;
  ContentAnalysisResponse response;
  lbus.MaybeUploadForDeepScanning(MakeRequest(config, &result, &response));

  task_environment_.RunUntilIdle();

  FakeContentAnalysisSdkClient* fake_client_ptr =
      fake_sdk_manager_.GetFakeClient(config);
  ASSERT_THAT(fake_client_ptr, NotNull());

  const content_analysis::sdk::ContentAnalysisRequest& sdk_request =
      fake_client_ptr->GetRequest();
  EXPECT_EQ(result, BinaryUploadService::Result::SUCCESS);
  EXPECT_TRUE(sdk_request.has_request_token());
  EXPECT_EQ(sdk_request.request_token(), response.request_token());
}

TEST_F(LocalBinaryUploadServiceTest, VerifyTabTitleIsSet) {
  content_analysis::sdk::Client::Config config{"local_system_path", false};
  FakeLocalBinaryUploadService lbus(&profile_);

  BinaryUploadService::Result result;
  ContentAnalysisResponse response;
  lbus.MaybeUploadForDeepScanning(MakeRequest(config, &result, &response));

  task_environment_.RunUntilIdle();

  FakeContentAnalysisSdkClient* fake_client_ptr =
      fake_sdk_manager_.GetFakeClient(config);
  ASSERT_THAT(fake_client_ptr, NotNull());

  const content_analysis::sdk::ContentAnalysisRequest& sdk_request =
      fake_client_ptr->GetRequest();

  EXPECT_EQ(sdk_request.request_data().tab_title(), "tab_title");
}

TEST_F(LocalBinaryUploadServiceTest, SomeRequestsArePending) {
  content_analysis::sdk::Client::Config config{"local_system_path", false};
  FakeLocalBinaryUploadService lbus(&profile_);

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
  content_analysis::sdk::Client::Config config{"local_system_path", false};
  FakeLocalBinaryUploadService lbus(&profile_);

  content_analysis::sdk::ContentAnalysisResponse response;
  fake_sdk_manager_.SetClientSendResponse(response);

  BinaryUploadService::Result
      results[LocalBinaryUploadService::kMaxActiveCount + 1];
  ContentAnalysisResponse
      responses[LocalBinaryUploadService::kMaxActiveCount + 1];

  for (size_t i = 0; i < LocalBinaryUploadService::kMaxActiveCount + 1; ++i) {
    lbus.MaybeUploadForDeepScanning(
        MakeRequest(config, results + i, responses + i));
  }

  task_environment_.RunUntilIdle();

  EXPECT_EQ(0u, lbus.GetActiveRequestCountForTesting());
  EXPECT_EQ(0u, lbus.GetPendingRequestCountForTesting());

  for (auto result : results) {
    EXPECT_EQ(BinaryUploadService::Result::SUCCESS, result);
  }
}

TEST_F(LocalBinaryUploadServiceTest, AgentErrorMakesRequestPending) {
  content_analysis::sdk::Client::Config config{"local_system_path", false};
  FakeLocalBinaryUploadService lbus(&profile_);

  fake_sdk_manager_.SetClientSendStatus(-1);

  BinaryUploadService::Result result;
  ContentAnalysisResponse response;
  lbus.MaybeUploadForDeepScanning(MakeRequest(config, &result, &response));

  EXPECT_EQ(1u, lbus.GetActiveRequestCountForTesting());
  EXPECT_EQ(0u, lbus.GetPendingRequestCountForTesting());

  task_environment_.RunUntilIdle();

  EXPECT_EQ(0u, lbus.GetActiveRequestCountForTesting());
  EXPECT_EQ(1u, lbus.GetPendingRequestCountForTesting());
}

TEST_F(LocalBinaryUploadServiceTest, TimeoutWhileActive) {
  content_analysis::sdk::Client::Config config{"local_system_path", false};
  FakeLocalBinaryUploadService lbus(&profile_);

  BinaryUploadService::Result result;
  ContentAnalysisResponse response;
  lbus.MaybeUploadForDeepScanning(MakeRequest(config, &result, &response));

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
  content_analysis::sdk::Client::Config config{"local_system_path", false};
  FakeLocalBinaryUploadService lbus(&profile_);

  fake_sdk_manager_.SetClientSendStatus(-1);

  BinaryUploadService::Result result;
  ContentAnalysisResponse response;
  lbus.MaybeUploadForDeepScanning(MakeRequest(config, &result, &response));

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
  content_analysis::sdk::Client::Config config{"local_system_path", false};
  FakeLocalBinaryUploadService lbus(&profile_);

  fake_sdk_manager_.SetClientSendStatus(-1);

  BinaryUploadService::Result result;
  ContentAnalysisResponse response;
  lbus.MaybeUploadForDeepScanning(MakeRequest(config, &result, &response));

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
  content_analysis::sdk::Client::Config config{"local_system_path", false};
  FakeLocalBinaryUploadService lbus(&profile_);

  fake_sdk_manager_.SetClientSendStatus(-1);

  BinaryUploadService::Result result;
  ContentAnalysisResponse response;
  lbus.MaybeUploadForDeepScanning(MakeRequest(config, &result, &response));

  task_environment_.FastForwardBy(base::Minutes(1));

  EXPECT_EQ(BinaryUploadService::Result::UPLOAD_FAILURE, result);
  EXPECT_EQ(0u, lbus.GetActiveRequestCountForTesting());
  EXPECT_EQ(0u, lbus.GetPendingRequestCountForTesting());

  // New requests should fail immediately.
  result = BinaryUploadService::Result::UNKNOWN;
  lbus.MaybeUploadForDeepScanning(MakeRequest(config, &result, &response));
  EXPECT_EQ(BinaryUploadService::Result::UPLOAD_FAILURE, result);
}

TEST_F(LocalBinaryUploadServiceTest, CancelRequests) {
  content_analysis::sdk::Client::Config config{"local_system_path", false};
  LocalAnalysisSettings local;
  local.local_path = config.name;
  local.user_specific = config.user_specific;

  CloudOrLocalAnalysisSettings cloud_or_local(local);
  FakeLocalBinaryUploadService lbus(&profile_);

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
  content_analysis::sdk::Client::Config config{"local_system_path", false};
  fake_sdk_manager_.SetClientCancelStatus(-1);

  LocalAnalysisSettings local;
  local.local_path = config.name;
  local.user_specific = config.user_specific;

  CloudOrLocalAnalysisSettings cloud_or_local(local);
  FakeLocalBinaryUploadService lbus(&profile_);

  auto cr = std::make_unique<LocalBinaryUploadService::CancelRequests>(
      cloud_or_local);
  cr->set_user_action_id("1234567890");
  lbus.MaybeCancelRequests(std::move(cr));

  EXPECT_TRUE(fake_sdk_manager_.HasClientForTesting(config));

  task_environment_.RunUntilIdle();

  EXPECT_FALSE(fake_sdk_manager_.HasClientForTesting(config));
}

TEST_F(LocalBinaryUploadServiceTest, VerifyAgent) {
  content_analysis::sdk::Client::Config config{"local_system_path", false};
  FakeLocalBinaryUploadService lbus(&profile_, /*auto_verify=*/false);

  BinaryUploadService::Result result;
  ContentAnalysisResponse response;
  lbus.MaybeUploadForDeepScanning(MakeRequest(config, &result, &response));

  task_environment_.RunUntilIdle();

  EXPECT_EQ(1u, lbus.GetActiveRequestCountForTesting());
  EXPECT_EQ(0u, lbus.GetPendingRequestCountForTesting());

  // Mark the agent as verified.
  device_signals::ExecutableMetadata metadata;
  metadata.is_os_verified = true;
  metadata.subject_name = "Foo";
  device_signals::FileSystemItem item;
  item.executable_metadata = metadata;
  std::array<const char*, 1> subject_names = {{"Foo"}};
  lbus.FinishAgentVerification(
      config, base::span<const char* const>(subject_names), {item});

  task_environment_.RunUntilIdle();

  EXPECT_EQ(BinaryUploadService::Result::SUCCESS, result);
  EXPECT_EQ(0u, lbus.GetActiveRequestCountForTesting());
  EXPECT_EQ(0u, lbus.GetPendingRequestCountForTesting());
}

TEST_F(LocalBinaryUploadServiceTest, VerifyAgent_NotOSVerified) {
  content_analysis::sdk::Client::Config config{"local_system_path", false};
  FakeLocalBinaryUploadService lbus(&profile_, /*auto_verify=*/false);

  BinaryUploadService::Result result;
  ContentAnalysisResponse response;
  lbus.MaybeUploadForDeepScanning(MakeRequest(config, &result, &response));

  task_environment_.RunUntilIdle();

  EXPECT_EQ(1u, lbus.GetActiveRequestCountForTesting());
  EXPECT_EQ(0u, lbus.GetPendingRequestCountForTesting());

  // Mark the agent as verified.
  device_signals::ExecutableMetadata metadata;
  metadata.is_os_verified = false;
  metadata.subject_name = "Foo";
  device_signals::FileSystemItem item;
  item.executable_metadata = metadata;
  std::array<const char*, 1> subject_names = {{"Foo"}};
  lbus.FinishAgentVerification(
      config, base::span<const char* const>(subject_names), {item});

  task_environment_.RunUntilIdle();

  EXPECT_EQ(BinaryUploadService::Result::UPLOAD_FAILURE, result);
  EXPECT_EQ(0u, lbus.GetActiveRequestCountForTesting());
  EXPECT_EQ(0u, lbus.GetPendingRequestCountForTesting());
}

TEST_F(LocalBinaryUploadServiceTest, VerifyAgent_NoSubject) {
  content_analysis::sdk::Client::Config config{"local_system_path", false};
  FakeLocalBinaryUploadService lbus(&profile_, /*auto_verify=*/false);

  BinaryUploadService::Result result;
  ContentAnalysisResponse response;
  lbus.MaybeUploadForDeepScanning(MakeRequest(config, &result, &response));

  task_environment_.RunUntilIdle();

  EXPECT_EQ(1u, lbus.GetActiveRequestCountForTesting());
  EXPECT_EQ(0u, lbus.GetPendingRequestCountForTesting());

  // Mark the agent as verified.
  device_signals::ExecutableMetadata metadata;
  metadata.is_os_verified = false;
  metadata.subject_name = "Foo";
  device_signals::FileSystemItem item;
  item.executable_metadata = metadata;
  std::array<const char*, 1> subject_names = {{"Foo"}};
  lbus.FinishAgentVerification(
      config, base::span<const char* const>(subject_names), {item});

  task_environment_.RunUntilIdle();

  EXPECT_EQ(BinaryUploadService::Result::UPLOAD_FAILURE, result);
  EXPECT_EQ(0u, lbus.GetActiveRequestCountForTesting());
  EXPECT_EQ(0u, lbus.GetPendingRequestCountForTesting());
}

TEST_F(LocalBinaryUploadServiceTest, VerifyAgent_VerifyNotNeeded) {
  content_analysis::sdk::Client::Config config{"local_system_path", false};
  FakeLocalBinaryUploadService lbus(&profile_, /*auto_verify=*/false);

  BinaryUploadService::Result result;
  ContentAnalysisResponse response;
  lbus.MaybeUploadForDeepScanning(MakeRequest(config, &result, &response));

  task_environment_.RunUntilIdle();

  EXPECT_EQ(1u, lbus.GetActiveRequestCountForTesting());
  EXPECT_EQ(0u, lbus.GetPendingRequestCountForTesting());

  // Mark the agent as verified.
  device_signals::ExecutableMetadata metadata;
  metadata.is_os_verified = false;
  metadata.subject_name = "";
  device_signals::FileSystemItem item;
  item.executable_metadata = metadata;
  lbus.FinishAgentVerification(config, base::span<const char* const>(), {item});

  task_environment_.RunUntilIdle();

  EXPECT_EQ(BinaryUploadService::Result::SUCCESS, result);
  EXPECT_EQ(0u, lbus.GetActiveRequestCountForTesting());
  EXPECT_EQ(0u, lbus.GetPendingRequestCountForTesting());
}

}  // namespace enterprise_connectors
