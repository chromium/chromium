// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/android/client_side_detection_intelligent_scan_delegate_android.h"

#include "base/command_line.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_broker.h"
#include "components/optimization_guide/core/model_execution/test/feature_config_builder.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

using optimization_guide::FakeAdaptationAsset;
using optimization_guide::proto::ModelExecutionFeature;
using optimization_guide::proto::OnDeviceModelExecutionFeatureConfig;
using ::testing::_;
using ::testing::ByMove;
using ::testing::Return;

class ClientSideDetectionIntelligentScanDelegateAndroidTestBase
    : public testing::Test {
 public:
  ClientSideDetectionIntelligentScanDelegateAndroidTestBase() {
    RegisterProfilePrefs(pref_service_.registry());
  }

 protected:
  void CreateDelegate(bool is_enhanced_protection_enabled,
                      ModelExecutionFeature asset_feature) {
    SetEnhancedProtectionPrefForTests(&pref_service_,
                                      is_enhanced_protection_enabled);
    fake_broker_ = std::make_unique<optimization_guide::FakeModelBroker>(
        GetFakeAsset(asset_feature));
    auto model_broker_client =
        std::make_unique<optimization_guide::ModelBrokerClient>(
            fake_broker_->BindAndPassRemote(),
            optimization_guide::CreateSessionArgs(nullptr, {}));
    delegate_ =
        std::make_unique<ClientSideDetectionIntelligentScanDelegateAndroid>(
            pref_service_, std::move(model_broker_client));
  }

  FakeAdaptationAsset GetFakeAsset(ModelExecutionFeature feature) {
    return FakeAdaptationAsset({.config = [feature] {
      OnDeviceModelExecutionFeatureConfig config;
      config.set_feature(feature);
      config.set_can_skip_text_safety(true);
      return config;
    }()});
  }

  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  std::unique_ptr<optimization_guide::FakeModelBroker> fake_broker_;
  std::unique_ptr<ClientSideDetectionIntelligentScanDelegateAndroid> delegate_;
};

class ClientSideDetectionIntelligentScanDelegateAndroidTest
    : public ClientSideDetectionIntelligentScanDelegateAndroidTestBase {
 protected:
  ClientSideDetectionIntelligentScanDelegateAndroidTest() {
    feature_list_.InitWithFeatures(
        {kClientSideDetectionSendIntelligentScanInfoAndroid},
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
}

TEST_F(ClientSideDetectionIntelligentScanDelegateAndroidTest,
       IsOnDeviceModelAvailable_FeatureNotAvailable) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true,
                 ModelExecutionFeature::MODEL_EXECUTION_FEATURE_TEST);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(delegate_->IsOnDeviceModelAvailable(
      /*log_failed_eligibility_reason=*/false));
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
  EXPECT_FALSE(delegate_->ShouldShowScamWarning(
      IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_1));
  EXPECT_FALSE(delegate_->ShouldShowScamWarning(
      IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_2));
  EXPECT_FALSE(delegate_->ShouldShowScamWarning(
      IntelligentScanVerdict::SCAM_EXPERIMENT_CATCH_ALL_ENFORCEMENT));
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
