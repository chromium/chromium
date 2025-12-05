// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/metrics/glic_instance_metrics.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/task_environment.h"
#include "chrome/browser/glic/glic_metrics.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/split_tab_id.h"
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

}  // namespace glic
