// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/enterprise/connectors/analysis/local_binary_upload_service.h"

#include "base/barrier_closure.h"
#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/connectors/test/fake_content_analysis_sdk_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "components/device_signals/core/browser/mock_system_signals_service_host.h"
#include "components/enterprise/connectors/core/analysis_settings.h"
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
constexpr char kFakeUserActionId2[] = "0987654321";

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
                                        bool auto_verify = true,
                                        bool use_system_signals_service = true)
      : LocalBinaryUploadService(profile),
        use_system_signals_service_(use_system_signals_service) {
    if (auto_verify && use_system_signals_service_) {
      SetFileSystemSignals(/*is_os_verified=*/true, "Foo");
    }
  }

  void set_cancel_quit_closure(base::RepeatingClosure closure) {
    cancel_quit_closure_ = closure;
  }

  void SetFileSystemSignals(bool is_os_verified,
                            const char* subject_name = nullptr) {
    EXPECT_CALL(mock_system_signals_service_, GetFileSystemSignals(_, _))
        .WillRepeatedly(Invoke(
            [is_os_verified, subject_name](
                const std::vector<device_signals::GetFileSystemInfoOptions>&
                    options,
                device_signals::mojom::SystemSignalsService::
                    GetFileSystemSignalsCallback callback) {
              // The real implementation is always async, so post a task to
              // reply later.
              base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE,
                  base::BindOnce(
                      [](bool is_os_verified, const char* subject_name,
                         device_signals::mojom::SystemSignalsService::
                             GetFileSystemSignalsCallback callback) {
                        device_signals::ExecutableMetadata metadata;
                        metadata.is_os_verified = is_os_verified;
                        if (subject_name) {
                          metadata.subject_name = subject_name;
                        }
                        device_signals::FileSystemItem item;
                        item.executable_metadata = metadata;
                        std::move(callback).Run({item});
                      },
                      is_os_verified, subject_name, std::move(callback)));
            }));
  }

 private:
  device_signals::mojom::SystemSignalsService* GetSystemSignalsService()
      override {
    if (!use_system_signals_service_) {
      return nullptr;
    }

    return &mock_system_signals_service_;
  }

  void OnCancelRequestSent(std::unique_ptr<CancelRequests> cancel) override {
    if (!cancel_quit_closure_.is_null()) {
      cancel_quit_closure_.Run();
    }
  }

  base::RepeatingClosure cancel_quit_closure_;
  device_signals::MockSystemSignalsService mock_system_signals_service_;
  bool use_system_signals_service_;
};

class LocalBinaryUploadServiceTest : public testing::Test {
 public:
  LocalBinaryUploadServiceTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  base::RepeatingClosure CreateQuitBarrier(size_t count) {
    return base::BarrierClosure(count, task_environment_.QuitClosure());
  }

  std::unique_ptr<MockRequest> MakeRequest(
      const content_analysis::sdk::Client::Config& config,
      BinaryUploadService::Result* scanning_result,
      ContentAnalysisResponse* scanning_response,
      base::RepeatingClosure barrier_closure = base::DoNothing(),
      base::span<const char* const> subject_names =
          base::span<const char* const>()) {
    LocalAnalysisSettings settings;
    settings.local_path = config.name;
    settings.user_specific = config.user_specific;
    settings.subject_names = subject_names;
    auto request = std::make_unique<NiceMock<MockRequest>>(
        base::BindOnce(
            [](BinaryUploadService::Result* target_result,
               ContentAnalysisResponse* target_response,
               base::RepeatingClosure closure,
               BinaryUploadService::Result result,
               ContentAnalysisResponse response) {
              *target_result = result;
              *target_response = response;
              closure.Run();
            },
            scanning_result, scanning_response, barrier_closure),
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
  EXPECT_TRUE(response.has_request_token());
  ASSERT_FALSE(response.request_token().empty());

  FakeContentAnalysisSdkClient* fake_client_ptr =
      fake_sdk_manager_.GetFakeClient(config);
  ASSERT_THAT(fake_client_ptr, NotNull());

  const content_analysis::sdk::ContentAnalysisRequest& sdk_request =
      fake_client_ptr->GetRequest(response.request_token());
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
  EXPECT_TRUE(response.has_request_token());
  ASSERT_FALSE(response.request_token().empty());

  FakeContentAnalysisSdkClient* fake_client_ptr =
      fake_sdk_manager_.GetFakeClient(config);
  ASSERT_THAT(fake_client_ptr, NotNull());

  const content_analysis::sdk::ContentAnalysisRequest& sdk_request =
      fake_client_ptr->GetRequest(response.request_token());

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

TEST_F(LocalBinaryUploadServiceTest, PendingRequestsGetProcessed) {
  content_analysis::sdk::Client::Config config{"local_system_path", false};
  FakeLocalBinaryUploadService lbus(&profile_);

  content_analysis::sdk::ContentAnalysisResponse response;
  fake_sdk_manager_.SetClientSendResponse(response);

  constexpr size_t kCount = LocalBinaryUploadService::kMaxActiveCount + 1;
  BinaryUploadService::Result results[kCount];
  ContentAnalysisResponse responses[kCount];
  auto barrier_closure = CreateQuitBarrier(kCount);
  for (size_t i = 0; i < kCount; ++i) {
    lbus.MaybeUploadForDeepScanning(
        MakeRequest(config, results + i, responses + i, barrier_closure));
  }

  task_environment_.RunUntilQuit();

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

TEST_F(LocalBinaryUploadServiceTest, AgentErrorMakesManyRequestsPending) {
  content_analysis::sdk::Client::Config config{"local_system_path", false};
  FakeLocalBinaryUploadService lbus(&profile_);

  fake_sdk_manager_.SetClientSendStatus(-1);

  constexpr size_t kCount = LocalBinaryUploadService::kMaxActiveCount + 1;

  BinaryUploadService::Result results[kCount];
  ContentAnalysisResponse responses[kCount];

  for (size_t i = 0; i < kCount; ++i) {
    lbus.MaybeUploadForDeepScanning(
        MakeRequest(config, results + i, responses + i));
  }

  task_environment_.RunUntilIdle();

  // Tasks should remain pending and not complete.

  EXPECT_EQ(0u, lbus.GetActiveRequestCountForTesting());
  EXPECT_EQ(kCount, lbus.GetPendingRequestCountForTesting());
}

TEST_F(LocalBinaryUploadServiceTest, TimeoutWhileActive) {
  content_analysis::sdk::Client::Config config{"local_system_path", false};
  FakeLocalBinaryUploadService lbus(&profile_);

  BinaryUploadService::Result result;
  ContentAnalysisResponse response;
  lbus.MaybeUploadForDeepScanning(MakeRequest(config, &result, &response));

  EXPECT_EQ(1u, lbus.GetActiveRequestCountForTesting());
  EXPECT_EQ(0u, lbus.GetPendingRequestCountForTesting());

  const std::map<LocalBinaryUploadService::Request::Id,
                 LocalBinaryUploadService::RequestInfo>& actives =
      lbus.GetActiveRequestsForTesting();
  LocalBinaryUploadService::Request::Id id = actives.begin()->first;
  lbus.OnTimeoutForTesting(id);

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
  LocalBinaryUploadService::Request::Id id = pendings[0].request->id();
  lbus.OnTimeoutForTesting(id);

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

TEST_F(LocalBinaryUploadServiceTest, OnConnectionRetryCompletesManyPending) {
  content_analysis::sdk::Client::Config config{"local_system_path", false};
  FakeLocalBinaryUploadService lbus(&profile_);

  fake_sdk_manager_.SetClientSendStatus(-1);

  constexpr size_t kCount = LocalBinaryUploadService::kMaxActiveCount + 1;
  BinaryUploadService::Result results[kCount];
  ContentAnalysisResponse responses[kCount];
  for (size_t i = 0; i < kCount; ++i) {
    lbus.MaybeUploadForDeepScanning(
        MakeRequest(config, results + i, responses + i));
  }

  task_environment_.RunUntilIdle();

  EXPECT_EQ(0u, lbus.GetActiveRequestCountForTesting());
  EXPECT_EQ(kCount, lbus.GetPendingRequestCountForTesting());

  // The next time the code tries to connect to a client it succeeds.
  fake_sdk_manager_.SetClientSendStatus(0);

  task_environment_.FastForwardBy(base::Minutes(2));

  EXPECT_EQ(0u, lbus.GetActiveRequestCountForTesting());
  EXPECT_EQ(0u, lbus.GetPendingRequestCountForTesting());
  for (size_t i = 0; i < kCount; ++i) {
    EXPECT_EQ(BinaryUploadService::Result::SUCCESS, results[i]);
  }
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

  // New requests should fail while agent is not running.
  result = BinaryUploadService::Result::UNKNOWN;
  lbus.MaybeUploadForDeepScanning(MakeRequest(config, &result, &response));

  task_environment_.RunUntilIdle();
  EXPECT_EQ(BinaryUploadService::Result::UPLOAD_FAILURE, result);

  // The next time the code tries to connect to a client it succeeds.
  fake_sdk_manager_.SetClientSendStatus(0);

  // New requests should succeed now that agent is running.
  result = BinaryUploadService::Result::UNKNOWN;
  lbus.MaybeUploadForDeepScanning(MakeRequest(config, &result, &response));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(BinaryUploadService::Result::SUCCESS, result);
}

TEST_F(LocalBinaryUploadServiceTest, CancelRequests) {
  content_analysis::sdk::Client::Config config{"local_system_path", false};
  LocalAnalysisSettings local;
  local.local_path = config.name;
  local.user_specific = config.user_specific;

  CloudOrLocalAnalysisSettings cloud_or_local(local);
  FakeLocalBinaryUploadService lbus(&profile_);

  constexpr size_t kCount = LocalBinaryUploadService::kMaxActiveCount + 1;
  BinaryUploadService::Result results[kCount];
  ContentAnalysisResponse responses[kCount];

  // Create a barrier closure whose count include one for each analysis request
  // plus one for the cancel request.
  auto barrier_closure = CreateQuitBarrier(kCount + 1);
  lbus.set_cancel_quit_closure(barrier_closure);

  for (size_t i = 0; i < kCount; ++i) {
    auto request =
        MakeRequest(config, results + i, responses + i, barrier_closure);
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

  FakeContentAnalysisSdkClient* fake_client_ptr =
      fake_sdk_manager_.GetFakeClient({local.local_path, local.user_specific});

  // Pending requests are cancelled, but active ones remain.
  EXPECT_EQ(LocalBinaryUploadService::kMaxActiveCount,
            lbus.GetActiveRequestCountForTesting());
  EXPECT_EQ(0u, lbus.GetPendingRequestCountForTesting());

  // Cancel request is not yet sent.
  EXPECT_TRUE(fake_client_ptr->GetCancelRequests().user_action_id().empty());

  task_environment_.RunUntilQuit();

  // Active requests are now cancelled.
  EXPECT_EQ(0u, lbus.GetActiveRequestCountForTesting());

  // Cancel request was sent.
  EXPECT_EQ(kFakeUserActionId,
            fake_client_ptr->GetCancelRequests().user_action_id());
}

TEST_F(LocalBinaryUploadServiceTest, CancelRequests_MultipleUserActions) {
  content_analysis::sdk::Client::Config config{"local_system_path", false};
  LocalAnalysisSettings local;
  local.local_path = config.name;
  local.user_specific = config.user_specific;

  CloudOrLocalAnalysisSettings cloud_or_local(local);
  FakeLocalBinaryUploadService lbus(&profile_);

  BinaryUploadService::Result results[2];
  ContentAnalysisResponse responses[2];

  // Create a barrier closure whose count include one for each analysis request
  // plus one for the cancel request.
  auto barrier_closure = CreateQuitBarrier(3);
  lbus.set_cancel_quit_closure(barrier_closure);

  auto request = MakeRequest(config, results, responses, barrier_closure);
  request->set_user_action_id(kFakeUserActionId);
  lbus.MaybeUploadForDeepScanning(std::move(request));

  request = MakeRequest(config, results + 1, responses + 1, barrier_closure);
  request->set_user_action_id(kFakeUserActionId2);
  lbus.MaybeUploadForDeepScanning(std::move(request));

  EXPECT_EQ(2u, lbus.GetActiveRequestCountForTesting());
  EXPECT_EQ(0u, lbus.GetPendingRequestCountForTesting());

  auto cr = std::make_unique<LocalBinaryUploadService::CancelRequests>(
      cloud_or_local);
  cr->set_user_action_id(kFakeUserActionId);
  lbus.MaybeCancelRequests(std::move(cr));

  // No active requests are cancelled yet.
  EXPECT_EQ(2u, lbus.GetActiveRequestCountForTesting());
  EXPECT_EQ(0u, lbus.GetPendingRequestCountForTesting());

  task_environment_.RunUntilQuit();

  // All requests are done.
  EXPECT_EQ(0u, lbus.GetActiveRequestCountForTesting());
  EXPECT_EQ(0u, lbus.GetPendingRequestCountForTesting());
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

  // Set agent verification info.
  lbus.SetFileSystemSignals(true, "Foo");

  BinaryUploadService::Result result;
  ContentAnalysisResponse response;
  auto barrier_closure = CreateQuitBarrier(1u);
  std::vector<const char*> kSubjectNames = {"Foo"};
  lbus.MaybeUploadForDeepScanning(
      MakeRequest(config, &result, &response, barrier_closure, kSubjectNames));

  EXPECT_EQ(1u, lbus.GetActiveRequestCountForTesting());
  EXPECT_EQ(0u, lbus.GetPendingRequestCountForTesting());

  task_environment_.RunUntilQuit();

  EXPECT_EQ(BinaryUploadService::Result::SUCCESS, result);
  EXPECT_EQ(0u, lbus.GetActiveRequestCountForTesting());
  EXPECT_EQ(0u, lbus.GetPendingRequestCountForTesting());
}

TEST_F(LocalBinaryUploadServiceTest, VerifyAgent_NotOSVerified) {
  content_analysis::sdk::Client::Config config{"local_system_path", false};
  FakeLocalBinaryUploadService lbus(&profile_, /*auto_verify=*/false);

  // Set agent verification info.
  lbus.SetFileSystemSignals(false);

  BinaryUploadService::Result result;
  ContentAnalysisResponse response;
  auto barrier_closure = CreateQuitBarrier(1u);
  std::vector<const char*> kSubjectNames = {"Foo"};
  lbus.MaybeUploadForDeepScanning(
      MakeRequest(config, &result, &response, barrier_closure, kSubjectNames));

  EXPECT_EQ(1u, lbus.GetActiveRequestCountForTesting());
  EXPECT_EQ(0u, lbus.GetPendingRequestCountForTesting());

  task_environment_.RunUntilQuit();

  EXPECT_EQ(BinaryUploadService::Result::UPLOAD_FAILURE, result);
  EXPECT_EQ(0u, lbus.GetActiveRequestCountForTesting());
  EXPECT_EQ(0u, lbus.GetPendingRequestCountForTesting());
}

TEST_F(LocalBinaryUploadServiceTest, VerifyAgent_NoSubject) {
  content_analysis::sdk::Client::Config config{"local_system_path", false};
  FakeLocalBinaryUploadService lbus(&profile_, /*auto_verify=*/false);

  // Set agent verification info.
  lbus.SetFileSystemSignals(true);

  BinaryUploadService::Result result;
  ContentAnalysisResponse response;
  auto barrier_closure = CreateQuitBarrier(1u);
  std::vector<const char*> kSubjectNames = {"Foo"};
  lbus.MaybeUploadForDeepScanning(
      MakeRequest(config, &result, &response, barrier_closure, kSubjectNames));

  EXPECT_EQ(1u, lbus.GetActiveRequestCountForTesting());
  EXPECT_EQ(0u, lbus.GetPendingRequestCountForTesting());

  task_environment_.RunUntilQuit();

  EXPECT_EQ(BinaryUploadService::Result::UPLOAD_FAILURE, result);
  EXPECT_EQ(0u, lbus.GetActiveRequestCountForTesting());
  EXPECT_EQ(0u, lbus.GetPendingRequestCountForTesting());
}

TEST_F(LocalBinaryUploadServiceTest, VerifyAgent_VerifyNotNeeded) {
  content_analysis::sdk::Client::Config config{"local_system_path", false};
  FakeLocalBinaryUploadService lbus(&profile_, /*auto_verify=*/false);

  // Set agent verification info.
  lbus.SetFileSystemSignals(false);

  BinaryUploadService::Result result;
  ContentAnalysisResponse response;
  auto barrier_closure = CreateQuitBarrier(1u);
  lbus.MaybeUploadForDeepScanning(
      MakeRequest(config, &result, &response, barrier_closure));

  EXPECT_EQ(1u, lbus.GetActiveRequestCountForTesting());
  EXPECT_EQ(0u, lbus.GetPendingRequestCountForTesting());

  task_environment_.RunUntilQuit();

  EXPECT_EQ(BinaryUploadService::Result::SUCCESS, result);
  EXPECT_EQ(0u, lbus.GetActiveRequestCountForTesting());
  EXPECT_EQ(0u, lbus.GetPendingRequestCountForTesting());
}

TEST_F(LocalBinaryUploadServiceTest, VerifyAgent_MissingSystemSignalService) {
  content_analysis::sdk::Client::Config config{"local_system_path", false};
  FakeLocalBinaryUploadService lbus(&profile_, /*auto_verify=*/false,
                                    /*use_system_signals_service=*/false);

  BinaryUploadService::Result result;
  ContentAnalysisResponse response;
  auto barrier_closure = CreateQuitBarrier(1u);
  std::vector<const char*> kSubjectNames = {"Foo"};
  lbus.MaybeUploadForDeepScanning(
      MakeRequest(config, &result, &response, barrier_closure, kSubjectNames));

  EXPECT_EQ(1u, lbus.GetActiveRequestCountForTesting());
  EXPECT_EQ(0u, lbus.GetPendingRequestCountForTesting());

  task_environment_.RunUntilQuit();

  EXPECT_EQ(BinaryUploadService::Result::SUCCESS, result);
  EXPECT_EQ(0u, lbus.GetActiveRequestCountForTesting());
  EXPECT_EQ(0u, lbus.GetPendingRequestCountForTesting());
}

}  // namespace enterprise_connectors
