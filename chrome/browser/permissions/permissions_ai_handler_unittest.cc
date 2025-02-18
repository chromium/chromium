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
}  // namespace

namespace permissions {

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
    permissions_ai_handler_ = std::make_unique<PermissionsAiHandler>(
        mock_optimization_guide_keyed_service_.get());
  }

  void SetupMockOptimizationGuideKeyedService() {
    mock_optimization_guide_keyed_service_ =
        static_cast<MockOptimizationGuideKeyedService*>(
            OptimizationGuideKeyedServiceFactory::GetInstance()
                ->SetTestingFactoryAndUse(
                    profile_,
                    base::BindRepeating([](content::BrowserContext* context)
                                            -> std::unique_ptr<KeyedService> {
                      return std::make_unique<testing::NiceMock<
                          MockOptimizationGuideKeyedService>>();
                    })));
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;

  std::unique_ptr<PermissionsAiHandler> permissions_ai_handler_;
  raw_ptr<MockOptimizationGuideKeyedService>
      mock_optimization_guide_keyed_service_;
  testing::NiceMock<MockSession> session_;
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
  base::HistogramTester histogram_tester;
  PermissionsAiHandler permissions_ai_handler(nullptr);

  base::test::TestFuture<std::optional<PermissionsAiResponse>> future;
  permissions_ai_handler.InquireAiOnDeviceModel(
      kRenderedText, GetParam().request_type, future.GetCallback());
  EXPECT_EQ(future.Take(), std::nullopt);

  histogram_tester.ExpectUniqueSample(kSessionCreationSuccessHistogram, false,
                                      1);
  histogram_tester.ExpectUniqueSample(kModelAvailableAtInquiryTimeHistogram,
                                      false, 1);
}

TEST_F(PermissionsAiHandlerTest, ModelNeedsDownloadForFirstInquiry) {
  base::HistogramTester histogram_tester;
  EXPECT_FALSE(permissions_ai_handler_->IsOnDeviceModelAvailable());

  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              StartSession(kFeatureKey, _))
      .WillOnce(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              const std::optional<optimization_guide::SessionConfigParams>&
                  config_params) { return nullptr; });

  // Installs model on first call.
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              AddOnDeviceModelAvailabilityChangeObserver(
                  kFeatureKey, permissions_ai_handler_.get()));
  base::test::TestFuture<std::optional<PermissionsAiResponse>> future;
  permissions_ai_handler_->InquireAiOnDeviceModel(
      kRenderedText, RequestType::kNotifications, future.GetCallback());
  EXPECT_EQ(future.Take(), std::nullopt);
  EXPECT_FALSE(permissions_ai_handler_->IsOnDeviceModelAvailable());

  // Fails for first created session (because model is not downloaded yet).
  histogram_tester.ExpectUniqueSample(kSessionCreationSuccessHistogram, false,
                                      1);
  histogram_tester.ExpectUniqueSample(kModelAvailableAtInquiryTimeHistogram,
                                      false, 1);
}

TEST_F(PermissionsAiHandlerTest, ModelDownloadFailureIsHandled) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
      .WillOnce(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              const std::optional<optimization_guide::SessionConfigParams>&
                  config_params) { return nullptr; });

  optimization_guide::OnDeviceModelAvailabilityObserver* availability_observer =
      nullptr;
  base::RunLoop run_loop_for_add_observer;
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              AddOnDeviceModelAvailabilityChangeObserver(_, _))
      .WillOnce(testing::Invoke(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              optimization_guide::OnDeviceModelAvailabilityObserver* observer) {
            availability_observer = observer;
            run_loop_for_add_observer.Quit();
          }));

  base::test::TestFuture<std::optional<PermissionsAiResponse>> future;
  permissions_ai_handler_->InquireAiOnDeviceModel(
      kRenderedText, RequestType::kNotifications, future.GetCallback());
  EXPECT_EQ(future.Take(), std::nullopt);

  // Fails for first created session (because model is not downloaded yet).
  histogram_tester.ExpectUniqueSample(kSessionCreationSuccessHistogram, false,
                                      1);
  histogram_tester.ExpectUniqueSample(kModelAvailableAtInquiryTimeHistogram,
                                      false, 1);

  run_loop_for_add_observer.Run();
  ASSERT_TRUE(availability_observer);

  // Now that the handler is observing, send `kTooManyRecentCrashes`
  // to the observer, which is not a waitable reason.
  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey::kPermissionsAi,
      optimization_guide::OnDeviceModelEligibilityReason::
          kTooManyRecentCrashes);

  histogram_tester.ExpectUniqueSample(kModelDownloadSuccessHistogram, false, 1);
}

TEST_F(PermissionsAiHandlerTest, ModelDownloadsSuccessfully) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
      .WillOnce(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              const std::optional<optimization_guide::SessionConfigParams>&
                  config_params) { return nullptr; });

  optimization_guide::OnDeviceModelAvailabilityObserver* availability_observer =
      nullptr;
  base::RunLoop run_loop_for_add_observer;
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              AddOnDeviceModelAvailabilityChangeObserver(_, _))
      .WillOnce(testing::Invoke(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              optimization_guide::OnDeviceModelAvailabilityObserver* observer) {
            availability_observer = observer;
            run_loop_for_add_observer.Quit();
          }));

  base::test::TestFuture<std::optional<PermissionsAiResponse>> future;
  permissions_ai_handler_->InquireAiOnDeviceModel(
      kRenderedText, RequestType::kNotifications, future.GetCallback());
  EXPECT_EQ(future.Take(), std::nullopt);

  // Fails for first created session (because model is not downloaded yet):
  histogram_tester.ExpectUniqueSample(kSessionCreationSuccessHistogram, false,
                                      1);
  histogram_tester.ExpectUniqueSample(kModelAvailableAtInquiryTimeHistogram,
                                      false, 1);

  run_loop_for_add_observer.Run();
  ASSERT_TRUE(availability_observer);

  // Waitable reason, should not do anything.
  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey::kPermissionsAi,
      optimization_guide::OnDeviceModelEligibilityReason::kModelToBeInstalled);

  histogram_tester.ExpectTotalCount(kModelDownloadSuccessHistogram, 0);

  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey::kPermissionsAi,
      optimization_guide::OnDeviceModelEligibilityReason::kSuccess);

  histogram_tester.ExpectUniqueSample(kModelDownloadSuccessHistogram, true, 1);
  histogram_tester.ExpectTotalCount(kModelDownloadTimeHistogram, 1);

  EXPECT_TRUE(permissions_ai_handler_->IsOnDeviceModelAvailable());
}

TEST_P(ParametrizedPermissionsAiHandlerTest, RequestIsBuildProperly) {
  // We will skip model download here, instantly returning a valid session.
  EXPECT_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
      .WillOnce(testing::Invoke(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              const std::optional<optimization_guide::SessionConfigParams>&
                  config_params) {
            return std::make_unique<NiceMock<MockSession>>(&session_);
          }));
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              AddOnDeviceModelAvailabilityChangeObserver(_, _));

  PermissionsAiRequest request;
  request.set_rendered_text(kRenderedText);
  request.set_permission_type(GetParam().permission_type);

  optimization_guide::proto::DefaultResponse default_response;
  optimization_guide::StreamingResponse default_streaming_response{
      .response = AnyWrapProto(default_response), .is_complete = true};
  EXPECT_CALL(session_, ExecuteModel(EqualsProto(request), _))
      .WillOnce(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            callback.Run(OptimizationGuideModelStreamingExecutionResult(
                base::ok(default_streaming_response),
                /*provided_by_on_device=*/true));
          })));
  base::test::TestFuture<std::optional<PermissionsAiResponse>> future;
  permissions_ai_handler_->InquireAiOnDeviceModel(
      kRenderedText, GetParam().request_type, future.GetCallback());
}

TEST_F(PermissionsAiHandlerTest, EmptyModelResponseIsHandled) {
  // We will skip model download here, instantly returning a valid session.
  EXPECT_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
      .WillOnce(testing::Invoke(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              const std::optional<optimization_guide::SessionConfigParams>&
                  config_params) {
            return std::make_unique<NiceMock<MockSession>>(&session_);
          }));
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              AddOnDeviceModelAvailabilityChangeObserver(_, _));

  optimization_guide::proto::DefaultResponse default_response;
  optimization_guide::StreamingResponse default_streaming_response{
      .response = AnyWrapProto(default_response), .is_complete = true};
  EXPECT_CALL(session_, ExecuteModel(_, _))
      .WillOnce(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            callback.Run(OptimizationGuideModelStreamingExecutionResult(
                base::ok(default_streaming_response),
                /*provided_by_on_device=*/true));
          })));

  base::HistogramTester histogram_tester;
  base::test::TestFuture<std::optional<PermissionsAiResponse>> future;
  permissions_ai_handler_->InquireAiOnDeviceModel(
      kRenderedText, RequestType::kNotifications, future.GetCallback());
  EXPECT_EQ(future.Take(), std::nullopt);
  EXPECT_TRUE(permissions_ai_handler_->IsOnDeviceModelAvailable());

  histogram_tester.ExpectUniqueSample(kModelAvailableAtInquiryTimeHistogram,
                                      true, 1);
  histogram_tester.ExpectUniqueSample(kSessionCreationSuccessHistogram, true,
                                      1);
  histogram_tester.ExpectTotalCount(kSessionCreationTimeHistogram, 1);
  histogram_tester.ExpectTotalCount(kExecutionDurationHistogram, 1);
  histogram_tester.ExpectUniqueSample(kExecutionSuccessHistogram, true, 1);
  histogram_tester.ExpectUniqueSample(kResponseParseSuccessHistogram, false, 1);
}

TEST_F(PermissionsAiHandlerTest, IncompleResponseIsIgnored) {
  // We will skip model download on the first session here, instantly
  // returning a valid session.
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              AddOnDeviceModelAvailabilityChangeObserver(_, _));

  EXPECT_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
      .WillOnce(testing::Invoke(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              const std::optional<optimization_guide::SessionConfigParams>&
                  config_params) {
            return std::make_unique<NiceMock<MockSession>>(&session_);
          }));

  optimization_guide::proto::DefaultResponse default_response;
  optimization_guide::StreamingResponse default_streaming_response{
      .response = AnyWrapProto(default_response), .is_complete = false};

  EXPECT_CALL(session_, ExecuteModel(_, _))
      .WillOnce(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            callback.Run(OptimizationGuideModelStreamingExecutionResult(
                base::ok(default_streaming_response),
                /*provided_by_on_device=*/true));
          })));

  base::HistogramTester histogram_tester;
  int call_count = 0;
  permissions_ai_handler_->InquireAiOnDeviceModel(
      kRenderedText, RequestType::kNotifications,
      BindOnce(&CallCounter, OwnedRef(call_count)));

  EXPECT_EQ(call_count, 0);

  histogram_tester.ExpectUniqueSample(kModelAvailableAtInquiryTimeHistogram,
                                      true, 1);
  histogram_tester.ExpectUniqueSample(kSessionCreationSuccessHistogram, true,
                                      1);
  histogram_tester.ExpectTotalCount(kSessionCreationTimeHistogram, 1);
  histogram_tester.ExpectTotalCount(kExecutionDurationHistogram, 0);
  histogram_tester.ExpectTotalCount(kExecutionSuccessHistogram, 0);
  histogram_tester.ExpectTotalCount(kResponseParseSuccessHistogram, 0);
}

TEST_P(ParametrizedPermissionsAiHandlerTest,
       ResponseIsParsedCorrectlyAndCallbackCalledWithResult) {
  // We will skip model download on the first session here, instantly
  // returning a valid session.
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              AddOnDeviceModelAvailabilityChangeObserver(_, _));

  EXPECT_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
      .WillOnce(testing::Invoke(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              const std::optional<optimization_guide::SessionConfigParams>&
                  config_params) {
            return std::make_unique<NiceMock<MockSession>>(&session_);
          }));

  PermissionsAiResponse response;
  response.set_is_permission_relevant(GetParam().is_permission_relevant);
  auto streaming_response = optimization_guide::StreamingResponse{
      .response = AnyWrapProto(response), .is_complete = true};

  EXPECT_CALL(session_, ExecuteModel(_, _))
      .WillOnce(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            callback.Run(OptimizationGuideModelStreamingExecutionResult(
                base::ok(streaming_response),
                /*provided_by_on_device=*/true));
          })));

  base::HistogramTester histogram_tester;

  base::test::TestFuture<std::optional<PermissionsAiResponse>> future;
  permissions_ai_handler_->InquireAiOnDeviceModel(
      kRenderedText, RequestType::kNotifications, future.GetCallback());
  EXPECT_THAT(future.Take(), Optional(EqualsProto(response)));

  histogram_tester.ExpectUniqueSample(kModelAvailableAtInquiryTimeHistogram,
                                      true, 1);
  histogram_tester.ExpectUniqueSample(kSessionCreationSuccessHistogram, true,
                                      1);
  histogram_tester.ExpectTotalCount(kSessionCreationTimeHistogram, 1);
  histogram_tester.ExpectTotalCount(kExecutionDurationHistogram, 1);
  histogram_tester.ExpectTotalCount(kExecutionSuccessHistogram, 1);
  histogram_tester.ExpectTotalCount(kResponseParseSuccessHistogram, 1);
}

TEST_F(PermissionsAiHandlerTest, FailForNewSessionIfOldIsStillRunning) {
  base::HistogramTester histogram_tester;
  // We will skip model download on the first session here, instantly
  // returning a valid session.
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              AddOnDeviceModelAvailabilityChangeObserver(_, _));

  EXPECT_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
      .WillOnce(testing::Invoke(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              const std::optional<optimization_guide::SessionConfigParams>&
                  config_params) {
            return std::make_unique<NiceMock<MockSession>>(&session_);
          }));

  EXPECT_CALL(session_, ExecuteModel(_, _));

  int call_count = 0;
  permissions_ai_handler_->InquireAiOnDeviceModel(
      kRenderedText, RequestType::kNotifications,
      BindOnce(&CallCounter, OwnedRef(call_count)));
  EXPECT_EQ(call_count, 0);

  base::test::TestFuture<std::optional<PermissionsAiResponse>> future;
  permissions_ai_handler_->InquireAiOnDeviceModel(
      kRenderedText, RequestType::kNotifications, future.GetCallback());
  EXPECT_EQ(future.Take(), std::nullopt);

  histogram_tester.ExpectUniqueSample(kModelAvailableAtInquiryTimeHistogram,
                                      true, 1);
  histogram_tester.ExpectUniqueSample(kSessionCreationSuccessHistogram, true,
                                      1);
  histogram_tester.ExpectUniqueSample(kPreviousSessionFinishedInTimeHistogram,
                                      false, 1);
  histogram_tester.ExpectTotalCount(kSessionCreationTimeHistogram, 1);
  histogram_tester.ExpectTotalCount(kExecutionDurationHistogram, 0);
  histogram_tester.ExpectTotalCount(kExecutionSuccessHistogram, 0);
  histogram_tester.ExpectTotalCount(kResponseParseSuccessHistogram, 0);
}

TEST_F(PermissionsAiHandlerTest,
       OldSessionGetsNotInterruptedByNewInquiryRequest) {
  // We will skip model download on the first session here, instantly
  // returning a valid session.
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              AddOnDeviceModelAvailabilityChangeObserver(_, _));

  EXPECT_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
      .WillOnce(testing::Invoke(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              const std::optional<optimization_guide::SessionConfigParams>&
                  config_params) {
            return std::make_unique<NiceMock<MockSession>>(&session_);
          }));

  optimization_guide::OptimizationGuideModelExecutionResultStreamingCallback
      internal_callback;
  EXPECT_CALL(session_, ExecuteModel(_, _))
      .WillOnce(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) { internal_callback = std::move(callback); })));

  base::test::TestFuture<std::optional<PermissionsAiResponse>> future_1;
  permissions_ai_handler_->InquireAiOnDeviceModel(
      kRenderedText, RequestType::kNotifications, future_1.GetCallback());

  {
    base::test::TestFuture<std::optional<PermissionsAiResponse>> future;
    permissions_ai_handler_->InquireAiOnDeviceModel(
        kRenderedText, RequestType::kNotifications, future.GetCallback());
    EXPECT_EQ(future.Take(), std::nullopt);
  }

  base::HistogramTester histogram_tester;
  PermissionsAiResponse response;
  response.set_is_permission_relevant(true);
  auto streaming_response = optimization_guide::StreamingResponse{
      .response = AnyWrapProto(response), .is_complete = true};
  internal_callback.Run(OptimizationGuideModelStreamingExecutionResult(
      base::ok(streaming_response),
      /*provided_by_on_device=*/true));
  EXPECT_THAT(future_1.Take(), Optional(EqualsProto(response)));
  histogram_tester.ExpectTotalCount(kExecutionDurationHistogram, 1);
  histogram_tester.ExpectTotalCount(kExecutionSuccessHistogram, 1);
  histogram_tester.ExpectTotalCount(kResponseParseSuccessHistogram, 1);
}

}  // namespace permissions
