// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/metrics/glic_instance_metrics.h"

#include <memory>
#include <optional>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/host/glic.mojom-shared.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/service/metrics/metrics_types.h"
#include "chrome/common/chrome_features.h"
#include "components/metrics/profile_metrics_service.h"
#include "components/skills/public/skills_metrics.h"
#include "components/split_tabs/split_tab_id.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "ui/gfx/geometry/size.h"

namespace glic {
class GlicInstanceMetricsTest : public testing::Test {
 public:
  GlicInstanceMetricsTest() {
    EXPECT_CALL(mock_tab_, GetUnownedUserDataHost())
        .WillRepeatedly(testing::ReturnRef(unowned_user_data_host_));
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
  ukm::TestAutoSetUkmRecorder ukm_tester_;
  metrics::ProfileMetricsService profile_metrics_service_;
  GlicInstanceMetrics metrics_{&profile_metrics_service_};
  tabs::MockTabInterface mock_tab_;
  ui::UnownedUserDataHost unowned_user_data_host_;
  base::UserActionTester user_action_tester_;
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kGlicCaptureRegion};
};

class GlicInstanceMetricsTestWithPolyline : public GlicInstanceMetricsTest {
 public:
  GlicInstanceMetricsTestWithPolyline() {
    feature_list_.InitAndEnableFeature(features::kGlicRegionSelectionLine);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(GlicInstanceMetricsTest, OptinImpression) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGlicOptInImpressionMetrics);

  metrics_.OnOptinImpression();
  EXPECT_EQ(
      user_action_tester_.GetActionCount("Glic.Onboarding.OptInImpression"), 0);

  metrics_.OnVisibilityChanged(true);
  metrics_.OnClientReady(GlicInstanceMetrics::EmbedderType::kSidePanel);

  EXPECT_EQ(
      user_action_tester_.GetActionCount("Glic.Onboarding.OptInImpression"), 1);
}

TEST_F(GlicInstanceMetricsTest, OptinImpression_KillSwitchDisabled) {
  base::test::ScopedFeatureList disabled_feature_list;
  disabled_feature_list.InitAndDisableFeature(
      features::kGlicOptInImpressionMetrics);

  metrics_.OnOptinImpression();
  metrics_.OnVisibilityChanged(true);
  metrics_.OnClientReady(GlicInstanceMetrics::EmbedderType::kSidePanel);

  EXPECT_EQ(
      user_action_tester_.GetActionCount("Glic.Onboarding.OptInImpression"), 0);
}

TEST_F(GlicInstanceMetricsTest, OnActivationChanged_LogsTimeSinceLastActive) {
  // First activation.
  metrics_.OnActivationChanged(true);
  histogram_tester_.ExpectTotalCount("Glic.Instance.TimeSinceLastActive", 0);

  // Deactivate.
  metrics_.OnActivationChanged(false);

  // Advance time.
  task_environment_.FastForwardBy(base::Minutes(5));

  // Second activation.
  metrics_.OnActivationChanged(true);
  histogram_tester_.ExpectUniqueTimeSample("Glic.Instance.TimeSinceLastActive",
                                           base::Minutes(5), 1);
  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Instance.TimeSinceLastActive.24H", base::Minutes(5), 1);
  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Instance.TimeSinceLastActive.7D", base::Minutes(5), 1);

  // Redundant activation should not log again.
  task_environment_.FastForwardBy(base::Minutes(1));
  metrics_.OnActivationChanged(true);
  histogram_tester_.ExpectTotalCount("Glic.Instance.TimeSinceLastActive", 1);
}

TEST_F(GlicInstanceMetricsTest, OnResponseStarted_WithoutInput_LogsError) {
  metrics_.OnResponseStarted();
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Metrics.Error",
      GlicInstanceMetricsError::kResponseStartWithoutInput, 1);
}

TEST_F(GlicInstanceMetricsTest,
       OnResponseStarted_WithActuation_DoesNotLogError) {
  metrics_.OnActionSubmitted(/*is_retry=*/false);
  metrics_.OnResponseStarted();
  histogram_tester_.ExpectBucketCount(
      "Glic.Instance.Metrics.Error",
      GlicInstanceMetricsError::kResponseStartWithoutInput, 0);
}

TEST_F(GlicInstanceMetricsTest, OnResponseStarted_WhileHidden_LogsError) {
  metrics_.OnVisibilityChanged(true);
  metrics_.OnUserInputSubmitted(mojom::WebClientMode::kText);
  metrics_.OnVisibilityChanged(false);
  metrics_.OnResponseStarted();
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Metrics.Error",
      GlicInstanceMetricsError::kResponseStartWhileHidingOrHidden, 1);
}

TEST_F(GlicInstanceMetricsTest, OnResponseStopped_WithoutInput_LogsError) {
  metrics_.OnResponseStopped(mojom::ResponseStopCause::kUser);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Metrics.Error",
      GlicInstanceMetricsError::kResponseStopWithoutInput, 1);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Metrics.Error.ByUser",
      GlicInstanceMetricsError::kResponseStopWithoutInput, 1);
}

TEST_F(GlicInstanceMetricsTest,
       OnResponseStopped_WithoutInputOtherCause_LogsError) {
  metrics_.OnResponseStopped(mojom::ResponseStopCause::kOther);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Metrics.Error",
      GlicInstanceMetricsError::kResponseStopWithoutInput, 1);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Metrics.Error.Other",
      GlicInstanceMetricsError::kResponseStopWithoutInput, 1);
}

TEST_F(GlicInstanceMetricsTest,
       OnResponseStopped_WithoutInputUnknownCause_LogsError) {
  metrics_.OnResponseStopped(mojom::ResponseStopCause::kUnknown);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Metrics.Error",
      GlicInstanceMetricsError::kResponseStopWithoutInput, 1);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Metrics.Error.UnknownCause",
      GlicInstanceMetricsError::kResponseStopWithoutInput, 1);
}

TEST_F(GlicInstanceMetricsTest,
       OnResponseStopped_WithActuation_DoesNotLogError) {
  metrics_.OnActionSubmitted(/*is_retry=*/false);
  metrics_.OnResponseStopped(mojom::ResponseStopCause::kUser);
  histogram_tester_.ExpectBucketCount(
      "Glic.Instance.Metrics.Error",
      GlicInstanceMetricsError::kResponseStopWithoutInput, 0);
}

TEST_F(GlicInstanceMetricsTest, OnFloatyClosed_WithoutOpening_LogsError) {
  metrics_.OnFloatyClosed();
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Metrics.Error",
      GlicInstanceMetricsError::kFloatyClosedWithoutOpen, 1);
}

TEST_F(GlicInstanceMetricsTest, OnSidePanelClosed_WithoutOpening_LogsError) {
  metrics_.OnSidePanelClosed(
      static_cast<tabs::TabInterface*>(&mock_tab_),
      GlicInstanceMetrics::CloseReason::kExplicitlyClosed);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Metrics.Error",
      GlicInstanceMetricsError::kSidePanelClosedWithoutOpen, 1);
}

TEST_F(GlicInstanceMetricsTest,
       OnUserInputSubmitted_WhileResponseInProgress_LogsError) {
  metrics_.OnVisibilityChanged(true);
  metrics_.OnUserInputSubmitted(mojom::WebClientMode::kText);
  metrics_.OnResponseStarted();
  metrics_.OnUserInputSubmitted(mojom::WebClientMode::kText);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Metrics.Error",
      GlicInstanceMetricsError::kInputSubmittedWhileResponseInProgress, 1);
}

TEST_F(GlicInstanceMetricsTest, OnUserInputSubmitted_WhileHidden_LogsError) {
  metrics_.OnUserInputSubmitted(mojom::WebClientMode::kText);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Metrics.Error",
      GlicInstanceMetricsError::kInputSubmittedWhileHidden, 1);
}

TEST_F(GlicInstanceMetricsTest, OnShowInFloaty_WhileAlreadyOpen_LogsError) {
  ShowOptions show_options{FloatingShowOptions{}};
  metrics_.OnShowInFloaty(show_options);
  metrics_.OnShowInFloaty(show_options);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Metrics.Error",
      GlicInstanceMetricsError::kFloatyOpenedWhileAlreadyOpen, 1);
}

TEST_F(GlicInstanceMetricsTest, OnShowInSidePanel_WhileAlreadyOpen_LogsError) {
  EXPECT_CALL(mock_tab_, GetTabHandle()).WillRepeatedly(testing::Return(1));
  metrics_.OnShowInSidePanel(&mock_tab_);
  metrics_.OnShowInSidePanel(&mock_tab_);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Metrics.Error",
      GlicInstanceMetricsError::kSidePanelOpenedWhileAlreadyOpen, 1);
}

TEST_F(GlicInstanceMetricsTest, OnUnbindEmbedder_WithoutOpening_LogsError) {
  tabs::TabInterface* tab_ptr = &mock_tab_;
  metrics_.OnUnbindEmbedder(tab_ptr);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Metrics.Error",
      GlicInstanceMetricsError::kTabUnbindWithoutOpen, 1);
}

TEST_F(GlicInstanceMetricsTest, OnUserResizeStarted) {
  gfx::Size test_size(800, 600);
  metrics_.OnUserResizeStarted(test_size);

  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "Glic.Instance.Floaty.UserResizeStarted"));
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Floaty.UserResizeStarted.Width", test_size.width(), 1);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Floaty.UserResizeStarted.Height", test_size.height(), 1);
}

TEST_F(GlicInstanceMetricsTest, OnUserResizeEnded) {
  gfx::Size test_size(1024, 768);
  metrics_.OnUserResizeEnded(test_size);

  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "Glic.Instance.Floaty.UserResizeEnded"));
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Floaty.UserResizeEnded.Width", test_size.width(), 1);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Floaty.UserResizeEnded.Height", test_size.height(), 1);
}

TEST_F(GlicInstanceMetricsTest, ValidFloatyFlow_DoesNotLogError) {
  ShowOptions show_options{FloatingShowOptions{}};
  metrics_.OnShowInFloaty(show_options);
  metrics_.OnFloatyClosed();
  histogram_tester_.ExpectTotalCount("Glic.Instance.Metrics.Error", 0);
}

TEST_F(GlicInstanceMetricsTest, ValidSidePanelFlow_DoesNotLogError) {
  EXPECT_CALL(mock_tab_, GetTabHandle()).WillRepeatedly(testing::Return(1));
  metrics_.OnShowInSidePanel(&mock_tab_);
  metrics_.OnSidePanelClosed(
      &mock_tab_, GlicInstanceMetrics::CloseReason::kExplicitlyClosed);
  histogram_tester_.ExpectTotalCount("Glic.Instance.Metrics.Error", 0);
}

TEST_F(GlicInstanceMetricsTest, OnOpen_DoesNotOverrideInitialEntrypoint) {
  ShowOptions show_options1{FloatingShowOptions{}};
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options1);
  EXPECT_EQ(metrics_.initial_invocation_source_for_testing(),
            mojom::InvocationSource::kTopChromeButton);

  ShowOptions show_options2{FloatingShowOptions{}};
  metrics_.OnOpen(mojom::InvocationSource::kOsButton, show_options2);
  EXPECT_EQ(metrics_.initial_invocation_source_for_testing(),
            mojom::InvocationSource::kTopChromeButton);
}

TEST_F(GlicInstanceMetricsTest, InitialInvocationSource_OnlyRecordedOnce) {
  ShowOptions show_options{FloatingShowOptions{}};
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.InitialInvocationSource",
      mojom::InvocationSource::kTopChromeButton, 1);

  metrics_.OnOpen(mojom::InvocationSource::kOsButton, show_options);
  // Should still be 1 sample of kTopChromeButton.
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.InitialInvocationSource",
      mojom::InvocationSource::kTopChromeButton, 1);
}

TEST_F(GlicInstanceMetricsTest, SidePanelFirstOpenDuration_LoggedOnFirstClose) {
  EXPECT_CALL(mock_tab_, GetTabHandle()).WillRepeatedly(testing::Return(1));

  ShowOptions show_options{SidePanelShowOptions{mock_tab_}};
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  metrics_.OnShowInSidePanel(&mock_tab_);
  task_environment_.FastForwardBy(base::Minutes(5));

  metrics_.OnSidePanelClosed(
      static_cast<tabs::TabInterface*>(&mock_tab_),
      GlicInstanceMetrics::CloseReason::kExplicitlyClosed);

  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.InvocationSource.TopChromeButton.SidePanelFirstOpenDuration",
      base::Minutes(5), 1);
}

TEST_F(GlicInstanceMetricsTest,
       SidePanelFirstOpenDuration_NotLoggedOnSecondClose) {
  EXPECT_CALL(mock_tab_, GetTabHandle()).WillRepeatedly(testing::Return(1));

  ShowOptions show_options{SidePanelShowOptions{mock_tab_}};
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  metrics_.OnShowInSidePanel(&mock_tab_);

  task_environment_.FastForwardBy(base::Minutes(5));
  metrics_.OnSidePanelClosed(
      static_cast<tabs::TabInterface*>(&mock_tab_),
      GlicInstanceMetrics::CloseReason::kExplicitlyClosed);

  histogram_tester_.ExpectTotalCount(
      "Glic.InvocationSource.TopChromeButton.SidePanelFirstOpenDuration", 1);

  metrics_.OnOpen(mojom::InvocationSource::kOsButton, show_options);
  metrics_.OnShowInSidePanel(&mock_tab_);
  task_environment_.FastForwardBy(base::Minutes(2));
  metrics_.OnSidePanelClosed(
      static_cast<tabs::TabInterface*>(&mock_tab_),
      GlicInstanceMetrics::CloseReason::kExplicitlyClosed);

  histogram_tester_.ExpectTotalCount(
      "Glic.InvocationSource.TopChromeButton.SidePanelFirstOpenDuration", 1);
  histogram_tester_.ExpectTotalCount(
      "Glic.InvocationSource.OsButton.SidePanelFirstOpenDuration", 0);
}

TEST_F(GlicInstanceMetricsTest,
       SidePanelFirstOpenDuration_ShownWithoutToggleCall) {
  metrics_.OnShowInSidePanel(&mock_tab_);
  task_environment_.FastForwardBy(base::Minutes(5));
  metrics_.OnSidePanelClosed(
      static_cast<tabs::TabInterface*>(&mock_tab_),
      GlicInstanceMetrics::CloseReason::kExplicitlyClosed);

  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.InvocationSource.Unsupported.SidePanelFirstOpenDuration",
      base::Minutes(5), 1);
}

TEST_F(GlicInstanceMetricsTest, InstanceEvents_LogsEventCountsAndHadEvent) {
  ShowOptions show_options{FloatingShowOptions{}};
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);

  // This causes a GlicInstanceEvent::kTurnCompleted to be logged.
  metrics_.OnTurnCompleted(mojom::WebClientModel::kActor,
                           base::Milliseconds(100));

  histogram_tester_.ExpectBucketCount("Glic.Instance.EventCounts",
                                      GlicInstanceEvent::kTurnCompleted, 1);
  histogram_tester_.ExpectBucketCount(
      "Glic.InvocationSource.TopChromeButton.EventCounts",
      GlicInstanceEvent::kTurnCompleted, 1);
  histogram_tester_.ExpectBucketCount("Glic.Instance.HadEvent",
                                      GlicInstanceEvent::kTurnCompleted, 1);
  histogram_tester_.ExpectBucketCount(
      "Glic.InvocationSource.TopChromeButton.HadEvent",
      GlicInstanceEvent::kTurnCompleted, 1);

  // Second event: only EventCounts increment, HadEvent does not.
  metrics_.OnTurnCompleted(mojom::WebClientModel::kActor,
                           base::Milliseconds(100));

  histogram_tester_.ExpectBucketCount("Glic.Instance.EventCounts",
                                      GlicInstanceEvent::kTurnCompleted, 2);
  histogram_tester_.ExpectBucketCount(
      "Glic.InvocationSource.TopChromeButton.EventCounts",
      GlicInstanceEvent::kTurnCompleted, 2);
  histogram_tester_.ExpectBucketCount("Glic.Instance.HadEvent",
                                      GlicInstanceEvent::kTurnCompleted, 1);
  histogram_tester_.ExpectBucketCount(
      "Glic.InvocationSource.TopChromeButton.HadEvent",
      GlicInstanceEvent::kTurnCompleted, 1);
}

TEST_F(GlicInstanceMetricsTest,
       InstanceEvents_NoCrashWithoutInitialEntrypoint) {
  // We ensure we don't crash before the initial entrypoint gets assigned.
  // We bypass OnToggle so initial_entrypoint is null.
  histogram_tester_.ExpectTotalCount(
      "Glic.InvocationSource.TopChromeButton.EventCounts", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.InvocationSource.TopChromeButton.HadEvent", 0);

  metrics_.OnReaction(mojom::MetricUserInputReactionType::kModel);
  // We don't get entrypoint-specific logs, but we do get generic ones.
  histogram_tester_.ExpectBucketCount("Glic.Instance.EventCounts",
                                      GlicInstanceEvent::kReaction, 1);
  histogram_tester_.ExpectBucketCount("Glic.Instance.HadEvent",
                                      GlicInstanceEvent::kReaction, 1);
}

TEST_F(GlicInstanceMetricsTest, LogEvent_LogsUnderInitialEntrypointIfReopened) {
  ShowOptions show_options1{FloatingShowOptions{}};
  // First toggle open with TopChromeButton.
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options1);

  // A turn completed event should increment TopChromeButton.
  metrics_.OnTurnCompleted(mojom::WebClientModel::kActor,
                           base::Milliseconds(100));

  histogram_tester_.ExpectBucketCount(
      "Glic.InvocationSource.TopChromeButton.EventCounts",
      GlicInstanceEvent::kTurnCompleted, 1);
  histogram_tester_.ExpectBucketCount(
      "Glic.InvocationSource.TopChromeButton.HadEvent",
      GlicInstanceEvent::kTurnCompleted, 1);

  // Simulate toggling with a different source later on.
  ShowOptions show_options2{FloatingShowOptions{}};
  metrics_.OnOpen(mojom::InvocationSource::kOsButton, show_options2);

  // Another turn completed event.
  metrics_.OnTurnCompleted(mojom::WebClientModel::kActor,
                           base::Milliseconds(100));

  // Should continue to log under the original TopChromeButton entrypoint.
  histogram_tester_.ExpectBucketCount(
      "Glic.InvocationSource.TopChromeButton.EventCounts",
      GlicInstanceEvent::kTurnCompleted, 2);
  histogram_tester_.ExpectBucketCount(
      "Glic.InvocationSource.TopChromeButton.HadEvent",
      GlicInstanceEvent::kTurnCompleted, 1);

  // Should NOT log under OsButton.
  histogram_tester_.ExpectBucketCount(
      "Glic.InvocationSource.OsButton.EventCounts",
      GlicInstanceEvent::kTurnCompleted, 0);
  histogram_tester_.ExpectBucketCount("Glic.InvocationSource.OsButton.HadEvent",
                                      GlicInstanceEvent::kTurnCompleted, 0);
}

TEST_F(GlicInstanceMetricsTest, WebUiLoadTime_Visible) {
  ShowOptions show_options{FloatingShowOptions{}};
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  metrics_.OnVisibilityChanged(true);

  metrics_.OnWebUiStateChanged(mojom::WebUiState::kBeginLoad);
  task_environment_.FastForwardBy(base::Milliseconds(300));
  metrics_.OnWebUiStateChanged(mojom::WebUiState::kReady);

  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Instance.WebUiLoadTime.Visible", base::Milliseconds(300), 1);
  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.InvocationSource.TopChromeButton.WebUiLoadTime.Visible",
      base::Milliseconds(300), 1);
}

TEST_F(GlicInstanceMetricsTest, WebUiLoadTime_Nonvisible) {
  ShowOptions show_options{FloatingShowOptions{}};
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  // Do not call OnVisibilityChanged(true) so it defaults to nonvisible.

  metrics_.OnWebUiStateChanged(mojom::WebUiState::kBeginLoad);
  task_environment_.FastForwardBy(base::Milliseconds(150));
  metrics_.OnWebUiStateChanged(mojom::WebUiState::kReady);

  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Instance.WebUiLoadTime.Nonvisible", base::Milliseconds(150), 1);
  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.InvocationSource.TopChromeButton.WebUiLoadTime.Nonvisible",
      base::Milliseconds(150), 1);
}

TEST_F(GlicInstanceMetricsTest, ValidResponseFlow_DoesNotLogError) {
  metrics_.OnVisibilityChanged(true);
  metrics_.OnUserInputSubmitted(mojom::WebClientMode::kText);
  metrics_.OnResponseStarted();
  metrics_.OnResponseStopped(mojom::ResponseStopCause::kUser);
  histogram_tester_.ExpectTotalCount("Glic.Instance.Metrics.Error", 0);

  EXPECT_EQ(1, user_action_tester_.GetActionCount("GlicResponseInputSubmit"));
  EXPECT_EQ(1, user_action_tester_.GetActionCount("GlicResponseStart"));
  EXPECT_EQ(1, user_action_tester_.GetActionCount("GlicResponse"));
  EXPECT_EQ(1, user_action_tester_.GetActionCount("GlicResponseStop"));
  EXPECT_EQ(1, user_action_tester_.GetActionCount("GlicResponseStopByUser"));
}

TEST_F(GlicInstanceMetricsTest, ActuationIsRetry) {
  metrics_.OnActionSubmitted(true);
  histogram_tester_.ExpectUniqueSample("Glic.Turn.Actuation.IsRetry", true, 1);

  metrics_.OnActionSubmitted(false);
  histogram_tester_.ExpectBucketCount("Glic.Turn.Actuation.IsRetry", false, 1);
  histogram_tester_.ExpectTotalCount("Glic.Turn.Actuation.IsRetry", 2);
}

TEST_F(GlicInstanceMetricsTest, ActuationResponseStopTime) {
  metrics_.OnVisibilityChanged(true);

  // Submit an action result which sets up the actuation timing.
  metrics_.OnActionSubmitted(false);

  // Advance time so we have a non-zero measurable latency.
  task_environment_.FastForwardBy(base::Milliseconds(123));

  metrics_.OnResponseStarted();
  metrics_.OnResponseStopped(mojom::ResponseStopCause::kUser);

  histogram_tester_.ExpectTotalCount("Glic.Turn.Actuation.ResponseStartTime",
                                     1);
  histogram_tester_.ExpectTotalCount("Glic.Turn.Actuation.ResponseStopTime", 1);
  histogram_tester_.ExpectTotalCount(
      "Glic.Turn.Actuation.ResponseStopTime.ByUser", 1);
}

TEST_F(GlicInstanceMetricsTest, InputModesUsed_IgnoresUnknown) {
  {
    GlicInstanceMetrics metrics(&profile_metrics_service_);
    metrics.OnVisibilityChanged(true);
    metrics.OnUserInputSubmitted(mojom::WebClientMode::kUnknown);
    metrics.OnUserInputSubmitted(mojom::WebClientMode::kAudio);
  }

  histogram_tester_.ExpectTotalCount("Glic.Instance.InputModesUsed", 1);
  histogram_tester_.ExpectBucketCount("Glic.Instance.InputModesUsed",
                                      InputModesUsed::kOnlyAudio, 1);

  {
    GlicInstanceMetrics metrics(&profile_metrics_service_);
    metrics.OnVisibilityChanged(true);
    metrics.OnUserInputSubmitted(mojom::WebClientMode::kUnknown);
  }

  histogram_tester_.ExpectTotalCount("Glic.Instance.InputModesUsed", 2);
  histogram_tester_.ExpectBucketCount("Glic.Instance.InputModesUsed",
                                      InputModesUsed::kNone, 1);
}

TEST_F(GlicInstanceMetricsTest, OnTurnCompleted_LogsHistograms) {
  metrics_.OnTurnCompleted(mojom::WebClientModel::kDefault,
                           base::Milliseconds(100));
  histogram_tester_.ExpectUniqueTimeSample("Glic.Turn.Duration.Default",
                                           base::Milliseconds(100), 1);

  metrics_.OnTurnCompleted(mojom::WebClientModel::kActor,
                           base::Milliseconds(200));
  histogram_tester_.ExpectUniqueTimeSample("Glic.Turn.Duration.Actor",
                                           base::Milliseconds(200), 1);
}

TEST_F(GlicInstanceMetricsTest, ScrollToMetrics) {
  base::test::ScopedFeatureList features(features::kGlicScrollTo);
  metrics_.OnVisibilityChanged(true);
  metrics_.OnUserInputSubmitted(mojom::WebClientMode::kText);
  metrics_.OnResponseStarted();
  metrics_.OnGlicScrollAttempt();
  task_environment_.FastForwardBy(base::Milliseconds(400));
  metrics_.OnResponseStopped(mojom::ResponseStopCause::kUnknown);
  metrics_.OnGlicScrollComplete(true);

  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.ScrollTo.UserPromptToScrollTime.Text", base::Milliseconds(400), 1);
}

TEST_F(GlicInstanceMetricsTest, SelectionUsed) {
  metrics_.OnVisibilityChanged(true);
  metrics_.OnSelectionAreasChanged(2);
  metrics_.OnUserInputSubmitted(mojom::WebClientMode::kText);
  histogram_tester_.ExpectBucketCount(
      "Glic.Instance.InputSubmitted.SelectionCount", 2, 1);

  // Check that it's NOT reset after submission.
  metrics_.OnUserInputSubmitted(mojom::WebClientMode::kText);
  histogram_tester_.ExpectBucketCount(
      "Glic.Instance.InputSubmitted.SelectionCount", 2, 2);

  // Check that it can be cleared.
  metrics_.OnSelectionAreasChanged(0);
  metrics_.OnUserInputSubmitted(mojom::WebClientMode::kText);
  histogram_tester_.ExpectBucketCount(
      "Glic.Instance.InputSubmitted.SelectionCount", 0, 1);
  histogram_tester_.ExpectBucketCount(
      "Glic.Instance.InputSubmitted.SelectionCount", 2, 2);
}

TEST_F(GlicInstanceMetricsTestWithPolyline, PolylineSelectionUsed) {
  metrics_.OnVisibilityChanged(true);
  metrics_.OnPolylinePointsChanged({4, 10});
  metrics_.OnUserInputSubmitted(mojom::WebClientMode::kText);

  histogram_tester_.ExpectBucketCount(
      "Glic.Instance.InputSubmitted.Selection.PolylinePointCount", 4, 1);
  histogram_tester_.ExpectBucketCount(
      "Glic.Instance.InputSubmitted.Selection.PolylinePointCount", 10, 1);
  histogram_tester_.ExpectTotalCount(
      "Glic.Instance.InputSubmitted.Selection.PolylinePointCount", 2);

  // Check that it's NOT reset after submission (Persistence)
  metrics_.OnPolylinePointsChanged({4, 8, 8, 10});
  metrics_.OnUserInputSubmitted(mojom::WebClientMode::kText);
  histogram_tester_.ExpectBucketCount(
      "Glic.Instance.InputSubmitted.Selection.PolylinePointCount", 4, 2);
  histogram_tester_.ExpectBucketCount(
      "Glic.Instance.InputSubmitted.Selection.PolylinePointCount", 8, 2);
  histogram_tester_.ExpectBucketCount(
      "Glic.Instance.InputSubmitted.Selection.PolylinePointCount", 10, 2);
  histogram_tester_.ExpectTotalCount(
      "Glic.Instance.InputSubmitted.Selection.PolylinePointCount", 6);

  // Check that it can be cleared
  metrics_.OnPolylinePointsChanged({});
  metrics_.OnUserInputSubmitted(mojom::WebClientMode::kText);

  histogram_tester_.ExpectTotalCount(
      "Glic.Instance.InputSubmitted.Selection.PolylinePointCount", 6);
}

TEST_F(GlicInstanceMetricsTest, Floaty_OpenCloseClose_LogsError) {
  ShowOptions show_options{FloatingShowOptions{}};
  metrics_.OnShowInFloaty(show_options);
  metrics_.OnFloatyClosed();
  metrics_.OnFloatyClosed();
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Metrics.Error",
      GlicInstanceMetricsError::kFloatyClosedWithoutOpen, 1);
}

TEST_F(GlicInstanceMetricsTest, SidePanel_OpenCloseClose_LogsError) {
  EXPECT_CALL(mock_tab_, GetTabHandle()).WillRepeatedly(testing::Return(1));
  metrics_.OnShowInSidePanel(&mock_tab_);
  metrics_.OnSidePanelClosed(
      &mock_tab_, GlicInstanceMetrics::CloseReason::kExplicitlyClosed);
  metrics_.OnSidePanelClosed(
      &mock_tab_, GlicInstanceMetrics::CloseReason::kExplicitlyClosed);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Metrics.Error",
      GlicInstanceMetricsError::kSidePanelClosedWithoutOpen, 1);
}

TEST_F(GlicInstanceMetricsTest, Response_InputStopStop_LogsError) {
  metrics_.OnVisibilityChanged(true);
  metrics_.OnUserInputSubmitted(mojom::WebClientMode::kText);
  metrics_.OnResponseStarted();
  metrics_.OnResponseStopped(mojom::ResponseStopCause::kUser);
  metrics_.OnResponseStopped(mojom::ResponseStopCause::kUser);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Metrics.Error",
      GlicInstanceMetricsError::kResponseStopWithoutInput, 1);
}

TEST_F(GlicInstanceMetricsTest, RecordTabPinningStatusEventLogs) {
  base::TimeTicks now = base::TimeTicks::Now();
  GlicPinEvent pin_event(GlicPinTrigger::kContextMenu, now);
  metrics_.RecordTabPinningStatusEvent(&mock_tab_, pin_event);
  histogram_tester_.ExpectUniqueSample("Glic.Instance.TabPinTrigger",
                                       GlicPinTrigger::kContextMenu, 1);
}

TEST_F(GlicInstanceMetricsTest,
       RecordTabPinningStatusEvent_LogsUnpinHistogram) {
  base::TimeTicks now = base::TimeTicks::Now();
  GlicPinnedTabUsage usage(GlicPinTrigger::kContextMenu, now);
  GlicUnpinEvent unpin_event(GlicUnpinTrigger::kContextMenu, std::move(usage),
                             now);
  metrics_.RecordTabPinningStatusEvent(&mock_tab_, unpin_event);
  histogram_tester_.ExpectUniqueSample("Glic.Instance.TabUnpinTrigger",
                                       GlicUnpinTrigger::kContextMenu, 1);
}

TEST_F(GlicInstanceMetricsTest, TurnSegmentation_OsButtonAttachedText) {
  ShowOptions show_options{FloatingShowOptions{}};
  metrics_.OnOpen(mojom::InvocationSource::kOsButton, show_options);
  metrics_.OnShowInSidePanel(&mock_tab_);
  metrics_.OnVisibilityChanged(true);
  metrics_.OnUserInputSubmitted(mojom::WebClientMode::kText);
  metrics_.OnResponseStarted();

  histogram_tester_.ExpectBucketCount(
      "Glic.Response.Segmentation", ResponseSegmentation::kOsButtonAttachedText,
      1);
}

TEST_F(GlicInstanceMetricsTest, TurnSegmentation_3DotsMenuDetachedAudio) {
  ShowOptions show_options{FloatingShowOptions{}};
  metrics_.OnOpen(mojom::InvocationSource::kThreeDotsMenu, show_options);
  metrics_.OnShowInFloaty(show_options);
  metrics_.OnVisibilityChanged(true);
  metrics_.OnUserInputSubmitted(mojom::WebClientMode::kAudio);
  metrics_.OnResponseStarted();

  histogram_tester_.ExpectBucketCount(
      "Glic.Response.Segmentation",
      ResponseSegmentation::kThreeDotsMenuDetachedAudio, 1);
}

TEST_F(GlicInstanceMetricsTest, TurnUkm) {
  ShowOptions show_options{FloatingShowOptions{}};
  metrics_.OnOpen(mojom::InvocationSource::kOsButton, show_options);
  metrics_.OnShowInSidePanel(&mock_tab_);
  metrics_.OnVisibilityChanged(true);
  metrics_.OnUserInputSubmitted(mojom::WebClientMode::kText);
  metrics_.OnResponseStarted();

  auto entries = ukm_tester_.GetEntriesByName("Glic.Response");
  ASSERT_EQ(entries.size(), 1u);
  auto entry = entries[0];
  ukm_tester_.ExpectEntryMetric(entry, "Attached", true);
  ukm_tester_.ExpectEntryMetric(
      entry, "InvocationSource",
      static_cast<int64_t>(mojom::InvocationSource::kOsButton));
  ukm_tester_.ExpectEntryMetric(
      entry, "WebClientMode",
      static_cast<int64_t>(mojom::WebClientMode::kText));
}

TEST_F(GlicInstanceMetricsTest, RecordSkillsWebClientEvent_RoutesOpenedMenu) {
  metrics_.RecordSkillsWebClientEvent(mojom::SkillsWebClientEvent::kOpenedMenu);

  histogram_tester_.ExpectUniqueSample("Glic.Skills.WebClient.Event",
                                       mojom::SkillsWebClientEvent::kOpenedMenu,
                                       1);
  histogram_tester_.ExpectUniqueSample("Glic.Skills.Invoke.Funnel",
                                       SkillsInvokeFunnel::kOpenedMenu, 1);
  // Ensure it didn't accidentally log an invocation action.
  histogram_tester_.ExpectTotalCount("Skills.Invoke.Action", 0);
}

TEST_F(GlicInstanceMetricsTest,
       RecordSkillsWebClientEvent_RoutesFirstPartySkill) {
  metrics_.RecordSkillsWebClientEvent(
      mojom::SkillsWebClientEvent::kUsedFirstPartySkill);

  histogram_tester_.ExpectUniqueSample(
      "Glic.Skills.WebClient.Event",
      mojom::SkillsWebClientEvent::kUsedFirstPartySkill, 1);
  histogram_tester_.ExpectUniqueSample(
      "Skills.Invoke.Action", skills::SkillsInvokeAction::kFirstParty, 1);
  histogram_tester_.ExpectUniqueSample("Glic.Skills.Invoke.Funnel",
                                       SkillsInvokeFunnel::kInvokedSkill, 1);
}
TEST_F(GlicInstanceMetricsTest,
       RecordSkillsWebClientEvent_RoutesSkillBuilderPromo) {
  metrics_.RecordSkillsWebClientEvent(
      mojom::SkillsWebClientEvent::kSkillBuilderClickedPromoChip);

  histogram_tester_.ExpectUniqueSample("Glic.Skills.SkillBuilder.Event",
                                       SkillBuilderEvent::kClickedPromoChip, 1);
}

TEST_F(GlicInstanceMetricsTest, RecordSkillsWebClientEvent_IsNoOpWhenUnknown) {
  metrics_.RecordSkillsWebClientEvent(mojom::SkillsWebClientEvent::kUnknown);

  // No metrics should be emitted.
  EXPECT_TRUE(histogram_tester_.GetTotalCountsForPrefix("Skills.").empty());
}

TEST_F(GlicInstanceMetricsTest, OnInstanceDestroyed_LogsPerProfileTurnCount) {
  metrics::ProfileMetricsContext context = 1;  // Profile 1 -> .Profile1
  metrics::ProfileMetricsService profile_metrics_service{context};
  {
    GlicInstanceMetrics metrics_with_profile(&profile_metrics_service);
    metrics_with_profile.OnTurnCompleted(mojom::WebClientModel::kDefault,
                                         base::Milliseconds(100));
    metrics_with_profile.OnTurnCompleted(mojom::WebClientModel::kDefault,
                                         base::Milliseconds(200));
  }  // Destructor calls OnInstanceDestroyed

  histogram_tester_.ExpectUniqueSample("Glic.Instance.TurnCount", 2, 1);
  histogram_tester_.ExpectUniqueSample("Glic.Instance.TurnCount.Profile1", 2,
                                       1);
}

TEST_F(GlicInstanceMetricsTest, ZoomChangeCount) {
  {
    GlicInstanceMetrics metrics(&profile_metrics_service_);
    metrics.OnZoomLevelChange();
    metrics.OnZoomLevelChange();
    metrics.OnZoomLevelChange();
    metrics.OnClose();
  }  // Destructor calls OnInstanceDestroyed

  histogram_tester_.ExpectUniqueSample("Glic.Instance.ZoomChangeCount", 3, 1);
}

}  // namespace glic
