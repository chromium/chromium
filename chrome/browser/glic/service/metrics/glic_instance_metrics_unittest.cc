// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/metrics/glic_instance_metrics.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/task_environment.h"
#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/host/glic.mojom-shared.h"
#include "components/skills/public/skills_metrics.h"
#include "components/split_tabs/split_tab_id.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
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
  GlicInstanceMetrics metrics_;
  tabs::MockTabInterface mock_tab_;
  ui::UnownedUserDataHost unowned_user_data_host_;
  base::UserActionTester user_action_tester_;
};

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

TEST_F(GlicInstanceMetricsTest, OnFloatyClosed_WithoutOpening_LogsError) {
  metrics_.OnFloatyClosed();
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Metrics.Error",
      GlicInstanceMetricsError::kFloatyClosedWithoutOpen, 1);
}

TEST_F(GlicInstanceMetricsTest, OnSidePanelClosed_WithoutOpening_LogsError) {
  metrics_.OnSidePanelClosed(static_cast<tabs::TabInterface*>(&mock_tab_));
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
  metrics_.OnSidePanelClosed(&mock_tab_);
  histogram_tester_.ExpectTotalCount("Glic.Instance.Metrics.Error", 0);
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

TEST_F(GlicInstanceMetricsTest, OnReaction_LogsUserActions) {
  metrics_.OnVisibilityChanged(true);
  metrics_.OnUserInputSubmitted(mojom::WebClientMode::kText);
  metrics_.OnResponseStarted();
  metrics_.OnResponseStopped(mojom::ResponseStopCause::kUnknown);

  metrics_.OnReaction(mojom::MetricUserInputReactionType::kCanned);
  EXPECT_EQ(1, user_action_tester_.GetActionCount("GlicReactionCanned"));

  metrics_.OnReaction(mojom::MetricUserInputReactionType::kModel);
  EXPECT_EQ(1, user_action_tester_.GetActionCount("GlicReactionModelled"));
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
  metrics_.OnSidePanelClosed(&mock_tab_);
  metrics_.OnSidePanelClosed(&mock_tab_);
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

}  // namespace glic
