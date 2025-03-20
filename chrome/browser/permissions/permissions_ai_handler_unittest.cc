// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permissions_ai_handler.h"

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
using ::base::OwnedRef;
using ::base::test::EqualsProto;
using ::content::BrowserThread;
using ::optimization_guide::AnyWrapProto;
using ::optimization_guide::MockSession;
using ::optimization_guide::OptimizationGuideModelExecutionError;
using ::optimization_guide::OptimizationGuideModelStreamingExecutionResult;
using ::optimization_guide::proto::PermissionsAiRequest;
using ::optimization_guide::proto::PermissionsAiResponse;
using ::optimization_guide::proto::PermissionType;
using ::permissions::RequestType;
using ::testing::_;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Optional;
using ::testing::StrictMock;
using ::testing::WithArg;
using ModelExecutionResultStreamingCallback = ::optimization_guide::
    OptimizationGuideModelExecutionResultStreamingCallback;
using ModelCallbackFuture =
    ::base::test::TestFuture<std::optional<PermissionsAiResponse>>;

constexpr optimization_guide::ModelBasedCapabilityKey kFeatureKey =
    optimization_guide::ModelBasedCapabilityKey::kPermissionsAi;

constexpr char kRenderedText[] = "rendered_text";
constexpr char kSessionCreationSuccessHistogram[] =
    "Permissions.AIv1.SessionCreationSuccess";
constexpr char kModelAvailableAtInquiryTimeHistogram[] =
    "Permissions.AIv1.AvailableAtInquiryTime";
constexpr char kModelDownloadSuccessHistogram[] =
    "Permissions.AIv1.DownloadSuccess";
constexpr char kModelDownloadTimeHistogram[] = "Permissions.AIv1.DownloadTime";
constexpr char kSessionCreationTimeHistogram[] =
    "Permissions.AIv1.SessionCreationTime";
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

class PermissionsAiHandlerTestBase : public testing::Test {
 protected:
  PermissionsAiHandlerTestBase()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    CHECK(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test-user");
  }

  void SetUp() override {
    SetupMockOptimizationGuideKeyedService();

    auto mock_execution_timer = std::make_unique<MockTimer>();
    mock_execution_timer_ = mock_execution_timer.get();
    permissions_ai_handler_ = std::make_unique<PermissionsAiHandler>(
        mock_optimization_guide_keyed_service_.get());
    permissions_ai_handler_->set_execution_timer_for_testing(
        std::move(mock_execution_timer));
  }

  void TearDown() override { mock_execution_timer_ = nullptr; }

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

  void ExpectSkipModelDownloadAndInstantlyReturnSession() {
    EXPECT_CALL(*mock_optimization_guide_keyed_service_,
                AddOnDeviceModelAvailabilityChangeObserver(_, _));
    ExpectStartSessionAndReturn(&session_);
  }

  void ResetHistograms() {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void DeletePermissionsAiHandler() {
    // Avoid dangling pointer warning
    mock_execution_timer_ = nullptr;
    permissions_ai_handler_.reset();
  }

  const base::HistogramTester& histogram_tester() { return *histogram_tester_; }

  std::unique_ptr<base::HistogramTester> histogram_tester_ =
      std::make_unique<base::HistogramTester>();
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;

  raw_ptr<MockTimer> mock_execution_timer_ = nullptr;
  std::unique_ptr<PermissionsAiHandler> permissions_ai_handler_;
  raw_ptr<MockOptimizationGuideKeyedService>
      mock_optimization_guide_keyed_service_;
  NiceMock<MockSession> session_;
};

class PermissionsAiHandlerTest : public PermissionsAiHandlerTestBase {};

struct PermissionsAiHandlerTestCase {
  PermissionType permission_type;
  RequestType request_type;
  bool is_permission_relevant;
};

class ParametrizedPermissionsAiHandlerTest
    : public PermissionsAiHandlerTestBase,
      public testing::WithParamInterface<PermissionsAiHandlerTestCase> {};

INSTANTIATE_TEST_SUITE_P(
    RequestTypes,
    ParametrizedPermissionsAiHandlerTest,
    testing::ValuesIn<PermissionsAiHandlerTestCase>({
        {PermissionType::PERMISSION_TYPE_NOTIFICATIONS,
         RequestType::kNotifications, /*is_permission_relevant=*/true},
        {PermissionType::PERMISSION_TYPE_GEOLOCATION, RequestType::kGeolocation,
         /*is_permission_relevant=*/false},
    }));

TEST_P(ParametrizedPermissionsAiHandlerTest,
       CanDealWithInvalidOptimizationGuide) {
  PermissionsAiHandler permissions_ai_handler(nullptr);

  ModelCallbackFuture future;
  permissions_ai_handler.InquireAiOnDeviceModel(
      kRenderedText, GetParam().request_type, future.GetCallback());
  EXPECT_EQ(future.Take(), std::nullopt);

  histogram_tester().ExpectUniqueSample(kSessionCreationSuccessHistogram, false,
                                        1);
  histogram_tester().ExpectUniqueSample(kModelAvailableAtInquiryTimeHistogram,
                                        false, 1);
}

TEST_F(PermissionsAiHandlerTest, ModelNeedsDownloadForFirstInquiry) {
  EXPECT_FALSE(permissions_ai_handler_->IsOnDeviceModelAvailable());

  // Installs model on first call.
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              AddOnDeviceModelAvailabilityChangeObserver(
                  kFeatureKey, permissions_ai_handler_.get()));
  ExpectStartSessionAndReturn(nullptr);

  ModelCallbackFuture future;
  permissions_ai_handler_->InquireAiOnDeviceModel(
      kRenderedText, RequestType::kNotifications, future.GetCallback());
  EXPECT_EQ(future.Take(), std::nullopt);
  EXPECT_FALSE(permissions_ai_handler_->IsOnDeviceModelAvailable());

  // Timer should not get started if no valid session is returned.
  ASSERT_FALSE(mock_execution_timer_->IsRunning());

  // Fails for first created session (because model is not downloaded yet).
  histogram_tester().ExpectUniqueSample(kSessionCreationSuccessHistogram, false,
                                        1);
  histogram_tester().ExpectUniqueSample(kModelAvailableAtInquiryTimeHistogram,
                                        false, 1);
}

TEST_F(PermissionsAiHandlerTest, ModelDownloadFailureIsHandled) {
  ExpectStartSessionAndReturn(nullptr);

  optimization_guide::OnDeviceModelAvailabilityObserver* availability_observer =
      nullptr;
  base::RunLoop run_loop_for_add_observer;
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              AddOnDeviceModelAvailabilityChangeObserver(_, _))
      .WillOnce(Invoke(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              optimization_guide::OnDeviceModelAvailabilityObserver* observer) {
            availability_observer = observer;
            run_loop_for_add_observer.Quit();
          }));

  ModelCallbackFuture future;
  permissions_ai_handler_->InquireAiOnDeviceModel(
      kRenderedText, RequestType::kNotifications, future.GetCallback());
  EXPECT_EQ(future.Take(), std::nullopt);
  // Timer should not get started if no model is available.
  ASSERT_FALSE(mock_execution_timer_->IsRunning());

  // Fails for first created session (because model is not downloaded yet).
  histogram_tester().ExpectUniqueSample(kSessionCreationSuccessHistogram, false,
                                        1);
  histogram_tester().ExpectUniqueSample(kModelAvailableAtInquiryTimeHistogram,
                                        false, 1);
  histogram_tester().ExpectTotalCount(kModelDownloadSuccessHistogram, 0);

  run_loop_for_add_observer.Run();
  ASSERT_TRUE(availability_observer);

  // Now that the handler is observing, send `kTooManyRecentCrashes`
  // to the observer, which is not a waitable reason.
  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey::kPermissionsAi,
      optimization_guide::OnDeviceModelEligibilityReason::
          kTooManyRecentCrashes);

  histogram_tester().ExpectUniqueSample(kModelDownloadSuccessHistogram, false,
                                        1);
}

TEST_F(PermissionsAiHandlerTest, ModelDownloadsSuccessfully) {
  ExpectStartSessionAndReturn(nullptr);

  optimization_guide::OnDeviceModelAvailabilityObserver* availability_observer =
      nullptr;
  base::RunLoop run_loop_for_add_observer;
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              AddOnDeviceModelAvailabilityChangeObserver(_, _))
      .WillOnce(Invoke(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              optimization_guide::OnDeviceModelAvailabilityObserver* observer) {
            availability_observer = observer;
            run_loop_for_add_observer.Quit();
          }));

  ModelCallbackFuture future;
  permissions_ai_handler_->InquireAiOnDeviceModel(
      kRenderedText, RequestType::kNotifications, future.GetCallback());
  EXPECT_EQ(future.Take(), std::nullopt);
  // Timer should not get started if no valid session is returned.
  ASSERT_FALSE(mock_execution_timer_->IsRunning());

  // Fails for first created session (because model is not downloaded yet):
  histogram_tester().ExpectUniqueSample(kSessionCreationSuccessHistogram, false,
                                        1);
  histogram_tester().ExpectUniqueSample(kModelAvailableAtInquiryTimeHistogram,
                                        false, 1);

  run_loop_for_add_observer.Run();
  ASSERT_TRUE(availability_observer);

  // Waitable reason, should not do anything.
  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey::kPermissionsAi,
      optimization_guide::OnDeviceModelEligibilityReason::kModelToBeInstalled);

  histogram_tester().ExpectTotalCount(kModelDownloadSuccessHistogram, 0);

  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey::kPermissionsAi,
      optimization_guide::OnDeviceModelEligibilityReason::kSuccess);

  histogram_tester().ExpectUniqueSample(kModelDownloadSuccessHistogram, true,
                                        1);
  histogram_tester().ExpectTotalCount(kModelDownloadTimeHistogram, 1);

  EXPECT_TRUE(permissions_ai_handler_->IsOnDeviceModelAvailable());
}

TEST_P(ParametrizedPermissionsAiHandlerTest, RequestIsBuildProperly) {
  ExpectSkipModelDownloadAndInstantlyReturnSession();

  PermissionsAiRequest request;
  request.set_rendered_text(kRenderedText);
  request.set_permission_type(GetParam().permission_type);

  optimization_guide::proto::DefaultResponse default_response;
  optimization_guide::StreamingResponse default_streaming_response{
      .response = AnyWrapProto(default_response), .is_complete = true};
  EXPECT_CALL(session_, ExecuteModel(EqualsProto(request), _))
      .WillOnce(WithArg<1>(
          Invoke([&](optimization_guide::
                         OptimizationGuideModelExecutionResultStreamingCallback
                             callback) {
            callback.Run(OptimizationGuideModelStreamingExecutionResult(
                base::ok(default_streaming_response),
                /*provided_by_on_device=*/true));
          })));

  ModelCallbackFuture future;
  permissions_ai_handler_->InquireAiOnDeviceModel(
      kRenderedText, GetParam().request_type, future.GetCallback());
  EXPECT_EQ(future.Take(), std::nullopt);
  // Timer should not get started since the callback gets called immediately
  // when we started the model execution.
  ASSERT_FALSE(mock_execution_timer_->IsRunning());
}

TEST_F(PermissionsAiHandlerTest, EmptyModelResponseIsHandled) {
  ExpectSkipModelDownloadAndInstantlyReturnSession();

  optimization_guide::proto::DefaultResponse default_response;
  optimization_guide::StreamingResponse default_streaming_response{
      .response = AnyWrapProto(default_response), .is_complete = true};
  EXPECT_CALL(session_, ExecuteModel(_, _))
      .WillOnce(WithArg<1>(
          Invoke([&](optimization_guide::
                         OptimizationGuideModelExecutionResultStreamingCallback
                             callback) {
            callback.Run(OptimizationGuideModelStreamingExecutionResult(
                base::ok(default_streaming_response),
                /*provided_by_on_device=*/true));
          })));

  ModelCallbackFuture future;
  permissions_ai_handler_->InquireAiOnDeviceModel(
      kRenderedText, RequestType::kNotifications, future.GetCallback());
  EXPECT_EQ(future.Take(), std::nullopt);
  EXPECT_TRUE(permissions_ai_handler_->IsOnDeviceModelAvailable());
  // Timer should not get started since the callback gets called immediately
  // we started the model execution.
  ASSERT_FALSE(mock_execution_timer_->IsRunning());

  histogram_tester().ExpectUniqueSample(kModelAvailableAtInquiryTimeHistogram,
                                        true, 1);
  histogram_tester().ExpectUniqueSample(kSessionCreationSuccessHistogram, true,
                                        1);
  histogram_tester().ExpectTotalCount(kSessionCreationTimeHistogram, 1);
  histogram_tester().ExpectTotalCount(kExecutionDurationHistogram, 1);
  histogram_tester().ExpectUniqueSample(kExecutionSuccessHistogram, true, 1);
  histogram_tester().ExpectUniqueSample(kResponseParseSuccessHistogram, false,
                                        1);
  histogram_tester().ExpectUniqueSample(kExecutionTimedOutHistogram, false, 1);
}

TEST_F(PermissionsAiHandlerTest, IncompleResponseIsIgnored) {
  ExpectSkipModelDownloadAndInstantlyReturnSession();

  optimization_guide::proto::DefaultResponse default_response;
  optimization_guide::StreamingResponse default_streaming_response{
      .response = AnyWrapProto(default_response), .is_complete = false};

  EXPECT_CALL(session_, ExecuteModel(_, _))
      .WillOnce(WithArg<1>(
          Invoke([&](optimization_guide::
                         OptimizationGuideModelExecutionResultStreamingCallback
                             callback) {
            callback.Run(OptimizationGuideModelStreamingExecutionResult(
                base::ok(default_streaming_response),
                /*provided_by_on_device=*/true));
          })));

  int call_count = 0;
  permissions_ai_handler_->InquireAiOnDeviceModel(
      kRenderedText, RequestType::kNotifications,
      BindOnce(&CallCounter, OwnedRef(call_count)));

  EXPECT_EQ(call_count, 0);
  // Timer should still run since we still wait for a completed result.
  ASSERT_TRUE(mock_execution_timer_->IsRunning());

  histogram_tester().ExpectUniqueSample(kModelAvailableAtInquiryTimeHistogram,
                                        true, 1);
  histogram_tester().ExpectUniqueSample(kSessionCreationSuccessHistogram, true,
                                        1);
  histogram_tester().ExpectTotalCount(kSessionCreationTimeHistogram, 1);
  histogram_tester().ExpectTotalCount(kExecutionDurationHistogram, 0);
  histogram_tester().ExpectTotalCount(kExecutionSuccessHistogram, 0);
  histogram_tester().ExpectTotalCount(kResponseParseSuccessHistogram, 0);
}

TEST_P(ParametrizedPermissionsAiHandlerTest,
       ResponseIsParsedCorrectlyAndCallbackCalledWithResult) {
  ExpectSkipModelDownloadAndInstantlyReturnSession();

  PermissionsAiResponse response;
  response.set_is_permission_relevant(GetParam().is_permission_relevant);
  auto streaming_response = optimization_guide::StreamingResponse{
      .response = AnyWrapProto(response), .is_complete = true};

  EXPECT_CALL(session_, ExecuteModel(_, _))
      .WillOnce(WithArg<1>(
          Invoke([&](optimization_guide::
                         OptimizationGuideModelExecutionResultStreamingCallback
                             callback) {
            callback.Run(OptimizationGuideModelStreamingExecutionResult(
                base::ok(streaming_response),
                /*provided_by_on_device=*/true));
          })));

  ModelCallbackFuture future;
  permissions_ai_handler_->InquireAiOnDeviceModel(
      kRenderedText, RequestType::kNotifications, future.GetCallback());
  EXPECT_THAT(future.Take(), Optional(EqualsProto(response)));
  // Timer should not run since we immediately returned a result when
  // ExecuteModel is called.
  ASSERT_FALSE(mock_execution_timer_->IsRunning());

  histogram_tester().ExpectUniqueSample(kModelAvailableAtInquiryTimeHistogram,
                                        true, 1);
  histogram_tester().ExpectUniqueSample(kSessionCreationSuccessHistogram, true,
                                        1);
  histogram_tester().ExpectTotalCount(kSessionCreationTimeHistogram, 1);
  histogram_tester().ExpectTotalCount(kExecutionDurationHistogram, 1);
  histogram_tester().ExpectTotalCount(kExecutionSuccessHistogram, 1);
  histogram_tester().ExpectTotalCount(kResponseParseSuccessHistogram, 1);
}

TEST_F(PermissionsAiHandlerTest, FailForNewSessionIfOldIsStillRunning) {
  ExpectSkipModelDownloadAndInstantlyReturnSession();

  EXPECT_CALL(session_, ExecuteModel(_, _));

  int call_count = 0;
  permissions_ai_handler_->InquireAiOnDeviceModel(
      kRenderedText, RequestType::kNotifications,
      BindOnce(&CallCounter, OwnedRef(call_count)));
  EXPECT_EQ(call_count, 0);

  ASSERT_TRUE(mock_execution_timer_->IsRunning());

  ModelCallbackFuture future;
  permissions_ai_handler_->InquireAiOnDeviceModel(
      kRenderedText, RequestType::kNotifications, future.GetCallback());
  EXPECT_EQ(future.Take(), std::nullopt);

  // Rejecting an inquiry if the old one is still running should not affect the
  // execution timer.
  ASSERT_TRUE(mock_execution_timer_->IsRunning());
  ASSERT_EQ(mock_execution_timer_->started_cnt, 1);

  histogram_tester().ExpectUniqueSample(kModelAvailableAtInquiryTimeHistogram,
                                        true, 1);
  histogram_tester().ExpectUniqueSample(kSessionCreationSuccessHistogram, true,
                                        1);
  histogram_tester().ExpectUniqueSample(kPreviousSessionFinishedInTimeHistogram,
                                        false, 1);
  histogram_tester().ExpectTotalCount(kSessionCreationTimeHistogram, 1);
  histogram_tester().ExpectTotalCount(kExecutionDurationHistogram, 0);
  histogram_tester().ExpectTotalCount(kExecutionSuccessHistogram, 0);
  histogram_tester().ExpectTotalCount(kResponseParseSuccessHistogram, 0);
}

TEST_F(PermissionsAiHandlerTest,
       OldSessionGetsNotInterruptedByNewInquiryRequest) {
  ExpectSkipModelDownloadAndInstantlyReturnSession();

  ModelExecutionResultStreamingCallback internal_callback;
  EXPECT_CALL(session_, ExecuteModel(_, _))
      .WillOnce(WithArg<1>(Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) { internal_callback = std::move(callback); })));

  ModelCallbackFuture future_first_request;
  permissions_ai_handler_->InquireAiOnDeviceModel(
      kRenderedText, RequestType::kNotifications,
      future_first_request.GetCallback());

  // First model inquiry still running, so the second request should return
  // immediately with no result.
  ModelCallbackFuture future_second_request;
  permissions_ai_handler_->InquireAiOnDeviceModel(
      kRenderedText, RequestType::kNotifications,
      future_second_request.GetCallback());
  EXPECT_EQ(future_second_request.Take(), std::nullopt);

  ResetHistograms();
  PermissionsAiResponse response;
  response.set_is_permission_relevant(true);
  auto streaming_response = optimization_guide::StreamingResponse{
      .response = AnyWrapProto(response), .is_complete = true};
  internal_callback.Run(OptimizationGuideModelStreamingExecutionResult(
      base::ok(streaming_response),
      /*provided_by_on_device=*/true));
  EXPECT_THAT(future_first_request.Take(), Optional(EqualsProto(response)));

  histogram_tester().ExpectTotalCount(kExecutionDurationHistogram, 1);
  histogram_tester().ExpectUniqueSample(kExecutionSuccessHistogram, true, 1);
  histogram_tester().ExpectUniqueSample(kResponseParseSuccessHistogram, true,
                                        1);
}

TEST_F(PermissionsAiHandlerTest, ModelHandlerUnsubscribesSuccessfully) {
  // Prevent regressions to behavior as the one fixed in crbug.com/399860232.
  ExpectStartSessionAndReturn(nullptr);

  optimization_guide::OnDeviceModelAvailabilityObserver* availability_observer =
      nullptr;
  base::RunLoop run_loop_for_add_observer;
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              AddOnDeviceModelAvailabilityChangeObserver(_, _))
      .WillOnce(Invoke(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              optimization_guide::OnDeviceModelAvailabilityObserver* observer) {
            availability_observer = observer;
            run_loop_for_add_observer.Quit();
          }));

  ModelCallbackFuture future;
  permissions_ai_handler_->InquireAiOnDeviceModel(
      kRenderedText, RequestType::kNotifications, future.GetCallback());
  EXPECT_EQ(future.Take(), std::nullopt);

  run_loop_for_add_observer.Run();
  ASSERT_TRUE(availability_observer);

  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey::kPermissionsAi,
      optimization_guide::OnDeviceModelEligibilityReason::kSuccess);

  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              RemoveOnDeviceModelAvailabilityChangeObserver(_, _));

  // Destructor shall trigger unsubscription.
  DeletePermissionsAiHandler();
}

TEST_F(PermissionsAiHandlerTest, OldSessionGetsCancelledByTimer) {
  ExpectSkipModelDownloadAndInstantlyReturnSession();
  EXPECT_CALL(session_, ExecuteModel(_, _));

  ModelCallbackFuture future_first_request;
  permissions_ai_handler_->InquireAiOnDeviceModel(
      kRenderedText, RequestType::kNotifications,
      future_first_request.GetCallback());
  ASSERT_TRUE(mock_execution_timer_->IsRunning());

  // Firing the timer should result in the current model execution being
  // cancelled.
  mock_execution_timer_->Fire();
  EXPECT_EQ(future_first_request.Take(), std::nullopt);
  ASSERT_FALSE(mock_execution_timer_->IsRunning());

  histogram_tester().ExpectUniqueSample(kExecutionTimedOutHistogram, true, 1);
  histogram_tester().ExpectTotalCount(kExecutionDurationHistogram, 0);
  histogram_tester().ExpectTotalCount(kExecutionSuccessHistogram, 0);
  histogram_tester().ExpectTotalCount(kResponseParseSuccessHistogram, 0);

  ResetHistograms();
  ExpectStartSessionAndReturn(&session_);
  ModelExecutionResultStreamingCallback internal_callback;
  EXPECT_CALL(session_, ExecuteModel(_, _))
      .WillOnce(WithArg<1>(Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) { internal_callback = std::move(callback); })));

  // Second request will not get rejected and starts a new timer interval.
  ModelCallbackFuture future_second_request;
  permissions_ai_handler_->InquireAiOnDeviceModel(
      kRenderedText, RequestType::kNotifications,
      future_second_request.GetCallback());
  ASSERT_TRUE(mock_execution_timer_->IsRunning());

  PermissionsAiResponse response;
  response.set_is_permission_relevant(true);
  auto streaming_response = optimization_guide::StreamingResponse{
      .response = AnyWrapProto(response), .is_complete = true};

  // Finishing the request also cancels the timer.
  internal_callback.Run(OptimizationGuideModelStreamingExecutionResult(
      base::ok(streaming_response),
      /*provided_by_on_device=*/true));
  EXPECT_THAT(future_second_request.Take(), Optional(EqualsProto(response)));
  ASSERT_FALSE(mock_execution_timer_->IsRunning());

  histogram_tester().ExpectUniqueSample(kExecutionTimedOutHistogram, false, 1);
  histogram_tester().ExpectTotalCount(kExecutionDurationHistogram, 1);
  histogram_tester().ExpectUniqueSample(kExecutionSuccessHistogram, true, 1);
  histogram_tester().ExpectUniqueSample(kResponseParseSuccessHistogram, true,
                                        1);
}

TEST_F(PermissionsAiHandlerTest, IncompleteResultsDoNotCancelTimer) {
  ExpectSkipModelDownloadAndInstantlyReturnSession();

  ModelExecutionResultStreamingCallback internal_callback;
  EXPECT_CALL(session_, ExecuteModel(_, _))
      .WillOnce(WithArg<1>(Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) { internal_callback = std::move(callback); })));

  // Timer gets started for inquiry request.
  ModelCallbackFuture future;
  permissions_ai_handler_->InquireAiOnDeviceModel(
      kRenderedText, RequestType::kNotifications, future.GetCallback());
  ASSERT_TRUE(mock_execution_timer_->IsRunning());

  PermissionsAiResponse response;
  response.set_is_permission_relevant(true);
  auto streaming_response = optimization_guide::StreamingResponse{
      .response = AnyWrapProto(response), .is_complete = false};

  // Returning an incomplete response should not affect the timer nor the
  // timeout histogram.
  internal_callback.Run(OptimizationGuideModelStreamingExecutionResult(
      base::ok(streaming_response),
      /*provided_by_on_device=*/true));
  ASSERT_TRUE(mock_execution_timer_->IsRunning());
  histogram_tester().ExpectTotalCount(kExecutionTimedOutHistogram, 0);

  // A completed streaming response will get accepted and finally cancels the
  // timer.
  streaming_response.is_complete = true;
  internal_callback.Run(OptimizationGuideModelStreamingExecutionResult(
      base::ok(streaming_response),
      /*provided_by_on_device=*/true));
  EXPECT_THAT(future.Take(), Optional(EqualsProto(response)));
  ASSERT_FALSE(mock_execution_timer_->IsRunning());

  histogram_tester().ExpectUniqueSample(kExecutionTimedOutHistogram, false, 1);
}

}  // namespace permissions
