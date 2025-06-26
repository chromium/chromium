// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/client_side_detection_intelligent_scan_delegate_desktop.h"

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "components/optimization_guide/core/mock_optimization_guide_model_executor.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::optimization_guide::MockSession;
using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;

namespace safe_browsing {

class ClientSideDetectionIntelligentScanDelegateDesktopTest
    : public testing::Test {
 public:
  ClientSideDetectionIntelligentScanDelegateDesktopTest() {
    feature_list_.InitWithFeatures(
        {kClientSideDetectionBrandAndIntentForScamDetection,
         kClientSideDetectionLlamaForcedTriggerInfoForScamDetection},
        {});
    RegisterProfilePrefs(pref_service_.registry());
    SetEnhancedProtectionPrefForTests(&pref_service_, true);

    delegate_ =
        std::make_unique<ClientSideDetectionIntelligentScanDelegateDesktop>(
            pref_service_, &mock_opt_guide_);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  NiceMock<MockOptimizationGuideKeyedService> mock_opt_guide_;
  std::unique_ptr<ClientSideDetectionIntelligentScanDelegateDesktop> delegate_;
  base::HistogramTester histogram_tester_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTest,
       ShouldRequestIntelligentScan_KeyboardLockRequested) {
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::KEYBOARD_LOCK_REQUESTED);
  EXPECT_TRUE(delegate_->ShouldRequestIntelligentScan(&verdict));
}

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTest,
       ShouldRequestIntelligentScan_IntelligentScanRequested) {
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::FORCE_REQUEST);
  verdict.mutable_llama_forced_trigger_info()->set_intelligent_scan(true);
  EXPECT_TRUE(delegate_->ShouldRequestIntelligentScan(&verdict));
}

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTest,
       ShouldNotRequestIntelligentScan_PointerLockRequested) {
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::POINTER_LOCK_REQUESTED);
  EXPECT_FALSE(delegate_->ShouldRequestIntelligentScan(&verdict));
}

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTest,
       ShouldNotRequestIntelligentScan_EnhancedProtectionDisabled) {
  SetEnhancedProtectionPrefForTests(&pref_service_, false);
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::KEYBOARD_LOCK_REQUESTED);
  EXPECT_FALSE(delegate_->ShouldRequestIntelligentScan(&verdict));
}

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTest,
       ShouldNotRequestIntelligentScan_EmptyLlamaForcedTriggerInfo) {
  SetEnhancedProtectionPrefForTests(&pref_service_, false);
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::FORCE_REQUEST);
  EXPECT_FALSE(delegate_->ShouldRequestIntelligentScan(&verdict));
}

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTest,
       ShouldNotRequestIntelligentScan_IntelligentScanDisabled) {
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::FORCE_REQUEST);
  verdict.mutable_llama_forced_trigger_info()->set_intelligent_scan(false);
  EXPECT_FALSE(delegate_->ShouldRequestIntelligentScan(&verdict));
}

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTest,
       TestOnDeviceModelFetchSuccessCall) {
  EXPECT_FALSE(delegate_->IsOnDeviceModelAvailable(
      /*log_failed_eligibility_reason=*/true));

  optimization_guide::OnDeviceModelAvailabilityObserver* availability_observer =
      nullptr;
  base::RunLoop run_loop_for_add_observer;
  EXPECT_CALL(mock_opt_guide_, AddOnDeviceModelAvailabilityChangeObserver(_, _))
      .WillOnce(Invoke(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              optimization_guide::OnDeviceModelAvailabilityObserver* observer) {
            availability_observer = observer;
            run_loop_for_add_observer.Quit();
          }));

  delegate_->StartListeningToOnDeviceModelUpdate();

  run_loop_for_add_observer.Run();
  CHECK(availability_observer);

  // Now that the delegate is observing, send `kConfigNotAvailableForFeature`
  // first to the observer, which will not stop the observing. We will then test
  // for all the possible waitable reasons, which should also not stop
  // observing.
  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey::kScamDetection,
      optimization_guide::OnDeviceModelEligibilityReason::
          kConfigNotAvailableForFeature);

  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey::kScamDetection,
      optimization_guide::OnDeviceModelEligibilityReason::kModelToBeInstalled);

  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey::kScamDetection,
      optimization_guide::OnDeviceModelEligibilityReason::
          kSafetyModelNotAvailable);

  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey::kScamDetection,
      optimization_guide::OnDeviceModelEligibilityReason::
          kLanguageDetectionModelNotAvailable);

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelDownloadSuccess", true, 0);

  EXPECT_FALSE(delegate_->IsOnDeviceModelAvailable(
      /*log_failed_eligibility_reason=*/true));

  // And then send `kSuccess` to the observer, which will log the histogram.
  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey::kScamDetection,
      optimization_guide::OnDeviceModelEligibilityReason::kSuccess);

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelDownloadSuccess", true, 1);
  histogram_tester_.ExpectTotalCount("SBClientPhishing.OnDeviceModelFetchTime",
                                     1);

  EXPECT_TRUE(delegate_->IsOnDeviceModelAvailable(
      /*log_failed_eligibility_reason=*/true));
}

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTest,
       TestOnDeviceModelFetchSuccessImmediateSessionCreation) {
  EXPECT_FALSE(delegate_->IsOnDeviceModelAvailable(
      /*log_failed_eligibility_reason=*/true));

  testing::NiceMock<MockSession> session;
  EXPECT_CALL(mock_opt_guide_, StartSession(_, _))
      .WillOnce(testing::Invoke(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              const std::optional<optimization_guide::SessionConfigParams>&
                  config_params) {
            return std::make_unique<NiceMock<MockSession>>(&session);
          }));
  // No need to add the observer because the session is created immediately.
  EXPECT_CALL(mock_opt_guide_, AddOnDeviceModelAvailabilityChangeObserver(_, _))
      .Times(0);

  delegate_->StartListeningToOnDeviceModelUpdate();

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelDownloadSuccess", true, 1);
  EXPECT_TRUE(delegate_->IsOnDeviceModelAvailable(
      /*log_failed_eligibility_reason=*/true));
}

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTest,
       TestOnDeviceModelFetchFailureCall) {
  EXPECT_FALSE(delegate_->IsOnDeviceModelAvailable(
      /*log_failed_eligibility_reason=*/true));

  optimization_guide::OnDeviceModelAvailabilityObserver* availability_observer =
      nullptr;
  base::RunLoop run_loop_for_add_observer;
  EXPECT_CALL(mock_opt_guide_, AddOnDeviceModelAvailabilityChangeObserver(_, _))
      .WillOnce(testing::Invoke(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              optimization_guide::OnDeviceModelAvailabilityObserver* observer) {
            availability_observer = observer;
            run_loop_for_add_observer.Quit();
          }));

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelDownloadSuccess", false, 0);

  delegate_->StartListeningToOnDeviceModelUpdate();

  // Now that the delegate is observing, send `kTooManyRecentCrashes`
  // to the observer, which is not a waitable reason.
  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey::kScamDetection,
      optimization_guide::OnDeviceModelEligibilityReason::
          kTooManyRecentCrashes);

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelDownloadSuccess", false, 1);

  EXPECT_FALSE(delegate_->IsOnDeviceModelAvailable(
      /*log_failed_eligibility_reason=*/true));
}

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTest,
       TestModelEligibilityReasonCheckAtFailedInquiry) {
  // The below function is called by the delegate when calling
  // IsOnDeviceModelAvailable but the on device model is not available yet.
  EXPECT_CALL(mock_opt_guide_, GetOnDeviceModelEligibility(_))
      .WillOnce([&](optimization_guide::ModelBasedCapabilityKey feature) {
        return optimization_guide::OnDeviceModelEligibilityReason::
            kModelToBeInstalled;
      });

  // We will start listening to the on device model when we call
  // StartListeningToOnDeviceModelUpdate, so we expect the call below.
  optimization_guide::OnDeviceModelAvailabilityObserver* availability_observer =
      nullptr;
  base::RunLoop run_loop_for_add_observer;
  EXPECT_CALL(mock_opt_guide_, AddOnDeviceModelAvailabilityChangeObserver(_, _))
      .WillOnce(testing::Invoke(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              optimization_guide::OnDeviceModelAvailabilityObserver* observer) {
            availability_observer = observer;
            run_loop_for_add_observer.Quit();
          }));

  delegate_->StartListeningToOnDeviceModelUpdate();

  run_loop_for_add_observer.Run();
  CHECK(availability_observer);

  // Now that the delegate is observing, send `kConfigNotAvailableForFeature`
  // first to the observer, which will not stop the observing. However, for the
  // purpose of this test, we will never fulfill the request to notify the
  // service class that the model installation is successful.
  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey::kScamDetection,
      optimization_guide::OnDeviceModelEligibilityReason::
          kConfigNotAvailableForFeature);

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelDownloadSuccess", true, 0);

  EXPECT_FALSE(delegate_->IsOnDeviceModelAvailable(
      /*log_failed_eligibility_reason=*/true));

  // We expect the histogram value for
  // SBClientPhishing.OnDeviceModelEligibilityReasonAtInquiryFailure to be
  // kModelTobeInstalled as we set the EXPECT_CALL above when calling for
  // function GetOnDeviceModelEligibility within the optimization guide service,
  // which is called in the service delegate.
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelEligibilityReasonAtInquiryFailure",
      optimization_guide::OnDeviceModelEligibilityReason::kModelToBeInstalled,
      1);

  EXPECT_FALSE(delegate_->IsOnDeviceModelAvailable(
      /*log_failed_eligibility_reason=*/false));

  // The histogram is not logged again because
  // log_failed_eligibility_reason is set to false.
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelEligibilityReasonAtInquiryFailure",
      optimization_guide::OnDeviceModelEligibilityReason::kModelToBeInstalled,
      1);

  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey::kScamDetection,
      optimization_guide::OnDeviceModelEligibilityReason::kSuccess);

  EXPECT_TRUE(delegate_->IsOnDeviceModelAvailable(
      /*log_failed_eligibility_reason=*/true));

  // The histogram is not logged again because
  // it is only logged when the model is not available.
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelEligibilityReasonAtInquiryFailure",
      optimization_guide::OnDeviceModelEligibilityReason::kModelToBeInstalled,
      1);
}

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTest,
       ModelFetchStopListeningBeforeSuccess) {
  optimization_guide::OnDeviceModelAvailabilityObserver* availability_observer =
      nullptr;
  base::RunLoop run_loop_for_add_observer;
  EXPECT_CALL(mock_opt_guide_, AddOnDeviceModelAvailabilityChangeObserver(_, _))
      .WillOnce(Invoke(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              optimization_guide::OnDeviceModelAvailabilityObserver* observer) {
            availability_observer = observer;
            run_loop_for_add_observer.Quit();
          }));

  delegate_->StartListeningToOnDeviceModelUpdate();

  run_loop_for_add_observer.Run();
  CHECK(availability_observer);

  delegate_->StopListeningToOnDeviceModelUpdate();

  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey::kScamDetection,
      optimization_guide::OnDeviceModelEligibilityReason::kSuccess);

  // The delegate should not be available because we stopped listening to the
  // model update before the model was available.
  EXPECT_FALSE(delegate_->IsOnDeviceModelAvailable(
      /*log_failed_eligibility_reason=*/true));
}

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTest,
       ModelFetchStopListeningAfterSuccess) {
  optimization_guide::OnDeviceModelAvailabilityObserver* availability_observer =
      nullptr;
  base::RunLoop run_loop_for_add_observer;
  EXPECT_CALL(mock_opt_guide_, AddOnDeviceModelAvailabilityChangeObserver(_, _))
      .WillOnce(Invoke(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              optimization_guide::OnDeviceModelAvailabilityObserver* observer) {
            availability_observer = observer;
            run_loop_for_add_observer.Quit();
          }));

  delegate_->StartListeningToOnDeviceModelUpdate();

  run_loop_for_add_observer.Run();
  CHECK(availability_observer);

  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey::kScamDetection,
      optimization_guide::OnDeviceModelEligibilityReason::kSuccess);

  EXPECT_TRUE(delegate_->IsOnDeviceModelAvailable(
      /*log_failed_eligibility_reason=*/true));

  delegate_->StopListeningToOnDeviceModelUpdate();

  EXPECT_FALSE(delegate_->IsOnDeviceModelAvailable(
      /*log_failed_eligibility_reason=*/true));

  // Start listening again should work.
  base::RunLoop run_loop_for_add_observer2;
  EXPECT_CALL(mock_opt_guide_, AddOnDeviceModelAvailabilityChangeObserver(_, _))
      .WillOnce(Invoke(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              optimization_guide::OnDeviceModelAvailabilityObserver* observer) {
            availability_observer = observer;
            run_loop_for_add_observer2.Quit();
          }));
  delegate_->StartListeningToOnDeviceModelUpdate();

  run_loop_for_add_observer2.Run();
  CHECK(availability_observer);

  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey::kScamDetection,
      optimization_guide::OnDeviceModelEligibilityReason::kSuccess);

  EXPECT_TRUE(delegate_->IsOnDeviceModelAvailable(
      /*log_failed_eligibility_reason=*/true));
}

class
    ClientSideDetectionIntelligentScanDelegateDesktopTestBrandAndIntentDisabled
    : public ClientSideDetectionIntelligentScanDelegateDesktopTest {
 public:
  ClientSideDetectionIntelligentScanDelegateDesktopTestBrandAndIntentDisabled() {
    feature_list_.InitWithFeatures(
        {kClientSideDetectionLlamaForcedTriggerInfoForScamDetection},
        {kClientSideDetectionBrandAndIntentForScamDetection});
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(
    ClientSideDetectionIntelligentScanDelegateDesktopTestBrandAndIntentDisabled,
    ShouldNotRequestIntelligentScan_KeyboardLockRequested) {
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::KEYBOARD_LOCK_REQUESTED);
  // Disabled because kClientSideDetectionBrandAndIntentForScamDetection
  // is disabled.
  EXPECT_FALSE(delegate_->ShouldRequestIntelligentScan(&verdict));
}

TEST_F(
    ClientSideDetectionIntelligentScanDelegateDesktopTestBrandAndIntentDisabled,
    ShouldRequestIntelligentScan_IntelligentScanRequested) {
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::FORCE_REQUEST);
  verdict.mutable_llama_forced_trigger_info()->set_intelligent_scan(true);
  // kClientSideDetectionBrandAndIntentForScamDetection shouldn't affect
  // intelligent scan requests.
  EXPECT_TRUE(delegate_->ShouldRequestIntelligentScan(&verdict));
}

class
    ClientSideDetectionIntelligentScanDelegateDesktopTestLlamaForcedTriggerInfoDisabled
    : public ClientSideDetectionIntelligentScanDelegateDesktopTest {
 public:
  ClientSideDetectionIntelligentScanDelegateDesktopTestLlamaForcedTriggerInfoDisabled() {
    feature_list_.InitWithFeatures(
        {kClientSideDetectionBrandAndIntentForScamDetection},
        {kClientSideDetectionLlamaForcedTriggerInfoForScamDetection});
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(
    ClientSideDetectionIntelligentScanDelegateDesktopTestLlamaForcedTriggerInfoDisabled,
    ShouldRequestIntelligentScan_KeyboardLockRequested) {
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::KEYBOARD_LOCK_REQUESTED);
  // kClientSideDetectionLlamaForcedTriggerInfoForScamDetection shouldn't affect
  // keyboard lock requests.
  EXPECT_TRUE(delegate_->ShouldRequestIntelligentScan(&verdict));
}

TEST_F(
    ClientSideDetectionIntelligentScanDelegateDesktopTestLlamaForcedTriggerInfoDisabled,
    ShouldNotRequestIntelligentScan_IntelligentScanRequested) {
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::FORCE_REQUEST);
  verdict.mutable_llama_forced_trigger_info()->set_intelligent_scan(true);
  // Disabled because kClientSideDetectionLlamaForcedTriggerInfoForScamDetection
  // is disabled.
  EXPECT_FALSE(delegate_->ShouldRequestIntelligentScan(&verdict));
}

}  // namespace safe_browsing
