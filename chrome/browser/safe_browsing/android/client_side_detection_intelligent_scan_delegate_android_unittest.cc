// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/android/client_side_detection_intelligent_scan_delegate_android.h"

#include "base/command_line.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/unguessable_token.h"
#include "components/optimization_guide/core/model_execution/on_device_capability.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_assets.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_broker.h"
#include "components/optimization_guide/core/model_execution/test/feature_config_builder.h"
#include "components/optimization_guide/core/model_execution/test/mock_remote_model_executor.h"
#include "components/optimization_guide/core/model_execution/test/substitution_builder.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/safe_browsing/core/browser/intelligent_scan_delegate.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {
using base::test::EqualsProto;
using optimization_guide::FakeAdaptationAsset;
using optimization_guide::FakeModelBroker;
using optimization_guide::proto::ModelExecutionFeature;
using optimization_guide::proto::OnDeviceModelExecutionFeatureConfig;
using ::testing::_;
using ::testing::NiceMock;
using IntelligentScanResult = IntelligentScanDelegate::IntelligentScanResult;
using ModelType = IntelligentScanDelegate::ModelType;
using RemoteModelExecutionCallback = base::OnceCallback<void(
    optimization_guide::OptimizationGuideModelExecutionResult,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry>)>;

OnDeviceModelExecutionFeatureConfig FeatureConfig() {
  OnDeviceModelExecutionFeatureConfig config;
  config.set_feature(
      ModelExecutionFeature::MODEL_EXECUTION_FEATURE_SCAM_DETECTION);

  auto& input_config = *config.mutable_input_config();
  input_config.set_request_base_name(
      optimization_guide::proto::ScamDetectionRequest().GetTypeName());
  auto& substitution = *input_config.add_execute_substitutions();
  substitution.set_string_template("%s");
  *substitution.add_substitutions()->add_candidates()->mutable_proto_field() =
      optimization_guide::StringValueField();

  auto& output_config = *config.mutable_output_config();
  output_config.set_proto_type(
      optimization_guide::proto::ScamDetectionResponse().GetTypeName());
  *output_config.mutable_proto_field() = optimization_guide::OutputField();
  output_config.set_parser_kind(
      optimization_guide::proto::ParserKind::PARSER_KIND_JSON);

  config.set_can_skip_text_safety(true);
  return config;
}

}  // namespace

class ClientSideDetectionIntelligentScanDelegateAndroidTestBase
    : public testing::Test {
 public:
  ClientSideDetectionIntelligentScanDelegateAndroidTestBase() {
    RegisterProfilePrefs(pref_service_.registry());
  }

 protected:
  void CreateDelegate(bool is_enhanced_protection_enabled) {
    CreateDelegate(
        is_enhanced_protection_enabled,
        ModelExecutionFeature::MODEL_EXECUTION_FEATURE_SCAM_DETECTION);
  }

  void CreateDelegate(bool is_enhanced_protection_enabled,
                      ModelExecutionFeature asset_feature) {
    CreateDelegateWithOnDeviceModelResponse(is_enhanced_protection_enabled,
                                            asset_feature, "");
  }

  void CreateDelegateWithOnDeviceModelResponse(
      bool is_enhanced_protection_enabled,
      ModelExecutionFeature asset_feature,
      std::string response) {
    SetEnhancedProtectionPrefForTests(&pref_service_,
                                      is_enhanced_protection_enabled);
    fake_broker_ =
        std::make_unique<FakeModelBroker>(FakeModelBroker::Options{});
    if (asset_feature ==
        ModelExecutionFeature::MODEL_EXECUTION_FEATURE_SCAM_DETECTION) {
      fake_broker_->UpdateModelAdaptation(fake_asset_);
    }
    fake_broker_->settings().set_execute_result({response});
    auto model_broker_client =
        std::make_unique<optimization_guide::ModelBrokerClient>(
            fake_broker_->BindAndPassRemote(), nullptr);
    delegate_ =
        std::make_unique<ClientSideDetectionIntelligentScanDelegateAndroid>(
            pref_service_, std::move(model_broker_client),
            &remote_model_executor_);
  }

  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  FakeAdaptationAsset fake_asset_{{.config = FeatureConfig()}};
  std::unique_ptr<FakeModelBroker> fake_broker_;
  NiceMock<optimization_guide::MockRemoteModelExecutor> remote_model_executor_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<ClientSideDetectionIntelligentScanDelegateAndroid> delegate_;
};

class ClientSideDetectionIntelligentScanDelegateAndroidTest
    : public ClientSideDetectionIntelligentScanDelegateAndroidTestBase {
 protected:
  ClientSideDetectionIntelligentScanDelegateAndroidTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{kClientSideDetectionSendIntelligentScanInfoAndroid, {}},
         {kClientSideDetectionShowScamVerdictWarningAndroid, {}},
         {kClientSideDetectionImageEmbeddingMatch,
          {{"CsdImageEmbeddingMatchWithIntelligentScan", "true"}}},
         {kClientSideDetectionServerModelForScamDetectionAndroid,
          {{"MaxIntelligentScansPerDay", "3"}}}},
        /*disabled_features=*/{kClientSideDetectionKillswitch});
  }
};

TEST_F(ClientSideDetectionIntelligentScanDelegateAndroidTest,
       ShouldRequestIntelligentScan) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::FORCE_REQUEST);
  verdict.mutable_llama_forced_trigger_info()->set_intelligent_scan(true);
  EXPECT_TRUE(delegate_->ShouldRequestIntelligentScan(&verdict));
}

TEST_F(
    ClientSideDetectionIntelligentScanDelegateAndroidTest,
    ShouldRequestIntelligentScan_ImageEmbeddingMatch_RequestIntelligentScan) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::IMAGE_EMBEDDING_MATCH);
  verdict.set_is_phishing(true);
  EXPECT_TRUE(delegate_->ShouldRequestIntelligentScan(&verdict));
}

TEST_F(
    ClientSideDetectionIntelligentScanDelegateAndroidTest,
    ShouldRequestIntelligentScan_ImageEmbeddingMatch_DoNotRequestIntelligentScan) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::IMAGE_EMBEDDING_MATCH);
  verdict.set_is_phishing(false);
  EXPECT_FALSE(delegate_->ShouldRequestIntelligentScan(&verdict));
}

TEST_F(ClientSideDetectionIntelligentScanDelegateAndroidTest,
       ShouldNotRequestIntelligentScan_EnhancedProtectionDisabled) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/false);
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::FORCE_REQUEST);
  verdict.mutable_llama_forced_trigger_info()->set_intelligent_scan(true);
  EXPECT_FALSE(delegate_->ShouldRequestIntelligentScan(&verdict));
}

TEST_F(ClientSideDetectionIntelligentScanDelegateAndroidTest,
       ShouldNotRequestIntelligentScan_WrongTriggerType) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::POINTER_LOCK_REQUESTED);
  verdict.mutable_llama_forced_trigger_info()->set_intelligent_scan(true);
  EXPECT_FALSE(delegate_->ShouldRequestIntelligentScan(&verdict));

  verdict.set_client_side_detection_type(
      ClientSideDetectionType::KEYBOARD_LOCK_REQUESTED);
  EXPECT_FALSE(delegate_->ShouldRequestIntelligentScan(&verdict));
}

TEST_F(ClientSideDetectionIntelligentScanDelegateAndroidTest,
       ShouldRequestIntelligentScan_CommandLineEnablesKeyboardLockTrigger) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      "scam-detection-keyboard-lock-trigger-android");
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::KEYBOARD_LOCK_REQUESTED);
  EXPECT_TRUE(delegate_->ShouldRequestIntelligentScan(&verdict));
}

TEST_F(ClientSideDetectionIntelligentScanDelegateAndroidTest,
       ShouldNotRequestIntelligentScan_CommandlineDoesNotEnableOtherTrigger) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      "scam-detection-keyboard-lock-trigger-android");
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::POINTER_LOCK_REQUESTED);
  EXPECT_FALSE(delegate_->ShouldRequestIntelligentScan(&verdict));
}

TEST_F(ClientSideDetectionIntelligentScanDelegateAndroidTest,
       ShouldNotRequestIntelligentScan_EmptyLlamaForcedTriggerInfo) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::FORCE_REQUEST);
  EXPECT_FALSE(delegate_->ShouldRequestIntelligentScan(&verdict));
}

TEST_F(ClientSideDetectionIntelligentScanDelegateAndroidTest,
       ShouldNotRequestIntelligentScan_IntelligentScanDisabled) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::FORCE_REQUEST);
  verdict.mutable_llama_forced_trigger_info()->set_intelligent_scan(false);
  EXPECT_FALSE(delegate_->ShouldRequestIntelligentScan(&verdict));
}

TEST_F(ClientSideDetectionIntelligentScanDelegateAndroidTest,
       ShouldShowScamWarning) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);
  EXPECT_FALSE(delegate_->ShouldShowScamWarning(std::nullopt));
  EXPECT_FALSE(delegate_->ShouldShowScamWarning(
      IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_SAFE));
  EXPECT_FALSE(delegate_->ShouldShowScamWarning(
      IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_UNSPECIFIED));
  EXPECT_FALSE(delegate_->ShouldShowScamWarning(
      IntelligentScanVerdict::SCAM_EXPERIMENT_CATCH_ALL_TELEMETRY));
  EXPECT_TRUE(delegate_->ShouldShowScamWarning(
      IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_1));
  EXPECT_TRUE(delegate_->ShouldShowScamWarning(
      IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_2));
  EXPECT_TRUE(delegate_->ShouldShowScamWarning(
      IntelligentScanVerdict::SCAM_EXPERIMENT_CATCH_ALL_ENFORCEMENT));
  // Do not show warnings if the enum value is unknown.
  EXPECT_FALSE(delegate_->ShouldShowScamWarning(
      static_cast<IntelligentScanVerdict>(12345)));
}

TEST_F(ClientSideDetectionIntelligentScanDelegateAndroidTest,
       GetIntelligentScanModelType) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);
  EXPECT_EQ(delegate_->GetIntelligentScanModelType(
                /*log_failed_eligibility_reason=*/false),
            ModelType::kServerSide);
  delegate_->Shutdown();
  EXPECT_EQ(delegate_->GetIntelligentScanModelType(
                /*log_failed_eligibility_reason=*/false),
            ModelType::kNotSupportedServerSide);
}

TEST_F(ClientSideDetectionIntelligentScanDelegateAndroidTest,
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

TEST_F(ClientSideDetectionIntelligentScanDelegateAndroidTest,
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

TEST_F(ClientSideDetectionIntelligentScanDelegateAndroidTest,
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

TEST_F(ClientSideDetectionIntelligentScanDelegateAndroidTest,
       StartIntelligentScan_MultipleInquiries) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);

  delegate_->SetPauseInquiryForTesting(true);
  base::test::TestFuture<IntelligentScanResult> future1;
  delegate_->StartIntelligentScan("test rendered text", future1.GetCallback());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(delegate_->GetAliveInquiryCountForTesting(), 1);

  base::test::TestFuture<IntelligentScanResult> future2;
  delegate_->StartIntelligentScan("test rendered text", future2.GetCallback());
  task_environment_.RunUntilIdle();

  EXPECT_EQ(delegate_->GetAliveInquiryCountForTesting(), 2);
}

TEST_F(ClientSideDetectionIntelligentScanDelegateAndroidTest,
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

TEST_F(ClientSideDetectionIntelligentScanDelegateAndroidTest,
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

TEST_F(ClientSideDetectionIntelligentScanDelegateAndroidTest,
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
}

TEST_F(ClientSideDetectionIntelligentScanDelegateAndroidTest,
       CancelIntelligentScan_MultipleInquiries) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);

  delegate_->SetPauseInquiryForTesting(true);
  std::optional<base::UnguessableToken> scan_id1 =
      delegate_->StartIntelligentScan("test rendered text", base::DoNothing());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(delegate_->GetAliveInquiryCountForTesting(), 1);
  std::optional<base::UnguessableToken> scan_id2 =
      delegate_->StartIntelligentScan("test rendered text", base::DoNothing());
  task_environment_.RunUntilIdle();
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

TEST_F(ClientSideDetectionIntelligentScanDelegateAndroidTest,
       ResetInquiry_EnhancedProtectionDisabled) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);

  delegate_->SetPauseInquiryForTesting(true);
  delegate_->StartIntelligentScan("test rendered text", base::DoNothing());
  EXPECT_EQ(delegate_->GetAliveInquiryCountForTesting(), 1);

  SetEnhancedProtectionPrefForTests(&pref_service_, false);
  // Inquiry should be reset after the enhanced protection is disabled.
  EXPECT_EQ(delegate_->GetAliveInquiryCountForTesting(), 0);
}

TEST_F(ClientSideDetectionIntelligentScanDelegateAndroidTest,
       DoNotStartOnDeviceModelDownload) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(delegate_->GetIntelligentScanModelType(
                /*log_failed_eligibility_reason=*/false),
            ModelType::kServerSide);
  // No on-device model download because the server model is enabled.
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.OnDeviceModelDownloadSuccess", 0);
}

class
    ClientSideDetectionIntelligentScanDelegateAndroidTestWithServerModelDisabled
    : public ClientSideDetectionIntelligentScanDelegateAndroidTestBase {
 protected:
  ClientSideDetectionIntelligentScanDelegateAndroidTestWithServerModelDisabled() {
    feature_list_.InitWithFeatures(
        {kClientSideDetectionSendIntelligentScanInfoAndroid,
         kClientSideDetectionShowScamVerdictWarningAndroid,
         kClientSideDetectionOnDeviceModelLazyDownloadAndroid},
        {kClientSideDetectionKillswitch,
         kClientSideDetectionServerModelForScamDetectionAndroid});
  }
};

TEST_F(
    ClientSideDetectionIntelligentScanDelegateAndroidTestWithServerModelDisabled,
    GetIntelligentScanModelType_ModelAvailable) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(delegate_->GetIntelligentScanModelType(
                /*log_failed_eligibility_reason=*/false),
            ModelType::kOnDevice);
  // No on-device model download because lazy download is enabled.
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.OnDeviceModelDownloadSuccess", 0);
}

TEST_F(
    ClientSideDetectionIntelligentScanDelegateAndroidTestWithServerModelDisabled,
    StartIntelligentScan_OnDeviceModelResponseSuccessful) {
  CreateDelegateWithOnDeviceModelResponse(
      /*is_enhanced_protection_enabled=*/true,
      ModelExecutionFeature::MODEL_EXECUTION_FEATURE_SCAM_DETECTION,
      "{\"brand\": \"test_brand\", \"intent\": \"test_intent\"}");
  base::test::TestFuture<IntelligentScanResult> future;
  delegate_->StartIntelligentScan("test rendered text", future.GetCallback());

  EXPECT_TRUE(future.Get().execution_success);
  EXPECT_EQ(future.Get().model_version, fake_asset_.version());
  EXPECT_EQ(future.Get().brand, "test_brand");
  EXPECT_EQ(future.Get().intent, "test_intent");
  EXPECT_EQ(future.Get().model_type, ModelType::kOnDevice);
  EXPECT_EQ(future.Get().no_info_reason,
            IntelligentScanInfo::NO_INFO_REASON_UNSPECIFIED);
  // Inquiry should be reset after a successful response.
  EXPECT_EQ(delegate_->GetAliveInquiryCountForTesting(), 0);
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.IsOnDeviceModelAvailableOnSessionCreation", true, 1);
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.OnDeviceModelSessionCreationTime", 1);
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelExecutionSuccess", true, 1);
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.OnDeviceModelExecutionDuration", 1);
  // Histograms related to quota should not be logged when server model is
  // disabled.
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.ServerSideModelHitQuotaAtInquiryTime", 0);

  // Model availability check is now handled during inference, so
  // GetIntelligentScanModelType will always return kOnDevice.
  EXPECT_EQ(delegate_->GetIntelligentScanModelType(
                /*log_failed_eligibility_reason=*/false),
            ModelType::kOnDevice);
  base::test::TestFuture<IntelligentScanResult> future2;
  delegate_->StartIntelligentScan("test rendered text", future2.GetCallback());
  EXPECT_TRUE(future2.Get().execution_success);

  // Model download is now handled by the ModelBroker, so we no longer log
  // the download histograms.
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.OnDeviceModelDownloadSuccess", 0);
  histogram_tester_.ExpectTotalCount("SBClientPhishing.OnDeviceModelFetchTime",
                                     0);
}

TEST_F(
    ClientSideDetectionIntelligentScanDelegateAndroidTestWithServerModelDisabled,
    StartIntelligentScan_OnDeviceModelResponseUnsuccessful) {
  CreateDelegateWithOnDeviceModelResponse(
      /*is_enhanced_protection_enabled=*/true,
      ModelExecutionFeature::MODEL_EXECUTION_FEATURE_SCAM_DETECTION, "");
  base::test::TestFuture<IntelligentScanResult> future;
  delegate_->StartIntelligentScan("test rendered text", future.GetCallback());
  EXPECT_FALSE(future.Get().execution_success);
  EXPECT_EQ(future.Get().model_version, fake_asset_.version());
  EXPECT_EQ(future.Get().brand, "");
  EXPECT_EQ(future.Get().intent, "");
  EXPECT_EQ(future.Get().model_type, ModelType::kOnDevice);
  EXPECT_EQ(future.Get().no_info_reason,
            IntelligentScanInfo::ON_DEVICE_MODEL_OUTPUT_MISSING);
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.IsOnDeviceModelAvailableOnSessionCreation", true, 1);
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.OnDeviceModelSessionCreationTime", 1);
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelExecutionSuccess", false, 1);
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.OnDeviceModelExecutionDuration", 1);
}

TEST_F(
    ClientSideDetectionIntelligentScanDelegateAndroidTestWithServerModelDisabled,
    StartIntelligentScan_OnDeviceModelNotAvailable) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);
  // Model availability check is now handled during inference, so
  // GetIntelligentScanModelType will always return kOnDevice.
  EXPECT_EQ(delegate_->GetIntelligentScanModelType(
                /*log_failed_eligibility_reason=*/false),
            ModelType::kOnDevice);
  base::test::TestFuture<IntelligentScanResult> future;
  delegate_->StartIntelligentScan("test rendered text", future.GetCallback());
  // Notify the subscriber that the model is not available.
  fake_broker_->model_provider().UpdateModelImmediatelyForTesting(
      optimization_guide::proto::
          OPTIMIZATION_TARGET_MODEL_EXECUTION_FEATURE_SCAM_DETECTION,
      /*model_info=*/nullptr);
  EXPECT_FALSE(future.Get().execution_success);
  EXPECT_EQ(future.Get().model_version, -1);
  EXPECT_EQ(future.Get().brand, "");
  EXPECT_EQ(future.Get().intent, "");
  EXPECT_EQ(future.Get().model_type, ModelType::kNotSupportedOnDevice);
  EXPECT_EQ(future.Get().no_info_reason,
            IntelligentScanInfo::ON_DEVICE_MODEL_UNAVAILABLE);
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.IsOnDeviceModelAvailableOnSessionCreation", false, 1);

  // Model availability check is now handled during inference, so
  // GetIntelligentScanModelType will always return kOnDevice.
  EXPECT_EQ(delegate_->GetIntelligentScanModelType(
                /*log_failed_eligibility_reason=*/true),
            ModelType::kOnDevice);
  base::test::TestFuture<IntelligentScanResult> future2;
  delegate_->StartIntelligentScan("test rendered text", future2.GetCallback());
  EXPECT_FALSE(future2.Get().execution_success);
  EXPECT_EQ(future.Get().model_type, ModelType::kNotSupportedOnDevice);

  // Model download is now handled by the ModelBroker, so we no longer log
  // the download histograms.
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.OnDeviceModelDownloadSuccess", 0);
  histogram_tester_.ExpectTotalCount("SBClientPhishing.OnDeviceModelFetchTime",
                                     0);
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.OnDeviceModelUnavailableReasonAtInquiry.Android", 0);
}

TEST_F(
    ClientSideDetectionIntelligentScanDelegateAndroidTestWithServerModelDisabled,
    StartIntelligentScan_SecondRequestsBeforeModelDownloadIsCompleted) {
  // Create delegate without providing the model.
  CreateDelegate(/*is_enhanced_protection_enabled=*/true,
                 ModelExecutionFeature::MODEL_EXECUTION_FEATURE_TEST);
  fake_broker_->settings().set_execute_result(
      {"{\"brand\": \"test_brand\", \"intent\": \"test_intent\"}"});

  base::test::TestFuture<IntelligentScanResult> future1;
  delegate_->StartIntelligentScan("test rendered text 1",
                                  future1.GetCallback());
  // The model is not available yet, so the inquiries are pending.
  EXPECT_EQ(delegate_->GetAliveInquiryCountForTesting(), 1);
  EXPECT_FALSE(future1.IsReady());

  // The second inquiry will also wait for the model to be downloaded.
  base::test::TestFuture<IntelligentScanResult> future2;
  delegate_->StartIntelligentScan("test rendered text 2",
                                  future2.GetCallback());
  EXPECT_EQ(delegate_->GetAliveInquiryCountForTesting(), 2);
  EXPECT_FALSE(future2.IsReady());

  // Simulate model download completion.
  fake_broker_->UpdateModelAdaptation(fake_asset_);
  task_environment_.RunUntilIdle();

  // The first inquiry should complete successfully.
  EXPECT_TRUE(future1.Get().execution_success);
  EXPECT_EQ(future1.Get().brand, "test_brand");
  EXPECT_EQ(future1.Get().intent, "test_intent");
  EXPECT_EQ(future1.Get().model_type, ModelType::kOnDevice);
  // The second inquiry should complete successfully.
  EXPECT_TRUE(future2.Get().execution_success);
  EXPECT_EQ(future2.Get().brand, "test_brand");
  EXPECT_EQ(future2.Get().intent, "test_intent");
  EXPECT_EQ(future2.Get().model_type, ModelType::kOnDevice);
}

TEST_F(
    ClientSideDetectionIntelligentScanDelegateAndroidTestWithServerModelDisabled,
    StartIntelligentScan_SecondRequestsAfterModelDownloadIsCompleted) {
  // Create delegate without providing the model.
  CreateDelegate(/*is_enhanced_protection_enabled=*/true,
                 ModelExecutionFeature::MODEL_EXECUTION_FEATURE_TEST);
  fake_broker_->settings().set_execute_result(
      {"{\"brand\": \"test_brand\", \"intent\": \"test_intent\"}"});

  base::test::TestFuture<IntelligentScanResult> future1;
  delegate_->StartIntelligentScan("test rendered text 1",
                                  future1.GetCallback());
  // The model is not available yet, so the inquiries are pending.
  EXPECT_EQ(delegate_->GetAliveInquiryCountForTesting(), 1);
  EXPECT_FALSE(future1.IsReady());
  // Simulate model download completion.
  fake_broker_->UpdateModelAdaptation(fake_asset_);
  task_environment_.RunUntilIdle();

  // The first inquiry should complete successfully.
  EXPECT_TRUE(future1.Get().execution_success);
  EXPECT_EQ(future1.Get().brand, "test_brand");
  EXPECT_EQ(future1.Get().intent, "test_intent");
  EXPECT_EQ(future1.Get().model_type, ModelType::kOnDevice);

  // The second inquiry comes in after the model is downloaded.
  base::test::TestFuture<IntelligentScanResult> future2;
  delegate_->StartIntelligentScan("test rendered text 2",
                                  future2.GetCallback());

  EXPECT_TRUE(future2.Get().execution_success);
  EXPECT_EQ(future2.Get().brand, "test_brand");
  EXPECT_EQ(future2.Get().intent, "test_intent");
  EXPECT_EQ(future2.Get().model_type, ModelType::kOnDevice);
}

TEST_F(
    ClientSideDetectionIntelligentScanDelegateAndroidTestWithServerModelDisabled,
    StartIntelligentScan_QuotaCheckIsDisabled) {
  CreateDelegateWithOnDeviceModelResponse(
      /*is_enhanced_protection_enabled=*/true,
      ModelExecutionFeature::MODEL_EXECUTION_FEATURE_SCAM_DETECTION,
      "{\"brand\": \"test_brand\", \"intent\": \"test_intent\"}");
  // With server model disabled, quota should not be enforced. We can make
  // more than 5 calls (default quota).
  for (int i = 0; i < 10; ++i) {
    base::test::TestFuture<IntelligentScanResult> future;
    delegate_->StartIntelligentScan("test rendered text", future.GetCallback());
    EXPECT_TRUE(future.Get().execution_success);
  }
}

class
    ClientSideDetectionIntelligentScanDelegateAndroidTestWithOnDeviceModelLazyDownloadDisabled
    : public ClientSideDetectionIntelligentScanDelegateAndroidTestBase {
 protected:
  ClientSideDetectionIntelligentScanDelegateAndroidTestWithOnDeviceModelLazyDownloadDisabled() {
    feature_list_.InitWithFeatures(
        {kClientSideDetectionSendIntelligentScanInfoAndroid,
         kClientSideDetectionShowScamVerdictWarningAndroid},
        {kClientSideDetectionKillswitch,
         kClientSideDetectionServerModelForScamDetectionAndroid,
         kClientSideDetectionOnDeviceModelLazyDownloadAndroid});
  }
};

TEST_F(
    ClientSideDetectionIntelligentScanDelegateAndroidTestWithOnDeviceModelLazyDownloadDisabled,
    GetIntelligentScanModelType_EnhancedProtectionDisabled) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/false);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(delegate_->GetIntelligentScanModelType(
                /*log_failed_eligibility_reason=*/false),
            ModelType::kNotSupportedOnDevice);
}

TEST_F(
    ClientSideDetectionIntelligentScanDelegateAndroidTestWithOnDeviceModelLazyDownloadDisabled,
    GetIntelligentScanModelType_EnhancedProtectionEnabledAfterStartup) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/false);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(delegate_->GetIntelligentScanModelType(
                /*log_failed_eligibility_reason=*/false),
            ModelType::kNotSupportedOnDevice);

  SetEnhancedProtectionPrefForTests(&pref_service_, true);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(delegate_->GetIntelligentScanModelType(
                /*log_failed_eligibility_reason=*/false),
            ModelType::kOnDevice);
}

TEST_F(
    ClientSideDetectionIntelligentScanDelegateAndroidTestWithOnDeviceModelLazyDownloadDisabled,
    GetIntelligentScanModelType_FeatureNotAvailable) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true,
                 ModelExecutionFeature::MODEL_EXECUTION_FEATURE_TEST);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(delegate_->GetIntelligentScanModelType(
                /*log_failed_eligibility_reason=*/false),
            ModelType::kNotSupportedOnDevice);
  // Not logged because log_failed_eligibility_reason is false.
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.OnDeviceModelUnavailableReasonAtInquiry.Android", 0);
}

TEST_F(
    ClientSideDetectionIntelligentScanDelegateAndroidTestWithOnDeviceModelLazyDownloadDisabled,
    GetIntelligentScanModelType_LogsUnavailableReason) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true,
                 ModelExecutionFeature::MODEL_EXECUTION_FEATURE_TEST);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(delegate_->GetIntelligentScanModelType(
                /*log_failed_eligibility_reason=*/true),
            ModelType::kNotSupportedOnDevice);
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelUnavailableReasonAtInquiry.Android",
      optimization_guide::mojom::ModelUnavailableReason::kPendingAssets, 1);
}

TEST_F(
    ClientSideDetectionIntelligentScanDelegateAndroidTestWithOnDeviceModelLazyDownloadDisabled,
    GetIntelligentScanModelType_ModelAvailable) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(delegate_->GetIntelligentScanModelType(
                /*log_failed_eligibility_reason=*/false),
            ModelType::kOnDevice);
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelDownloadSuccess", true, 1);
  histogram_tester_.ExpectTotalCount("SBClientPhishing.OnDeviceModelFetchTime",
                                     1);
}

TEST_F(
    ClientSideDetectionIntelligentScanDelegateAndroidTestWithOnDeviceModelLazyDownloadDisabled,
    StartIntelligentScan_OnDeviceModelNotAvailable) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/false);
  task_environment_.RunUntilIdle();
  base::test::TestFuture<IntelligentScanResult> future;
  delegate_->StartIntelligentScan("test rendered text", future.GetCallback());
  EXPECT_FALSE(future.Get().execution_success);
  EXPECT_EQ(future.Get().model_version, -1);
  EXPECT_EQ(future.Get().brand, "");
  EXPECT_EQ(future.Get().intent, "");
  EXPECT_EQ(future.Get().model_type, ModelType::kNotSupportedOnDevice);
  EXPECT_EQ(future.Get().no_info_reason,
            IntelligentScanInfo::ON_DEVICE_MODEL_UNAVAILABLE);
}

class
    ClientSideDetectionIntelligentScanDelegateAndroidTestWithSendScanInfoDisabled
    : public ClientSideDetectionIntelligentScanDelegateAndroidTestBase {
 protected:
  ClientSideDetectionIntelligentScanDelegateAndroidTestWithSendScanInfoDisabled() {
    feature_list_.InitWithFeatures(
        {kClientSideDetectionServerModelForScamDetectionAndroid},
        {kClientSideDetectionSendIntelligentScanInfoAndroid});
  }
};

TEST_F(
    ClientSideDetectionIntelligentScanDelegateAndroidTestWithSendScanInfoDisabled,
    ShouldRequestIntelligentScan) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::FORCE_REQUEST);
  verdict.mutable_llama_forced_trigger_info()->set_intelligent_scan(true);
  // Enabling the server model flag should be sufficient to request intelligent
  // scan.
  EXPECT_TRUE(delegate_->ShouldRequestIntelligentScan(&verdict));
}

TEST_F(
    ClientSideDetectionIntelligentScanDelegateAndroidTestWithSendScanInfoDisabled,
    GetIntelligentScanModelType) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);
  task_environment_.RunUntilIdle();
  // Enabling the server model flag should be sufficient to request intelligent
  // scan.
  EXPECT_EQ(delegate_->GetIntelligentScanModelType(
                /*log_failed_eligibility_reason=*/false),
            ModelType::kServerSide);
}

class
    ClientSideDetectionIntelligentScanDelegateAndroidTestWithSendScanInfoAndServerModelDisabled
    : public ClientSideDetectionIntelligentScanDelegateAndroidTestBase {
 protected:
  ClientSideDetectionIntelligentScanDelegateAndroidTestWithSendScanInfoAndServerModelDisabled() {
    feature_list_.InitWithFeatures(
        {}, {kClientSideDetectionSendIntelligentScanInfoAndroid,
             kClientSideDetectionServerModelForScamDetectionAndroid,
             kClientSideDetectionImageEmbeddingMatch});
  }
};

TEST_F(
    ClientSideDetectionIntelligentScanDelegateAndroidTestWithSendScanInfoAndServerModelDisabled,
    ShouldNotRequestIntelligentScan) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::FORCE_REQUEST);
  verdict.mutable_llama_forced_trigger_info()->set_intelligent_scan(true);
  EXPECT_FALSE(delegate_->ShouldRequestIntelligentScan(&verdict));
}

TEST_F(
    ClientSideDetectionIntelligentScanDelegateAndroidTestWithSendScanInfoAndServerModelDisabled,
    GetIntelligentScanModelType) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(delegate_->GetIntelligentScanModelType(
                /*log_failed_eligibility_reason=*/false),
            ModelType::kNotSupportedOnDevice);
}

class ClientSideDetectionIntelligentScanDelegateAndroidTestWithWarningDisabled
    : public ClientSideDetectionIntelligentScanDelegateAndroidTestBase {
 protected:
  ClientSideDetectionIntelligentScanDelegateAndroidTestWithWarningDisabled() {
    feature_list_.InitWithFeatures(
        {kClientSideDetectionSendIntelligentScanInfoAndroid,
         kClientSideDetectionServerModelForScamDetectionAndroid},
        {kClientSideDetectionKillswitch,
         kClientSideDetectionShowScamVerdictWarningAndroid});
  }
};

TEST_F(ClientSideDetectionIntelligentScanDelegateAndroidTestWithWarningDisabled,
       ShouldShowScamWarning) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);
  // Enabling the server model flag should be sufficient to show scam warning.
  EXPECT_TRUE(delegate_->ShouldShowScamWarning(
      IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_1));
  EXPECT_TRUE(delegate_->ShouldShowScamWarning(
      IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_2));
  EXPECT_TRUE(delegate_->ShouldShowScamWarning(
      IntelligentScanVerdict::SCAM_EXPERIMENT_CATCH_ALL_ENFORCEMENT));
}

class
    ClientSideDetectionIntelligentScanDelegateAndroidTestWithWarningAndServerModelDisabled
    : public ClientSideDetectionIntelligentScanDelegateAndroidTestBase {
 protected:
  ClientSideDetectionIntelligentScanDelegateAndroidTestWithWarningAndServerModelDisabled() {
    feature_list_.InitWithFeatures(
        {kClientSideDetectionSendIntelligentScanInfoAndroid},
        {kClientSideDetectionKillswitch,
         kClientSideDetectionShowScamVerdictWarningAndroid,
         kClientSideDetectionServerModelForScamDetectionAndroid});
  }
};

TEST_F(
    ClientSideDetectionIntelligentScanDelegateAndroidTestWithWarningAndServerModelDisabled,
    ShouldNotShowScamWarning) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);
  EXPECT_FALSE(delegate_->ShouldShowScamWarning(
      IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_1));
  EXPECT_FALSE(delegate_->ShouldShowScamWarning(
      IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_2));
  EXPECT_FALSE(delegate_->ShouldShowScamWarning(
      IntelligentScanVerdict::SCAM_EXPERIMENT_CATCH_ALL_ENFORCEMENT));
}

class ClientSideDetectionIntelligentScanDelegateAndroidTestWithKillSwitchEnabled
    : public ClientSideDetectionIntelligentScanDelegateAndroidTestBase {
 protected:
  ClientSideDetectionIntelligentScanDelegateAndroidTestWithKillSwitchEnabled() {
    feature_list_.InitWithFeatures(
        {kClientSideDetectionKillswitch,
         kClientSideDetectionServerModelForScamDetectionAndroid},
        {});
  }
};

TEST_F(
    ClientSideDetectionIntelligentScanDelegateAndroidTestWithKillSwitchEnabled,
    GetIntelligentScanModelType) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);
  EXPECT_EQ(delegate_->GetIntelligentScanModelType(
                /*log_failed_eligibility_reason=*/false),
            ModelType::kNotSupportedServerSide);
}

class
    ClientSideDetectionIntelligentScanDelegateAndroidTestWithImageEmbeddingDisabled
    : public ClientSideDetectionIntelligentScanDelegateAndroidTestBase {
 protected:
  ClientSideDetectionIntelligentScanDelegateAndroidTestWithImageEmbeddingDisabled() {
    feature_list_.InitWithFeaturesAndParameters(
        {{kClientSideDetectionSendIntelligentScanInfoAndroid, {}},
         {kClientSideDetectionImageEmbeddingMatch,
          {{"CsdImageEmbeddingMatchWithIntelligentScan", "false"}}}},
        /*disabled_features=*/{kClientSideDetectionKillswitch});
  }
};

TEST_F(
    ClientSideDetectionIntelligentScanDelegateAndroidTestWithImageEmbeddingDisabled,
    ShouldRequestIntelligentScan_DoNotRequestIntelligentScan) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::IMAGE_EMBEDDING_MATCH);
  verdict.set_is_phishing(true);
  EXPECT_FALSE(delegate_->ShouldRequestIntelligentScan(&verdict));
}

}  // namespace safe_browsing
