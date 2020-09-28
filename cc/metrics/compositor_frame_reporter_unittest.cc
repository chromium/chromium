// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/compositor_frame_reporter.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_tick_clock.h"
#include "cc/metrics/compositor_frame_reporting_controller.h"
#include "cc/metrics/event_metrics.h"
#include "components/viz/common/frame_timing_details.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/types/scroll_input_type.h"

namespace cc {
namespace {

using ::testing::Each;
using ::testing::IsEmpty;
using ::testing::NotNull;

class CompositorFrameReporterTest : public testing::Test {
 public:
  CompositorFrameReporterTest()
      : pipeline_reporter_(std::make_unique<CompositorFrameReporter>(
            CompositorFrameReporter::ActiveTrackers(),
            viz::BeginFrameArgs(),
            nullptr,
            /*should_report_metrics=*/true,
            CompositorFrameReporter::SmoothThread::kSmoothBoth,
            /*layer_tree_host_id=*/1)) {
    pipeline_reporter_->set_tick_clock(&test_tick_clock_);
    AdvanceNowByMs(1);
  }

 protected:
  base::TimeTicks AdvanceNowByMs(int advance_ms) {
    test_tick_clock_.Advance(base::TimeDelta::FromMicroseconds(advance_ms));
    return test_tick_clock_.NowTicks();
  }

  base::TimeTicks Now() { return test_tick_clock_.NowTicks(); }

  std::unique_ptr<BeginMainFrameMetrics> BuildBlinkBreakdown() {
    auto breakdown = std::make_unique<BeginMainFrameMetrics>();
    breakdown->handle_input_events = base::TimeDelta::FromMicroseconds(11);
    breakdown->animate = base::TimeDelta::FromMicroseconds(10);
    breakdown->style_update = base::TimeDelta::FromMicroseconds(9);
    breakdown->layout_update = base::TimeDelta::FromMicroseconds(8);
    breakdown->compositing_inputs = base::TimeDelta::FromMicroseconds(7);
    breakdown->prepaint = base::TimeDelta::FromMicroseconds(6);
    breakdown->compositing_assignments = base::TimeDelta::FromMicroseconds(5);
    breakdown->paint = base::TimeDelta::FromMicroseconds(4);
    breakdown->scrolling_coordinator = base::TimeDelta::FromMicroseconds(3);
    breakdown->composite_commit = base::TimeDelta::FromMicroseconds(2);
    breakdown->update_layers = base::TimeDelta::FromMicroseconds(1);

    // Advance now by the sum of the breakdowns.
    AdvanceNowByMs(11 + 10 + 9 + 8 + 7 + 6 + 5 + 4 + 3 + 2 + 1);

    return breakdown;
  }

  viz::FrameTimingDetails BuildVizBreakdown() {
    viz::FrameTimingDetails viz_breakdown;
    viz_breakdown.received_compositor_frame_timestamp = AdvanceNowByMs(1);
    viz_breakdown.draw_start_timestamp = AdvanceNowByMs(2);
    viz_breakdown.swap_timings.swap_start = AdvanceNowByMs(3);
    viz_breakdown.swap_timings.swap_end = AdvanceNowByMs(4);
    viz_breakdown.presentation_feedback.timestamp = AdvanceNowByMs(5);
    return viz_breakdown;
  }

  // This should be defined before |pipeline_reporter_| so it is created before
  // and destroyed after that.
  base::SimpleTestTickClock test_tick_clock_;

  std::unique_ptr<CompositorFrameReporter> pipeline_reporter_;
};

TEST_F(CompositorFrameReporterTest, MainFrameAbortedReportingTest) {
  base::HistogramTester histogram_tester;

  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kBeginImplFrameToSendBeginMainFrame,
      Now());
  EXPECT_EQ(0, pipeline_reporter_->StageHistorySizeForTesting());

  AdvanceNowByMs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kSendBeginMainFrameToCommit, Now());
  EXPECT_EQ(1, pipeline_reporter_->StageHistorySizeForTesting());

  AdvanceNowByMs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kEndActivateToSubmitCompositorFrame,
      Now());
  EXPECT_EQ(2, pipeline_reporter_->StageHistorySizeForTesting());

  AdvanceNowByMs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::
          kSubmitCompositorFrameToPresentationCompositorFrame,
      Now());
  EXPECT_EQ(3, pipeline_reporter_->StageHistorySizeForTesting());

  AdvanceNowByMs(3);
  pipeline_reporter_->TerminateFrame(
      CompositorFrameReporter::FrameTerminationStatus::kPresentedFrame, Now());
  EXPECT_EQ(4, pipeline_reporter_->StageHistorySizeForTesting());

  pipeline_reporter_ = nullptr;
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.BeginImplFrameToSendBeginMainFrame", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SendBeginMainFrameToCommit", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.Commit", 0);
  histogram_tester.ExpectTotalCount("CompositorLatency.EndCommitToActivation",
                                    0);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.EndActivateToSubmitCompositorFrame", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SubmitCompositorFrameToPresentationCompositorFrame",
      1);
}

TEST_F(CompositorFrameReporterTest, ReplacedByNewReporterReportingTest) {
  base::HistogramTester histogram_tester;

  pipeline_reporter_->StartStage(CompositorFrameReporter::StageType::kCommit,
                                 Now());
  EXPECT_EQ(0, pipeline_reporter_->StageHistorySizeForTesting());

  AdvanceNowByMs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kEndCommitToActivation, Now());
  EXPECT_EQ(1, pipeline_reporter_->StageHistorySizeForTesting());

  AdvanceNowByMs(2);
  pipeline_reporter_->TerminateFrame(
      CompositorFrameReporter::FrameTerminationStatus::kReplacedByNewReporter,
      Now());
  EXPECT_EQ(2, pipeline_reporter_->StageHistorySizeForTesting());

  pipeline_reporter_ = nullptr;
  histogram_tester.ExpectTotalCount("CompositorLatency.Commit", 0);
  histogram_tester.ExpectTotalCount("CompositorLatency.EndCommitToActivation",
                                    0);
}

TEST_F(CompositorFrameReporterTest, SubmittedFrameReportingTest) {
  base::HistogramTester histogram_tester;

  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kActivation, Now());
  EXPECT_EQ(0, pipeline_reporter_->StageHistorySizeForTesting());

  AdvanceNowByMs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kEndActivateToSubmitCompositorFrame,
      Now());
  EXPECT_EQ(1, pipeline_reporter_->StageHistorySizeForTesting());

  AdvanceNowByMs(2);
  pipeline_reporter_->TerminateFrame(
      CompositorFrameReporter::FrameTerminationStatus::kPresentedFrame, Now());
  EXPECT_EQ(2, pipeline_reporter_->StageHistorySizeForTesting());

  pipeline_reporter_ = nullptr;
  histogram_tester.ExpectTotalCount("CompositorLatency.Activation", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.EndActivateToSubmitCompositorFrame", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.TotalLatency", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.DroppedFrame.Activation",
                                    0);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.DroppedFrame.EndActivateToSubmitCompositorFrame", 0);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.DroppedFrame.TotalLatency", 0);

  histogram_tester.ExpectBucketCount("CompositorLatency.Activation", 3, 1);
  histogram_tester.ExpectBucketCount(
      "CompositorLatency.EndActivateToSubmitCompositorFrame", 2, 1);
  histogram_tester.ExpectBucketCount("CompositorLatency.TotalLatency", 5, 1);
}

TEST_F(CompositorFrameReporterTest, SubmittedDroppedFrameReportingTest) {
  base::HistogramTester histogram_tester;

  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kSendBeginMainFrameToCommit, Now());
  EXPECT_EQ(0, pipeline_reporter_->StageHistorySizeForTesting());

  AdvanceNowByMs(3);
  pipeline_reporter_->StartStage(CompositorFrameReporter::StageType::kCommit,
                                 Now());
  EXPECT_EQ(1, pipeline_reporter_->StageHistorySizeForTesting());

  AdvanceNowByMs(2);
  pipeline_reporter_->TerminateFrame(
      CompositorFrameReporter::FrameTerminationStatus::kDidNotPresentFrame,
      Now());
  EXPECT_EQ(2, pipeline_reporter_->StageHistorySizeForTesting());

  pipeline_reporter_ = nullptr;
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.DroppedFrame.SendBeginMainFrameToCommit", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.DroppedFrame.Commit", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.DroppedFrame.TotalLatency", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SendBeginMainFrameToCommit", 0);
  histogram_tester.ExpectTotalCount("CompositorLatency.Commit", 0);
  histogram_tester.ExpectTotalCount("CompositorLatency.TotalLatency", 0);

  histogram_tester.ExpectBucketCount(
      "CompositorLatency.DroppedFrame.SendBeginMainFrameToCommit", 3, 1);
  histogram_tester.ExpectBucketCount("CompositorLatency.DroppedFrame.Commit", 2,
                                     1);
  histogram_tester.ExpectBucketCount(
      "CompositorLatency.DroppedFrame.TotalLatency", 5, 1);
}

// Tests that when a frame is presented to the user, total event latency metrics
// are reported properly.
TEST_F(CompositorFrameReporterTest,
       EventLatencyTotalForPresentedFrameReported) {
  base::HistogramTester histogram_tester;

  const base::TimeTicks event_time = Now();
  std::unique_ptr<EventMetrics> event_metrics_ptrs[] = {
      EventMetrics::Create(ui::ET_TOUCH_PRESSED, base::nullopt, event_time,
                           base::nullopt),
      EventMetrics::Create(ui::ET_TOUCH_MOVED, base::nullopt, event_time,
                           base::nullopt),
      EventMetrics::Create(ui::ET_TOUCH_MOVED, base::nullopt, event_time,
                           base::nullopt),
  };
  EXPECT_THAT(event_metrics_ptrs, Each(NotNull()));
  std::vector<EventMetrics> events_metrics = {
      *event_metrics_ptrs[0], *event_metrics_ptrs[1], *event_metrics_ptrs[2]};

  AdvanceNowByMs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kBeginImplFrameToSendBeginMainFrame,
      Now());

  AdvanceNowByMs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kEndActivateToSubmitCompositorFrame,
      Now());

  AdvanceNowByMs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::
          kSubmitCompositorFrameToPresentationCompositorFrame,
      Now());
  pipeline_reporter_->SetEventsMetrics(std::move(events_metrics));

  AdvanceNowByMs(3);
  const base::TimeTicks presentation_time = Now();
  pipeline_reporter_->TerminateFrame(
      CompositorFrameReporter::FrameTerminationStatus::kPresentedFrame,
      presentation_time);

  pipeline_reporter_ = nullptr;

  const int latency_ms = (presentation_time - event_time).InMicroseconds();
  histogram_tester.ExpectTotalCount("EventLatency.TouchPressed.TotalLatency",
                                    1);
  histogram_tester.ExpectTotalCount("EventLatency.TouchMoved.TotalLatency", 2);
  histogram_tester.ExpectTotalCount("EventLatency.TotalLatency", 3);
  histogram_tester.ExpectBucketCount("EventLatency.TouchPressed.TotalLatency",
                                     latency_ms, 1);
  histogram_tester.ExpectBucketCount("EventLatency.TouchMoved.TotalLatency",
                                     latency_ms, 2);
  histogram_tester.ExpectBucketCount("EventLatency.TotalLatency", latency_ms,
                                     3);
}

// Tests that when a frame is presented to the user, event latency breakdown
// metrics are reported properly.
TEST_F(CompositorFrameReporterTest,
       EventLatencyBreakdownsForPresentedFrameReported) {
  base::HistogramTester histogram_tester;

  const base::TimeTicks event_time = Now();
  std::unique_ptr<EventMetrics> event_metrics_ptrs[] = {
      EventMetrics::Create(ui::ET_TOUCH_PRESSED, base::nullopt, event_time,
                           base::nullopt),
  };
  EXPECT_THAT(event_metrics_ptrs, Each(NotNull()));
  std::vector<EventMetrics> events_metrics = {*event_metrics_ptrs[0]};

  auto begin_impl_time = AdvanceNowByMs(2);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kBeginImplFrameToSendBeginMainFrame,
      begin_impl_time);

  auto begin_main_time = AdvanceNowByMs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kSendBeginMainFrameToCommit,
      begin_main_time);

  auto begin_main_start_time = AdvanceNowByMs(4);
  std::unique_ptr<BeginMainFrameMetrics> blink_breakdown =
      BuildBlinkBreakdown();
  // Make a copy of the breakdown to use in verifying expectations in the end.
  BeginMainFrameMetrics blink_breakdown_copy = *blink_breakdown;
  pipeline_reporter_->SetBlinkBreakdown(std::move(blink_breakdown),
                                        begin_main_start_time);
  auto begin_commit_time = AdvanceNowByMs(5);
  pipeline_reporter_->StartStage(CompositorFrameReporter::StageType::kCommit,
                                 begin_commit_time);

  auto end_commit_time = AdvanceNowByMs(6);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kEndCommitToActivation,
      end_commit_time);

  auto begin_activation_time = AdvanceNowByMs(7);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kActivation, begin_activation_time);

  auto end_activation_time = AdvanceNowByMs(8);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kEndActivateToSubmitCompositorFrame,
      end_activation_time);

  auto submit_time = AdvanceNowByMs(9);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::
          kSubmitCompositorFrameToPresentationCompositorFrame,
      submit_time);
  pipeline_reporter_->SetEventsMetrics(std::move(events_metrics));

  AdvanceNowByMs(10);
  viz::FrameTimingDetails viz_breakdown = BuildVizBreakdown();
  pipeline_reporter_->SetVizBreakdown(viz_breakdown);
  pipeline_reporter_->TerminateFrame(
      CompositorFrameReporter::FrameTerminationStatus::kPresentedFrame,
      viz_breakdown.presentation_feedback.timestamp);

  pipeline_reporter_ = nullptr;

  struct {
    const char* name;
    const base::TimeDelta latency;
  } expected_latencies[] = {
      {"EventLatency.TouchPressed.BrowserToRendererCompositor",
       begin_impl_time - event_time},
      {"EventLatency.TouchPressed.BeginImplFrameToSendBeginMainFrame",
       begin_main_time - begin_impl_time},
      {"EventLatency.TouchPressed.SendBeginMainFrameToCommit",
       begin_commit_time - begin_main_time},
      {"EventLatency.TouchPressed.SendBeginMainFrameToCommit.HandleInputEvents",
       blink_breakdown_copy.handle_input_events},
      {"EventLatency.TouchPressed.SendBeginMainFrameToCommit.Animate",
       blink_breakdown_copy.animate},
      {"EventLatency.TouchPressed.SendBeginMainFrameToCommit.StyleUpdate",
       blink_breakdown_copy.style_update},
      {"EventLatency.TouchPressed.SendBeginMainFrameToCommit.LayoutUpdate",
       blink_breakdown_copy.layout_update},
      {"EventLatency.TouchPressed.SendBeginMainFrameToCommit.CompositingInputs",
       blink_breakdown_copy.compositing_inputs},
      {"EventLatency.TouchPressed.SendBeginMainFrameToCommit.Prepaint",
       blink_breakdown_copy.prepaint},
      {"EventLatency.TouchPressed.SendBeginMainFrameToCommit"
       ".CompositingAssignments",
       blink_breakdown_copy.compositing_assignments},
      {"EventLatency.TouchPressed.SendBeginMainFrameToCommit.Paint",
       blink_breakdown_copy.paint},
      {"EventLatency.TouchPressed.SendBeginMainFrameToCommit."
       "ScrollingCoordinator",
       blink_breakdown_copy.scrolling_coordinator},
      {"EventLatency.TouchPressed.SendBeginMainFrameToCommit.CompositeCommit",
       blink_breakdown_copy.composite_commit},
      {"EventLatency.TouchPressed.SendBeginMainFrameToCommit.UpdateLayers",
       blink_breakdown_copy.update_layers},
      {"EventLatency.TouchPressed.SendBeginMainFrameToCommit."
       "BeginMainSentToStarted",
       begin_main_start_time - begin_main_time},
      {"EventLatency.TouchPressed.Commit", end_commit_time - begin_commit_time},
      {"EventLatency.TouchPressed.EndCommitToActivation",
       begin_activation_time - end_commit_time},
      {"EventLatency.TouchPressed.Activation",
       end_activation_time - begin_activation_time},
      {"EventLatency.TouchPressed.EndActivateToSubmitCompositorFrame",
       submit_time - end_activation_time},
      {"EventLatency.TouchPressed."
       "SubmitCompositorFrameToPresentationCompositorFrame",
       viz_breakdown.presentation_feedback.timestamp - submit_time},
      {"EventLatency.TouchPressed."
       "SubmitCompositorFrameToPresentationCompositorFrame."
       "SubmitToReceiveCompositorFrame",
       viz_breakdown.received_compositor_frame_timestamp - submit_time},
      {"EventLatency.TouchPressed."
       "SubmitCompositorFrameToPresentationCompositorFrame."
       "ReceivedCompositorFrameToStartDraw",
       viz_breakdown.draw_start_timestamp -
           viz_breakdown.received_compositor_frame_timestamp},
      {"EventLatency.TouchPressed."
       "SubmitCompositorFrameToPresentationCompositorFrame."
       "StartDrawToSwapStart",
       viz_breakdown.swap_timings.swap_start -
           viz_breakdown.draw_start_timestamp},
      {"EventLatency.TouchPressed."
       "SubmitCompositorFrameToPresentationCompositorFrame.SwapStartToSwapEnd",
       viz_breakdown.swap_timings.swap_end -
           viz_breakdown.swap_timings.swap_start},
      {"EventLatency.TouchPressed."
       "SubmitCompositorFrameToPresentationCompositorFrame."
       "SwapEndToPresentationCompositorFrame",
       viz_breakdown.presentation_feedback.timestamp -
           viz_breakdown.swap_timings.swap_end},
      {"EventLatency.TouchPressed.TotalLatency",
       viz_breakdown.presentation_feedback.timestamp - event_time},
      {"EventLatency.TotalLatency",
       viz_breakdown.presentation_feedback.timestamp - event_time},
  };

  for (const auto& expected_latency : expected_latencies) {
    histogram_tester.ExpectTotalCount(expected_latency.name, 1);
    histogram_tester.ExpectBucketCount(
        expected_latency.name, expected_latency.latency.InMicroseconds(), 1);
  }
}

// Tests that when a frame is presented to the user, total scroll event latency
// metrics are reported properly.
TEST_F(CompositorFrameReporterTest,
       EventLatencyScrollTotalForPresentedFrameReported) {
  base::HistogramTester histogram_tester;

  const base::TimeTicks event_time = Now();
  std::unique_ptr<EventMetrics> event_metrics_ptrs[] = {
      EventMetrics::Create(ui::ET_GESTURE_SCROLL_BEGIN, base::nullopt,
                           event_time, ui::ScrollInputType::kWheel),
      EventMetrics::Create(ui::ET_GESTURE_SCROLL_UPDATE,
                           EventMetrics::ScrollUpdateType::kStarted, event_time,
                           ui::ScrollInputType::kWheel),
      EventMetrics::Create(ui::ET_GESTURE_SCROLL_UPDATE,
                           EventMetrics::ScrollUpdateType::kContinued,
                           event_time, ui::ScrollInputType::kWheel),
  };
  EXPECT_THAT(event_metrics_ptrs, Each(NotNull()));
  std::vector<EventMetrics> events_metrics = {
      *event_metrics_ptrs[0], *event_metrics_ptrs[1], *event_metrics_ptrs[2]};

  AdvanceNowByMs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kBeginImplFrameToSendBeginMainFrame,
      Now());

  AdvanceNowByMs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kEndActivateToSubmitCompositorFrame,
      Now());

  AdvanceNowByMs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::
          kSubmitCompositorFrameToPresentationCompositorFrame,
      Now());
  pipeline_reporter_->SetEventsMetrics(std::move(events_metrics));

  AdvanceNowByMs(3);
  viz::FrameTimingDetails viz_breakdown = BuildVizBreakdown();
  pipeline_reporter_->SetVizBreakdown(viz_breakdown);
  pipeline_reporter_->TerminateFrame(
      CompositorFrameReporter::FrameTerminationStatus::kPresentedFrame,
      viz_breakdown.presentation_feedback.timestamp);

  pipeline_reporter_ = nullptr;

  const int total_latency_ms =
      (viz_breakdown.presentation_feedback.timestamp - event_time)
          .InMicroseconds();
  const int swap_begin_latency_ms =
      (viz_breakdown.swap_timings.swap_start - event_time).InMicroseconds();
  struct {
    const char* name;
    const int64_t latency_ms;
  } expected_metrics[] = {
      {"EventLatency.GestureScrollBegin.Wheel.TotalLatency", total_latency_ms},
      {"EventLatency.GestureScrollBegin.Wheel.TotalLatencyToSwapBegin",
       swap_begin_latency_ms},
      {"EventLatency.FirstGestureScrollUpdate.Wheel.TotalLatency",
       total_latency_ms},
      {"EventLatency.FirstGestureScrollUpdate.Wheel.TotalLatencyToSwapBegin",
       swap_begin_latency_ms},
      {"EventLatency.GestureScrollUpdate.Wheel.TotalLatency", total_latency_ms},
      {"EventLatency.GestureScrollUpdate.Wheel.TotalLatencyToSwapBegin",
       swap_begin_latency_ms},
  };
  for (const auto& expected_metric : expected_metrics) {
    histogram_tester.ExpectTotalCount(expected_metric.name, 1);
    histogram_tester.ExpectBucketCount(expected_metric.name,
                                       expected_metric.latency_ms, 1);
  }
  histogram_tester.ExpectTotalCount("EventLatency.TotalLatency", 3);
}

// Tests that when the frame is not presented to the user, event latency metrics
// are not reported.
TEST_F(CompositorFrameReporterTest,
       EventLatencyForDidNotPresentFrameNotReported) {
  base::HistogramTester histogram_tester;

  const base::TimeTicks event_time = Now();
  std::unique_ptr<EventMetrics> event_metrics_ptrs[] = {
      EventMetrics::Create(ui::ET_TOUCH_PRESSED, base::nullopt, event_time,
                           base::nullopt),
      EventMetrics::Create(ui::ET_TOUCH_MOVED, base::nullopt, event_time,
                           base::nullopt),
      EventMetrics::Create(ui::ET_TOUCH_MOVED, base::nullopt, event_time,
                           base::nullopt),
  };
  EXPECT_THAT(event_metrics_ptrs, Each(NotNull()));
  std::vector<EventMetrics> events_metrics = {
      *event_metrics_ptrs[0], *event_metrics_ptrs[1], *event_metrics_ptrs[2]};

  AdvanceNowByMs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kBeginImplFrameToSendBeginMainFrame,
      Now());

  AdvanceNowByMs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kEndActivateToSubmitCompositorFrame,
      Now());

  AdvanceNowByMs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::
          kSubmitCompositorFrameToPresentationCompositorFrame,
      Now());
  pipeline_reporter_->SetEventsMetrics(std::move(events_metrics));

  AdvanceNowByMs(3);
  pipeline_reporter_->TerminateFrame(
      CompositorFrameReporter::FrameTerminationStatus::kDidNotPresentFrame,
      Now());

  pipeline_reporter_ = nullptr;

  EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix("EventLaterncy."),
              IsEmpty());
}

}  // namespace
}  // namespace cc
