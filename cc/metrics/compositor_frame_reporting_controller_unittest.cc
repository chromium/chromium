// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/compositor_frame_reporting_controller.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_tick_clock.h"
#include "cc/metrics/dropped_frame_counter.h"
#include "cc/metrics/event_metrics.h"
#include "cc/metrics/total_frame_counter.h"
#include "components/viz/common/frame_timing_details.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/types/scroll_input_type.h"

namespace cc {
namespace {

using ::testing::Each;
using ::testing::IsEmpty;
using ::testing::NotNull;

class TestCompositorFrameReportingController
    : public CompositorFrameReportingController {
 public:
  TestCompositorFrameReportingController()
      : CompositorFrameReportingController(/*should_report_metrics=*/true,
                                           /*layer_tree_host_id=*/1) {}

  TestCompositorFrameReportingController(
      const TestCompositorFrameReportingController& controller) = delete;

  TestCompositorFrameReportingController& operator=(
      const TestCompositorFrameReportingController& controller) = delete;

  int ActiveReporters() {
    int count = 0;
    for (int i = 0; i < PipelineStage::kNumPipelineStages; ++i) {
      if (reporters()[i])
        ++count;
    }
    return count;
  }

  void ResetReporters() {
    for (int i = 0; i < PipelineStage::kNumPipelineStages; ++i) {
      reporters()[i] = nullptr;
    }
  }

  size_t GetBlockingReportersCount() {
    size_t count = 0;
    const PipelineStage kStages[] = {
        PipelineStage::kBeginImplFrame,
        PipelineStage::kBeginMainFrame,
        PipelineStage::kCommit,
        PipelineStage::kActivate,
    };
    for (auto stage : kStages) {
      auto& reporter = reporters()[stage];
      if (reporter && reporter->GetPartialUpdateDependentsCount() > 0) {
        ++count;
      }
    }
    return count;
  }

  size_t GetBlockedReportersCount() {
    size_t count = 0;
    const PipelineStage kStages[] = {
        PipelineStage::kBeginImplFrame,
        PipelineStage::kBeginMainFrame,
        PipelineStage::kCommit,
        PipelineStage::kActivate,
    };
    for (auto stage : kStages) {
      auto& reporter = reporters()[stage];
      if (reporter)
        count += reporter->GetPartialUpdateDependentsCount();
    }
    return count;
  }
};

class CompositorFrameReportingControllerTest : public testing::Test {
 public:
  CompositorFrameReportingControllerTest() : current_id_(1, 1) {
    test_tick_clock_.SetNowTicks(base::TimeTicks::Now());
    reporting_controller_.set_tick_clock(&test_tick_clock_);
    args_ = SimulateBeginFrameArgs(current_id_);
    reporting_controller_.SetDroppedFrameCounter(&dropped_counter);
    dropped_counter.set_total_counter(&total_frame_counter_);
  }

  // The following functions simulate the actions that would
  // occur for each phase of the reporting controller.
  void SimulateBeginImplFrame() {
    IncrementCurrentId();
    begin_impl_time_ = AdvanceNowByMs(10);
    reporting_controller_.WillBeginImplFrame(args_);
  }

  void SimulateBeginMainFrame() {
    if (!reporting_controller_.reporters()[CompositorFrameReportingController::
                                               PipelineStage::kBeginImplFrame])
      SimulateBeginImplFrame();
    CHECK(
        reporting_controller_.reporters()[CompositorFrameReportingController::
                                              PipelineStage::kBeginImplFrame]);
    begin_main_time_ = AdvanceNowByMs(10);
    reporting_controller_.WillBeginMainFrame(args_);
    begin_main_start_time_ = AdvanceNowByMs(10);
  }

  void SimulateCommit(std::unique_ptr<BeginMainFrameMetrics> blink_breakdown) {
    if (!reporting_controller_
             .reporters()[CompositorFrameReportingController::PipelineStage::
                              kBeginMainFrame]) {
      SimulateBeginMainFrame();
    }
    CHECK(
        reporting_controller_.reporters()[CompositorFrameReportingController::
                                              PipelineStage::kBeginMainFrame]);
    reporting_controller_.SetBlinkBreakdown(std::move(blink_breakdown),
                                            begin_main_start_time_);
    begin_commit_time_ = AdvanceNowByMs(10);
    reporting_controller_.WillCommit();
    end_commit_time_ = AdvanceNowByMs(10);
    reporting_controller_.DidCommit();
  }

  void SimulateActivate() {
    if (!reporting_controller_.reporters()
             [CompositorFrameReportingController::PipelineStage::kCommit])
      SimulateCommit(nullptr);
    CHECK(reporting_controller_.reporters()
              [CompositorFrameReportingController::PipelineStage::kCommit]);
    begin_activation_time_ = AdvanceNowByMs(10);
    reporting_controller_.WillActivate();
    end_activation_time_ = AdvanceNowByMs(10);
    reporting_controller_.DidActivate();
    last_activated_id_ = current_id_;
  }

  void SimulateSubmitCompositorFrame(uint32_t frame_token,
                                     EventMetricsSet events_metrics) {
    if (!reporting_controller_.reporters()
             [CompositorFrameReportingController::PipelineStage::kActivate])
      SimulateActivate();
    CHECK(reporting_controller_.reporters()
              [CompositorFrameReportingController::PipelineStage::kActivate]);
    submit_time_ = AdvanceNowByMs(10);
    reporting_controller_.DidSubmitCompositorFrame(frame_token, current_id_,
                                                   last_activated_id_,
                                                   std::move(events_metrics));
  }

  void SimulatePresentCompositorFrame() {
    ++next_token_;
    SimulateSubmitCompositorFrame(*next_token_, {});
    viz::FrameTimingDetails details = {};
    details.presentation_feedback.timestamp = AdvanceNowByMs(10);
    reporting_controller_.DidPresentCompositorFrame(*next_token_, details);
  }

  viz::BeginFrameArgs SimulateBeginFrameArgs(viz::BeginFrameId frame_id) {
    args_ = viz::BeginFrameArgs();
    args_.frame_id = frame_id;
    args_.frame_time = AdvanceNowByMs(10);
    args_.interval = base::TimeDelta::FromMilliseconds(16);
    current_id_ = frame_id;
    return args_;
  }

  std::unique_ptr<BeginMainFrameMetrics> BuildBlinkBreakdown() {
    auto breakdown = std::make_unique<BeginMainFrameMetrics>();
    breakdown->handle_input_events = base::TimeDelta::FromMicroseconds(10);
    breakdown->animate = base::TimeDelta::FromMicroseconds(9);
    breakdown->style_update = base::TimeDelta::FromMicroseconds(8);
    breakdown->layout_update = base::TimeDelta::FromMicroseconds(7);
    breakdown->compositing_inputs = base::TimeDelta::FromMicroseconds(6);
    breakdown->prepaint = base::TimeDelta::FromMicroseconds(5);
    breakdown->compositing_assignments = base::TimeDelta::FromMicroseconds(4);
    breakdown->paint = base::TimeDelta::FromMicroseconds(3);
    breakdown->composite_commit = base::TimeDelta::FromMicroseconds(2);
    breakdown->update_layers = base::TimeDelta::FromMicroseconds(1);

    // Advance now by the sum of the breakdowns.
    AdvanceNowByMs(10 + 9 + 8 + 7 + 6 + 5 + 4 + 3 + 2 + 1);

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

  void IncrementCurrentId() {
    current_id_.sequence_number++;
    args_.frame_id = current_id_;
  }

  base::TimeTicks AdvanceNowByMs(int64_t advance_ms) {
    test_tick_clock_.Advance(base::TimeDelta::FromMicroseconds(advance_ms));
    return test_tick_clock_.NowTicks();
  }

  std::unique_ptr<EventMetrics> CreateEventMetrics(
      ui::EventType type,
      base::Optional<EventMetrics::ScrollUpdateType> scroll_update_type,
      base::Optional<ui::ScrollInputType> scroll_input_type) {
    const base::TimeTicks event_time = AdvanceNowByMs(10);
    AdvanceNowByMs(10);
    std::unique_ptr<EventMetrics> metrics = EventMetrics::CreateForTesting(
        type, scroll_update_type, scroll_input_type, event_time,
        &test_tick_clock_);
    if (metrics) {
      AdvanceNowByMs(10);
      metrics->SetDispatchStageTimestamp(
          EventMetrics::DispatchStage::kRendererCompositorStarted);
      AdvanceNowByMs(10);
      metrics->SetDispatchStageTimestamp(
          EventMetrics::DispatchStage::kRendererCompositorFinished);
    }
    return metrics;
  }

  std::vector<base::TimeTicks> GetEventTimestamps(
      const EventMetrics::List& events_metrics) {
    std::vector<base::TimeTicks> event_times;
    event_times.reserve(events_metrics.size());
    std::transform(events_metrics.cbegin(), events_metrics.cend(),
                   std::back_inserter(event_times),
                   [](const auto& event_metrics) {
                     return event_metrics->GetDispatchStageTimestamp(
                         EventMetrics::DispatchStage::kGenerated);
                   });
    return event_times;
  }

 protected:
  // This should be defined before |reporting_controller_| so it is created
  // before and destroyed after that.
  base::SimpleTestTickClock test_tick_clock_;

  TestCompositorFrameReportingController reporting_controller_;
  viz::BeginFrameArgs args_;
  viz::BeginFrameId current_id_;
  viz::BeginFrameId last_activated_id_;
  base::TimeTicks begin_impl_time_;
  base::TimeTicks begin_main_time_;
  base::TimeTicks begin_main_start_time_;
  base::TimeTicks begin_commit_time_;
  base::TimeTicks end_commit_time_;
  base::TimeTicks begin_activation_time_;
  base::TimeTicks end_activation_time_;
  base::TimeTicks submit_time_;
  viz::FrameTokenGenerator next_token_;
  DroppedFrameCounter dropped_counter;
  TotalFrameCounter total_frame_counter_;
};

TEST_F(CompositorFrameReportingControllerTest, ActiveReporterCounts) {
  // Check that there are no leaks with the CompositorFrameReporter
  // objects no matter what the sequence of scheduled actions is
  // Note that due to DCHECKs in WillCommit(), WillActivate(), etc., it
  // is impossible to have 2 reporters both in BMF or Commit

  // Tests Cases:
  // - 2 Reporters at Activate phase
  // - 2 back-to-back BeginImplFrames
  // - 4 Simultaneous Reporters

  viz::BeginFrameId current_id_1(1, 1);
  viz::BeginFrameArgs args_1 = SimulateBeginFrameArgs(current_id_1);

  viz::BeginFrameId current_id_2(1, 2);
  viz::BeginFrameArgs args_2 = SimulateBeginFrameArgs(current_id_2);

  viz::BeginFrameId current_id_3(1, 3);
  viz::BeginFrameArgs args_3 = SimulateBeginFrameArgs(current_id_3);

  // BF
  reporting_controller_.WillBeginImplFrame(args_1);
  EXPECT_EQ(1, reporting_controller_.ActiveReporters());
  reporting_controller_.OnFinishImplFrame(args_1.frame_id);
  reporting_controller_.DidNotProduceFrame(args_1.frame_id,
                                           FrameSkippedReason::kNoDamage);

  // BF -> BF
  // Should replace previous reporter.
  reporting_controller_.WillBeginImplFrame(args_2);
  EXPECT_EQ(1, reporting_controller_.ActiveReporters());
  reporting_controller_.OnFinishImplFrame(args_2.frame_id);
  reporting_controller_.DidNotProduceFrame(args_2.frame_id,
                                           FrameSkippedReason::kNoDamage);

  // BF -> BMF -> BF
  // Should add new reporter.
  reporting_controller_.WillBeginMainFrame(args_2);
  reporting_controller_.WillBeginImplFrame(args_3);
  EXPECT_EQ(2, reporting_controller_.ActiveReporters());

  // BF -> BMF -> BF -> Commit
  // Should stay same.
  reporting_controller_.WillCommit();
  reporting_controller_.DidCommit();
  EXPECT_EQ(2, reporting_controller_.ActiveReporters());

  // BF -> BMF -> BF -> Commit -> BMF -> Activate -> Commit -> Activation
  // Having two reporters at Activate phase should delete the older one.
  reporting_controller_.WillBeginMainFrame(args_3);
  reporting_controller_.WillActivate();
  reporting_controller_.DidActivate();

  // There is a reporters tracking frame_3 in BeginMain state and one reporter
  // for frame_2 in activate state.
  EXPECT_EQ(2, reporting_controller_.ActiveReporters());

  reporting_controller_.WillCommit();
  reporting_controller_.DidCommit();
  reporting_controller_.WillActivate();
  reporting_controller_.DidActivate();
  // Reporter in activate state for frame_2 is overwritten by the reporter for
  // frame_3.
  EXPECT_EQ(1, reporting_controller_.ActiveReporters());

  last_activated_id_ = current_id_3;
  reporting_controller_.DidSubmitCompositorFrame(0, current_id_3,
                                                 last_activated_id_, {});
  EXPECT_EQ(0, reporting_controller_.ActiveReporters());

  // Start a frame and take it all the way to the activate stage.
  SimulateActivate();
  EXPECT_EQ(1, reporting_controller_.ActiveReporters());

  // Start another frame and let it progress up to the commit stage.
  SimulateCommit(nullptr);
  EXPECT_EQ(2, reporting_controller_.ActiveReporters());

  // Start the next frame, and let it progress up to the main-frame.
  SimulateBeginMainFrame();
  EXPECT_EQ(3, reporting_controller_.ActiveReporters());

  // Start the next frame.
  SimulateBeginImplFrame();
  EXPECT_EQ(4, reporting_controller_.ActiveReporters());

  reporting_controller_.OnFinishImplFrame(args_.frame_id);
  reporting_controller_.DidNotProduceFrame(args_.frame_id,
                                           FrameSkippedReason::kNoDamage);

  // Any additional BeginImplFrame's would be ignored.
  SimulateBeginImplFrame();
  EXPECT_EQ(4, reporting_controller_.ActiveReporters());
}

TEST_F(CompositorFrameReportingControllerTest,
       StopRequestingFramesCancelsInFlightFrames) {
  base::HistogramTester histogram_tester;

  // 2 reporters active.
  SimulateActivate();
  SimulateCommit(nullptr);

  reporting_controller_.OnStoppedRequestingBeginFrames();
  reporting_controller_.ResetReporters();
  histogram_tester.ExpectBucketCount(
      "CompositorLatency.Type",
      CompositorFrameReporter::FrameReportType::kDroppedFrame, 0);
}

TEST_F(CompositorFrameReportingControllerTest,
       SubmittedFrameHistogramReporting) {
  base::HistogramTester histogram_tester;

  // 2 reporters active.
  SimulateActivate();
  SimulateCommit(nullptr);

  // Submitting and Presenting the next reporter which will be a normal frame.
  SimulatePresentCompositorFrame();

  histogram_tester.ExpectTotalCount(
      "CompositorLatency.DroppedFrame.BeginImplFrameToSendBeginMainFrame", 0);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.DroppedFrame.SendBeginMainFrameToCommit", 0);
  histogram_tester.ExpectTotalCount("CompositorLatency.DroppedFrame.Commit", 0);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.DroppedFrame.EndCommitToActivation", 0);
  histogram_tester.ExpectTotalCount("CompositorLatency.DroppedFrame.Activation",
                                    0);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.DroppedFrame.EndActivateToSubmitCompositorFrame", 0);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.BeginImplFrameToSendBeginMainFrame", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SendBeginMainFrameToCommit", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.Commit", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.EndCommitToActivation",
                                    1);
  histogram_tester.ExpectTotalCount("CompositorLatency.Activation", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.EndActivateToSubmitCompositorFrame", 1);

  // Submitting the next reporter will be replaced as a result of a new commit.
  // And this will be reported for all stage before activate as a missed frame.
  SimulateCommit(nullptr);
  // Non Missed frame histogram counts should not change.
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.BeginImplFrameToSendBeginMainFrame", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SendBeginMainFrameToCommit", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.Commit", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.EndCommitToActivation",
                                    1);
  histogram_tester.ExpectTotalCount("CompositorLatency.Activation", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.EndActivateToSubmitCompositorFrame", 1);

  // Other histograms should be reported updated.
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.DroppedFrame.BeginImplFrameToSendBeginMainFrame", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.DroppedFrame.SendBeginMainFrameToCommit", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.DroppedFrame.Commit", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.DroppedFrame.EndCommitToActivation", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.DroppedFrame.Activation",
                                    0);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.DroppedFrame.EndActivateToSubmitCompositorFrame", 0);
}

TEST_F(CompositorFrameReportingControllerTest, MainFrameCausedNoDamage) {
  base::HistogramTester histogram_tester;
  viz::BeginFrameId current_id_1(1, 1);
  viz::BeginFrameArgs args_1 = SimulateBeginFrameArgs(current_id_1);

  viz::BeginFrameId current_id_2(1, 2);
  viz::BeginFrameArgs args_2 = SimulateBeginFrameArgs(current_id_2);

  viz::BeginFrameId current_id_3(1, 3);
  viz::BeginFrameArgs args_3 = SimulateBeginFrameArgs(current_id_3);

  reporting_controller_.WillBeginImplFrame(args_1);
  reporting_controller_.WillBeginMainFrame(args_1);
  reporting_controller_.BeginMainFrameAborted(current_id_1);
  reporting_controller_.OnFinishImplFrame(current_id_1);
  reporting_controller_.DidNotProduceFrame(current_id_1,
                                           FrameSkippedReason::kNoDamage);

  reporting_controller_.WillBeginImplFrame(args_2);
  reporting_controller_.WillBeginMainFrame(args_2);
  reporting_controller_.OnFinishImplFrame(current_id_2);
  reporting_controller_.BeginMainFrameAborted(current_id_2);
  reporting_controller_.DidNotProduceFrame(current_id_2,
                                           FrameSkippedReason::kNoDamage);

  reporting_controller_.WillBeginImplFrame(args_3);
  reporting_controller_.WillBeginMainFrame(args_3);

  histogram_tester.ExpectTotalCount(
      "CompositorLatency.DroppedFrame.BeginImplFrameToSendBeginMainFrame", 0);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.DroppedFrame.SendBeginMainFrameToCommit", 0);
}

TEST_F(CompositorFrameReportingControllerTest, DidNotProduceFrame) {
  base::HistogramTester histogram_tester;

  viz::BeginFrameId current_id_1(1, 1);
  viz::BeginFrameArgs args_1 = SimulateBeginFrameArgs(current_id_1);

  viz::BeginFrameId current_id_2(1, 2);
  viz::BeginFrameArgs args_2 = SimulateBeginFrameArgs(current_id_2);

  reporting_controller_.WillBeginImplFrame(args_1);
  reporting_controller_.WillBeginMainFrame(args_1);
  reporting_controller_.OnFinishImplFrame(current_id_1);
  reporting_controller_.DidNotProduceFrame(current_id_1,
                                           FrameSkippedReason::kNoDamage);

  reporting_controller_.WillBeginImplFrame(args_2);
  reporting_controller_.OnFinishImplFrame(current_id_2);
  reporting_controller_.WillCommit();
  reporting_controller_.DidCommit();
  reporting_controller_.WillActivate();
  reporting_controller_.DidActivate();
  reporting_controller_.DidSubmitCompositorFrame(1, current_id_2, current_id_1,
                                                 {});
  viz::FrameTimingDetails details = {};
  reporting_controller_.DidPresentCompositorFrame(1, details);

  histogram_tester.ExpectTotalCount(
      "CompositorLatency.DroppedFrame.BeginImplFrameToSendBeginMainFrame", 0);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.DroppedFrame.SendBeginMainFrameToCommit", 0);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.BeginImplFrameToSendBeginMainFrame", 2);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SendBeginMainFrameToCommit", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.Commit", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.EndCommitToActivation",
                                    1);
  histogram_tester.ExpectTotalCount("CompositorLatency.Activation", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.EndActivateToSubmitCompositorFrame", 2);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SubmitCompositorFrameToPresentationCompositorFrame",
      2);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.CompositorOnlyFrame.BeginImplFrameToFinishImpl", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.CompositorOnlyFrame."
      "ImplFrameDoneToSubmitCompositorFrame",
      1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.CompositorOnlyFrame."
      "SubmitCompositorFrameToPresentationCompositorFrame",
      1);
}

TEST_F(CompositorFrameReportingControllerTest,
       DidNotProduceFrameDueToWaitingOnMain) {
  base::HistogramTester histogram_tester;

  viz::BeginFrameId current_id_1(1, 1);
  viz::BeginFrameArgs args_1 = SimulateBeginFrameArgs(current_id_1);

  viz::BeginFrameId current_id_2(1, 2);
  viz::BeginFrameArgs args_2 = SimulateBeginFrameArgs(current_id_2);
  args_2.frame_time = args_1.frame_time + args_1.interval;

  viz::BeginFrameId current_id_3(1, 3);
  viz::BeginFrameArgs args_3 = SimulateBeginFrameArgs(current_id_3);
  args_3.frame_time = args_2.frame_time + args_2.interval;

  reporting_controller_.WillBeginImplFrame(args_1);
  reporting_controller_.WillBeginMainFrame(args_1);
  reporting_controller_.OnFinishImplFrame(current_id_1);
  reporting_controller_.DidNotProduceFrame(current_id_1,
                                           FrameSkippedReason::kWaitingOnMain);

  reporting_controller_.WillBeginImplFrame(args_2);
  reporting_controller_.OnFinishImplFrame(current_id_2);
  reporting_controller_.DidNotProduceFrame(current_id_2,
                                           FrameSkippedReason::kWaitingOnMain);

  reporting_controller_.WillBeginImplFrame(args_3);
  reporting_controller_.WillCommit();
  reporting_controller_.DidCommit();
  reporting_controller_.WillActivate();
  reporting_controller_.DidActivate();
  reporting_controller_.OnFinishImplFrame(current_id_3);
  reporting_controller_.DidSubmitCompositorFrame(1, current_id_3, current_id_1,
                                                 {});
  viz::FrameTimingDetails details;
  details.presentation_feedback = {args_3.frame_time + args_3.interval,
                                   args_3.interval, 0};
  reporting_controller_.DidPresentCompositorFrame(1, details);

  // Frames for |args_1| and |args_2| were dropped waiting on the main-thread.
  histogram_tester.ExpectBucketCount(
      "CompositorLatency.Type",
      CompositorFrameReporter::FrameReportType::kDroppedFrame, 2);

  // Frames for |args_1| and |args_3| were presented with |args_3|, and |args_1|
  // missed its deadline.
  histogram_tester.ExpectBucketCount(
      "CompositorLatency.Type",
      CompositorFrameReporter::FrameReportType::kNonDroppedFrame, 2);
  histogram_tester.ExpectBucketCount(
      "CompositorLatency.Type",
      CompositorFrameReporter::FrameReportType::kMissedDeadlineFrame, 1);
  histogram_tester.ExpectBucketCount(
      "CompositorLatency.Type",
      CompositorFrameReporter::FrameReportType::kCompositorOnlyFrame, 1);
}

TEST_F(CompositorFrameReportingControllerTest, MainFrameAborted) {
  base::HistogramTester histogram_tester;

  reporting_controller_.WillBeginImplFrame(args_);
  reporting_controller_.WillBeginMainFrame(args_);
  reporting_controller_.BeginMainFrameAborted(current_id_);
  reporting_controller_.OnFinishImplFrame(current_id_);
  reporting_controller_.DidSubmitCompositorFrame(1, current_id_,
                                                 last_activated_id_, {});

  viz::FrameTimingDetails details = {};
  reporting_controller_.DidPresentCompositorFrame(1, details);

  histogram_tester.ExpectTotalCount(
      "CompositorLatency.BeginImplFrameToSendBeginMainFrame", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SendBeginMainFrameToCommit", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.Commit", 0);
  histogram_tester.ExpectTotalCount("CompositorLatency.Activation", 0);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.EndActivateToSubmitCompositorFrame", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SubmitCompositorFrameToPresentationCompositorFrame",
      1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.CompositorOnlyFrame.BeginImplFrameToFinishImpl", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.CompositorOnlyFrame."
      "SendBeginMainFrameToBeginMainAbort",
      1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.CompositorOnlyFrame."
      "ImplFrameDoneToSubmitCompositorFrame",
      1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.CompositorOnlyFrame."
      "SubmitCompositorFrameToPresentationCompositorFrame",
      1);
}

TEST_F(CompositorFrameReportingControllerTest, MainFrameAborted2) {
  base::HistogramTester histogram_tester;
  viz::BeginFrameId current_id_1(1, 1);
  viz::BeginFrameArgs args_1 = SimulateBeginFrameArgs(current_id_1);

  viz::BeginFrameId current_id_2(1, 2);
  viz::BeginFrameArgs args_2 = SimulateBeginFrameArgs(current_id_2);

  viz::BeginFrameId current_id_3(1, 3);
  viz::BeginFrameArgs args_3 = SimulateBeginFrameArgs(current_id_3);

  reporting_controller_.WillBeginImplFrame(args_1);
  reporting_controller_.OnFinishImplFrame(current_id_1);
  reporting_controller_.WillBeginMainFrame(args_1);
  reporting_controller_.WillCommit();
  reporting_controller_.DidCommit();
  reporting_controller_.WillActivate();
  reporting_controller_.DidActivate();
  reporting_controller_.WillBeginImplFrame(args_2);
  reporting_controller_.WillBeginMainFrame(args_2);
  reporting_controller_.OnFinishImplFrame(current_id_2);
  reporting_controller_.BeginMainFrameAborted(current_id_2);
  reporting_controller_.DidSubmitCompositorFrame(1, current_id_2, current_id_1,
                                                 {});
  viz::FrameTimingDetails details = {};
  reporting_controller_.DidPresentCompositorFrame(1, details);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.DroppedFrame.BeginImplFrameToSendBeginMainFrame", 0);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.BeginImplFrameToSendBeginMainFrame", 2);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SendBeginMainFrameToCommit", 2);
  histogram_tester.ExpectTotalCount("CompositorLatency.Commit", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.EndCommitToActivation",
                                    1);
  histogram_tester.ExpectTotalCount("CompositorLatency.Activation", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.EndActivateToSubmitCompositorFrame", 2);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SubmitCompositorFrameToPresentationCompositorFrame",
      2);
  reporting_controller_.DidSubmitCompositorFrame(2, current_id_2, current_id_1,
                                                 {});
  reporting_controller_.DidPresentCompositorFrame(2, details);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.DroppedFrame.BeginImplFrameToSendBeginMainFrame", 0);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.BeginImplFrameToSendBeginMainFrame", 2);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SendBeginMainFrameToCommit", 2);
  histogram_tester.ExpectTotalCount("CompositorLatency.Commit", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.EndCommitToActivation",
                                    1);
  histogram_tester.ExpectTotalCount("CompositorLatency.Activation", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.EndActivateToSubmitCompositorFrame", 2);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SubmitCompositorFrameToPresentationCompositorFrame",
      2);
  reporting_controller_.WillBeginImplFrame(args_3);
  reporting_controller_.OnFinishImplFrame(current_id_3);
  reporting_controller_.DidSubmitCompositorFrame(3, current_id_3, current_id_1,
                                                 {});
  reporting_controller_.DidPresentCompositorFrame(3, details);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.DroppedFrame.BeginImplFrameToSendBeginMainFrame", 0);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.BeginImplFrameToSendBeginMainFrame", 3);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SendBeginMainFrameToCommit", 2);
  histogram_tester.ExpectTotalCount("CompositorLatency.Commit", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.EndCommitToActivation",
                                    1);
  histogram_tester.ExpectTotalCount("CompositorLatency.Activation", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.EndActivateToSubmitCompositorFrame", 3);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SubmitCompositorFrameToPresentationCompositorFrame",
      3);
}

TEST_F(CompositorFrameReportingControllerTest, LongMainFrame) {
  base::HistogramTester histogram_tester;
  viz::BeginFrameId current_id_1(1, 1);
  viz::BeginFrameArgs args_1 = SimulateBeginFrameArgs(current_id_1);

  viz::BeginFrameId current_id_2(1, 2);
  viz::BeginFrameArgs args_2 = SimulateBeginFrameArgs(current_id_2);

  viz::BeginFrameId current_id_3(1, 3);
  viz::BeginFrameArgs args_3 = SimulateBeginFrameArgs(current_id_3);

  viz::FrameTimingDetails details = {};
  reporting_controller_.WillBeginImplFrame(args_1);
  reporting_controller_.OnFinishImplFrame(current_id_1);
  reporting_controller_.WillBeginMainFrame(args_1);
  reporting_controller_.WillCommit();
  reporting_controller_.DidCommit();
  reporting_controller_.WillActivate();
  reporting_controller_.DidActivate();
  reporting_controller_.DidSubmitCompositorFrame(1, current_id_1, current_id_1,
                                                 {});
  reporting_controller_.DidPresentCompositorFrame(1, details);

  histogram_tester.ExpectTotalCount(
      "CompositorLatency.BeginImplFrameToSendBeginMainFrame", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SendBeginMainFrameToCommit", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.Commit", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.EndCommitToActivation",
                                    1);
  histogram_tester.ExpectTotalCount("CompositorLatency.Activation", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.EndActivateToSubmitCompositorFrame", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SubmitCompositorFrameToPresentationCompositorFrame",
      1);

  // Second frame will not have the main frame update ready and will only submit
  // the Impl update
  reporting_controller_.WillBeginImplFrame(args_2);
  reporting_controller_.WillBeginMainFrame(args_2);
  reporting_controller_.OnFinishImplFrame(current_id_2);
  reporting_controller_.DidSubmitCompositorFrame(2, current_id_2, current_id_1,
                                                 {});
  reporting_controller_.DidPresentCompositorFrame(2, details);

  // The reporting for the second frame is delayed until the main-thread
  // responds back.
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.BeginImplFrameToSendBeginMainFrame", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SendBeginMainFrameToCommit", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.Commit", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.EndCommitToActivation",
                                    1);
  histogram_tester.ExpectTotalCount("CompositorLatency.Activation", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.EndActivateToSubmitCompositorFrame", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SubmitCompositorFrameToPresentationCompositorFrame",
      1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.CompositorOnlyFrame.BeginImplFrameToFinishImpl", 0);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.CompositorOnlyFrame."
      "SendBeginMainFrameToBeginMainAbort",
      0);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.CompositorOnlyFrame."
      "ImplFrameDoneToSubmitCompositorFrame",
      0);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.CompositorOnlyFrame."
      "SubmitCompositorFrameToPresentationCompositorFrame",
      0);

  reporting_controller_.WillBeginImplFrame(args_3);
  reporting_controller_.OnFinishImplFrame(current_id_3);
  reporting_controller_.WillCommit();
  reporting_controller_.DidCommit();
  reporting_controller_.WillActivate();
  reporting_controller_.DidActivate();
  reporting_controller_.DidSubmitCompositorFrame(3, current_id_3, current_id_2,
                                                 {});
  reporting_controller_.DidPresentCompositorFrame(3, details);

  // The main-thread responded, so the metrics for |args_2| should now be
  // reported.
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.BeginImplFrameToSendBeginMainFrame", 4);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SendBeginMainFrameToCommit", 2);
  histogram_tester.ExpectTotalCount("CompositorLatency.Commit", 2);
  histogram_tester.ExpectTotalCount("CompositorLatency.EndCommitToActivation",
                                    2);
  histogram_tester.ExpectTotalCount("CompositorLatency.Activation", 2);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.EndActivateToSubmitCompositorFrame", 4);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SubmitCompositorFrameToPresentationCompositorFrame",
      4);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.CompositorOnlyFrame.BeginImplFrameToFinishImpl", 2);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.CompositorOnlyFrame."
      "SendBeginMainFrameToBeginMainAbort",
      0);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.CompositorOnlyFrame."
      "ImplFrameDoneToSubmitCompositorFrame",
      2);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.CompositorOnlyFrame."
      "SubmitCompositorFrameToPresentationCompositorFrame",
      2);
}

TEST_F(CompositorFrameReportingControllerTest, LongMainFrame2) {
  base::HistogramTester histogram_tester;
  viz::BeginFrameId current_id_1(1, 1);
  viz::BeginFrameArgs args_1 = SimulateBeginFrameArgs(current_id_1);

  viz::BeginFrameId current_id_2(1, 2);
  viz::BeginFrameArgs args_2 = SimulateBeginFrameArgs(current_id_2);

  viz::FrameTimingDetails details = {};
  reporting_controller_.WillBeginImplFrame(args_1);
  reporting_controller_.OnFinishImplFrame(current_id_1);
  reporting_controller_.WillBeginMainFrame(args_1);
  reporting_controller_.WillCommit();
  reporting_controller_.DidCommit();
  reporting_controller_.WillActivate();
  reporting_controller_.DidActivate();
  reporting_controller_.DidSubmitCompositorFrame(1, current_id_1, current_id_1,
                                                 {});
  reporting_controller_.DidPresentCompositorFrame(1, details);

  histogram_tester.ExpectTotalCount(
      "CompositorLatency.BeginImplFrameToSendBeginMainFrame", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SendBeginMainFrameToCommit", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.Commit", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.EndCommitToActivation",
                                    1);
  histogram_tester.ExpectTotalCount("CompositorLatency.Activation", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.EndActivateToSubmitCompositorFrame", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SubmitCompositorFrameToPresentationCompositorFrame",
      1);

  // The reporting for the second frame is delayed until activation happens.
  reporting_controller_.WillBeginImplFrame(args_2);
  reporting_controller_.WillBeginMainFrame(args_2);
  reporting_controller_.WillCommit();
  reporting_controller_.DidCommit();
  reporting_controller_.OnFinishImplFrame(current_id_2);
  reporting_controller_.DidSubmitCompositorFrame(2, current_id_2, current_id_1,
                                                 {});
  reporting_controller_.DidPresentCompositorFrame(2, details);

  histogram_tester.ExpectTotalCount(
      "CompositorLatency.BeginImplFrameToSendBeginMainFrame", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SendBeginMainFrameToCommit", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.Commit", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.EndCommitToActivation",
                                    1);
  histogram_tester.ExpectTotalCount("CompositorLatency.Activation", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.EndActivateToSubmitCompositorFrame", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SubmitCompositorFrameToPresentationCompositorFrame",
      1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.CompositorOnlyFrame.BeginImplFrameToFinishImpl", 0);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.CompositorOnlyFrame."
      "SendBeginMainFrameToBeginMainAbort",
      0);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.CompositorOnlyFrame."
      "ImplFrameDoneToSubmitCompositorFrame",
      0);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.CompositorOnlyFrame."
      "SubmitCompositorFrameToPresentationCompositorFrame",
      0);

  viz::BeginFrameId current_id_3(1, 3);
  viz::BeginFrameArgs args_3 = SimulateBeginFrameArgs(current_id_3);

  // The metrics are reported for |args_2| after activation finally happens and
  // a new frame is submitted.
  reporting_controller_.WillActivate();
  reporting_controller_.DidActivate();
  reporting_controller_.WillBeginImplFrame(args_3);
  reporting_controller_.OnFinishImplFrame(current_id_3);
  reporting_controller_.DidSubmitCompositorFrame(3, current_id_3, current_id_2,
                                                 {});
  reporting_controller_.DidPresentCompositorFrame(3, details);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.BeginImplFrameToSendBeginMainFrame", 4);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SendBeginMainFrameToCommit", 2);
  histogram_tester.ExpectTotalCount("CompositorLatency.Commit", 2);
  histogram_tester.ExpectTotalCount("CompositorLatency.EndCommitToActivation",
                                    2);
  histogram_tester.ExpectTotalCount("CompositorLatency.Activation", 2);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.EndActivateToSubmitCompositorFrame", 4);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SubmitCompositorFrameToPresentationCompositorFrame",
      4);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.CompositorOnlyFrame.BeginImplFrameToFinishImpl", 2);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.CompositorOnlyFrame."
      "SendBeginMainFrameToBeginMainAbort",
      0);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.CompositorOnlyFrame."
      "ImplFrameDoneToSubmitCompositorFrame",
      2);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.CompositorOnlyFrame."
      "SubmitCompositorFrameToPresentationCompositorFrame",
      2);
}

TEST_F(CompositorFrameReportingControllerTest, BlinkBreakdown) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<BeginMainFrameMetrics> blink_breakdown =
      BuildBlinkBreakdown();
  SimulateActivate();
  SimulateCommit(std::move(blink_breakdown));
  SimulatePresentCompositorFrame();

  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SendBeginMainFrameToCommit", 1);
  histogram_tester.ExpectUniqueSample(
      "CompositorLatency.SendBeginMainFrameToCommit.HandleInputEvents",
      base::TimeDelta::FromMicroseconds(10).InMilliseconds(), 1);
  histogram_tester.ExpectUniqueSample(
      "CompositorLatency.SendBeginMainFrameToCommit.Animate",
      base::TimeDelta::FromMicroseconds(9).InMilliseconds(), 1);
  histogram_tester.ExpectUniqueSample(
      "CompositorLatency.SendBeginMainFrameToCommit.StyleUpdate",
      base::TimeDelta::FromMicroseconds(8).InMilliseconds(), 1);
  histogram_tester.ExpectUniqueSample(
      "CompositorLatency.SendBeginMainFrameToCommit.LayoutUpdate",
      base::TimeDelta::FromMicroseconds(7).InMilliseconds(), 1);
  histogram_tester.ExpectUniqueSample(
      "CompositorLatency.SendBeginMainFrameToCommit.CompositingInputs",
      base::TimeDelta::FromMicroseconds(6).InMilliseconds(), 1);
  histogram_tester.ExpectUniqueSample(
      "CompositorLatency.SendBeginMainFrameToCommit.Prepaint",
      base::TimeDelta::FromMicroseconds(5).InMilliseconds(), 1);
  histogram_tester.ExpectUniqueSample(
      "CompositorLatency.SendBeginMainFrameToCommit.CompositingAssignments",
      base::TimeDelta::FromMicroseconds(4).InMilliseconds(), 1);
  histogram_tester.ExpectUniqueSample(
      "CompositorLatency.SendBeginMainFrameToCommit.Paint",
      base::TimeDelta::FromMicroseconds(3).InMilliseconds(), 1);
  histogram_tester.ExpectUniqueSample(
      "CompositorLatency.SendBeginMainFrameToCommit.CompositeCommit",
      base::TimeDelta::FromMicroseconds(2).InMilliseconds(), 1);
  histogram_tester.ExpectUniqueSample(
      "CompositorLatency.SendBeginMainFrameToCommit.UpdateLayers",
      base::TimeDelta::FromMicroseconds(1).InMilliseconds(), 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SendBeginMainFrameToCommit.BeginMainSentToStarted", 1);
}

// If the presentation of the frame happens before deadline.
TEST_F(CompositorFrameReportingControllerTest, ReportingMissedDeadlineFrame1) {
  base::HistogramTester histogram_tester;

  reporting_controller_.WillBeginImplFrame(args_);
  reporting_controller_.OnFinishImplFrame(current_id_);
  reporting_controller_.WillBeginMainFrame(args_);
  reporting_controller_.WillCommit();
  reporting_controller_.DidCommit();
  reporting_controller_.WillActivate();
  reporting_controller_.DidActivate();
  reporting_controller_.DidSubmitCompositorFrame(1, current_id_, current_id_,
                                                 {});
  viz::FrameTimingDetails details = {};
  details.presentation_feedback.timestamp =
      args_.frame_time + args_.interval * 1.5 -
      base::TimeDelta::FromMicroseconds(100);
  reporting_controller_.DidPresentCompositorFrame(1, details);

  histogram_tester.ExpectTotalCount(
      "CompositorLatency.BeginImplFrameToSendBeginMainFrame", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.TotalLatency", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.MissedDeadlineFrame."
      "BeginImplFrameToSendBeginMainFrame",
      0);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.MissedDeadlineFrame.TotalLatency", 0);

  // Non-dropped cases.
  histogram_tester.ExpectBucketCount("CompositorLatency.Type", 0, 1);
  // Missed-deadline cases.
  histogram_tester.ExpectBucketCount("CompositorLatency.Type", 1, 0);
  // Dropped cases.
  histogram_tester.ExpectBucketCount("CompositorLatency.Type", 2, 0);
  // Impl only cases.
  histogram_tester.ExpectBucketCount("CompositorLatency.Type", 3, 0);
}

// If the presentation of the frame happens after deadline.
TEST_F(CompositorFrameReportingControllerTest, ReportingMissedDeadlineFrame2) {
  base::HistogramTester histogram_tester;

  reporting_controller_.WillBeginImplFrame(args_);
  reporting_controller_.OnFinishImplFrame(current_id_);
  reporting_controller_.WillBeginMainFrame(args_);
  reporting_controller_.WillCommit();
  reporting_controller_.DidCommit();
  reporting_controller_.WillActivate();
  reporting_controller_.DidActivate();
  reporting_controller_.DidSubmitCompositorFrame(1, current_id_, current_id_,
                                                 {});
  viz::FrameTimingDetails details = {};
  details.presentation_feedback.timestamp =
      args_.frame_time + args_.interval * 1.5 +
      base::TimeDelta::FromMicroseconds(100);
  reporting_controller_.DidPresentCompositorFrame(1, details);

  histogram_tester.ExpectTotalCount(
      "CompositorLatency.BeginImplFrameToSendBeginMainFrame", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.TotalLatency", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.MissedDeadlineFrame."
      "BeginImplFrameToSendBeginMainFrame",
      1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.MissedDeadlineFrame.TotalLatency", 1);

  // Non-dropped cases.
  histogram_tester.ExpectBucketCount("CompositorLatency.Type", 0, 1);
  // Missed-deadline cases.
  histogram_tester.ExpectBucketCount("CompositorLatency.Type", 1, 1);
  // Dropped cases.
  histogram_tester.ExpectBucketCount("CompositorLatency.Type", 2, 0);
}

// Testing CompositorLatency.Type metrics
TEST_F(CompositorFrameReportingControllerTest, ReportingLatencyType) {
  base::HistogramTester histogram_tester;

  SimulatePresentCompositorFrame();
  reporting_controller_.AddActiveTracker(
      FrameSequenceTrackerType::kCompositorAnimation);
  SimulatePresentCompositorFrame();
  reporting_controller_.AddActiveTracker(
      FrameSequenceTrackerType::kWheelScroll);
  SimulatePresentCompositorFrame();
  reporting_controller_.RemoveActiveTracker(
      FrameSequenceTrackerType::kCompositorAnimation);
  SimulatePresentCompositorFrame();
  reporting_controller_.RemoveActiveTracker(
      FrameSequenceTrackerType::kWheelScroll);
  SimulatePresentCompositorFrame();

  // All frames are presented so only test on-dropped cases.
  histogram_tester.ExpectBucketCount("CompositorLatency.Type", 0, 5);
  histogram_tester.ExpectBucketCount(
      "CompositorLatency.Type.CompositorAnimation", 0, 2);
  histogram_tester.ExpectBucketCount("CompositorLatency.Type.WheelScroll", 0,
                                     2);
  histogram_tester.ExpectBucketCount("CompositorLatency.Type.AnyInteraction", 0,
                                     3);
  histogram_tester.ExpectBucketCount("CompositorLatency.Type.NoInteraction", 0,
                                     2);
}

// Tests that EventLatency total latency histograms are reported properly when a
// frame is presented to the user.
TEST_F(CompositorFrameReportingControllerTest,
       EventLatencyTotalForPresentedFrameReported) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<EventMetrics> event_metrics_ptrs[] = {
      CreateEventMetrics(ui::ET_TOUCH_PRESSED, base::nullopt, base::nullopt),
      CreateEventMetrics(ui::ET_TOUCH_MOVED, base::nullopt, base::nullopt),
      CreateEventMetrics(ui::ET_TOUCH_MOVED, base::nullopt, base::nullopt),
  };
  EXPECT_THAT(event_metrics_ptrs, Each(NotNull()));
  EventMetrics::List events_metrics(
      std::make_move_iterator(std::begin(event_metrics_ptrs)),
      std::make_move_iterator(std::end(event_metrics_ptrs)));
  std::vector<base::TimeTicks> event_times = GetEventTimestamps(events_metrics);

  // Submit a compositor frame and notify CompositorFrameReporter of the events
  // affecting the frame.
  ++next_token_;
  SimulateSubmitCompositorFrame(*next_token_, {std::move(events_metrics), {}});

  // Present the submitted compositor frame to the user.
  const base::TimeTicks presentation_time = AdvanceNowByMs(10);
  viz::FrameTimingDetails details;
  details.presentation_feedback.timestamp = presentation_time;
  reporting_controller_.DidPresentCompositorFrame(*next_token_, details);

  // Verify that EventLatency histograms are recorded.
  struct {
    const char* name;
    const base::HistogramBase::Count count;
  } expected_counts[] = {
      {"EventLatency.TouchPressed.TotalLatency", 1},
      {"EventLatency.TouchMoved.TotalLatency", 2},
      {"EventLatency.TotalLatency", 3},
  };
  for (const auto& expected_count : expected_counts) {
    histogram_tester.ExpectTotalCount(expected_count.name,
                                      expected_count.count);
  }

  struct {
    const char* name;
    const base::HistogramBase::Sample latency_ms;
  } expected_latencies[] = {
      {"EventLatency.TouchPressed.TotalLatency",
       (presentation_time - event_times[0]).InMicroseconds()},
      {"EventLatency.TouchMoved.TotalLatency",
       (presentation_time - event_times[1]).InMicroseconds()},
      {"EventLatency.TouchMoved.TotalLatency",
       (presentation_time - event_times[2]).InMicroseconds()},
      {"EventLatency.TotalLatency",
       (presentation_time - event_times[0]).InMicroseconds()},
      {"EventLatency.TotalLatency",
       (presentation_time - event_times[1]).InMicroseconds()},
      {"EventLatency.TotalLatency",
       (presentation_time - event_times[2]).InMicroseconds()},
  };
  for (const auto& expected_latency : expected_latencies) {
    histogram_tester.ExpectBucketCount(expected_latency.name,
                                       expected_latency.latency_ms, 1);
  }
}

// Tests that EventLatency total latency histograms are reported properly for
// scroll events when a frame is presented to the user.
TEST_F(CompositorFrameReportingControllerTest,
       EventLatencyScrollTotalForPresentedFrameReported) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<EventMetrics> event_metrics_ptrs[] = {
      CreateEventMetrics(ui::ET_GESTURE_SCROLL_BEGIN, base::nullopt,
                         ui::ScrollInputType::kWheel),
      CreateEventMetrics(ui::ET_GESTURE_SCROLL_UPDATE,
                         EventMetrics::ScrollUpdateType::kStarted,
                         ui::ScrollInputType::kWheel),
      CreateEventMetrics(ui::ET_GESTURE_SCROLL_UPDATE,
                         EventMetrics::ScrollUpdateType::kContinued,
                         ui::ScrollInputType::kWheel),
  };
  EXPECT_THAT(event_metrics_ptrs, Each(NotNull()));
  EventMetrics::List events_metrics(
      std::make_move_iterator(std::begin(event_metrics_ptrs)),
      std::make_move_iterator(std::end(event_metrics_ptrs)));
  std::vector<base::TimeTicks> event_times = GetEventTimestamps(events_metrics);

  // Submit a compositor frame and notify CompositorFrameReporter of the events
  // affecting the frame.
  ++next_token_;
  SimulateSubmitCompositorFrame(*next_token_, {std::move(events_metrics), {}});

  // Present the submitted compositor frame to the user.
  viz::FrameTimingDetails details;
  details.received_compositor_frame_timestamp = AdvanceNowByMs(10);
  details.draw_start_timestamp = AdvanceNowByMs(10);
  details.swap_timings.swap_start = AdvanceNowByMs(10);
  details.swap_timings.swap_end = AdvanceNowByMs(10);
  details.presentation_feedback.timestamp = AdvanceNowByMs(10);
  reporting_controller_.DidPresentCompositorFrame(*next_token_, details);

  // Verify that EventLatency histograms are recorded.
  struct {
    const char* name;
    const base::HistogramBase::Count count;
  } expected_counts[] = {
      {"EventLatency.GestureScrollBegin.Wheel.TotalLatency", 1},
      {"EventLatency.GestureScrollBegin.Wheel.TotalLatencyToSwapBegin", 1},
      {"EventLatency.FirstGestureScrollUpdate.Wheel.TotalLatency", 1},
      {"EventLatency.FirstGestureScrollUpdate.Wheel.TotalLatencyToSwapBegin",
       1},
      {"EventLatency.GestureScrollUpdate.Wheel.TotalLatency", 1},
      {"EventLatency.GestureScrollUpdate.Wheel.TotalLatencyToSwapBegin", 1},
      {"EventLatency.TotalLatency", 3},
  };
  for (const auto& expected_count : expected_counts) {
    histogram_tester.ExpectTotalCount(expected_count.name,
                                      expected_count.count);
  }

  const base::TimeTicks presentation_time =
      details.presentation_feedback.timestamp;
  const base::TimeTicks swap_begin_time = details.swap_timings.swap_start;
  struct {
    const char* name;
    const base::HistogramBase::Sample latency_ms;
  } expected_latencies[] = {
      {"EventLatency.GestureScrollBegin.Wheel.TotalLatency",
       (presentation_time - event_times[0]).InMicroseconds()},
      {"EventLatency.GestureScrollBegin.Wheel.TotalLatencyToSwapBegin",
       (swap_begin_time - event_times[0]).InMicroseconds()},
      {"EventLatency.FirstGestureScrollUpdate.Wheel.TotalLatency",
       (presentation_time - event_times[1]).InMicroseconds()},
      {"EventLatency.FirstGestureScrollUpdate.Wheel.TotalLatencyToSwapBegin",
       (swap_begin_time - event_times[1]).InMicroseconds()},
      {"EventLatency.GestureScrollUpdate.Wheel.TotalLatency",
       (presentation_time - event_times[2]).InMicroseconds()},
      {"EventLatency.GestureScrollUpdate.Wheel.TotalLatencyToSwapBegin",
       (swap_begin_time - event_times[2]).InMicroseconds()},
  };
  for (const auto& expected_latency : expected_latencies) {
    histogram_tester.ExpectBucketCount(expected_latency.name,
                                       expected_latency.latency_ms, 1);
  }
}

// Tests that EventLatency histograms are not reported when the frame is dropped
// and not presented to the user.
TEST_F(CompositorFrameReportingControllerTest,
       EventLatencyForDidNotPresentFrameNotReported) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<EventMetrics> event_metrics_ptrs[] = {
      CreateEventMetrics(ui::ET_TOUCH_PRESSED, base::nullopt, base::nullopt),
      CreateEventMetrics(ui::ET_TOUCH_MOVED, base::nullopt, base::nullopt),
      CreateEventMetrics(ui::ET_TOUCH_MOVED, base::nullopt, base::nullopt),
  };
  EXPECT_THAT(event_metrics_ptrs, Each(NotNull()));
  EventMetrics::List events_metrics(
      std::make_move_iterator(std::begin(event_metrics_ptrs)),
      std::make_move_iterator(std::end(event_metrics_ptrs)));

  // Submit a compositor frame and notify CompositorFrameReporter of the events
  // affecting the frame.
  ++next_token_;
  SimulateSubmitCompositorFrame(*next_token_, {std::move(events_metrics), {}});

  // Submit another compositor frame.
  ++next_token_;
  IncrementCurrentId();
  SimulateSubmitCompositorFrame(*next_token_, {});

  // Present the second compositor frame to the uesr, dropping the first one.
  viz::FrameTimingDetails details;
  details.presentation_feedback.timestamp = AdvanceNowByMs(10);
  reporting_controller_.DidPresentCompositorFrame(*next_token_, details);

  // Verify that no EventLatency histogram is recorded.
  EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix("EventLatency."),
              IsEmpty());
}

TEST_F(CompositorFrameReportingControllerTest,
       NewMainUpdateIsNotPartialUpdate) {
  // Start a frame with main-thread update. Submit the frame (and present)
  // before the main-thread responds. This creates two reporters: R1C and R1M
  // (R1C for the submitted frame with updates from compositor-thread, and R1M
  // for the pending main-thread frame).
  SimulateBeginMainFrame();
  reporting_controller_.OnFinishImplFrame(current_id_);
  reporting_controller_.DidSubmitCompositorFrame(1u, current_id_, {}, {});
  viz::FrameTimingDetails details = {};
  details.presentation_feedback.timestamp = AdvanceNowByMs(10);
  reporting_controller_.DidPresentCompositorFrame(1u, details);

  // The main-thread responds now, triggering a commit and activation.
  reporting_controller_.WillCommit();
  reporting_controller_.DidCommit();
  reporting_controller_.WillActivate();
  reporting_controller_.DidActivate();

  const auto previous_id = current_id_;

  // Start a new frame with main-thread update. Submit the frame (and present)
  // before the main-thread responds. This also again creates two reporters: R2C
  // and R2M.
  SimulateBeginMainFrame();
  reporting_controller_.OnFinishImplFrame(current_id_);
  reporting_controller_.DidSubmitCompositorFrame(1u, current_id_, previous_id,
                                                 {});
  details.presentation_feedback.timestamp = AdvanceNowByMs(10);
  reporting_controller_.DidPresentCompositorFrame(1u, details);

  // In total, two frames have been completed: R1C, and R1M.
  // R2C has been presented, but it is blocked on R2M to know whether R2C
  // contains partial update, or complete updates. So it is kept alive.
  EXPECT_EQ(2u, dropped_counter.total_frames());
  EXPECT_EQ(1u, dropped_counter.total_main_dropped());
  EXPECT_EQ(1u, reporting_controller_.GetBlockingReportersCount());
  EXPECT_EQ(1u, reporting_controller_.GetBlockedReportersCount());

  reporting_controller_.ResetReporters();
  reporting_controller_.SetDroppedFrameCounter(nullptr);
}

TEST_F(CompositorFrameReportingControllerTest,
       SkippedFramesFromDisplayCompositorAreDropped) {
  // Submit and present two compositor frames.
  SimulatePresentCompositorFrame();
  EXPECT_EQ(1u, dropped_counter.total_frames());
  EXPECT_EQ(0u, dropped_counter.total_main_dropped());
  EXPECT_EQ(0u, dropped_counter.total_compositor_dropped());

  SimulatePresentCompositorFrame();
  EXPECT_EQ(2u, dropped_counter.total_frames());
  EXPECT_EQ(0u, dropped_counter.total_main_dropped());
  EXPECT_EQ(0u, dropped_counter.total_compositor_dropped());

  // Now skip over a few frames, and submit + present another frame.
  const uint32_t kSkipFrames = 5;
  for (uint32_t i = 0; i < kSkipFrames; ++i)
    IncrementCurrentId();
  SimulatePresentCompositorFrame();
  EXPECT_EQ(3u + kSkipFrames, dropped_counter.total_frames());
  EXPECT_EQ(0u, dropped_counter.total_main_dropped());
  EXPECT_EQ(kSkipFrames, dropped_counter.total_compositor_dropped());

  // Stop requesting frames, skip over a few frames, and submit + present
  // another frame. There should no new dropped frames.
  dropped_counter.Reset();
  reporting_controller_.OnStoppedRequestingBeginFrames();
  for (uint32_t i = 0; i < kSkipFrames; ++i)
    IncrementCurrentId();
  SimulatePresentCompositorFrame();
  EXPECT_EQ(1u, dropped_counter.total_frames());
  EXPECT_EQ(0u, dropped_counter.total_main_dropped());
  EXPECT_EQ(0u, dropped_counter.total_compositor_dropped());

  reporting_controller_.ResetReporters();
  reporting_controller_.SetDroppedFrameCounter(nullptr);
}

TEST_F(CompositorFrameReportingControllerTest,
       SkippedFramesFromDisplayCompositorAreDroppedUpToLimit) {
  // Submit and present two compositor frames.
  SimulatePresentCompositorFrame();
  EXPECT_EQ(1u, dropped_counter.total_frames());
  EXPECT_EQ(0u, dropped_counter.total_main_dropped());
  EXPECT_EQ(0u, dropped_counter.total_compositor_dropped());

  SimulatePresentCompositorFrame();
  EXPECT_EQ(2u, dropped_counter.total_frames());
  EXPECT_EQ(0u, dropped_counter.total_main_dropped());
  EXPECT_EQ(0u, dropped_counter.total_compositor_dropped());

  // Now skip over a 101 frames (It should be ignored as it more than 100)
  // and submit + present another frame.
  const uint32_t kSkipFrames = 101;
  const uint32_t kSkipFramesActual = 0;
  for (uint32_t i = 0; i < kSkipFrames; ++i)
    IncrementCurrentId();
  SimulatePresentCompositorFrame();
  EXPECT_EQ(3u + kSkipFramesActual, dropped_counter.total_frames());
  EXPECT_EQ(0u, dropped_counter.total_main_dropped());
  EXPECT_EQ(kSkipFramesActual, dropped_counter.total_compositor_dropped());
}

TEST_F(CompositorFrameReportingControllerTest,
       CompositorFrameBlockedOnMainFrameWithNoDamage) {
  viz::BeginFrameId current_id_1(1, 1);
  viz::BeginFrameArgs args_1 = SimulateBeginFrameArgs(current_id_1);

  viz::BeginFrameId current_id_2(1, 2);
  viz::BeginFrameArgs args_2 = SimulateBeginFrameArgs(current_id_2);

  viz::BeginFrameId current_id_3(1, 3);
  viz::BeginFrameArgs args_3 = SimulateBeginFrameArgs(current_id_3);

  viz::BeginFrameId current_id_4(1, 4);
  viz::BeginFrameArgs args_4 = SimulateBeginFrameArgs(current_id_4);

  reporting_controller_.WillBeginImplFrame(args_1);
  reporting_controller_.WillBeginMainFrame(args_1);
  reporting_controller_.OnFinishImplFrame(current_id_1);
  EXPECT_EQ(0u, dropped_counter.total_compositor_dropped());
  reporting_controller_.DidNotProduceFrame(args_1.frame_id,
                                           FrameSkippedReason::kWaitingOnMain);

  reporting_controller_.WillBeginImplFrame(args_2);
  reporting_controller_.OnFinishImplFrame(args_2.frame_id);
  reporting_controller_.DidNotProduceFrame(args_2.frame_id,
                                           FrameSkippedReason::kWaitingOnMain);

  reporting_controller_.WillBeginImplFrame(args_3);
  reporting_controller_.OnFinishImplFrame(args_3.frame_id);
  reporting_controller_.DidNotProduceFrame(args_3.frame_id,
                                           FrameSkippedReason::kWaitingOnMain);

  EXPECT_EQ(1u, reporting_controller_.GetBlockingReportersCount());
  EXPECT_EQ(3u, reporting_controller_.GetBlockedReportersCount());

  // All frames are waiting for the main frame
  EXPECT_EQ(0u, dropped_counter.total_main_dropped());
  EXPECT_EQ(0u, dropped_counter.total_compositor_dropped());
  EXPECT_EQ(0u, dropped_counter.total_frames());

  reporting_controller_.BeginMainFrameAborted(args_1.frame_id);
  reporting_controller_.DidNotProduceFrame(args_1.frame_id,
                                           FrameSkippedReason::kNoDamage);
  EXPECT_EQ(0u, dropped_counter.total_compositor_dropped());

  // New reporters replace older reporters
  reporting_controller_.WillBeginImplFrame(args_4);
  reporting_controller_.WillBeginMainFrame(args_4);

  EXPECT_EQ(4u, dropped_counter.total_frames());
  EXPECT_EQ(0u, dropped_counter.total_main_dropped());
  EXPECT_EQ(0u, dropped_counter.total_compositor_dropped());
}

TEST_F(CompositorFrameReportingControllerTest,
       SkippedFramesFromDisplayCompositorHaveSmoothThread) {
  auto thread_type_compositor = FrameSequenceMetrics::ThreadType::kCompositor;
  reporting_controller_.SetThreadAffectsSmoothness(thread_type_compositor,
                                                   true);
  dropped_counter.OnFcpReceived();

  // Submit and present two compositor frames.
  SimulatePresentCompositorFrame();
  EXPECT_EQ(1u, dropped_counter.total_frames());
  EXPECT_EQ(0u, dropped_counter.total_main_dropped());
  EXPECT_EQ(0u, dropped_counter.total_compositor_dropped());

  SimulatePresentCompositorFrame();
  EXPECT_EQ(2u, dropped_counter.total_frames());
  EXPECT_EQ(0u, dropped_counter.total_main_dropped());
  EXPECT_EQ(0u, dropped_counter.total_compositor_dropped());

  // Now skip over a few frames, and submit + present another frame.
  const uint32_t kSkipFrames_1 = 5;
  for (uint32_t i = 0; i < kSkipFrames_1; ++i)
    IncrementCurrentId();
  SimulatePresentCompositorFrame();
  EXPECT_EQ(3u + kSkipFrames_1, dropped_counter.total_frames());
  EXPECT_EQ(0u, dropped_counter.total_main_dropped());
  EXPECT_EQ(kSkipFrames_1, dropped_counter.total_compositor_dropped());
  EXPECT_EQ(kSkipFrames_1, dropped_counter.total_smoothness_dropped());

  // Now skip over a few frames which are not affecting smoothness.
  reporting_controller_.SetThreadAffectsSmoothness(thread_type_compositor,
                                                   false);
  const uint32_t kSkipFrames_2 = 7;
  for (uint32_t i = 0; i < kSkipFrames_2; ++i)
    IncrementCurrentId();
  SimulatePresentCompositorFrame();  // Present another frame.
  EXPECT_EQ(4u + kSkipFrames_1 + kSkipFrames_2, dropped_counter.total_frames());
  EXPECT_EQ(0u, dropped_counter.total_main_dropped());
  EXPECT_EQ(kSkipFrames_1 + kSkipFrames_2,
            dropped_counter.total_compositor_dropped());
  EXPECT_EQ(kSkipFrames_1, dropped_counter.total_smoothness_dropped());

  // Now skip over a few frames more frames which are affecting smoothness.
  reporting_controller_.SetThreadAffectsSmoothness(thread_type_compositor,
                                                   true);
  const uint32_t kSkipFrames_3 = 10;
  for (uint32_t i = 0; i < kSkipFrames_3; ++i)
    IncrementCurrentId();
  SimulatePresentCompositorFrame();  // Present another frame.
  EXPECT_EQ(5u + kSkipFrames_1 + kSkipFrames_2 + kSkipFrames_3,
            dropped_counter.total_frames());
  EXPECT_EQ(0u, dropped_counter.total_main_dropped());
  EXPECT_EQ(kSkipFrames_1 + kSkipFrames_2 + kSkipFrames_3,
            dropped_counter.total_compositor_dropped());
  EXPECT_EQ(kSkipFrames_1 + kSkipFrames_3,
            dropped_counter.total_smoothness_dropped());
}

}  // namespace
}  // namespace cc
