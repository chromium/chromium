// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/client_side_detection_intelligent_scan_delegate_desktop.h"

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "components/optimization_guide/core/model_execution/test/mock_on_device_capability.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/model_quality_metadata.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-data-view.h"
#include "components/safe_browsing/content/browser/client_side_detection_host.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::optimization_guide::AnyWrapProto;
using ::optimization_guide::MockSession;
using ::optimization_guide::OptimizationGuideModelExecutionError;
using ::optimization_guide::OptimizationGuideModelStreamingExecutionResult;
using ::optimization_guide::proto::ModelExecutionInfo;
using ::optimization_guide::proto::ScamDetectionResponse;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

namespace safe_browsing {

using IntelligentScanResult =
    ClientSideDetectionHost::IntelligentScanDelegate::IntelligentScanResult;

class ClientSideDetectionIntelligentScanDelegateDesktopTest
    : public testing::Test {
 public:
  ClientSideDetectionIntelligentScanDelegateDesktopTest() {
    feature_list_.InitWithFeatures(
        {kClientSideDetectionLlamaForcedTriggerInfoForScamDetection,
         kClientSideDetectionShowLlamaScamVerdictWarning},
        {kClientSideDetectionKillswitch});
    RegisterProfilePrefs(pref_service_.registry());
  }

 protected:
  void CreateDelegate(bool is_enhanced_protection_enabled) {
    SetEnhancedProtectionPrefForTests(&pref_service_,
                                      is_enhanced_protection_enabled);
    delegate_ =
        std::make_unique<ClientSideDetectionIntelligentScanDelegateDesktop>(
            pref_service_, &mock_opt_guide_);
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

    ASSERT_TRUE(delegate_->IsOnDeviceModelAvailable(
        /*log_failed_eligibility_reason=*/true));
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

  content::BrowserTaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  NiceMock<MockOptimizationGuideKeyedService> mock_opt_guide_;
  testing::NiceMock<MockSession> session_;
  std::unique_ptr<ClientSideDetectionIntelligentScanDelegateDesktop> delegate_;
  base::HistogramTester histogram_tester_;
  base::test::ScopedFeatureList feature_list_;
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
       TestOnDeviceModelFetchSuccessCall) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/false);
  EXPECT_FALSE(delegate_->IsOnDeviceModelAvailable(
      /*log_failed_eligibility_reason=*/true));

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

  EXPECT_FALSE(delegate_->IsOnDeviceModelAvailable(
      /*log_failed_eligibility_reason=*/true));

  // And then send `kSuccess` to the observer, which will log the histogram.
  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::mojom::OnDeviceFeature::kScamDetection,
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
  CreateDelegate(/*is_enhanced_protection_enabled=*/false);
  EXPECT_FALSE(delegate_->IsOnDeviceModelAvailable(
      /*log_failed_eligibility_reason=*/true));

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
  EXPECT_TRUE(delegate_->IsOnDeviceModelAvailable(
      /*log_failed_eligibility_reason=*/true));
}

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTest,
       TestOnDeviceModelFetchFailureCall) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/false);
  EXPECT_FALSE(delegate_->IsOnDeviceModelAvailable(
      /*log_failed_eligibility_reason=*/true));

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

  EXPECT_FALSE(delegate_->IsOnDeviceModelAvailable(
      /*log_failed_eligibility_reason=*/true));
}

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTest,
       TestModelEligibilityReasonCheckAtFailedInquiry) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/false);

  // The below function is called by the delegate when calling
  // IsOnDeviceModelAvailable but the on device model is not available yet.
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
      optimization_guide::mojom::OnDeviceFeature::kScamDetection,
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
  EXPECT_FALSE(delegate_->IsOnDeviceModelAvailable(
      /*log_failed_eligibility_reason=*/true));
}

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTest,
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

  EXPECT_TRUE(delegate_->IsOnDeviceModelAvailable(
      /*log_failed_eligibility_reason=*/true));

  SetEnhancedProtectionPrefForTests(&pref_service_, false);

  EXPECT_FALSE(delegate_->IsOnDeviceModelAvailable(
      /*log_failed_eligibility_reason=*/true));

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

  EXPECT_TRUE(delegate_->IsOnDeviceModelAvailable(
      /*log_failed_eligibility_reason=*/true));
}

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTest,
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

  EXPECT_FALSE(delegate_->IsOnDeviceModelAvailable(
      /*log_failed_eligibility_reason=*/true));

  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::mojom::OnDeviceFeature::kScamDetection,
      optimization_guide::OnDeviceModelEligibilityReason::kSuccess);

  EXPECT_TRUE(delegate_->IsOnDeviceModelAvailable(
      /*log_failed_eligibility_reason=*/true));
}

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTest,
       TestSessionCreationFailure) {
  EnableOnDeviceModel();

  EXPECT_CALL(mock_opt_guide_, StartSession(_, _, _)).WillOnce(Return(nullptr));

  base::test::TestFuture<IntelligentScanResult> future;
  delegate_->InquireOnDeviceModel("", future.GetCallback());

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelSessionCreationSuccess", false, 1);
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.OnDeviceModelSessionCreationTime", 0);
  EXPECT_FALSE(future.Get().execution_success);
}

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTest,
       TestSessionCreationSuccess) {
  EnableOnDeviceModelWithSession();

  base::test::TestFuture<IntelligentScanResult> future;
  delegate_->InquireOnDeviceModel("", future.GetCallback());

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelSessionCreationSuccess", true, 1);
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.OnDeviceModelSessionCreationTime", 1);
  // We do not test for execution_success field here because the session
  // creation has succeeded, but model execution callback is not set, so the
  // future callback won't be answered.
}

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTest,
       TestSessionCreationSuccessWithAPreviousAliveSession) {
  EnableOnDeviceModelWithSession();

  base::test::TestFuture<IntelligentScanResult> future;
  delegate_->InquireOnDeviceModel("", future.GetCallback());

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelSessionCreationSuccess", true, 1);

  // A second session can be created while the first one is still alive.
  EXPECT_CALL(mock_opt_guide_, StartSession(_, _, _))
      .WillOnce(
          [&](optimization_guide::mojom::OnDeviceFeature feature,
              const optimization_guide::SessionConfigParams& config_params,
              base::WeakPtr<OptimizationGuideLogger> logger) {
            return std::make_unique<NiceMock<MockSession>>(&session_);
          });

  base::test::TestFuture<IntelligentScanResult> future2;
  delegate_->InquireOnDeviceModel("", future2.GetCallback());

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelSessionCreationSuccess", true, 2);
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.OnDeviceModelSessionCreationTime", 2);
}

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTest,
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
  std::optional<base::UnguessableToken> session_id1 =
      delegate_->InquireOnDeviceModel("", future1.GetCallback());
  EXPECT_FALSE(session_id1->is_empty());

  testing::NiceMock<MockSession> session2;
  EXPECT_CALL(mock_opt_guide_, StartSession(_, _, _))
      .WillOnce(
          [&](optimization_guide::mojom::OnDeviceFeature feature,
              const optimization_guide::SessionConfigParams& config_params,
              base::WeakPtr<OptimizationGuideLogger> logger) {
            return std::make_unique<NiceMock<MockSession>>(&session2);
          });

  base::test::TestFuture<IntelligentScanResult> future2;
  std::optional<base::UnguessableToken> session_id2 =
      delegate_->InquireOnDeviceModel("", future2.GetCallback());

  // Both session IDs should still be alive.
  EXPECT_FALSE(session_id1->is_empty());
  EXPECT_FALSE(session_id2->is_empty());
  EXPECT_EQ(delegate_->GetAliveSessionCountForTesting(), 2);
}

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTest,
       TestCancelSession) {
  EnableOnDeviceModelWithSession();

  base::test::TestFuture<IntelligentScanResult> future;
  std::optional<base::UnguessableToken> session_id =
      delegate_->InquireOnDeviceModel("", future.GetCallback());
  EXPECT_FALSE(session_id->is_empty());

  EXPECT_EQ(delegate_->GetAliveSessionCountForTesting(), 1);
  EXPECT_TRUE(delegate_->CancelSession(*session_id));
  EXPECT_EQ(delegate_->GetAliveSessionCountForTesting(), 0);

  // The callback should not be called.
  EXPECT_FALSE(future.IsReady());
}

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTest,
       TestMultipleSessionsCancellation) {
  EnableOnDeviceModel();

  EXPECT_CALL(mock_opt_guide_, StartSession(_, _, _))
      .WillOnce(
          [&](optimization_guide::mojom::OnDeviceFeature feature,
              const optimization_guide::SessionConfigParams& config_params,
              base::WeakPtr<OptimizationGuideLogger> logger) {
            return std::make_unique<NiceMock<MockSession>>(&session_);
          });

  base::test::TestFuture<IntelligentScanResult> future1;
  std::optional<base::UnguessableToken> session_id1 =
      delegate_->InquireOnDeviceModel("", future1.GetCallback());
  EXPECT_FALSE(session_id1->is_empty());

  testing::NiceMock<MockSession> session2;
  EXPECT_CALL(mock_opt_guide_, StartSession(_, _, _))
      .WillOnce(
          [&](optimization_guide::mojom::OnDeviceFeature feature,
              const optimization_guide::SessionConfigParams& config_params,
              base::WeakPtr<OptimizationGuideLogger> logger) {
            return std::make_unique<NiceMock<MockSession>>(&session2);
          });

  base::test::TestFuture<IntelligentScanResult> future2;
  std::optional<base::UnguessableToken> session_id2 =
      delegate_->InquireOnDeviceModel("", future2.GetCallback());

  // Both session IDs should still be alive.
  EXPECT_FALSE(session_id1->is_empty());
  EXPECT_FALSE(session_id2->is_empty());

  EXPECT_EQ(delegate_->GetAliveSessionCountForTesting(), 2);
  EXPECT_TRUE(delegate_->CancelSession(*session_id1));
  EXPECT_EQ(delegate_->GetAliveSessionCountForTesting(), 1);
  EXPECT_TRUE(delegate_->CancelSession(*session_id2));
  EXPECT_EQ(delegate_->GetAliveSessionCountForTesting(), 0);
}

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTest,
       TestSessionExecutionFailure) {
  EnableOnDeviceModelWithSession();

  EXPECT_CALL(session_, ExecuteModel(_, _))
      .WillOnce(testing::WithArg<1>(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            callback.Run(OptimizationGuideModelStreamingExecutionResult(
                base::unexpected(
                    OptimizationGuideModelExecutionError::
                        FromModelExecutionError(
                            OptimizationGuideModelExecutionError::
                                ModelExecutionError::kGenericFailure)),
                /*provided_by_on_device=*/true,
                /*execution_info=*/CreateExecutionInfo(/*model_version=*/123)));
          }));

  base::test::TestFuture<IntelligentScanResult> future;
  delegate_->InquireOnDeviceModel("", future.GetCallback());

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
}

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTest,
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
  delegate_->InquireOnDeviceModel("", future.GetCallback());

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

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTest,
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
  delegate_->InquireOnDeviceModel("", future.GetCallback());

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
}

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTest,
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
  delegate_->InquireOnDeviceModel("", future.GetCallback());

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

  EXPECT_TRUE(future.Get().execution_success);
  EXPECT_EQ(future.Get().brand, "Google");
  EXPECT_EQ(future.Get().intent, "Search Engine");
  EXPECT_EQ(future.Get().model_version, 123);
}

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTest,
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
  ClientSideDetectionHost::IntelligentScanDelegate::
      InquireOnDeviceModelDoneCallback host_callback;
  delegate_->InquireOnDeviceModel("", std::move(host_callback));

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

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTest,
       ResetOnDeviceSession) {
  EnableOnDeviceModelWithSession();

  bool did_reset = delegate_->ResetAllSessions();
  EXPECT_FALSE(did_reset);

  base::test::TestFuture<IntelligentScanResult> future;
  delegate_->InquireOnDeviceModel("", future.GetCallback());

  EXPECT_EQ(delegate_->GetAliveSessionCountForTesting(), 1);

  // Create a second session
  EXPECT_CALL(mock_opt_guide_, StartSession(_, _, _))
      .WillOnce(
          [&](optimization_guide::mojom::OnDeviceFeature feature,
              const optimization_guide::SessionConfigParams& config_params,
              base::WeakPtr<OptimizationGuideLogger> logger) {
            return std::make_unique<NiceMock<MockSession>>(&session_);
          });
  base::test::TestFuture<IntelligentScanResult> future2;
  delegate_->InquireOnDeviceModel("", future2.GetCallback());

  EXPECT_EQ(delegate_->GetAliveSessionCountForTesting(), 2);

  did_reset = delegate_->ResetAllSessions();
  EXPECT_TRUE(did_reset);

  EXPECT_EQ(delegate_->GetAliveSessionCountForTesting(), 0);
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
  EXPECT_TRUE(delegate_->ShouldShowScamWarning(
      IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_1));
  EXPECT_TRUE(delegate_->ShouldShowScamWarning(
      IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_2));
  EXPECT_TRUE(delegate_->ShouldShowScamWarning(
      IntelligentScanVerdict::SCAM_EXPERIMENT_CATCH_ALL_ENFORCEMENT));
}

class
    ClientSideDetectionIntelligentScanDelegateDesktopTestLlamaForcedTriggerInfoDisabled
    : public ClientSideDetectionIntelligentScanDelegateDesktopTest {
 public:
  ClientSideDetectionIntelligentScanDelegateDesktopTestLlamaForcedTriggerInfoDisabled() {
    feature_list_.InitWithFeatures(
        {}, {kClientSideDetectionLlamaForcedTriggerInfoForScamDetection});
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(
    ClientSideDetectionIntelligentScanDelegateDesktopTestLlamaForcedTriggerInfoDisabled,
    ShouldRequestIntelligentScan_KeyboardLockRequested) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);
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
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::FORCE_REQUEST);
  verdict.mutable_llama_forced_trigger_info()->set_intelligent_scan(true);
  // Disabled because kClientSideDetectionLlamaForcedTriggerInfoForScamDetection
  // is disabled.
  EXPECT_FALSE(delegate_->ShouldRequestIntelligentScan(&verdict));
}

class ClientSideDetectionIntelligentScanDelegateDesktopTestKillSwitchEnabled
    : public ClientSideDetectionIntelligentScanDelegateDesktopTest {
 public:
  ClientSideDetectionIntelligentScanDelegateDesktopTestKillSwitchEnabled() {
    feature_list_.InitWithFeatures({kClientSideDetectionKillswitch}, {});
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTestKillSwitchEnabled,
       NotListenToModelUpdateOnCreation) {
  // The killswitch flag is enabled, so we shouldn't listen to model updates.
  EXPECT_CALL(mock_opt_guide_, AddOnDeviceModelAvailabilityChangeObserver(_, _))
      .Times(0);
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);

  EXPECT_FALSE(delegate_->IsOnDeviceModelAvailable(
      /*log_failed_eligibility_reason=*/true));
}

class
    ClientSideDetectionIntelligentScanDelegateDesktopTestShowLlamaWarningDisabled
    : public ClientSideDetectionIntelligentScanDelegateDesktopTest {
 public:
  ClientSideDetectionIntelligentScanDelegateDesktopTestShowLlamaWarningDisabled() {
    feature_list_.InitWithFeatures(
        {}, {kClientSideDetectionShowLlamaScamVerdictWarning});
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(
    ClientSideDetectionIntelligentScanDelegateDesktopTestShowLlamaWarningDisabled,
    ShouldShowScamWarning) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);
  EXPECT_TRUE(delegate_->ShouldShowScamWarning(
      IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_1));
  EXPECT_FALSE(delegate_->ShouldShowScamWarning(
      IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_2));
  EXPECT_TRUE(delegate_->ShouldShowScamWarning(
      IntelligentScanVerdict::SCAM_EXPERIMENT_CATCH_ALL_ENFORCEMENT));
}

}  // namespace safe_browsing
