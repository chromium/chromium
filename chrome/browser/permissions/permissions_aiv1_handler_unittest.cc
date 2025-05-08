// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permissions_aiv1_handler.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/test_future.h"
#include "base/timer/mock_timer.h"
#include "base/timer/timer.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/optimization_guide/core/mock_optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/features/permissions_ai.pb.h"
#include "components/permissions/request_type.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::base::BindOnce;
using ::base::test::EqualsProto;
using ::content::BrowserThread;
using ::optimization_guide::AnyWrapProto;
using ::optimization_guide::MockSession;
using ::optimization_guide::OptimizationGuideModelExecutionError;
using ::optimization_guide::OptimizationGuideModelStreamingExecutionResult;
using ::optimization_guide::StreamingResponse;
using ::optimization_guide::proto::DefaultResponse;
using ::optimization_guide::proto::PermissionsAiRequest;
using ::optimization_guide::proto::PermissionsAiResponse;
using ::optimization_guide::proto::PermissionType;
using ::permissions::RequestType;
using ::testing::_;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Optional;
using ::testing::StrictMock;
using ::testing::WithArg;
using ModelExecutionResultStreamingCallback = ::optimization_guide::
    OptimizationGuideModelExecutionResultStreamingCallback;
using ModelCallbackFuture =
    ::base::test::TestFuture<std::optional<PermissionsAiResponse>>;
using EligibilityReason = ::optimization_guide::OnDeviceModelEligibilityReason;

constexpr optimization_guide::ModelBasedCapabilityKey kFeatureKey =
    optimization_guide::ModelBasedCapabilityKey::kPermissionsAi;

constexpr char kRenderedText[] = "rendered_text";
constexpr char kSessionCreationSuccessHistogram[] =
    "Permissions.AIv1.SessionCreationSuccess";
constexpr char kModelAvailableAtInquiryTimeHistogram[] =
    "Permissions.AIv1.AvailableAtInquiryTime";
constexpr char kExecutionDurationHistogram[] =
    "Permissions.AIv1.ExecutionDuration";
constexpr char kExecutionSuccessHistogram[] =
    "Permissions.AIv1.ExecutionSuccess";
constexpr char kResponseParseSuccessHistogram[] =
    "Permissions.AIv1.ResponseParseSuccess";
constexpr char kPreviousSessionFinishedInTimeHistogram[] =
    "Permissions.AIv1.PreviousSessionFinishedInTime";
constexpr char kExecutionTimedOutHistogram[] =
    "Permissions.AIv1.ExecutionTimedOut";
}  // namespace

void CheckHistogramsAreEmptyExcept(
    const base::HistogramTester& histogram_tester,
    base::span<const std::string> allowed_histograms) {
  // Static list of all histogram names to check
  static const std::vector<std::string> kAllHistogramNames = {
      kExecutionDurationHistogram,
      kExecutionSuccessHistogram,
      kExecutionTimedOutHistogram,
      kModelAvailableAtInquiryTimeHistogram,
      kPreviousSessionFinishedInTimeHistogram,
      kResponseParseSuccessHistogram,
      kSessionCreationSuccessHistogram,
  };

  // Convert the allowed_histograms span to an unordered_set for fast lookup
  std::unordered_set<std::string> allowed_set(allowed_histograms.begin(),
                                              allowed_histograms.end());

  // Iterate through all histogram names in the static list
  for (const auto& histogram_name : kAllHistogramNames) {
    if (allowed_set.find(histogram_name) == allowed_set.end()) {
      // If the histogram is not in the allowed set, ensure its count is 0
      histogram_tester.ExpectTotalCount(histogram_name, 0);
    }
  }
}

namespace permissions {

class MockTimer : public base::MockOneShotTimer {
 public:
  MockTimer() = default;

  MockTimer(const MockTimer&) = delete;
  MockTimer& operator=(const MockTimer&) = delete;

  ~MockTimer() override = default;

  void Start(const base::Location& posted_from,
             base::TimeDelta delay,
             base::OnceClosure user_task) override {
    base::MockOneShotTimer::Start(posted_from, delay, std::move(user_task));
    ++started_cnt;
  }

  int started_cnt = 0;
};

void CallCounter(int& cnt, std::optional<PermissionsAiResponse> _) {
  ++cnt;
}

class PermissionsAiv1HandlerTestBase : public testing::Test {
 protected:
  PermissionsAiv1HandlerTestBase()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    CHECK(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test-user");
  }

  void SetUp() override {
    SetupMockOptimizationGuideKeyedService();

    auto mock_execution_timer = std::make_unique<MockTimer>();
    mock_execution_timer_ = mock_execution_timer.get();
    permissions_aiv1_handler_ = std::make_unique<PermissionsAiv1Handler>(
        mock_optimization_guide_keyed_service_.get());
    permissions_aiv1_handler_->set_execution_timer_for_testing(
        std::move(mock_execution_timer));
  }

  void SetupMockOptimizationGuideKeyedService() {
    mock_optimization_guide_keyed_service_ =
        static_cast<MockOptimizationGuideKeyedService*>(
            OptimizationGuideKeyedServiceFactory::GetInstance()
                ->SetTestingFactoryAndUse(
                    profile_,
                    base::BindRepeating([](content::BrowserContext* context)
                                            -> std::unique_ptr<KeyedService> {
                      return std::make_unique<
                          NiceMock<MockOptimizationGuideKeyedService>>();
                    })));
  }

  void ExpectStartSessionAndReturn(
      optimization_guide::OptimizationGuideModelExecutor::Session* session) {
    EXPECT_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
        .WillOnce(Invoke(
            [session](
                optimization_guide::ModelBasedCapabilityKey feature,
                const std::optional<optimization_guide::SessionConfigParams>&
                    config_params) {
              return session ? std::make_unique<NiceMock<MockSession>>(session)
                             : nullptr;
            }));
  }

  void ExpectGetOnDeviceModelEligibilityAndReturn(EligibilityReason reason) {
    EXPECT_CALL(*mock_optimization_guide_keyed_service_,
                GetOnDeviceModelEligibility(Eq(kFeatureKey)))
        .WillOnce(Invoke(
            [reason](optimization_guide::ModelBasedCapabilityKey feature) {
              return reason;
            }));
  }

  inline ModelExecutionResultStreamingCallback ExecuteModelAndReturnCallback(
      const PermissionsAiRequest& request) {
    ModelExecutionResultStreamingCallback response_callback;
    EXPECT_CALL(session_, ExecuteModel(EqualsProto(request), _))
        .WillOnce(WithArg<1>(
            Invoke([&](ModelExecutionResultStreamingCallback callback) {
              response_callback = std::move(callback);
            })));
    return response_callback;
  }

  PermissionsAiRequest BuildRequest(PermissionType type) {
    PermissionsAiRequest request;
    request.set_rendered_text(kRenderedText);
    request.set_permission_type(type);
    return request;
  }

  void ReceiveResponseSuccessfully(
      ModelExecutionResultStreamingCallback& response_callback,
      ModelCallbackFuture& future) {
    // We can finish the first inquiry normally.
    PermissionsAiResponse response;
    response.set_is_permission_relevant(true);
    auto streaming_response = StreamingResponse{
        .response = AnyWrapProto(response), .is_complete = true};
    EXPECT_FALSE(response_callback.is_null());
    response_callback.Run(OptimizationGuideModelStreamingExecutionResult(
        base::ok(streaming_response),
        /*provided_by_on_device=*/true));
    EXPECT_THAT(future.Take(), Optional(EqualsProto(response)));
  }

  void ResetHistograms() {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  const base::HistogramTester& histogram_tester() { return *histogram_tester_; }

  std::unique_ptr<base::HistogramTester> histogram_tester_ =
      std::make_unique<base::HistogramTester>();
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;

  std::unique_ptr<PermissionsAiv1Handler> permissions_aiv1_handler_;
  raw_ptr<MockTimer> mock_execution_timer_ = nullptr;
  raw_ptr<MockOptimizationGuideKeyedService>
      mock_optimization_guide_keyed_service_;
  NiceMock<MockSession> session_;
};

class PermissionsAiv1HandlerTest : public PermissionsAiv1HandlerTestBase {};

struct PermissionsAiv1HandlerTestCase {
  PermissionType permission_type;
  RequestType request_type;
  bool is_permission_relevant;
};

class ParametrizedPermissionsAiv1HandlerTest
    : public PermissionsAiv1HandlerTestBase,
      public testing::WithParamInterface<PermissionsAiv1HandlerTestCase> {};

INSTANTIATE_TEST_SUITE_P(
    RequestTypes,
    ParametrizedPermissionsAiv1HandlerTest,
    testing::ValuesIn<PermissionsAiv1HandlerTestCase>({
        {PermissionType::PERMISSION_TYPE_NOTIFICATIONS,
         RequestType::kNotifications, /*is_permission_relevant=*/true},
        {PermissionType::PERMISSION_TYPE_GEOLOCATION, RequestType::kGeolocation,
         /*is_permission_relevant=*/false},
    }));

TEST_P(ParametrizedPermissionsAiv1HandlerTest,
       CanDealWithNullptrOptimizationGuide) {
  PermissionsAiv1Handler permissions_aiv1_handler(nullptr);

  ModelCallbackFuture future;
  permissions_aiv1_handler.InquireAiOnDeviceModel(
      kRenderedText, GetParam().request_type, future.GetCallback());
  EXPECT_EQ(future.Take(), std::nullopt);

  // Timer should not get started at all.
  ASSERT_FALSE(mock_execution_timer_->IsRunning());
  ASSERT_EQ(mock_execution_timer_->started_cnt, 0);

  CheckHistogramsAreEmptyExcept(histogram_tester(), {});
}

TEST_F(PermissionsAiv1HandlerTest, DealsWithModelInavailability) {
  // We assume here, that model download was already start but did not finish
  // yet or a transient error is happening.
  ExpectGetOnDeviceModelEligibilityAndReturn(
      EligibilityReason::kTooManyRecentCrashes);

  // We do not deal witherrors at the moment; we just try to create the session
  // and hope for the best.
  ExpectStartSessionAndReturn(nullptr);

  ModelCallbackFuture future;
  permissions_aiv1_handler_->InquireAiOnDeviceModel(
      kRenderedText, RequestType::kNotifications, future.GetCallback());
  EXPECT_EQ(future.Take(), std::nullopt);

  // Timer should not get started if no valid session is returned.
  ASSERT_FALSE(mock_execution_timer_->IsRunning());
  ASSERT_EQ(mock_execution_timer_->started_cnt, 0);

  histogram_tester().ExpectUniqueSample(kModelAvailableAtInquiryTimeHistogram,
                                        false, 1);
  histogram_tester().ExpectUniqueSample(kSessionCreationSuccessHistogram, false,
                                        1);
  CheckHistogramsAreEmptyExcept(histogram_tester(),
                                {kModelAvailableAtInquiryTimeHistogram,
                                 kSessionCreationSuccessHistogram});
}

TEST_F(PermissionsAiv1HandlerTest, StartsModelDownload) {
  ExpectGetOnDeviceModelEligibilityAndReturn(
      EligibilityReason::kNoOnDeviceFeatureUsed);
  // First StartSession call triggers model download;
  ExpectStartSessionAndReturn(nullptr);
  ModelCallbackFuture future;
  permissions_aiv1_handler_->InquireAiOnDeviceModel(
      kRenderedText, RequestType::kNotifications, future.GetCallback());
  EXPECT_EQ(future.Take(), std::nullopt);

  // Timer should not get started if no valid session is returned.
  ASSERT_FALSE(mock_execution_timer_->IsRunning());
  ASSERT_EQ(mock_execution_timer_->started_cnt, 0);

  histogram_tester().ExpectUniqueSample(kModelAvailableAtInquiryTimeHistogram,
                                        false, 1);
  histogram_tester().ExpectUniqueSample(kSessionCreationSuccessHistogram, false,
                                        1);
  CheckHistogramsAreEmptyExcept(histogram_tester(),
                                {kModelAvailableAtInquiryTimeHistogram,
                                 kSessionCreationSuccessHistogram});
}

TEST_P(ParametrizedPermissionsAiv1HandlerTest,
       SendsValidRequestAndReceivesValidResponse) {
  ExpectGetOnDeviceModelEligibilityAndReturn(EligibilityReason::kSuccess);
  ExpectStartSessionAndReturn(&session_);

  PermissionsAiRequest request = BuildRequest(GetParam().permission_type);
  ModelExecutionResultStreamingCallback response_callback;
  EXPECT_CALL(session_, ExecuteModel(EqualsProto(request), _))
      .WillOnce(WithArg<1>(
          Invoke([&](ModelExecutionResultStreamingCallback callback) {
            response_callback = std::move(callback);
          })));

  ModelCallbackFuture future;
  permissions_aiv1_handler_->InquireAiOnDeviceModel(
      kRenderedText, GetParam().request_type, future.GetCallback());

  ASSERT_TRUE(mock_execution_timer_->IsRunning());

  ReceiveResponseSuccessfully(response_callback, future);

  ASSERT_FALSE(mock_execution_timer_->IsRunning());
  ASSERT_EQ(mock_execution_timer_->started_cnt, 1);

  histogram_tester().ExpectUniqueSample(kModelAvailableAtInquiryTimeHistogram,
                                        true, 1);
  histogram_tester().ExpectUniqueSample(kSessionCreationSuccessHistogram, true,
                                        1);
  histogram_tester().ExpectUniqueSample(kResponseParseSuccessHistogram, true,
                                        1);
  histogram_tester().ExpectUniqueSample(kPreviousSessionFinishedInTimeHistogram,
                                        true, 1);
  histogram_tester().ExpectUniqueSample(kExecutionSuccessHistogram, true, 1);
  histogram_tester().ExpectUniqueSample(kExecutionTimedOutHistogram, false, 1);
  histogram_tester().ExpectTotalCount(kExecutionDurationHistogram, 1);
  CheckHistogramsAreEmptyExcept(
      histogram_tester(),
      {kModelAvailableAtInquiryTimeHistogram, kSessionCreationSuccessHistogram,
       kResponseParseSuccessHistogram, kPreviousSessionFinishedInTimeHistogram,
       kExecutionSuccessHistogram, kExecutionDurationHistogram,
       kExecutionTimedOutHistogram});
}

TEST_F(PermissionsAiv1HandlerTest, IncompleResponseIsIgnored) {
  ExpectGetOnDeviceModelEligibilityAndReturn(EligibilityReason::kSuccess);
  ExpectStartSessionAndReturn(&session_);

  PermissionsAiResponse response;
  response.set_is_permission_relevant(true);
  StreamingResponse streaming_response{.response = AnyWrapProto(response),
                                       .is_complete = false};

  PermissionsAiRequest request =
      BuildRequest(PermissionType::PERMISSION_TYPE_NOTIFICATIONS);
  ModelExecutionResultStreamingCallback response_callback;
  EXPECT_CALL(session_, ExecuteModel(EqualsProto(request), _))
      .WillOnce(WithArg<1>(
          Invoke([&](ModelExecutionResultStreamingCallback callback) {
            response_callback = std::move(callback);
          })));

  int call_count = 0;
  permissions_aiv1_handler_->InquireAiOnDeviceModel(
      kRenderedText, RequestType::kNotifications,
      BindOnce(&CallCounter, std::ref(call_count)));

  EXPECT_FALSE(response_callback.is_null());
  response_callback.Run(OptimizationGuideModelStreamingExecutionResult(
      base::ok(streaming_response),
      /*provided_by_on_device=*/true));
  // For incomplete results we do not want to answer the callback but rather
  // wait some more.
  EXPECT_EQ(call_count, 0);

  // Timer should still run since we still wait for a completed result.
  ASSERT_TRUE(mock_execution_timer_->IsRunning());

  histogram_tester().ExpectUniqueSample(kModelAvailableAtInquiryTimeHistogram,
                                        true, 1);
  histogram_tester().ExpectUniqueSample(kSessionCreationSuccessHistogram, true,
                                        1);
  histogram_tester().ExpectUniqueSample(kPreviousSessionFinishedInTimeHistogram,
                                        true, 1);

  CheckHistogramsAreEmptyExcept(
      histogram_tester(),
      {kModelAvailableAtInquiryTimeHistogram, kSessionCreationSuccessHistogram,
       kPreviousSessionFinishedInTimeHistogram});
  ResetHistograms();

  // We can follow up with a complete response afterwards.
  streaming_response.is_complete = true;
  response_callback.Run(OptimizationGuideModelStreamingExecutionResult(
      base::ok(streaming_response),
      /*provided_by_on_device=*/true));
  EXPECT_EQ(call_count, 1);
  ASSERT_FALSE(mock_execution_timer_->IsRunning());
  ASSERT_EQ(mock_execution_timer_->started_cnt, 1);

  histogram_tester().ExpectTotalCount(kExecutionDurationHistogram, 1);
  histogram_tester().ExpectUniqueSample(kExecutionTimedOutHistogram, false, 1);
  histogram_tester().ExpectUniqueSample(kResponseParseSuccessHistogram, true,
                                        1);
  histogram_tester().ExpectUniqueSample(kExecutionSuccessHistogram, true, 1);
  CheckHistogramsAreEmptyExcept(
      histogram_tester(),
      {kExecutionDurationHistogram, kExecutionTimedOutHistogram,
       kResponseParseSuccessHistogram, kExecutionSuccessHistogram});
}

TEST_F(PermissionsAiv1HandlerTest,
       OldSessionGetsNotInterruptedByNewInquiryRequest) {
  ExpectGetOnDeviceModelEligibilityAndReturn(EligibilityReason::kSuccess);
  ExpectStartSessionAndReturn(&session_);

  PermissionsAiRequest request =
      BuildRequest(PermissionType::PERMISSION_TYPE_NOTIFICATIONS);
  ModelExecutionResultStreamingCallback response_callback;
  EXPECT_CALL(session_, ExecuteModel(EqualsProto(request), _))
      .WillOnce(WithArg<1>(
          Invoke([&](ModelExecutionResultStreamingCallback callback) {
            response_callback = std::move(callback);
          })));

  ModelCallbackFuture future_first_request;
  permissions_aiv1_handler_->InquireAiOnDeviceModel(
      kRenderedText, RequestType::kNotifications,
      future_first_request.GetCallback());
  ASSERT_TRUE(mock_execution_timer_->IsRunning());

  // We already test the histograms up to this point above.
  ResetHistograms();
  ExpectGetOnDeviceModelEligibilityAndReturn(EligibilityReason::kSuccess);

  ModelCallbackFuture future_second_request;
  permissions_aiv1_handler_->InquireAiOnDeviceModel(
      kRenderedText, RequestType::kNotifications,
      future_second_request.GetCallback());
  EXPECT_EQ(future_second_request.Take(), std::nullopt);

  histogram_tester().ExpectUniqueSample(kModelAvailableAtInquiryTimeHistogram,
                                        true, 1);
  histogram_tester().ExpectUniqueSample(kPreviousSessionFinishedInTimeHistogram,
                                        false, 1);
  CheckHistogramsAreEmptyExcept(histogram_tester(),
                                {kModelAvailableAtInquiryTimeHistogram,
                                 kPreviousSessionFinishedInTimeHistogram});
  ResetHistograms();

  // Rejecting an inquiry if the old one is still running should not
  // affect the execution timer.
  ASSERT_TRUE(mock_execution_timer_->IsRunning());
  ASSERT_EQ(mock_execution_timer_->started_cnt, 1);

  // We can finish the first inquiry normally.
  ReceiveResponseSuccessfully(response_callback, future_first_request);

  histogram_tester().ExpectTotalCount(kExecutionDurationHistogram, 1);
  histogram_tester().ExpectUniqueSample(kExecutionTimedOutHistogram, false, 1);
  histogram_tester().ExpectUniqueSample(kExecutionSuccessHistogram, true, 1);
  histogram_tester().ExpectUniqueSample(kResponseParseSuccessHistogram, true,
                                        1);
  CheckHistogramsAreEmptyExcept(histogram_tester(),
                                {
                                    kExecutionDurationHistogram,
                                    kExecutionTimedOutHistogram,
                                    kExecutionSuccessHistogram,
                                    kResponseParseSuccessHistogram,
                                });
}

TEST_F(PermissionsAiv1HandlerTest, OldSessionGetsCancelledByTimer) {
  ExpectGetOnDeviceModelEligibilityAndReturn(EligibilityReason::kSuccess);
  ExpectStartSessionAndReturn(&session_);
  EXPECT_CALL(session_, ExecuteModel(_, _));

  ModelCallbackFuture future_first_request;
  permissions_aiv1_handler_->InquireAiOnDeviceModel(
      kRenderedText, RequestType::kNotifications,
      future_first_request.GetCallback());
  ASSERT_TRUE(mock_execution_timer_->IsRunning());
  ResetHistograms();

  // Firing the timer should result in the current model execution being
  // cancelled.
  mock_execution_timer_->Fire();
  EXPECT_EQ(future_first_request.Take(), std::nullopt);
  ASSERT_FALSE(mock_execution_timer_->IsRunning());

  histogram_tester().ExpectUniqueSample(kExecutionTimedOutHistogram, true, 1);
  CheckHistogramsAreEmptyExcept(histogram_tester(),
                                {
                                    kExecutionTimedOutHistogram,
                                });
  ResetHistograms();

  ExpectGetOnDeviceModelEligibilityAndReturn(EligibilityReason::kSuccess);
  ExpectStartSessionAndReturn(&session_);
  ModelExecutionResultStreamingCallback response_callback;
  EXPECT_CALL(session_, ExecuteModel(_, _))
      .WillOnce(WithArg<1>(Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) { response_callback = std::move(callback); })));

  // Second request will not get rejected and starts a new timer interval.
  ModelCallbackFuture future_second_request;
  permissions_aiv1_handler_->InquireAiOnDeviceModel(
      kRenderedText, RequestType::kNotifications,
      future_second_request.GetCallback());
  ASSERT_TRUE(mock_execution_timer_->IsRunning());

  ResetHistograms();
  ReceiveResponseSuccessfully(response_callback, future_second_request);
  ASSERT_FALSE(mock_execution_timer_->IsRunning());

  histogram_tester().ExpectUniqueSample(kExecutionTimedOutHistogram, false, 1);
  histogram_tester().ExpectTotalCount(kExecutionDurationHistogram, 1);
  histogram_tester().ExpectUniqueSample(kExecutionSuccessHistogram, true, 1);
  histogram_tester().ExpectUniqueSample(kResponseParseSuccessHistogram, true,
                                        1);
  CheckHistogramsAreEmptyExcept(
      histogram_tester(),
      {kExecutionTimedOutHistogram, kExecutionDurationHistogram,
       kExecutionSuccessHistogram, kResponseParseSuccessHistogram});
}

TEST_F(PermissionsAiv1HandlerTest, IncompleteResultsDoNotCancelTimer) {
  ExpectGetOnDeviceModelEligibilityAndReturn(EligibilityReason::kSuccess);
  ExpectStartSessionAndReturn(&session_);

  ModelExecutionResultStreamingCallback response_callback;
  EXPECT_CALL(session_, ExecuteModel(_, _))
      .WillOnce(WithArg<1>(Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) { response_callback = std::move(callback); })));

  // Timer gets started for inquiry request.
  ModelCallbackFuture future;
  permissions_aiv1_handler_->InquireAiOnDeviceModel(
      kRenderedText, RequestType::kNotifications, future.GetCallback());
  ASSERT_TRUE(mock_execution_timer_->IsRunning());

  PermissionsAiResponse response;
  response.set_is_permission_relevant(true);
  auto streaming_response = StreamingResponse{
      .response = AnyWrapProto(response), .is_complete = false};

  // Returning an incomplete response should not affect the timer nor the
  // timeout histogram.
  response_callback.Run(OptimizationGuideModelStreamingExecutionResult(
      base::ok(streaming_response),
      /*provided_by_on_device=*/true));
  ASSERT_TRUE(mock_execution_timer_->IsRunning());
  histogram_tester().ExpectTotalCount(kExecutionTimedOutHistogram, 0);

  // A completed streaming response will get accepted and finally cancels the
  // timer.
  streaming_response.is_complete = true;
  response_callback.Run(OptimizationGuideModelStreamingExecutionResult(
      base::ok(streaming_response),
      /*provided_by_on_device=*/true));
  EXPECT_THAT(future.Take(), Optional(EqualsProto(response)));
  ASSERT_FALSE(mock_execution_timer_->IsRunning());

  histogram_tester().ExpectUniqueSample(kExecutionTimedOutHistogram, false, 1);
}

}  // namespace permissions
