// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/client_side_detection_intelligent_scan_delegate_desktop.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/model_execution/test/mock_on_device_capability.h"
#include "components/optimization_guide/core/model_execution/test/mock_remote_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/model_quality_metadata.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-shared.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/safe_browsing/core/browser/intelligent_scan_delegate.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::EqualsProto;
using ::optimization_guide::AnyWrapProto;
using ::optimization_guide::MockSession;
using ::optimization_guide::OnDeviceError;
using ::optimization_guide::OptimizationGuideModelStreamingExecutionResult;
using ::optimization_guide::proto::ModelExecutionInfo;
using RemoteModelExecutionCallback = base::OnceCallback<void(
    optimization_guide::OptimizationGuideModelExecutionResult,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry>)>;
using ::optimization_guide::proto::ScamDetectionResponse;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

namespace safe_browsing {

using IntelligentScanResult = IntelligentScanDelegate::IntelligentScanResult;
using ModelType = IntelligentScanDelegate::ModelType;

class ClientSideDetectionIntelligentScanDelegateDesktopTestBase
    : public testing::Test {
 public:
  ClientSideDetectionIntelligentScanDelegateDesktopTestBase() {
    RegisterProfilePrefs(pref_service_.registry());
  }

 protected:
  void CreateDelegate(bool is_enhanced_protection_enabled,
                      policy::ManagementService* management_service = nullptr) {
    SetEnhancedProtectionPrefForTests(&pref_service_,
                                      is_enhanced_protection_enabled);
    delegate_ =
        std::make_unique<ClientSideDetectionIntelligentScanDelegateDesktop>(
            pref_service_, &mock_opt_guide_, management_service,
            &remote_model_executor_);
  }

  void EnableOnDeviceModel() {
    CreateDelegate(/*is_enhanced_protection_enabled=*/false);
    optimization_guide::OnDeviceModelAvailabilityObserver*
        availability_observer = nullptr;
    base::RunLoop run_loop_for_add_observer;
    EXPECT_CALL(mock_opt_guide_,
                AddOnDeviceModelAvailabilityChangeObserver(_, _))
        .WillOnce([&](optimization_guide::mojom::OnDeviceFeature feature,
                      optimization_guide::OnDeviceModelAvailabilityObserver*
                          observer) {
          availability_observer = observer;
          run_loop_for_add_observer.Quit();
        });

    SetEnhancedProtectionPrefForTests(&pref_service_, true);
    run_loop_for_add_observer.Run();
    CHECK(availability_observer);

    availability_observer->OnDeviceModelAvailabilityChanged(
        optimization_guide::mojom::OnDeviceFeature::kScamDetection,
        optimization_guide::OnDeviceModelEligibilityReason::kSuccess);

    ASSERT_EQ(delegate_->GetIntelligentScanModelType(
                  /*log_failed_eligibility_reason=*/true),
              ModelType::kOnDevice);
  }

  void EnableOnDeviceModelWithSession() {
    EnableOnDeviceModel();
    EXPECT_CALL(mock_opt_guide_, StartSession(_, _, _))
        .WillOnce(
            [&](optimization_guide::mojom::OnDeviceFeature feature,
                const optimization_guide::SessionConfigParams& config_params,
                base::WeakPtr<OptimizationGuideLogger> logger) {
              return std::make_unique<NiceMock<MockSession>>(&session_);
            });
  }

  optimization_guide::StreamingResponse CreateScamDetectionResponse(
      const std::string& brand,
      const std::string& intent,
      bool is_complete) {
    ScamDetectionResponse response;
    response.set_brand(brand);
    response.set_intent(intent);
    return optimization_guide::StreamingResponse{
        .response = AnyWrapProto(response), .is_complete = is_complete};
  }

  std::unique_ptr<ModelExecutionInfo> CreateExecutionInfo(int model_version) {
    std::unique_ptr<ModelExecutionInfo> execution_info =
        std::make_unique<ModelExecutionInfo>();
    execution_info->mutable_on_device_model_execution_info()
        ->mutable_model_versions()
        ->mutable_on_device_model_service_version()
        ->set_model_adaptation_version(model_version);
    return execution_info;
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  NiceMock<MockOptimizationGuideKeyedService> mock_opt_guide_;
  testing::NiceMock<MockSession> session_;
  std::unique_ptr<ClientSideDetectionIntelligentScanDelegateDesktop> delegate_;
  base::HistogramTester histogram_tester_;
  base::test::ScopedFeatureList feature_list_;
  NiceMock<optimization_guide::MockRemoteModelExecutor> remote_model_executor_;
};

class ClientSideDetectionIntelligentScanDelegateDesktopTest
    : public ClientSideDetectionIntelligentScanDelegateDesktopTestBase {
 protected:
  ClientSideDetectionIntelligentScanDelegateDesktopTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{kClientSideDetectionServerModelForScamDetectionDesktop,
          {{"MaxIntelligentScansPerDayDesktop", "3"}}}},
        /*disabled_features=*/{kClientSideDetectionKillswitch});
  }
};

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTest,
       ShouldRequestIntelligentScan_KeyboardLockRequested) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::KEYBOARD_LOCK_REQUESTED);
  EXPECT_TRUE(delegate_->ShouldRequestIntelligentScan(&verdict));
}

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTest,
       ShouldRequestIntelligentScan_IntelligentScanRequested) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::FORCE_REQUEST);
  verdict.mutable_llama_forced_trigger_info()->set_intelligent_scan(true);
  EXPECT_TRUE(delegate_->ShouldRequestIntelligentScan(&verdict));
}

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTest,
       ShouldNotRequestIntelligentScan_PointerLockRequested) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::POINTER_LOCK_REQUESTED);
  EXPECT_FALSE(delegate_->ShouldRequestIntelligentScan(&verdict));
}

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTest,
       ShouldNotRequestIntelligentScan_EnhancedProtectionDisabled) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/false);
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::KEYBOARD_LOCK_REQUESTED);
  EXPECT_FALSE(delegate_->ShouldRequestIntelligentScan(&verdict));
}

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTest,
       ShouldNotRequestIntelligentScan_EmptyLlamaForcedTriggerInfo) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::FORCE_REQUEST);
  EXPECT_FALSE(delegate_->ShouldRequestIntelligentScan(&verdict));
}

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTest,
       ShouldNotRequestIntelligentScan_IntelligentScanDisabled) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::FORCE_REQUEST);
  verdict.mutable_llama_forced_trigger_info()->set_intelligent_scan(false);
  EXPECT_FALSE(delegate_->ShouldRequestIntelligentScan(&verdict));
}

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTest,
       ShouldShowScamWarning) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);

  EXPECT_FALSE(delegate_->ShouldShowScamWarning(std::nullopt));
  EXPECT_FALSE(delegate_->ShouldShowScamWarning(
      IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_SAFE));
  EXPECT_FALSE(delegate_->ShouldShowScamWarning(
      IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_UNSPECIFIED));
  EXPECT_FALSE(delegate_->ShouldShowScamWarning(
      IntelligentScanVerdict::SCAM_EXPERIMENT_CATCH_ALL_TELEMETRY));
  // Undefined verdict doesn't show warning.
  EXPECT_FALSE(
      delegate_->ShouldShowScamWarning(static_cast<IntelligentScanVerdict>(6)));
  // Do not show warnings if the enum value is unknown.
  EXPECT_FALSE(delegate_->ShouldShowScamWarning(
      static_cast<IntelligentScanVerdict>(12345)));
  EXPECT_TRUE(delegate_->ShouldShowScamWarning(
      IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_1));
  EXPECT_TRUE(delegate_->ShouldShowScamWarning(
      IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_2));
  EXPECT_TRUE(delegate_->ShouldShowScamWarning(
      IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_3));
  EXPECT_TRUE(delegate_->ShouldShowScamWarning(
      IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_4));
  EXPECT_TRUE(delegate_->ShouldShowScamWarning(
      IntelligentScanVerdict::SCAM_EXPERIMENT_CATCH_ALL_ENFORCEMENT));
}

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTest,
       GetIntelligentScanModelType) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);
  EXPECT_EQ(delegate_->GetIntelligentScanModelType(
                /*log_failed_eligibility_reason=*/false),
            ModelType::kServerSide);
  // Shutdown delegate to make remote model executor unavailable.
  delegate_->Shutdown();
  EXPECT_EQ(delegate_->GetIntelligentScanModelType(
                /*log_failed_eligibility_reason=*/false),
            ModelType::kNotSupportedServerSide);
}

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTest,
       StartIntelligentScan_ModelResponseSuccessful) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);

  optimization_guide::proto::ScamDetectionRequest expected_request;
  *expected_request.mutable_rendered_text() = "test rendered text";
  optimization_guide::ModelExecutionOptions expected_options{};

  optimization_guide::proto::ScamDetectionResponse returned_response;
  returned_response.set_brand("test_brand");
  returned_response.set_intent("test_intent");

  EXPECT_CALL(
      remote_model_executor_,
      ExecuteModel(optimization_guide::ModelBasedCapabilityKey::kScamDetection,
                   EqualsProto(expected_request),
                   ::testing::Eq(expected_options),
                   ::testing::A<RemoteModelExecutionCallback>()))
      .WillOnce(base::test::RunOnceCallback<3>(
          optimization_guide::OptimizationGuideModelExecutionResult(
              optimization_guide::AnyWrapProto(returned_response),
              /*execution_info=*/nullptr),
          /*log_entry=*/nullptr));
  base::test::TestFuture<IntelligentScanResult> future;
  delegate_->StartIntelligentScan("test rendered text", future.GetCallback());

  EXPECT_TRUE(future.Get().execution_success);
  EXPECT_EQ(future.Get().model_version, 1000);
  EXPECT_EQ(future.Get().brand, "test_brand");
  EXPECT_EQ(future.Get().intent, "test_intent");
  EXPECT_EQ(future.Get().model_type, ModelType::kServerSide);
  EXPECT_EQ(future.Get().no_info_reason,
            IntelligentScanInfo::NO_INFO_REASON_UNSPECIFIED);
  EXPECT_EQ(delegate_->GetAliveInquiryCountForTesting(), 0);

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.ServerSideModelExecutionSuccess", true, 1);
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.ServerSideModelExecutionDuration", 1);
  // server-side model execution shouldn't log on-device model execution
  // metrics.
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.OnDeviceModelExecutionSuccess", 0);
}

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTest,
       StartIntelligentScan_ModelResponseUnsuccessful) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);

  optimization_guide::proto::ScamDetectionRequest expected_request;
  *expected_request.mutable_rendered_text() = "test rendered text";
  optimization_guide::ModelExecutionOptions expected_options{};

  EXPECT_CALL(
      remote_model_executor_,
      ExecuteModel(optimization_guide::ModelBasedCapabilityKey::kScamDetection,
                   EqualsProto(expected_request),
                   ::testing::Eq(expected_options),
                   ::testing::A<RemoteModelExecutionCallback>()))
      .WillOnce(base::test::RunOnceCallback<3>(
          optimization_guide::OptimizationGuideModelExecutionResult(
              base::unexpected(
                  optimization_guide::OptimizationGuideModelExecutionError::
                      FromModelExecutionError(
                          optimization_guide::
                              OptimizationGuideModelExecutionError::
                                  ModelExecutionError::kGenericFailure)),
              /*execution_info=*/nullptr),
          /*log_entry=*/nullptr));
  base::test::TestFuture<IntelligentScanResult> future;
  delegate_->StartIntelligentScan("test rendered text", future.GetCallback());

  EXPECT_FALSE(future.Get().execution_success);
  EXPECT_EQ(future.Get().brand, "");
  EXPECT_EQ(future.Get().intent, "");
  EXPECT_EQ(future.Get().model_type, ModelType::kServerSide);
  EXPECT_EQ(future.Get().no_info_reason,
            IntelligentScanInfo::SERVER_SIDE_MODEL_OUTPUT_MISSING);
  EXPECT_EQ(delegate_->GetAliveInquiryCountForTesting(), 0);
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.ServerSideModelExecutionSuccess", false, 1);
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.ServerSideModelExecutionError",
      optimization_guide::OptimizationGuideModelExecutionError::
          ModelExecutionError::kGenericFailure,
      1);
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.ServerSideModelExecutionDuration", 1);
}

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTest,
       StartIntelligentScan_ModelNotAvailable) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);
  delegate_->Shutdown();

  base::test::TestFuture<IntelligentScanResult> future;
  delegate_->StartIntelligentScan("test rendered text", future.GetCallback());

  EXPECT_FALSE(future.Get().execution_success);
  EXPECT_EQ(future.Get().model_type, ModelType::kNotSupportedServerSide);
  EXPECT_EQ(future.Get().no_info_reason,
            IntelligentScanInfo::SERVER_SIDE_MODEL_UNAVAILABLE);
}

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTest,
       StartIntelligentScan_MultipleInquiries) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);

  base::test::TestFuture<IntelligentScanResult> future1;
  delegate_->StartIntelligentScan("test rendered text", future1.GetCallback());
  EXPECT_EQ(delegate_->GetAliveInquiryCountForTesting(), 1);

  base::test::TestFuture<IntelligentScanResult> future2;
  delegate_->StartIntelligentScan("test rendered text", future2.GetCallback());

  EXPECT_EQ(delegate_->GetAliveInquiryCountForTesting(), 2);
}

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTest,
       StartIntelligentScan_QuotaChecks) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);
  constexpr int kMaxScansPerDay = 3;

  EXPECT_CALL(
      remote_model_executor_,
      ExecuteModel(optimization_guide::ModelBasedCapabilityKey::kScamDetection,
                   _, _, _))
      .Times(kMaxScansPerDay + 1);

  for (int i = 0; i < kMaxScansPerDay; ++i) {
    delegate_->StartIntelligentScan("test", base::DoNothing());
    histogram_tester_.ExpectBucketCount(
        "SBClientPhishing.ServerSideModelQuotaCountOnLookup", i + 1, 1);
  }
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.ServerSideModelHitQuotaAtInquiryTime", false,
      kMaxScansPerDay);
  histogram_tester_.ExpectBucketCount(
      "SBClientPhishing.ServerSideModelHitQuotaAtInquiryTime", true, 0);

  // At quota, scan should fail.
  {
    base::test::TestFuture<IntelligentScanResult> future;
    std::optional<base::UnguessableToken> token =
        delegate_->StartIntelligentScan("test rendered text",
                                        future.GetCallback());
    EXPECT_FALSE(token.has_value());
    ASSERT_TRUE(future.IsReady());
    EXPECT_FALSE(future.Get().execution_success);
    EXPECT_EQ(future.Get().model_type, ModelType::kServerSide);
    EXPECT_EQ(future.Get().no_info_reason,
              IntelligentScanInfo::SERVER_SIDE_MODEL_EXCEED_QUOTA);
  }
  histogram_tester_.ExpectBucketCount(
      "SBClientPhishing.ServerSideModelHitQuotaAtInquiryTime", true, 1);

  // Fast forward time by 2 days, the quota should be cleared.
  task_environment_.FastForwardBy(base::Days(2));

  // Scan should succeed now.
  {
    base::test::TestFuture<IntelligentScanResult> future;
    std::optional<base::UnguessableToken> token =
        delegate_->StartIntelligentScan("test rendered text",
                                        future.GetCallback());
    EXPECT_TRUE(token.has_value());
  }
  histogram_tester_.ExpectBucketCount(
      "SBClientPhishing.ServerSideModelHitQuotaAtInquiryTime", false,
      kMaxScansPerDay + 1);
}

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTest,
       StartIntelligentScan_QuotaConsumedOnModelFailure) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);
  constexpr int kMaxScansPerDay = 3;

  for (int i = 0; i < kMaxScansPerDay; ++i) {
    EXPECT_CALL(remote_model_executor_,
                ExecuteModel(
                    optimization_guide::ModelBasedCapabilityKey::kScamDetection,
                    _, _, _))
        .WillOnce(base::test::RunOnceCallback<3>(
            optimization_guide::OptimizationGuideModelExecutionResult(
                base::unexpected(
                    optimization_guide::OptimizationGuideModelExecutionError::
                        FromModelExecutionError(
                            optimization_guide::
                                OptimizationGuideModelExecutionError::
                                    ModelExecutionError::kGenericFailure)),
                /*execution_info=*/nullptr),
            /*log_entry=*/nullptr));
    base::test::TestFuture<IntelligentScanResult> future;
    delegate_->StartIntelligentScan("test", future.GetCallback());
    EXPECT_FALSE(future.Get().execution_success);
    EXPECT_EQ(future.Get().model_type, ModelType::kServerSide);
    EXPECT_EQ(future.Get().no_info_reason,
              IntelligentScanInfo::SERVER_SIDE_MODEL_OUTPUT_MISSING);
    EXPECT_EQ(delegate_->GetAliveInquiryCountForTesting(), 0);
  }

  // Next scan should fail due to quota.
  {
    base::test::TestFuture<IntelligentScanResult> future;
    std::optional<base::UnguessableToken> token =
        delegate_->StartIntelligentScan("test rendered text",
                                        future.GetCallback());
    EXPECT_FALSE(token.has_value());
    ASSERT_TRUE(future.IsReady());
    EXPECT_FALSE(future.Get().execution_success);
    EXPECT_EQ(future.Get().model_type, ModelType::kServerSide);
    EXPECT_EQ(future.Get().no_info_reason,
              IntelligentScanInfo::SERVER_SIDE_MODEL_EXCEED_QUOTA);
  }
}

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTest,
       OnScamWarningShown_RefundsQuota) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);
  constexpr int kMaxScansPerDay = 3;

  EXPECT_CALL(
      remote_model_executor_,
      ExecuteModel(optimization_guide::ModelBasedCapabilityKey::kScamDetection,
                   _, _, _))
      .Times(kMaxScansPerDay + 1);

  // Fill up the quota.
  for (int i = 0; i < kMaxScansPerDay; ++i) {
    delegate_->StartIntelligentScan("test", base::DoNothing());
    histogram_tester_.ExpectBucketCount(
        "SBClientPhishing.ServerSideModelQuotaCountOnLookup", i + 1, 1);
  }

  // A scam warning should refund one quota.
  delegate_->OnScamWarningShown();
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.ServerSideModelQuotaCountOnScamWarningShown",
      kMaxScansPerDay, 1);

  // Now a scan should succeed.
  std::optional<base::UnguessableToken> token =
      delegate_->StartIntelligentScan("test rendered text", base::DoNothing());
  EXPECT_TRUE(token.has_value());
  histogram_tester_.ExpectBucketCount(
      "SBClientPhishing.ServerSideModelQuotaCountOnLookup", kMaxScansPerDay, 2);
}

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTest,
       CancelIntelligentScan_MultipleInquiries) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);

  std::optional<base::UnguessableToken> scan_id1 =
      delegate_->StartIntelligentScan("test rendered text", base::DoNothing());
  EXPECT_EQ(delegate_->GetAliveInquiryCountForTesting(), 1);
  std::optional<base::UnguessableToken> scan_id2 =
      delegate_->StartIntelligentScan("test rendered text", base::DoNothing());
  EXPECT_EQ(delegate_->GetAliveInquiryCountForTesting(), 2);

  // Cancel the inquiry after inquiry is created.
  EXPECT_TRUE(delegate_->CancelIntelligentScan(*scan_id1));
  // CancelIntelligentScan should return false if the scan ID is already
  // cancelled.
  EXPECT_FALSE(delegate_->CancelIntelligentScan(*scan_id1));
  EXPECT_EQ(delegate_->GetAliveInquiryCountForTesting(), 1);
  EXPECT_TRUE(delegate_->CancelIntelligentScan(*scan_id2));
  EXPECT_EQ(delegate_->GetAliveInquiryCountForTesting(), 0);
}

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTest,
       ResetInquiry_EnhancedProtectionDisabled) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);

  delegate_->StartIntelligentScan("test rendered text", base::DoNothing());
  delegate_->StartIntelligentScan("test rendered text", base::DoNothing());
  EXPECT_EQ(delegate_->GetAliveInquiryCountForTesting(), 2);

  SetEnhancedProtectionPrefForTests(&pref_service_, false);
  // Inquiries should be reset after the enhanced protection is disabled.
  EXPECT_EQ(delegate_->GetAliveInquiryCountForTesting(), 0);
}

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTest,
       DoNotStartOnDeviceModelDownload) {
  EXPECT_CALL(mock_opt_guide_, AddOnDeviceModelAvailabilityChangeObserver(_, _))
      .Times(0);
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);
  EXPECT_EQ(delegate_->GetIntelligentScanModelType(
                /*log_failed_eligibility_reason=*/false),
            ModelType::kServerSide);
  // No on-device model download because the server model is enabled.
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.OnDeviceModelDownloadSuccess", 0);
}

class ClientSideDetectionIntelligentScanDelegateDesktopTestKillSwitchEnabled
    : public ClientSideDetectionIntelligentScanDelegateDesktopTestBase {
 public:
  ClientSideDetectionIntelligentScanDelegateDesktopTestKillSwitchEnabled() {
    feature_list_.InitWithFeatures({kClientSideDetectionKillswitch}, {});
  }
};

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTestKillSwitchEnabled,
       NotListenToModelUpdateOnCreation) {
  // The killswitch flag is enabled, so we shouldn't listen to model updates.
  EXPECT_CALL(mock_opt_guide_, AddOnDeviceModelAvailabilityChangeObserver(_, _))
      .Times(0);
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);

  EXPECT_EQ(delegate_->GetIntelligentScanModelType(
                /*log_failed_eligibility_reason=*/true),
            ModelType::kNotSupportedOnDevice);
}

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTestKillSwitchEnabled,
       ShouldNotRequestIntelligentScan) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::KEYBOARD_LOCK_REQUESTED);
  EXPECT_FALSE(delegate_->ShouldRequestIntelligentScan(&verdict));
}

class ClientSideDetectionIntelligentScanDelegateDesktopServerModelRolloutTest
    : public ClientSideDetectionIntelligentScanDelegateDesktopTestBase {
 protected:
  ClientSideDetectionIntelligentScanDelegateDesktopServerModelRolloutTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{kClientSideDetectionServerModelForScamDetectionDesktop,
          {{"MaxIntelligentScansPerDayDesktop", "3"}}},
         {kClientSideDetectionServerModelRolloutDesktop,
          {{"ModelVersion", "2000"}}}},
        /*disabled_features=*/{kClientSideDetectionKillswitch});
  }
};

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopServerModelRolloutTest,
       UsesRolloutVersionWhenFlagIsEnabled) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);

  optimization_guide::proto::ScamDetectionRequest expected_request;
  *expected_request.mutable_rendered_text() = "test rendered text";
  optimization_guide::ModelExecutionOptions expected_options{};

  optimization_guide::proto::ScamDetectionResponse returned_response;
  returned_response.set_brand("test_brand");
  returned_response.set_intent("test_intent");

  EXPECT_CALL(
      remote_model_executor_,
      ExecuteModel(optimization_guide::ModelBasedCapabilityKey::kScamDetection,
                   EqualsProto(expected_request),
                   ::testing::Eq(expected_options),
                   ::testing::A<RemoteModelExecutionCallback>()))
      .WillOnce(base::test::RunOnceCallback<3>(
          optimization_guide::OptimizationGuideModelExecutionResult(
              optimization_guide::AnyWrapProto(returned_response),
              /*execution_info=*/nullptr),
          /*log_entry=*/nullptr));
  base::test::TestFuture<IntelligentScanResult> future;
  delegate_->StartIntelligentScan("test rendered text", future.GetCallback());

  EXPECT_TRUE(future.Get().execution_success);
  EXPECT_EQ(future.Get().model_version, 2000);
}

class
    ClientSideDetectionIntelligentScanDelegateDesktopTestWithServerModelDisabled
    : public ClientSideDetectionIntelligentScanDelegateDesktopTestBase {
 protected:
  ClientSideDetectionIntelligentScanDelegateDesktopTestWithServerModelDisabled() {
    feature_list_.InitWithFeatures(
        {}, {kClientSideDetectionKillswitch,
             kClientSideDetectionServerModelForScamDetectionDesktop});
  }
};

TEST_F(
    ClientSideDetectionIntelligentScanDelegateDesktopTestWithServerModelDisabled,
    TestMultipleSessions) {
  EnableOnDeviceModel();

  EXPECT_CALL(mock_opt_guide_, StartSession(_, _, _))
      .WillOnce(
          [&](optimization_guide::mojom::OnDeviceFeature feature,
              const optimization_guide::SessionConfigParams& config_params,
              base::WeakPtr<OptimizationGuideLogger> logger) {
            return std::make_unique<NiceMock<MockSession>>(&session_);
          });

  base::test::TestFuture<IntelligentScanResult> future1;
  std::optional<base::UnguessableToken> scan_id1 =
      delegate_->StartIntelligentScan("", future1.GetCallback());
  EXPECT_FALSE(scan_id1->is_empty());

  testing::NiceMock<MockSession> session2;
  EXPECT_CALL(mock_opt_guide_, StartSession(_, _, _))
      .WillOnce(
          [&](optimization_guide::mojom::OnDeviceFeature feature,
              const optimization_guide::SessionConfigParams& config_params,
              base::WeakPtr<OptimizationGuideLogger> logger) {
            return std::make_unique<NiceMock<MockSession>>(&session2);
          });

  base::test::TestFuture<IntelligentScanResult> future2;
  std::optional<base::UnguessableToken> scan_id2 =
      delegate_->StartIntelligentScan("", future2.GetCallback());

  // Both scan IDs should still be alive.
  EXPECT_FALSE(scan_id1->is_empty());
  EXPECT_FALSE(scan_id2->is_empty());
  EXPECT_EQ(delegate_->GetAliveInquiryCountForTesting(), 2);

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelSessionCreationSuccess", true, 2);
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.OnDeviceModelSessionCreationTime", 2);
}

TEST_F(
    ClientSideDetectionIntelligentScanDelegateDesktopTestWithServerModelDisabled,
    TestCancelIntelligentScan) {
  EnableOnDeviceModelWithSession();

  base::test::TestFuture<IntelligentScanResult> future;
  std::optional<base::UnguessableToken> scan_id =
      delegate_->StartIntelligentScan("", future.GetCallback());
  EXPECT_FALSE(scan_id->is_empty());

  EXPECT_EQ(delegate_->GetAliveInquiryCountForTesting(), 1);
  EXPECT_TRUE(delegate_->CancelIntelligentScan(*scan_id));
  EXPECT_EQ(delegate_->GetAliveInquiryCountForTesting(), 0);

  // The callback should not be called.
  EXPECT_FALSE(future.IsReady());
}

TEST_F(
    ClientSideDetectionIntelligentScanDelegateDesktopTestWithServerModelDisabled,
    TestMultipleScansCancellation) {
  EnableOnDeviceModel();

  EXPECT_CALL(mock_opt_guide_, StartSession(_, _, _))
      .WillOnce(
          [&](optimization_guide::mojom::OnDeviceFeature feature,
              const optimization_guide::SessionConfigParams& config_params,
              base::WeakPtr<OptimizationGuideLogger> logger) {
            return std::make_unique<NiceMock<MockSession>>(&session_);
          });

  base::test::TestFuture<IntelligentScanResult> future1;
  std::optional<base::UnguessableToken> scan_id1 =
      delegate_->StartIntelligentScan("", future1.GetCallback());
  EXPECT_FALSE(scan_id1->is_empty());

  testing::NiceMock<MockSession> session2;
  EXPECT_CALL(mock_opt_guide_, StartSession(_, _, _))
      .WillOnce(
          [&](optimization_guide::mojom::OnDeviceFeature feature,
              const optimization_guide::SessionConfigParams& config_params,
              base::WeakPtr<OptimizationGuideLogger> logger) {
            return std::make_unique<NiceMock<MockSession>>(&session2);
          });

  base::test::TestFuture<IntelligentScanResult> future2;
  std::optional<base::UnguessableToken> scan_id2 =
      delegate_->StartIntelligentScan("", future2.GetCallback());

  // Both scan IDs should still be alive.
  EXPECT_FALSE(scan_id1->is_empty());
  EXPECT_FALSE(scan_id2->is_empty());

  EXPECT_EQ(delegate_->GetAliveInquiryCountForTesting(), 2);
  EXPECT_TRUE(delegate_->CancelIntelligentScan(*scan_id1));
  EXPECT_EQ(delegate_->GetAliveInquiryCountForTesting(), 1);
  EXPECT_TRUE(delegate_->CancelIntelligentScan(*scan_id2));
  EXPECT_EQ(delegate_->GetAliveInquiryCountForTesting(), 0);

  // The callbacks should not be called.
  EXPECT_FALSE(future1.IsReady());
  EXPECT_FALSE(future2.IsReady());
}

TEST_F(
    ClientSideDetectionIntelligentScanDelegateDesktopTestWithServerModelDisabled,
    TestSessionExecutionFailure) {
  EnableOnDeviceModelWithSession();

  EXPECT_CALL(session_, ExecuteModel(_, _))
      .WillOnce(testing::WithArg<1>(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            callback.Run(OptimizationGuideModelStreamingExecutionResult(
                base::unexpected(OnDeviceError::kGenericFailure),
                /*provided_by_on_device=*/true,
                /*execution_info=*/CreateExecutionInfo(/*model_version=*/123)));
          }));

  base::test::TestFuture<IntelligentScanResult> future;
  delegate_->StartIntelligentScan("", future.GetCallback());

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelSessionCreationSuccess", true, 1);
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.OnDeviceModelSessionCreationTime", 1);
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.OnDeviceModelExecutionDuration", 1);
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelExecutionSuccess", false, 1);

  EXPECT_FALSE(future.Get().execution_success);
  EXPECT_EQ(future.Get().model_version, 123);
  EXPECT_EQ(future.Get().model_type, ModelType::kOnDevice);
  EXPECT_EQ(future.Get().no_info_reason,
            IntelligentScanInfo::ON_DEVICE_MODEL_OUTPUT_MISSING);
}

TEST_F(
    ClientSideDetectionIntelligentScanDelegateDesktopTestWithServerModelDisabled,
    TestSessionExecutionSuccessButFailedParsing) {
  EnableOnDeviceModelWithSession();

  optimization_guide::proto::DefaultResponse default_response;
  optimization_guide::StreamingResponse default_streaming_response{
      .response = AnyWrapProto(default_response), .is_complete = true};

  EXPECT_CALL(session_, ExecuteModel(_, _))
      .WillOnce(testing::WithArg<1>(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            callback.Run(OptimizationGuideModelStreamingExecutionResult(
                base::ok(default_streaming_response),
                /*provided_by_on_device=*/true,
                /*execution_info=*/CreateExecutionInfo(/*model_version=*/123)));
          }));

  base::test::TestFuture<IntelligentScanResult> future;
  delegate_->StartIntelligentScan("", future.GetCallback());

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelSessionCreationSuccess", true, 1);
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.OnDeviceModelSessionCreationTime", 1);
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.OnDeviceModelExecutionDuration", 1);
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelExecutionSuccess", true, 1);
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelResponseParseSuccess", false, 1);

  EXPECT_FALSE(future.Get().execution_success);
  EXPECT_EQ(future.Get().model_version, 123);
  EXPECT_EQ(future.Get().model_type, ModelType::kOnDevice);
  EXPECT_EQ(future.Get().no_info_reason,
            IntelligentScanInfo::ON_DEVICE_MODEL_OUTPUT_MISSING);
}

TEST_F(
    ClientSideDetectionIntelligentScanDelegateDesktopTestWithServerModelDisabled,
    TestSessionExecutionAndResponseParseSuccess) {
  EnableOnDeviceModelWithSession();

  EXPECT_CALL(session_, ExecuteModel(_, _))
      .WillOnce(testing::WithArg<1>(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            callback.Run(OptimizationGuideModelStreamingExecutionResult(
                base::ok(CreateScamDetectionResponse("Google", "Search Engine",
                                                     /*is_complete=*/true)),
                /*provided_by_on_device=*/false,
                /*execution_info=*/CreateExecutionInfo(/*model_version=*/123)));
          }));

  base::test::TestFuture<IntelligentScanResult> future;
  delegate_->StartIntelligentScan("", future.GetCallback());

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelSessionCreationSuccess", true, 1);
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.OnDeviceModelSessionCreationTime", 1);
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.OnDeviceModelExecutionDuration", 1);
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelExecutionSuccess", true, 1);
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelResponseParseSuccess", true, 1);
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelSuccessfulResponseCallbackAlive", true, 1);
  // Histograms related to quota should not be logged when server model is
  // disabled.
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.ServerSideModelHitQuotaAtInquiryTime", 0);

  EXPECT_TRUE(future.Get().execution_success);
  EXPECT_EQ(future.Get().brand, "Google");
  EXPECT_EQ(future.Get().intent, "Search Engine");
  EXPECT_EQ(future.Get().model_version, 123);
  EXPECT_EQ(future.Get().model_type, ModelType::kOnDevice);
  EXPECT_EQ(future.Get().no_info_reason,
            IntelligentScanInfo::NO_INFO_REASON_UNSPECIFIED);
}

TEST_F(
    ClientSideDetectionIntelligentScanDelegateDesktopTestWithServerModelDisabled,
    TestExecutionSuccessButCallbackIsNotAlive) {
  EnableOnDeviceModelWithSession();

  EXPECT_CALL(session_, ExecuteModel(_, _))
      .WillOnce(testing::WithArg<1>(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            callback.Run(OptimizationGuideModelStreamingExecutionResult(
                base::ok(CreateScamDetectionResponse("Google", "Search Engine",
                                                     /*is_complete=*/true)),
                /*provided_by_on_device=*/false,
                /*execution_info=*/CreateExecutionInfo(/*model_version=*/123)));
          }));

  // Create an empty callback.
  IntelligentScanDelegate::IntelligentScanDoneCallback host_callback;
  delegate_->StartIntelligentScan("", std::move(host_callback));

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelSessionCreationSuccess", true, 1);
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.OnDeviceModelSessionCreationTime", 1);
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.OnDeviceModelExecutionDuration", 1);
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelExecutionSuccess", true, 1);
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelResponseParseSuccess", true, 1);
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelSuccessfulResponseCallbackAlive", false,
      1);
}

TEST_F(
    ClientSideDetectionIntelligentScanDelegateDesktopTestWithServerModelDisabled,
    OnScamWarningShown_ServerModelDisabled) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);
  delegate_->OnScamWarningShown();
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.ServerSideModelQuotaCountOnScamWarningShown", 0);
}

TEST_F(
    ClientSideDetectionIntelligentScanDelegateDesktopTestWithServerModelDisabled,
    TestOnDeviceModelFetchSuccessCall) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/false);
  EXPECT_EQ(delegate_->GetIntelligentScanModelType(
                /*log_failed_eligibility_reason=*/true),
            ModelType::kNotSupportedOnDevice);

  optimization_guide::OnDeviceModelAvailabilityObserver* availability_observer =
      nullptr;
  base::RunLoop run_loop_for_add_observer;
  EXPECT_CALL(mock_opt_guide_, AddOnDeviceModelAvailabilityChangeObserver(_, _))
      .WillOnce(
          [&](optimization_guide::mojom::OnDeviceFeature feature,
              optimization_guide::OnDeviceModelAvailabilityObserver* observer) {
            availability_observer = observer;
            run_loop_for_add_observer.Quit();
          });

  SetEnhancedProtectionPrefForTests(&pref_service_, true);

  run_loop_for_add_observer.Run();
  CHECK(availability_observer);

  // Now that the delegate is observing, send `kConfigNotAvailableForFeature`
  // first to the observer, which will not stop the observing. We will then test
  // for all the possible waitable reasons, which should also not stop
  // observing.
  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::mojom::OnDeviceFeature::kScamDetection,
      optimization_guide::OnDeviceModelEligibilityReason::
          kConfigNotAvailableForFeature);

  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::mojom::OnDeviceFeature::kScamDetection,
      optimization_guide::OnDeviceModelEligibilityReason::kModelToBeInstalled);

  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::mojom::OnDeviceFeature::kScamDetection,
      optimization_guide::OnDeviceModelEligibilityReason::
          kSafetyModelNotAvailable);

  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::mojom::OnDeviceFeature::kScamDetection,
      optimization_guide::OnDeviceModelEligibilityReason::
          kLanguageDetectionModelNotAvailable);

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelDownloadSuccess", true, 0);

  EXPECT_EQ(delegate_->GetIntelligentScanModelType(
                /*log_failed_eligibility_reason=*/true),
            ModelType::kNotSupportedOnDevice);

  // And then send `kSuccess` to the observer, which will log the histogram.
  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::mojom::OnDeviceFeature::kScamDetection,
      optimization_guide::OnDeviceModelEligibilityReason::kSuccess);

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelDownloadSuccess", true, 1);
  histogram_tester_.ExpectTotalCount("SBClientPhishing.OnDeviceModelFetchTime",
                                     1);

  EXPECT_EQ(delegate_->GetIntelligentScanModelType(
                /*log_failed_eligibility_reason=*/true),
            ModelType::kOnDevice);
}

TEST_F(
    ClientSideDetectionIntelligentScanDelegateDesktopTestWithServerModelDisabled,
    TestOnDeviceModelFetchSuccessImmediateSessionCreation) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/false);
  EXPECT_EQ(delegate_->GetIntelligentScanModelType(
                /*log_failed_eligibility_reason=*/true),
            ModelType::kNotSupportedOnDevice);

  testing::NiceMock<MockSession> session;
  EXPECT_CALL(mock_opt_guide_, StartSession(_, _, _))
      .WillOnce(
          [&](optimization_guide::mojom::OnDeviceFeature feature,
              const optimization_guide::SessionConfigParams& config_params,
              base::WeakPtr<OptimizationGuideLogger> logger) {
            return std::make_unique<NiceMock<MockSession>>(&session);
          });
  // No need to add the observer because the session is created immediately.
  EXPECT_CALL(mock_opt_guide_, AddOnDeviceModelAvailabilityChangeObserver(_, _))
      .Times(0);

  SetEnhancedProtectionPrefForTests(&pref_service_, true);

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelDownloadSuccess", true, 1);
  EXPECT_EQ(delegate_->GetIntelligentScanModelType(
                /*log_failed_eligibility_reason=*/true),
            ModelType::kOnDevice);
}

TEST_F(
    ClientSideDetectionIntelligentScanDelegateDesktopTestWithServerModelDisabled,
    TestOnDeviceModelFetchFailureCall) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/false);
  EXPECT_EQ(delegate_->GetIntelligentScanModelType(
                /*log_failed_eligibility_reason=*/true),
            ModelType::kNotSupportedOnDevice);

  optimization_guide::OnDeviceModelAvailabilityObserver* availability_observer =
      nullptr;
  base::RunLoop run_loop_for_add_observer;
  EXPECT_CALL(mock_opt_guide_, AddOnDeviceModelAvailabilityChangeObserver(_, _))
      .WillOnce(
          [&](optimization_guide::mojom::OnDeviceFeature feature,
              optimization_guide::OnDeviceModelAvailabilityObserver* observer) {
            availability_observer = observer;
            run_loop_for_add_observer.Quit();
          });

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelDownloadSuccess", false, 0);

  SetEnhancedProtectionPrefForTests(&pref_service_, true);

  // Now that the delegate is observing, send `kTooManyRecentCrashes`
  // to the observer, which is not a waitable reason.
  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::mojom::OnDeviceFeature::kScamDetection,
      optimization_guide::OnDeviceModelEligibilityReason::
          kTooManyRecentCrashes);

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelDownloadSuccess", false, 1);
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelEligibilityReasonAtDownloadFailure",
      optimization_guide::OnDeviceModelEligibilityReason::kTooManyRecentCrashes,
      1);

  EXPECT_EQ(delegate_->GetIntelligentScanModelType(
                /*log_failed_eligibility_reason=*/true),
            ModelType::kNotSupportedOnDevice);
}

TEST_F(
    ClientSideDetectionIntelligentScanDelegateDesktopTestWithServerModelDisabled,
    TestModelEligibilityReasonCheckAtFailedInquiry) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/false);

  // The below function is called by the delegate when calling
  // GetIntelligentScanModelType, but the on device model is not available yet.
  EXPECT_CALL(mock_opt_guide_, GetOnDeviceModelEligibility(_))
      .WillOnce([&](optimization_guide::mojom::OnDeviceFeature feature) {
        return optimization_guide::OnDeviceModelEligibilityReason::
            kModelToBeInstalled;
      });

  // We will start listening to the on device model when enhanced protection is
  // enabled, so we expect the call below.
  optimization_guide::OnDeviceModelAvailabilityObserver* availability_observer =
      nullptr;
  base::RunLoop run_loop_for_add_observer;
  EXPECT_CALL(mock_opt_guide_, AddOnDeviceModelAvailabilityChangeObserver(_, _))
      .WillOnce(
          [&](optimization_guide::mojom::OnDeviceFeature feature,
              optimization_guide::OnDeviceModelAvailabilityObserver* observer) {
            availability_observer = observer;
            run_loop_for_add_observer.Quit();
          });

  SetEnhancedProtectionPrefForTests(&pref_service_, true);

  run_loop_for_add_observer.Run();
  CHECK(availability_observer);

  // Now that the delegate is observing, send `kConfigNotAvailableForFeature`
  // first to the observer, which will not stop the observing. However, for the
  // purpose of this test, we will never fulfill the request to notify the
  // service class that the model installation is successful.
  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::mojom::OnDeviceFeature::kScamDetection,
      optimization_guide::OnDeviceModelEligibilityReason::
          kConfigNotAvailableForFeature);

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelDownloadSuccess", true, 0);

  EXPECT_EQ(delegate_->GetIntelligentScanModelType(
                /*log_failed_eligibility_reason=*/true),
            ModelType::kNotSupportedOnDevice);

  // We expect the histogram value for
  // SBClientPhishing.OnDeviceModelEligibilityReasonAtInquiryFailure to be
  // kModelTobeInstalled as we set the EXPECT_CALL above when calling for
  // function GetOnDeviceModelEligibility within the optimization guide service,
  // which is called in the service delegate.
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelEligibilityReasonAtInquiryFailure",
      optimization_guide::OnDeviceModelEligibilityReason::kModelToBeInstalled,
      1);

  EXPECT_EQ(delegate_->GetIntelligentScanModelType(
                /*log_failed_eligibility_reason=*/false),
            ModelType::kNotSupportedOnDevice);

  // The histogram is not logged again because
  // log_failed_eligibility_reason is set to false.
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelEligibilityReasonAtInquiryFailure",
      optimization_guide::OnDeviceModelEligibilityReason::kModelToBeInstalled,
      1);

  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::mojom::OnDeviceFeature::kScamDetection,
      optimization_guide::OnDeviceModelEligibilityReason::kSuccess);

  EXPECT_EQ(delegate_->GetIntelligentScanModelType(
                /*log_failed_eligibility_reason=*/true),
            ModelType::kOnDevice);

  // The histogram is not logged again because
  // it is only logged when the model is not available.
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelEligibilityReasonAtInquiryFailure",
      optimization_guide::OnDeviceModelEligibilityReason::kModelToBeInstalled,
      1);
}

TEST_F(
    ClientSideDetectionIntelligentScanDelegateDesktopTestWithServerModelDisabled,
    ModelFetchStopListeningBeforeSuccess) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/false);
  optimization_guide::OnDeviceModelAvailabilityObserver* availability_observer =
      nullptr;
  base::RunLoop run_loop_for_add_observer;
  EXPECT_CALL(mock_opt_guide_, AddOnDeviceModelAvailabilityChangeObserver(_, _))
      .WillOnce(
          [&](optimization_guide::mojom::OnDeviceFeature feature,
              optimization_guide::OnDeviceModelAvailabilityObserver* observer) {
            availability_observer = observer;
            run_loop_for_add_observer.Quit();
          });

  SetEnhancedProtectionPrefForTests(&pref_service_, true);

  run_loop_for_add_observer.Run();
  CHECK(availability_observer);

  SetEnhancedProtectionPrefForTests(&pref_service_, false);

  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::mojom::OnDeviceFeature::kScamDetection,
      optimization_guide::OnDeviceModelEligibilityReason::kSuccess);

  // The delegate should not be available because we stopped listening to the
  // model update before the model was available.
  EXPECT_EQ(delegate_->GetIntelligentScanModelType(
                /*log_failed_eligibility_reason=*/true),
            ModelType::kNotSupportedOnDevice);
}

TEST_F(
    ClientSideDetectionIntelligentScanDelegateDesktopTestWithServerModelDisabled,
    ModelFetchStopListeningAfterSuccess) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/false);
  optimization_guide::OnDeviceModelAvailabilityObserver* availability_observer =
      nullptr;
  base::RunLoop run_loop_for_add_observer;
  EXPECT_CALL(mock_opt_guide_, AddOnDeviceModelAvailabilityChangeObserver(_, _))
      .WillOnce(
          [&](optimization_guide::mojom::OnDeviceFeature feature,
              optimization_guide::OnDeviceModelAvailabilityObserver* observer) {
            availability_observer = observer;
            run_loop_for_add_observer.Quit();
          });

  SetEnhancedProtectionPrefForTests(&pref_service_, true);

  run_loop_for_add_observer.Run();
  CHECK(availability_observer);

  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::mojom::OnDeviceFeature::kScamDetection,
      optimization_guide::OnDeviceModelEligibilityReason::kSuccess);

  EXPECT_EQ(delegate_->GetIntelligentScanModelType(
                /*log_failed_eligibility_reason=*/true),
            ModelType::kOnDevice);

  SetEnhancedProtectionPrefForTests(&pref_service_, false);

  EXPECT_EQ(delegate_->GetIntelligentScanModelType(
                /*log_failed_eligibility_reason=*/true),
            ModelType::kNotSupportedOnDevice);

  // Start listening again should work.
  base::RunLoop run_loop_for_add_observer2;
  EXPECT_CALL(mock_opt_guide_, AddOnDeviceModelAvailabilityChangeObserver(_, _))
      .WillOnce(
          [&](optimization_guide::mojom::OnDeviceFeature feature,
              optimization_guide::OnDeviceModelAvailabilityObserver* observer) {
            availability_observer = observer;
            run_loop_for_add_observer2.Quit();
          });
  SetEnhancedProtectionPrefForTests(&pref_service_, true);

  run_loop_for_add_observer2.Run();
  CHECK(availability_observer);

  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::mojom::OnDeviceFeature::kScamDetection,
      optimization_guide::OnDeviceModelEligibilityReason::kSuccess);

  EXPECT_EQ(delegate_->GetIntelligentScanModelType(
                /*log_failed_eligibility_reason=*/true),
            ModelType::kOnDevice);
}

TEST_F(
    ClientSideDetectionIntelligentScanDelegateDesktopTestWithServerModelDisabled,
    ListenToModelUpdateOnCreation) {
  optimization_guide::OnDeviceModelAvailabilityObserver* availability_observer =
      nullptr;
  base::RunLoop run_loop_for_add_observer;
  EXPECT_CALL(mock_opt_guide_, AddOnDeviceModelAvailabilityChangeObserver(_, _))
      .WillOnce(
          [&](optimization_guide::mojom::OnDeviceFeature feature,
              optimization_guide::OnDeviceModelAvailabilityObserver* observer) {
            availability_observer = observer;
            run_loop_for_add_observer.Quit();
          });

  CreateDelegate(/*is_enhanced_protection_enabled=*/true);

  // Since enhanced protection is enabled, the delegate should start listening
  // to the model update as soon as it is created.
  run_loop_for_add_observer.Run();
  CHECK(availability_observer);

  EXPECT_EQ(delegate_->GetIntelligentScanModelType(
                /*log_failed_eligibility_reason=*/true),
            ModelType::kNotSupportedOnDevice);

  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::mojom::OnDeviceFeature::kScamDetection,
      optimization_guide::OnDeviceModelEligibilityReason::kSuccess);

  EXPECT_EQ(delegate_->GetIntelligentScanModelType(
                /*log_failed_eligibility_reason=*/true),
            ModelType::kOnDevice);
}

TEST_F(
    ClientSideDetectionIntelligentScanDelegateDesktopTestWithServerModelDisabled,
    TestSessionExecutionSuccessButNotComplete) {
  EnableOnDeviceModelWithSession();

  EXPECT_CALL(session_, ExecuteModel(_, _))
      .WillOnce(testing::WithArg<1>(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            callback.Run(OptimizationGuideModelStreamingExecutionResult(
                base::ok(CreateScamDetectionResponse("Google", "Search Engine",
                                                     /*is_complete=*/false)),
                /*provided_by_on_device=*/false,
                /*execution_info=*/CreateExecutionInfo(/*model_version=*/123)));
          }));

  base::test::TestFuture<IntelligentScanResult> future;
  delegate_->StartIntelligentScan("", future.GetCallback());

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelSessionCreationSuccess", true, 1);
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.OnDeviceModelSessionCreationTime", 1);

  // Because the execution result isn't complete yet, we do not intend on
  // tracking the duration or success since we're still waiting. For the purpose
  // of the test, we do not complete the execution result to make sure that
  // they're not logged. We also do not test the model version attached because
  // of this.
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.OnDeviceModelExecutionDuration", 0);
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.OnDeviceModelExecutionSuccess", 0);
}

TEST_F(
    ClientSideDetectionIntelligentScanDelegateDesktopTestWithServerModelDisabled,
    TestOnDeviceModelNoFetchForManagedProfile) {
  std::vector<std::unique_ptr<policy::ManagementStatusProvider>> providers;
  policy::ManagementService management_service(std::move(providers));
  management_service.SetManagementAuthoritiesForTesting(
      policy::EnterpriseManagementAuthority::CLOUD);

  CreateDelegate(/*is_enhanced_protection_enabled=*/false, &management_service);
  EXPECT_EQ(delegate_->GetIntelligentScanModelType(
                /*log_failed_eligibility_reason=*/true),
            ModelType::kNotSupportedOnDevice);

  // We expect that AddOnDeviceModelAvailabilityChangeObserver is NOT called
  // even when ESB is enabled because the profile is managed.
  EXPECT_CALL(mock_opt_guide_, AddOnDeviceModelAvailabilityChangeObserver(_, _))
      .Times(0);

  SetEnhancedProtectionPrefForTests(&pref_service_, true);
}

TEST_F(
    ClientSideDetectionIntelligentScanDelegateDesktopTestWithServerModelDisabled,
    TestOnDeviceModelNoFetchForManagedProfileAtStartup) {
  std::vector<std::unique_ptr<policy::ManagementStatusProvider>> providers;
  policy::ManagementService management_service(std::move(providers));
  management_service.SetManagementAuthoritiesForTesting(
      policy::EnterpriseManagementAuthority::CLOUD);

  // Expect no observer registration even if ESB is already enabled at startup.
  EXPECT_CALL(mock_opt_guide_, AddOnDeviceModelAvailabilityChangeObserver(_, _))
      .Times(0);
  CreateDelegate(/*is_enhanced_protection_enabled=*/true, &management_service);
  EXPECT_EQ(delegate_->GetIntelligentScanModelType(
                /*log_failed_eligibility_reason=*/true),
            ModelType::kNotSupportedOnDevice);
}

}  // namespace safe_browsing
