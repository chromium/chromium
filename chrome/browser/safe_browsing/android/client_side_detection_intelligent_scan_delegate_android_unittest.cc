// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/android/client_side_detection_intelligent_scan_delegate_android.h"

#include "base/command_line.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/unguessable_token.h"
#include "components/optimization_guide/core/model_execution/on_device_capability.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_assets.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_broker.h"
#include "components/optimization_guide/core/model_execution/test/feature_config_builder.h"
#include "components/optimization_guide/core/model_execution/test/substitution_builder.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {
using optimization_guide::FakeAdaptationAsset;
using optimization_guide::FakeModelBroker;
using optimization_guide::proto::ModelExecutionFeature;
using optimization_guide::proto::OnDeviceModelExecutionFeatureConfig;
using ::testing::_;
using IntelligentScanResult =
    ClientSideDetectionHost::IntelligentScanDelegate::IntelligentScanResult;

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
  void CreateDelegate(bool is_enhanced_protection_enabled,
                      ModelExecutionFeature asset_feature) {
    CreateDelegateWithSessionResponse(is_enhanced_protection_enabled,
                                      asset_feature, "");
  }

  void CreateDelegateWithSessionResponse(bool is_enhanced_protection_enabled,
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
            pref_service_, std::move(model_broker_client));
  }

  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  FakeAdaptationAsset fake_asset_{{.config = FeatureConfig()}};
  std::unique_ptr<FakeModelBroker> fake_broker_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<ClientSideDetectionIntelligentScanDelegateAndroid> delegate_;
};

class ClientSideDetectionIntelligentScanDelegateAndroidTest
    : public ClientSideDetectionIntelligentScanDelegateAndroidTestBase {
 protected:
  ClientSideDetectionIntelligentScanDelegateAndroidTest() {
    feature_list_.InitWithFeatures(
        {kClientSideDetectionSendIntelligentScanInfoAndroid,
         kClientSideDetectionShowScamVerdictWarningAndroid},
        {kClientSideDetectionKillswitch});
  }
};

TEST_F(ClientSideDetectionIntelligentScanDelegateAndroidTest,
       ShouldRequestIntelligentScan) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true,
                 ModelExecutionFeature::MODEL_EXECUTION_FEATURE_SCAM_DETECTION);
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::FORCE_REQUEST);
  verdict.mutable_llama_forced_trigger_info()->set_intelligent_scan(true);
  EXPECT_TRUE(delegate_->ShouldRequestIntelligentScan(&verdict));
}

TEST_F(ClientSideDetectionIntelligentScanDelegateAndroidTest,
       ShouldNotRequestIntelligentScan_EnhancedProtectionDisabled) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/false,
                 ModelExecutionFeature::MODEL_EXECUTION_FEATURE_SCAM_DETECTION);
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::FORCE_REQUEST);
  verdict.mutable_llama_forced_trigger_info()->set_intelligent_scan(true);
  EXPECT_FALSE(delegate_->ShouldRequestIntelligentScan(&verdict));
}

TEST_F(ClientSideDetectionIntelligentScanDelegateAndroidTest,
       ShouldNotRequestIntelligentScan_WrongTriggerType) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true,
                 ModelExecutionFeature::MODEL_EXECUTION_FEATURE_SCAM_DETECTION);
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
  CreateDelegate(/*is_enhanced_protection_enabled=*/true,
                 ModelExecutionFeature::MODEL_EXECUTION_FEATURE_SCAM_DETECTION);
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::KEYBOARD_LOCK_REQUESTED);
  EXPECT_TRUE(delegate_->ShouldRequestIntelligentScan(&verdict));
}

TEST_F(ClientSideDetectionIntelligentScanDelegateAndroidTest,
       ShouldNotRequestIntelligentScan_CommandlineDoesNotEnableOtherTrigger) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      "scam-detection-keyboard-lock-trigger-android");
  CreateDelegate(/*is_enhanced_protection_enabled=*/true,
                 ModelExecutionFeature::MODEL_EXECUTION_FEATURE_SCAM_DETECTION);
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::POINTER_LOCK_REQUESTED);
  EXPECT_FALSE(delegate_->ShouldRequestIntelligentScan(&verdict));
}

TEST_F(ClientSideDetectionIntelligentScanDelegateAndroidTest,
       ShouldNotRequestIntelligentScan_EmptyLlamaForcedTriggerInfo) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true,
                 ModelExecutionFeature::MODEL_EXECUTION_FEATURE_SCAM_DETECTION);
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::FORCE_REQUEST);
  EXPECT_FALSE(delegate_->ShouldRequestIntelligentScan(&verdict));
}

TEST_F(ClientSideDetectionIntelligentScanDelegateAndroidTest,
       ShouldNotRequestIntelligentScan_IntelligentScanDisabled) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true,
                 ModelExecutionFeature::MODEL_EXECUTION_FEATURE_SCAM_DETECTION);
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::FORCE_REQUEST);
  verdict.mutable_llama_forced_trigger_info()->set_intelligent_scan(false);
  EXPECT_FALSE(delegate_->ShouldRequestIntelligentScan(&verdict));
}

TEST_F(ClientSideDetectionIntelligentScanDelegateAndroidTest,
       IsOnDeviceModelAvailable_ModelAvailable) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true,
                 ModelExecutionFeature::MODEL_EXECUTION_FEATURE_SCAM_DETECTION);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(delegate_->IsOnDeviceModelAvailable(
      /*log_failed_eligibility_reason=*/false));
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelDownloadSuccess", true, 1);
  histogram_tester_.ExpectTotalCount("SBClientPhishing.OnDeviceModelFetchTime",
                                     1);
}

TEST_F(ClientSideDetectionIntelligentScanDelegateAndroidTest,
       IsOnDeviceModelAvailable_FeatureNotAvailable) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true,
                 ModelExecutionFeature::MODEL_EXECUTION_FEATURE_TEST);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(delegate_->IsOnDeviceModelAvailable(
      /*log_failed_eligibility_reason=*/false));
  // Not logged because log_failed_eligibility_reason is false.
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.OnDeviceModelUnavailableReasonAtInquiry.Android", 0);
}

TEST_F(ClientSideDetectionIntelligentScanDelegateAndroidTest,
       IsOnDeviceModelAvailable_LogsUnavailableReason) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true,
                 ModelExecutionFeature::MODEL_EXECUTION_FEATURE_TEST);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(delegate_->IsOnDeviceModelAvailable(
      /*log_failed_eligibility_reason=*/true));
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelUnavailableReasonAtInquiry.Android",
      optimization_guide::mojom::ModelUnavailableReason::kPendingAssets, 1);
}

TEST_F(ClientSideDetectionIntelligentScanDelegateAndroidTest,
       IsOnDeviceModelAvailable_EnhancedProtectionDisabled) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/false,
                 ModelExecutionFeature::MODEL_EXECUTION_FEATURE_SCAM_DETECTION);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(delegate_->IsOnDeviceModelAvailable(
      /*log_failed_eligibility_reason=*/false));
}

TEST_F(ClientSideDetectionIntelligentScanDelegateAndroidTest,
       IsOnDeviceModelAvailable_EnhancedProtectionEnabledAfterStartup) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/false,
                 ModelExecutionFeature::MODEL_EXECUTION_FEATURE_SCAM_DETECTION);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(delegate_->IsOnDeviceModelAvailable(
      /*log_failed_eligibility_reason=*/false));

  SetEnhancedProtectionPrefForTests(&pref_service_, true);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(delegate_->IsOnDeviceModelAvailable(
      /*log_failed_eligibility_reason=*/false));
}

TEST_F(ClientSideDetectionIntelligentScanDelegateAndroidTest,
       ShouldShowScamWarning) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true,
                 ModelExecutionFeature::MODEL_EXECUTION_FEATURE_SCAM_DETECTION);
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
       InquireOnDeviceModel_ResponseSuccessful) {
  CreateDelegateWithSessionResponse(
      /*is_enhanced_protection_enabled=*/true,
      ModelExecutionFeature::MODEL_EXECUTION_FEATURE_SCAM_DETECTION,
      "{\"brand\": \"test_brand\", \"intent\": \"test_intent\"}");
  // Wait for the model to be available.
  task_environment_.RunUntilIdle();
  base::test::TestFuture<IntelligentScanResult> future;
  delegate_->InquireOnDeviceModel("test rendered text", future.GetCallback());

  EXPECT_TRUE(future.Get().execution_success);
  EXPECT_EQ(future.Get().model_version, fake_asset_.version());
  EXPECT_EQ(future.Get().brand, "test_brand");
  EXPECT_EQ(future.Get().intent, "test_intent");
  // Session should be reset after a successful response.
  EXPECT_EQ(delegate_->GetAliveSessionCountForTesting(), 0);
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.OnDeviceModelSessionCreationTime", 1);
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelExecutionSuccess", true, 1);
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.OnDeviceModelExecutionDuration", 1);
}

TEST_F(ClientSideDetectionIntelligentScanDelegateAndroidTest,
       InquireOnDeviceModel_ResponseUnsuccessful) {
  CreateDelegateWithSessionResponse(
      /*is_enhanced_protection_enabled=*/true,
      ModelExecutionFeature::MODEL_EXECUTION_FEATURE_SCAM_DETECTION, "");
  // Wait for the model to be available.
  task_environment_.RunUntilIdle();
  base::test::TestFuture<IntelligentScanResult> future;
  delegate_->InquireOnDeviceModel("test rendered text", future.GetCallback());
  EXPECT_FALSE(future.Get().execution_success);
  EXPECT_EQ(future.Get().model_version, fake_asset_.version());
  EXPECT_EQ(future.Get().brand, "");
  EXPECT_EQ(future.Get().intent, "");
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.OnDeviceModelSessionCreationTime", 1);
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelExecutionSuccess", false, 1);
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.OnDeviceModelExecutionDuration", 1);
}

TEST_F(ClientSideDetectionIntelligentScanDelegateAndroidTest,
       InquireOnDeviceModel_OnDeviceModelNotAvailable) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/false,
                 ModelExecutionFeature::MODEL_EXECUTION_FEATURE_SCAM_DETECTION);
  task_environment_.RunUntilIdle();
  base::test::TestFuture<IntelligentScanResult> future;
  delegate_->InquireOnDeviceModel("test rendered text", future.GetCallback());
  EXPECT_FALSE(future.Get().execution_success);
  EXPECT_EQ(future.Get().model_version, -1);
  EXPECT_EQ(future.Get().brand, "");
  EXPECT_EQ(future.Get().intent, "");
}

TEST_F(ClientSideDetectionIntelligentScanDelegateAndroidTest,
       InquireOnDeviceModel_SecondInquiryBeforeFirstResponse) {
  CreateDelegateWithSessionResponse(
      /*is_enhanced_protection_enabled=*/true,
      ModelExecutionFeature::MODEL_EXECUTION_FEATURE_SCAM_DETECTION,
      "{\"brand\": \"test_brand\", \"intent\": \"test_intent\"}");
  task_environment_.RunUntilIdle();

  delegate_->SetPauseSessionExecutionForTesting(true);
  base::test::TestFuture<IntelligentScanResult> future1;
  delegate_->InquireOnDeviceModel("test rendered text", future1.GetCallback());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(delegate_->GetAliveSessionCountForTesting(), 1);

  // The second inquire is sent before the first one completes.
  delegate_->SetPauseSessionExecutionForTesting(false);
  base::test::TestFuture<IntelligentScanResult> future2;
  delegate_->InquireOnDeviceModel("test rendered text", future2.GetCallback());
  task_environment_.RunUntilIdle();

  // Only the second inquire callback should be called.
  EXPECT_FALSE(future1.IsReady());
  EXPECT_TRUE(future2.IsReady());
}

TEST_F(ClientSideDetectionIntelligentScanDelegateAndroidTest,
       CancelOnDeviceSession_AfterSessionCreation) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true,
                 ModelExecutionFeature::MODEL_EXECUTION_FEATURE_SCAM_DETECTION);
  task_environment_.RunUntilIdle();
  delegate_->SetPauseSessionExecutionForTesting(true);
  std::optional<base::UnguessableToken> session_id =
      delegate_->InquireOnDeviceModel("test rendered text", base::DoNothing());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(delegate_->GetAliveSessionCountForTesting(), 1);

  // Reset the session after session is created.
  EXPECT_TRUE(delegate_->CancelSession(*session_id));
  EXPECT_EQ(delegate_->GetAliveSessionCountForTesting(), 0);
}

TEST_F(ClientSideDetectionIntelligentScanDelegateAndroidTest,
       ResetOnDeviceSession_EnhancedProtectionDisabled) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true,
                 ModelExecutionFeature::MODEL_EXECUTION_FEATURE_SCAM_DETECTION);
  task_environment_.RunUntilIdle();
  delegate_->SetPauseSessionExecutionForTesting(true);
  delegate_->InquireOnDeviceModel("test rendered text", base::DoNothing());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(delegate_->GetAliveSessionCountForTesting(), 1);
  SetEnhancedProtectionPrefForTests(&pref_service_, false);
  task_environment_.RunUntilIdle();
  // Session should be reset after the enhanced protection is disabled.
  EXPECT_EQ(delegate_->GetAliveSessionCountForTesting(), 0);
}

class ClientSideDetectionIntelligentScanDelegateAndroidTestWithFeatureDisabled
    : public ClientSideDetectionIntelligentScanDelegateAndroidTestBase {
 protected:
  ClientSideDetectionIntelligentScanDelegateAndroidTestWithFeatureDisabled() {
    feature_list_.InitAndDisableFeature(
        kClientSideDetectionSendIntelligentScanInfoAndroid);
  }
};

TEST_F(ClientSideDetectionIntelligentScanDelegateAndroidTestWithFeatureDisabled,
       ShouldNotRequestIntelligentScan_FeatureDisabled) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true,
                 ModelExecutionFeature::MODEL_EXECUTION_FEATURE_SCAM_DETECTION);
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::FORCE_REQUEST);
  verdict.mutable_llama_forced_trigger_info()->set_intelligent_scan(true);
  EXPECT_FALSE(delegate_->ShouldRequestIntelligentScan(&verdict));
}

TEST_F(ClientSideDetectionIntelligentScanDelegateAndroidTestWithFeatureDisabled,
       IsOnDeviceModelAvailable) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true,
                 ModelExecutionFeature::MODEL_EXECUTION_FEATURE_SCAM_DETECTION);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(delegate_->IsOnDeviceModelAvailable(
      /*log_failed_eligibility_reason=*/false));
}

class ClientSideDetectionIntelligentScanDelegateAndroidTestWithWarningDisabled
    : public ClientSideDetectionIntelligentScanDelegateAndroidTestBase {
 protected:
  ClientSideDetectionIntelligentScanDelegateAndroidTestWithWarningDisabled() {
    feature_list_.InitWithFeatures(
        {kClientSideDetectionSendIntelligentScanInfoAndroid},
        {kClientSideDetectionKillswitch,
         kClientSideDetectionShowScamVerdictWarningAndroid});
  }
};

TEST_F(ClientSideDetectionIntelligentScanDelegateAndroidTestWithWarningDisabled,
       ShouldShowScamWarning) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true,
                 ModelExecutionFeature::MODEL_EXECUTION_FEATURE_SCAM_DETECTION);
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
    feature_list_.InitAndEnableFeature(kClientSideDetectionKillswitch);
  }
};

TEST_F(
    ClientSideDetectionIntelligentScanDelegateAndroidTestWithKillSwitchEnabled,
    IsOnDeviceModelAvailable) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true,
                 ModelExecutionFeature::MODEL_EXECUTION_FEATURE_SCAM_DETECTION);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(delegate_->IsOnDeviceModelAvailable(
      /*log_failed_eligibility_reason=*/false));
}

}  // namespace safe_browsing
