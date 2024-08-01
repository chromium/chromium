// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "cc/metrics/compositor_frame_reporting_controller.h"

#include <string>
#include <utility>
#include <vector>

#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_trace_processor.h"
#include "base/time/time.h"
#include "cc/metrics/dropped_frame_counter.h"
#include "cc/metrics/event_metrics.h"
#include "cc/metrics/total_frame_counter.h"
#include "cc/scheduler/commit_earlyout_reason.h"
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
using SmoothEffectDrivingThread = FrameInfo::SmoothEffectDrivingThread;

class TestCompositorFrameReportingController
    : public CompositorFrameReportingController {
 public:
  TestCompositorFrameReportingController()
      : CompositorFrameReportingController(/*should_report_histograms=*/true,
                                           /*should_report_ukm=*/false,
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
      if (reporter &&
          reporter->partial_update_dependents_size_for_testing() > 0) {
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
        count += reporter->partial_update_dependents_size_for_testing();
    }
    return count;
  }

  size_t GetAdoptedReportersCount() {
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
        count += reporter->owned_partial_update_dependents_size_for_testing();
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
    reporting_controller_.SetDroppedFrameCounter(&dropped_counter_);
    dropped_counter_.set_total_counter(&total_frame_counter_);
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
    reporting_controller_.BeginMainFrameStarted(begin_main_start_time_);
    reporting_controller_.NotifyReadyToCommit(std::move(blink_breakdown));
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

  void SimulateSubmitCompositorFrame(EventMetricsSet events_metrics) {
    if (!reporting_controller_.reporters()
             [CompositorFrameReportingController::PipelineStage::kActivate])
      SimulateActivate();
    CHECK(reporting_controller_.reporters()
              [CompositorFrameReportingController::PipelineStage::kActivate]);
    submit_time_ = AdvanceNowByMs(10);
    ++current_token_;
    SubmitInfo submit_info = {*current_token_, submit_time_};
    submit_info.events_metrics = std::move(events_metrics);
    reporting_controller_.DidSubmitCompositorFrame(submit_info, current_id_,
                                                   last_activated_id_);
  }

  void SimulatePresentCompositorFrame() {
    SimulateSubmitCompositorFrame({});
    viz::FrameTimingDetails details = {};
    details.presentation_feedback.timestamp = AdvanceNowByMs(10);
    reporting_controller_.DidPresentCompositorFrame(*current_token_, details);
  }

  viz::BeginFrameArgs SimulateBeginFrameArgs(viz::BeginFrameId frame_id) {
    args_ = viz::BeginFrameArgs();
    args_.frame_id = frame_id;
    args_.frame_time = AdvanceNowByMs(10);
    args_.interval = base::Milliseconds(16);
    current_id_ = frame_id;
    return args_;
  }

  std::unique_ptr<BeginMainFrameMetrics> BuildBlinkBreakdown() {
    auto breakdown = std::make_unique<BeginMainFrameMetrics>();
    breakdown->handle_input_events = base::Microseconds(10);
    breakdown->animate = base::Microseconds(9);
    breakdown->style_update = base::Microseconds(8);
    breakdown->layout_update = base::Microseconds(7);
    breakdown->compositing_inputs = base::Microseconds(6);
    breakdown->prepaint = base::Microseconds(5);
    breakdown->paint = base::Microseconds(3);
    breakdown->composite_commit = base::Microseconds(2);
    breakdown->update_layers = base::Microseconds(1);

    // Advance now by the sum of the breakdowns.
    AdvanceNowByMs(10 + 9 + 8 + 7 + 6 + 5 + 3 + 2 + 1);

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
    test_tick_clock_.Advance(base::Microseconds(advance_ms));
    return test_tick_clock_.NowTicks();
  }

  std::unique_ptr<EventMetrics> SetupEventMetrics(
      std::unique_ptr<EventMetrics> metrics) {
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

  std::unique_ptr<EventMetrics> CreateEventMetrics(
      ui::EventType type,
      std::optional<EventMetrics::TraceId> trace_id) {
    const base::TimeTicks event_time = AdvanceNowByMs(10);
    const base::TimeTicks arrived_in_browser_main_timestamp = AdvanceNowByMs(3);
    AdvanceNowByMs(10);
    return SetupEventMetrics(EventMetrics::CreateForTesting(
        type, event_time, arrived_in_browser_main_timestamp, &test_tick_clock_,
        trace_id));
  }

  std::unique_ptr<EventMetrics> CreateScrollBeginEventMetrics(
      ui::ScrollInputType input_type) {
    const base::TimeTicks event_time = AdvanceNowByMs(10);
    const base::TimeTicks arrived_in_browser_main_timestamp = AdvanceNowByMs(3);
    AdvanceNowByMs(10);
    return SetupEventMetrics(ScrollEventMetrics::CreateForTesting(
        ui::EventType::kGestureScrollBegin, input_type,
        /*is_inertial=*/false, event_time, arrived_in_browser_main_timestamp,
        &test_tick_clock_));
  }

  std::unique_ptr<EventMetrics> CreateScrollUpdateEventMetrics(
      ui::ScrollInputType input_type,
      bool is_inertial,
      ScrollUpdateEventMetrics::ScrollUpdateType scroll_update_type,
      std::optional<EventMetrics::TraceId> trace_id) {
    const base::TimeTicks event_time = AdvanceNowByMs(10);
    const base::TimeTicks arrived_in_browser_main_timestamp = AdvanceNowByMs(3);
    AdvanceNowByMs(10);
    return SetupEventMetrics(ScrollUpdateEventMetrics::CreateForTesting(
        ui::EventType::kGestureScrollUpdate, input_type, is_inertial,
        scroll_update_type, /*delta=*/10.0f, event_time,
        arrived_in_browser_main_timestamp, &test_tick_clock_, trace_id));
  }

  std::unique_ptr<EventMetrics> CreatePinchEventMetrics(
      ui::EventType type,
      ui::ScrollInputType input_type) {
    const base::TimeTicks event_time = AdvanceNowByMs(10);
    AdvanceNowByMs(10);
    return SetupEventMetrics(PinchEventMetrics::CreateForTesting(
        type, input_type, event_time, &test_tick_clock_));
  }

  std::vector<base::TimeTicks> GetEventTimestamps(
      const EventMetrics::List& events_metrics) {
    std::vector<base::TimeTicks> event_times;
    event_times.reserve(events_metrics.size());
    base::ranges::transform(events_metrics, std::back_inserter(event_times),
                            [](const auto& event_metrics) {
                              return event_metrics->GetDispatchStageTimestamp(
                                  EventMetrics::DispatchStage::kGenerated);
                            });
    return event_times;
  }

 protected:
  // Disable sub-sampling to deterministically record histograms under test.
  base::MetricsSubSampler::ScopedAlwaysSampleForTesting no_subsampling_;

  // This should be defined before |reporting_controller_| so it is created
  // before and destroyed after that.
  base::SimpleTestTickClock test_tick_clock_;

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
  viz::FrameTokenGenerator current_token_;
  DroppedFrameCounter dropped_counter_;
  TotalFrameCounter total_frame_counter_;
  TestCompositorFrameReportingController reporting_controller_;
  ::base::test::TracingEnvironment tracing_environment_;
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
  reporting_controller_.NotifyReadyToCommit(nullptr);
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

  reporting_controller_.NotifyReadyToCommit(nullptr);
  reporting_controller_.WillCommit();
  reporting_controller_.DidCommit();
  reporting_controller_.WillActivate();
  reporting_controller_.DidActivate();
  // Reporter in activate state for frame_2 is overwritten by the reporter for
  // frame_3.
  EXPECT_EQ(1, reporting_controller_.ActiveReporters());

  last_activated_id_ = current_id_3;
  SubmitInfo submit_info;
  submit_info.time = AdvanceNowByMs(10);
  reporting_controller_.DidSubmitCompositorFrame(submit_info, current_id_3,
                                                 last_activated_id_);
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
  reporting_controller_.BeginMainFrameAborted(
      current_id_1, CommitEarlyOutReason::kFinishedNoUpdates);
  reporting_controller_.OnFinishImplFrame(current_id_1);
  reporting_controller_.DidNotProduceFrame(current_id_1,
                                           FrameSkippedReason::kNoDamage);

  reporting_controller_.WillBeginImplFrame(args_2);
  reporting_controller_.WillBeginMainFrame(args_2);
  reporting_controller_.OnFinishImplFrame(current_id_2);
  reporting_controller_.BeginMainFrameAborted(
      current_id_2, CommitEarlyOutReason::kFinishedNoUpdates);
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
  reporting_controller_.NotifyReadyToCommit(nullptr);
  reporting_controller_.WillCommit();
  reporting_controller_.DidCommit();
  reporting_controller_.WillActivate();
  reporting_controller_.DidActivate();
  SubmitInfo submit_info = {1u, AdvanceNowByMs(10)};
  reporting_controller_.DidSubmitCompositorFrame(submit_info, current_id_2,
                                                 current_id_1);
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
  reporting_controller_.NotifyReadyToCommit(nullptr);
  reporting_controller_.WillCommit();
  reporting_controller_.DidCommit();
  reporting_controller_.WillActivate();
  reporting_controller_.DidActivate();
  reporting_controller_.OnFinishImplFrame(current_id_3);
  SubmitInfo submit_info = {1u, AdvanceNowByMs(10)};
  reporting_controller_.DidSubmitCompositorFrame(submit_info, current_id_3,
                                                 current_id_1);
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
  reporting_controller_.BeginMainFrameAborted(
      current_id_, CommitEarlyOutReason::kFinishedNoUpdates);
  reporting_controller_.OnFinishImplFrame(current_id_);
  SubmitInfo submit_info = {1u, AdvanceNowByMs(10)};
  reporting_controller_.DidSubmitCompositorFrame(submit_info, current_id_,
                                                 last_activated_id_);

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
  reporting_controller_.NotifyReadyToCommit(nullptr);
  reporting_controller_.WillCommit();
  reporting_controller_.DidCommit();
  reporting_controller_.WillActivate();
  reporting_controller_.DidActivate();
  reporting_controller_.WillBeginImplFrame(args_2);
  reporting_controller_.WillBeginMainFrame(args_2);
  reporting_controller_.OnFinishImplFrame(current_id_2);
  reporting_controller_.BeginMainFrameAborted(
      current_id_2, CommitEarlyOutReason::kFinishedNoUpdates);
  SubmitInfo submit_info = {1u, AdvanceNowByMs(10)};
  reporting_controller_.DidSubmitCompositorFrame(submit_info, current_id_2,
                                                 current_id_1);
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
  SubmitInfo submit_info2 = {2u, AdvanceNowByMs(10)};
  reporting_controller_.DidSubmitCompositorFrame(submit_info2, current_id_2,
                                                 current_id_1);
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
  SubmitInfo submit_info3 = {3u, AdvanceNowByMs(10)};
  reporting_controller_.DidSubmitCompositorFrame(submit_info3, current_id_3,
                                                 current_id_1);
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
  reporting_controller_.NotifyReadyToCommit(nullptr);
  reporting_controller_.WillCommit();
  reporting_controller_.DidCommit();
  reporting_controller_.WillActivate();
  reporting_controller_.DidActivate();
  SubmitInfo submit_info = {1u, AdvanceNowByMs(10)};
  reporting_controller_.DidSubmitCompositorFrame(submit_info, current_id_1,
                                                 current_id_1);
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
  SubmitInfo submit_info2 = {2u, AdvanceNowByMs(10)};
  reporting_controller_.DidSubmitCompositorFrame(submit_info2, current_id_2,
                                                 current_id_1);
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
  reporting_controller_.NotifyReadyToCommit(nullptr);
  reporting_controller_.WillCommit();
  reporting_controller_.DidCommit();
  reporting_controller_.WillActivate();
  reporting_controller_.DidActivate();
  SubmitInfo submit_info3 = {3u, AdvanceNowByMs(10)};
  reporting_controller_.DidSubmitCompositorFrame(submit_info3, current_id_3,
                                                 current_id_2);
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
  reporting_controller_.NotifyReadyToCommit(nullptr);
  reporting_controller_.WillCommit();
  reporting_controller_.DidCommit();
  reporting_controller_.WillActivate();
  reporting_controller_.DidActivate();
  SubmitInfo submit_info = {1u, AdvanceNowByMs(10)};
  reporting_controller_.DidSubmitCompositorFrame(submit_info, current_id_1,
                                                 current_id_1);
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
  reporting_controller_.NotifyReadyToCommit(nullptr);
  reporting_controller_.WillCommit();
  reporting_controller_.DidCommit();
  reporting_controller_.OnFinishImplFrame(current_id_2);
  SubmitInfo submit_info2 = {2u, AdvanceNowByMs(10)};
  reporting_controller_.DidSubmitCompositorFrame(submit_info2, current_id_2,
                                                 current_id_1);
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
  SubmitInfo submit_info3 = {3u, AdvanceNowByMs(10)};
  reporting_controller_.DidSubmitCompositorFrame(submit_info3, current_id_3,
                                                 current_id_2);
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
      base::Microseconds(10).InMilliseconds(), 1);
  histogram_tester.ExpectUniqueSample(
      "CompositorLatency.SendBeginMainFrameToCommit.Animate",
      base::Microseconds(9).InMilliseconds(), 1);
  histogram_tester.ExpectUniqueSample(
      "CompositorLatency.SendBeginMainFrameToCommit.StyleUpdate",
      base::Microseconds(8).InMilliseconds(), 1);
  histogram_tester.ExpectUniqueSample(
      "CompositorLatency.SendBeginMainFrameToCommit.LayoutUpdate",
      base::Microseconds(7).InMilliseconds(), 1);
  histogram_tester.ExpectUniqueSample(
      "CompositorLatency.SendBeginMainFrameToCommit.CompositingInputs",
      base::Microseconds(6).InMilliseconds(), 1);
  histogram_tester.ExpectUniqueSample(
      "CompositorLatency.SendBeginMainFrameToCommit.Prepaint",
      base::Microseconds(5).InMilliseconds(), 1);
  histogram_tester.ExpectUniqueSample(
      "CompositorLatency.SendBeginMainFrameToCommit.Paint",
      base::Microseconds(3).InMilliseconds(), 1);
  histogram_tester.ExpectUniqueSample(
      "CompositorLatency.SendBeginMainFrameToCommit.CompositeCommit",
      base::Microseconds(2).InMilliseconds(), 1);
  histogram_tester.ExpectUniqueSample(
      "CompositorLatency.SendBeginMainFrameToCommit.UpdateLayers",
      base::Microseconds(1).InMilliseconds(), 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SendBeginMainFrameToCommit.BeginMainSentToStarted", 1);
}

// If the presentation of the frame happens before deadline.
TEST_F(CompositorFrameReportingControllerTest, ReportingMissedDeadlineFrame1) {
  base::HistogramTester histogram_tester;

  reporting_controller_.WillBeginImplFrame(args_);
  reporting_controller_.OnFinishImplFrame(current_id_);
  reporting_controller_.WillBeginMainFrame(args_);
  reporting_controller_.NotifyReadyToCommit(nullptr);
  reporting_controller_.WillCommit();
  reporting_controller_.DidCommit();
  reporting_controller_.WillActivate();
  reporting_controller_.DidActivate();
  SubmitInfo submit_info = {1u, AdvanceNowByMs(10)};
  reporting_controller_.DidSubmitCompositorFrame(submit_info, current_id_,
                                                 current_id_);
  viz::FrameTimingDetails details = {};
  details.presentation_feedback.timestamp =
      args_.frame_time + args_.interval * 1.5 - base::Microseconds(100);
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
  reporting_controller_.NotifyReadyToCommit(nullptr);
  reporting_controller_.WillCommit();
  reporting_controller_.DidCommit();
  reporting_controller_.WillActivate();
  reporting_controller_.DidActivate();
  SubmitInfo submit_info = {1u, AdvanceNowByMs(10)};
  reporting_controller_.DidSubmitCompositorFrame(submit_info, current_id_,
                                                 current_id_);
  viz::FrameTimingDetails details = {};
  details.presentation_feedback.timestamp =
      args_.frame_time + args_.interval * 1.5 + base::Microseconds(100);
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

// If a compositor animation takes too long and throttles draw
TEST_F(CompositorFrameReportingControllerTest, LongCompositorAnimation) {
  base::HistogramTester histogram_tester;

  SimulatePresentCompositorFrame();

  reporting_controller_.WillBeginImplFrame(args_);
  reporting_controller_.OnFinishImplFrame(current_id_);
  SubmitInfo submit_info = {1u, AdvanceNowByMs(10)};
  reporting_controller_.DidSubmitCompositorFrame(submit_info, current_id_,
                                                 last_activated_id_);
  viz::FrameTimingDetails details = {};
  reporting_controller_.DidPresentCompositorFrame(*current_token_, details);

  IncrementCurrentId();
  reporting_controller_.WillBeginImplFrame(args_);
  reporting_controller_.OnFinishImplFrame(current_id_);
  reporting_controller_.DidNotProduceFrame(args_.frame_id,
                                           FrameSkippedReason::kDrawThrottled);

  IncrementCurrentId();
  // Flushing the last no damage frame.
  reporting_controller_.WillBeginImplFrame(args_);
  reporting_controller_.OnFinishImplFrame(current_id_);

  EXPECT_EQ(3u, dropped_counter_.total_frames());
  EXPECT_EQ(1u, dropped_counter_.total_dropped());
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
      CreateEventMetrics(ui::EventType::kTouchPressed, std::nullopt),
      CreateEventMetrics(ui::EventType::kTouchMoved, std::nullopt),
      CreateEventMetrics(ui::EventType::kTouchMoved, std::nullopt),
  };
  EXPECT_THAT(event_metrics_ptrs, Each(NotNull()));
  EventMetrics::List events_metrics(
      std::make_move_iterator(std::begin(event_metrics_ptrs)),
      std::make_move_iterator(std::end(event_metrics_ptrs)));
  std::vector<base::TimeTicks> event_times = GetEventTimestamps(events_metrics);

  // Submit a compositor frame and notify CompositorFrameReporter of the events
  // affecting the frame.
  SimulateSubmitCompositorFrame({std::move(events_metrics), {}});

  // Present the submitted compositor frame to the user.
  const base::TimeTicks presentation_time = AdvanceNowByMs(10);
  viz::FrameTimingDetails details;
  details.presentation_feedback.timestamp = presentation_time;
  reporting_controller_.DidPresentCompositorFrame(*current_token_, details);

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
       static_cast<base::HistogramBase::Sample>(
           (presentation_time - event_times[0]).InMicroseconds())},
      {"EventLatency.TouchMoved.TotalLatency",
       static_cast<base::HistogramBase::Sample>(
           (presentation_time - event_times[1]).InMicroseconds())},
      {"EventLatency.TouchMoved.TotalLatency",
       static_cast<base::HistogramBase::Sample>(
           (presentation_time - event_times[2]).InMicroseconds())},
      {"EventLatency.TotalLatency",
       static_cast<base::HistogramBase::Sample>(
           (presentation_time - event_times[0]).InMicroseconds())},
      {"EventLatency.TotalLatency",
       static_cast<base::HistogramBase::Sample>(
           (presentation_time - event_times[1]).InMicroseconds())},
      {"EventLatency.TotalLatency",
       static_cast<base::HistogramBase::Sample>(
           (presentation_time - event_times[2]).InMicroseconds())},
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

  const bool kScrollIsInertial = true;
  const bool kScrollIsNotInertial = false;
  std::unique_ptr<EventMetrics> event_metrics_ptrs[] = {
      CreateScrollBeginEventMetrics(ui::ScrollInputType::kWheel),
      CreateScrollUpdateEventMetrics(
          ui::ScrollInputType::kWheel, kScrollIsNotInertial,
          ScrollUpdateEventMetrics::ScrollUpdateType::kStarted, std::nullopt),
      CreateScrollUpdateEventMetrics(
          ui::ScrollInputType::kWheel, kScrollIsNotInertial,
          ScrollUpdateEventMetrics::ScrollUpdateType::kContinued, std::nullopt),
      CreateScrollUpdateEventMetrics(
          ui::ScrollInputType::kWheel, kScrollIsInertial,
          ScrollUpdateEventMetrics::ScrollUpdateType::kContinued, std::nullopt),
      CreateScrollBeginEventMetrics(ui::ScrollInputType::kTouchscreen),
      CreateScrollUpdateEventMetrics(
          ui::ScrollInputType::kTouchscreen, kScrollIsNotInertial,
          ScrollUpdateEventMetrics::ScrollUpdateType::kStarted, std::nullopt),
      CreateScrollUpdateEventMetrics(
          ui::ScrollInputType::kTouchscreen, kScrollIsNotInertial,
          ScrollUpdateEventMetrics::ScrollUpdateType::kContinued, std::nullopt),
      CreateScrollUpdateEventMetrics(
          ui::ScrollInputType::kTouchscreen, kScrollIsInertial,
          ScrollUpdateEventMetrics::ScrollUpdateType::kContinued, std::nullopt),
  };
  EXPECT_THAT(event_metrics_ptrs, Each(NotNull()));
  EventMetrics::List events_metrics(
      std::make_move_iterator(std::begin(event_metrics_ptrs)),
      std::make_move_iterator(std::end(event_metrics_ptrs)));
  std::vector<base::TimeTicks> event_times = GetEventTimestamps(events_metrics);

  // Submit a compositor frame and notify CompositorFrameReporter of the events
  // affecting the frame.
  SimulateSubmitCompositorFrame({std::move(events_metrics), {}});

  // Present the submitted compositor frame to the user.
  viz::FrameTimingDetails details;
  details.received_compositor_frame_timestamp = AdvanceNowByMs(10);
  details.draw_start_timestamp = AdvanceNowByMs(10);
  details.swap_timings.swap_start = AdvanceNowByMs(10);
  details.swap_timings.swap_end = AdvanceNowByMs(10);
  details.presentation_feedback.timestamp = AdvanceNowByMs(10);
  reporting_controller_.DidPresentCompositorFrame(*current_token_, details);

  // Verify that EventLatency histograms are recorded.
  struct {
    const char* name;
    const base::HistogramBase::Count count;
  } expected_counts[] = {
      {"EventLatency.GestureScrollBegin.Wheel.TotalLatency2", 1},
      {"EventLatency.FirstGestureScrollUpdate.Wheel.TotalLatency2", 1},
      {"EventLatency.GestureScrollUpdate.Wheel.TotalLatency2", 1},
      {"EventLatency.InertialGestureScrollUpdate.Wheel.TotalLatency2", 1},
      {"EventLatency.GestureScrollBegin.Touchscreen.TotalLatency2", 1},
      {"EventLatency.FirstGestureScrollUpdate.Touchscreen.TotalLatency2", 1},
      {"EventLatency.GestureScrollUpdate.Touchscreen.TotalLatency", 1},
      {"EventLatency.GestureScrollUpdate.Touchscreen.TotalLatency2", 1},
      {"EventLatency.InertialGestureScrollUpdate.Touchscreen.TotalLatency2", 1},
      {"EventLatency.GestureScrollBegin.TotalLatency2", 2},
      {"EventLatency.GestureScrollBegin.GenerationToBrowserMain", 2},
      {"EventLatency.FirstGestureScrollUpdate.TotalLatency2", 2},
      {"EventLatency.FirstGestureScrollUpdate.GenerationToBrowserMain", 2},
      {"EventLatency.GestureScrollUpdate.TotalLatency2", 2},
      {"EventLatency.GestureScrollUpdate.GenerationToBrowserMain", 2},
      {"EventLatency.InertialGestureScrollUpdate.TotalLatency2", 2},
      {"EventLatency.InertialGestureScrollUpdate.GenerationToBrowserMain", 2},
      {"EventLatency.TotalLatency", 8},
  };
  for (const auto& expected_count : expected_counts) {
    histogram_tester.ExpectTotalCount(expected_count.name,
                                      expected_count.count);
  }

  const base::TimeTicks presentation_time =
      details.presentation_feedback.timestamp;
  struct {
    const char* name;
    const base::HistogramBase::Sample latency_ms;
  } expected_latencies[] = {
      {"EventLatency.GestureScrollBegin.Wheel.TotalLatency2",
       static_cast<base::HistogramBase::Sample>(
           (presentation_time - event_times[0]).InMicroseconds())},
      {"EventLatency.FirstGestureScrollUpdate.Wheel.TotalLatency2",
       static_cast<base::HistogramBase::Sample>(
           (presentation_time - event_times[1]).InMicroseconds())},
      {"EventLatency.GestureScrollUpdate.Wheel.TotalLatency2",
       static_cast<base::HistogramBase::Sample>(
           (presentation_time - event_times[2]).InMicroseconds())},
      {"EventLatency.InertialGestureScrollUpdate.Wheel.TotalLatency2",
       static_cast<base::HistogramBase::Sample>(
           (presentation_time - event_times[3]).InMicroseconds())},
      {"EventLatency.GestureScrollBegin.Touchscreen.TotalLatency2",
       static_cast<base::HistogramBase::Sample>(
           (presentation_time - event_times[4]).InMicroseconds())},
      {"EventLatency.FirstGestureScrollUpdate.Touchscreen.TotalLatency2",
       static_cast<base::HistogramBase::Sample>(
           (presentation_time - event_times[5]).InMicroseconds())},
      {"EventLatency.GestureScrollUpdate.Touchscreen.TotalLatency",
       static_cast<base::HistogramBase::Sample>(
           (presentation_time - event_times[6]).InMicroseconds())},
      {"EventLatency.GestureScrollUpdate.Touchscreen.TotalLatency2",
       static_cast<base::HistogramBase::Sample>(
           (presentation_time - event_times[6]).InMicroseconds())},
      {"EventLatency.InertialGestureScrollUpdate.Touchscreen.TotalLatency2",
       static_cast<base::HistogramBase::Sample>(
           (presentation_time - event_times[7]).InMicroseconds())},
  };
  for (const auto& expected_latency : expected_latencies) {
    histogram_tester.ExpectBucketCount(expected_latency.name,
                                       expected_latency.latency_ms, 1);
  }
}

TEST_F(CompositorFrameReportingControllerTest,
       EventLatencyMainRepaintedScroll) {
  base::HistogramTester histogram_tester;

  // Set up two EventMetrics objects.
  std::unique_ptr<EventMetrics> metrics_1 = CreateScrollUpdateEventMetrics(
      ui::ScrollInputType::kWheel, /*is_inertial=*/false,
      ScrollUpdateEventMetrics::ScrollUpdateType::kStarted, std::nullopt);
  base::TimeTicks start_time_1 = metrics_1->GetDispatchStageTimestamp(
      EventMetrics::DispatchStage::kGenerated);

  // The second EventMetrics does not have set_requires_main_thread_update().
  // (It's not very realistic for the same scroll gesture to produce two events
  // with differing values for this bit, but let's test both conditions here.)
  std::unique_ptr<EventMetrics> metrics_2 = CreateScrollUpdateEventMetrics(
      ui::ScrollInputType::kWheel, /*is_inertial=*/false,
      ScrollUpdateEventMetrics::ScrollUpdateType::kContinued, std::nullopt);
  metrics_2->set_requires_main_thread_update();
  base::TimeTicks start_time_2 = metrics_2->GetDispatchStageTimestamp(
      EventMetrics::DispatchStage::kGenerated);

  // Simulate a frame getting stuck in the main thread.
  SimulateBeginImplFrame();
  SimulateBeginMainFrame();
  reporting_controller_.OnFinishImplFrame(current_id_);

  // Submit a partial update with our events from the compositor thread.
  EventMetrics::List metrics_list;
  metrics_list.push_back(std::move(metrics_1));
  metrics_list.push_back(std::move(metrics_2));
  SubmitInfo submit_info = {*current_token_, AdvanceNowByMs(10)};
  submit_info.events_metrics = {{}, std::move(metrics_list)};
  reporting_controller_.DidSubmitCompositorFrame(submit_info, current_id_, {});

  // Present the partial update.
  viz::FrameTimingDetails details_1 = {};
  details_1.presentation_feedback.timestamp = AdvanceNowByMs(10);
  reporting_controller_.DidPresentCompositorFrame(*current_token_, details_1);

  // Let the main thread finish its work.
  SimulateCommit(nullptr);
  SimulateActivate();

  // Submit the final update.
  SimulateBeginImplFrame();
  reporting_controller_.OnFinishImplFrame(current_id_);
  SimulateSubmitCompositorFrame({});

  // Present the final update.
  viz::FrameTimingDetails details_2 = {};
  details_2.presentation_feedback.timestamp = AdvanceNowByMs(10);
  reporting_controller_.DidPresentCompositorFrame(*current_token_, details_2);

  // metrics_1 did NOT have requires_main_thread_update(), so its latency is
  // based on the partial-update presentation (details_1).
  base::TimeDelta expected_latency_1 =
      details_1.presentation_feedback.timestamp - start_time_1;
  histogram_tester.ExpectBucketCount(
      "EventLatency.FirstGestureScrollUpdate.Wheel.TotalLatency2",
      expected_latency_1.InMicroseconds(), 1);

  // metrics_2 has requires_main_thread_update(), so its latency is based on the
  // final-update presentation (details_2).
  base::TimeDelta expected_latency_2 =
      details_2.presentation_feedback.timestamp - start_time_2;
  histogram_tester.ExpectBucketCount(
      "EventLatency.GestureScrollUpdate.Wheel.TotalLatency2",
      expected_latency_2.InMicroseconds(), 1);
}

// Tests that EventLatency total latency histograms are reported properly for
// pinch events when a frame is presented to the user.
TEST_F(CompositorFrameReportingControllerTest,
       EventLatencyPinchTotalForPresentedFrameReported) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<EventMetrics> event_metrics_ptrs[] = {
      CreatePinchEventMetrics(ui::EventType::kGesturePinchBegin,
                              ui::ScrollInputType::kWheel),
      CreatePinchEventMetrics(ui::EventType::kGesturePinchUpdate,
                              ui::ScrollInputType::kWheel),
      CreatePinchEventMetrics(ui::EventType::kGesturePinchBegin,
                              ui::ScrollInputType::kTouchscreen),
      CreatePinchEventMetrics(ui::EventType::kGesturePinchUpdate,
                              ui::ScrollInputType::kTouchscreen),
  };
  EXPECT_THAT(event_metrics_ptrs, Each(NotNull()));
  EventMetrics::List events_metrics(
      std::make_move_iterator(std::begin(event_metrics_ptrs)),
      std::make_move_iterator(std::end(event_metrics_ptrs)));
  std::vector<base::TimeTicks> event_times = GetEventTimestamps(events_metrics);

  // Submit a compositor frame and notify CompositorFrameReporter of the events
  // affecting the frame.
  SimulateSubmitCompositorFrame({std::move(events_metrics), {}});

  // Present the submitted compositor frame to the user.
  viz::FrameTimingDetails details;
  details.received_compositor_frame_timestamp = AdvanceNowByMs(10);
  details.draw_start_timestamp = AdvanceNowByMs(10);
  details.swap_timings.swap_start = AdvanceNowByMs(10);
  details.swap_timings.swap_end = AdvanceNowByMs(10);
  details.presentation_feedback.timestamp = AdvanceNowByMs(10);
  reporting_controller_.DidPresentCompositorFrame(*current_token_, details);

  // Verify that EventLatency histograms are recorded.
  struct {
    const char* name;
    const base::HistogramBase::Count count;
  } expected_counts[] = {
      {"EventLatency.GesturePinchBegin.Touchpad.TotalLatency", 1},
      {"EventLatency.GesturePinchUpdate.Touchpad.TotalLatency", 1},
      {"EventLatency.GesturePinchBegin.Touchscreen.TotalLatency", 1},
      {"EventLatency.GesturePinchUpdate.Touchscreen.TotalLatency", 1},
      {"EventLatency.TotalLatency", 4},
  };
  for (const auto& expected_count : expected_counts) {
    histogram_tester.ExpectTotalCount(expected_count.name,
                                      expected_count.count);
  }

  const base::TimeTicks presentation_time =
      details.presentation_feedback.timestamp;
  struct {
    const char* name;
    const base::HistogramBase::Sample latency_ms;
  } expected_latencies[] = {
      {"EventLatency.GesturePinchBegin.Touchpad.TotalLatency",
       static_cast<base::HistogramBase::Sample>(
           (presentation_time - event_times[0]).InMicroseconds())},
      {"EventLatency.GesturePinchUpdate.Touchpad.TotalLatency",
       static_cast<base::HistogramBase::Sample>(
           (presentation_time - event_times[1]).InMicroseconds())},
      {"EventLatency.GesturePinchBegin.Touchscreen.TotalLatency",
       static_cast<base::HistogramBase::Sample>(
           (presentation_time - event_times[2]).InMicroseconds())},
      {"EventLatency.GesturePinchUpdate.Touchscreen.TotalLatency",
       static_cast<base::HistogramBase::Sample>(
           (presentation_time - event_times[3]).InMicroseconds())},
  };
  for (const auto& expected_latency : expected_latencies) {
    histogram_tester.ExpectBucketCount(expected_latency.name,
                                       expected_latency.latency_ms, 1);
  }
}

// Tests that EventLatency histograms for events of a dropped frame are reported
// in the first subsequent presented frame.
TEST_F(CompositorFrameReportingControllerTest,
       EventLatencyForDidNotPresentFrameReportedOnNextPresent) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<EventMetrics> event_metrics_ptrs[] = {
      CreateEventMetrics(ui::EventType::kTouchPressed, std::nullopt),
      CreateEventMetrics(ui::EventType::kTouchMoved, std::nullopt),
      CreateEventMetrics(ui::EventType::kTouchMoved, std::nullopt),
  };
  EXPECT_THAT(event_metrics_ptrs, Each(NotNull()));
  EventMetrics::List events_metrics(
      std::make_move_iterator(std::begin(event_metrics_ptrs)),
      std::make_move_iterator(std::end(event_metrics_ptrs)));
  std::vector<base::TimeTicks> event_times = GetEventTimestamps(events_metrics);

  // Submit a compositor frame and notify CompositorFrameReporter of the events
  // affecting the frame.
  SimulateSubmitCompositorFrame({std::move(events_metrics), {}});

  // Submit another compositor frame.
  IncrementCurrentId();
  SimulateSubmitCompositorFrame({});

  // Present the second compositor frame to the user, dropping the first one.
  const base::TimeTicks presentation_time = AdvanceNowByMs(10);
  viz::FrameTimingDetails details;
  details.presentation_feedback.timestamp = presentation_time;
  reporting_controller_.DidPresentCompositorFrame(*current_token_, details);

  // Verify that EventLatency histograms for the first frame (dropped) are
  // recorded using the presentation time of the second frame (presented).
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
       static_cast<base::HistogramBase::Sample>(
           (presentation_time - event_times[0]).InMicroseconds())},
      {"EventLatency.TouchMoved.TotalLatency",
       static_cast<base::HistogramBase::Sample>(
           (presentation_time - event_times[1]).InMicroseconds())},
      {"EventLatency.TouchMoved.TotalLatency",
       static_cast<base::HistogramBase::Sample>(
           (presentation_time - event_times[2]).InMicroseconds())},
      {"EventLatency.TotalLatency",
       static_cast<base::HistogramBase::Sample>(
           (presentation_time - event_times[0]).InMicroseconds())},
      {"EventLatency.TotalLatency",
       static_cast<base::HistogramBase::Sample>(
           (presentation_time - event_times[1]).InMicroseconds())},
      {"EventLatency.TotalLatency",
       static_cast<base::HistogramBase::Sample>(
           (presentation_time - event_times[2]).InMicroseconds())},
  };
  for (const auto& expected_latency : expected_latencies) {
    histogram_tester.ExpectBucketCount(expected_latency.name,
                                       expected_latency.latency_ms, 1);
  }
}

TEST_F(CompositorFrameReportingControllerTest,
       NewMainUpdateIsNotPartialUpdate) {
  // Start a frame with main-thread update. Submit the frame (and present)
  // before the main-thread responds. This creates two reporters: R1C and R1M
  // (R1C for the submitted frame with updates from compositor-thread, and R1M
  // for the pending main-thread frame).
  SimulateBeginMainFrame();
  reporting_controller_.OnFinishImplFrame(current_id_);
  SubmitInfo submit_info = {1u, AdvanceNowByMs(10)};
  reporting_controller_.DidSubmitCompositorFrame(submit_info, current_id_, {});
  viz::FrameTimingDetails details = {};
  details.presentation_feedback.timestamp = AdvanceNowByMs(10);
  reporting_controller_.DidPresentCompositorFrame(1u, details);

  // The main-thread responds now, triggering a commit and activation.
  reporting_controller_.NotifyReadyToCommit(nullptr);
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
  SubmitInfo submit_info_main = {1u, AdvanceNowByMs(10)};
  reporting_controller_.DidSubmitCompositorFrame(submit_info_main, current_id_,
                                                 previous_id);
  details.presentation_feedback.timestamp = AdvanceNowByMs(10);
  reporting_controller_.DidPresentCompositorFrame(1u, details);

  // In total, two frames have been completed: R1C, and R1M.
  // R2C has been presented, but it is blocked on R2M to know whether R2C
  // contains partial update, or complete updates. So it is kept alive.
  EXPECT_EQ(2u, dropped_counter_.total_frames());
  EXPECT_EQ(1u, dropped_counter_.total_partial());
  EXPECT_EQ(1u, reporting_controller_.GetBlockingReportersCount());
  EXPECT_EQ(1u, reporting_controller_.GetBlockedReportersCount());

  reporting_controller_.ResetReporters();
  reporting_controller_.SetDroppedFrameCounter(nullptr);
}

// Verifies that when a dependent frame is submitted to Viz, but not presented
// (hence dropped), should have its reporter immediately terminated and not
// adopted by the decider reporter.
TEST_F(CompositorFrameReportingControllerTest,
       DependentDroppedFrameTerminatesReporterImmediately) {
  // Start a frame with main-thread update and let it get stuck in main-thread.
  SimulateBeginMainFrame();
  reporting_controller_.OnFinishImplFrame(current_id_);

  // Start another frame that has impl-thread update and submit and present it
  // successfully. The reporter for this frame should become dependent of the
  // main reporter and adopted by it.
  SimulateBeginImplFrame();
  reporting_controller_.OnFinishImplFrame(current_id_);
  SubmitInfo submit_info = {1u, AdvanceNowByMs(10)};
  reporting_controller_.DidSubmitCompositorFrame(submit_info, current_id_, {});

  viz::FrameTimingDetails details_1 = {};
  details_1.presentation_feedback.timestamp = AdvanceNowByMs(10);
  reporting_controller_.DidPresentCompositorFrame(1u, details_1);

  // There should be 1 blocking reporter, 1 blocked reporter, and 1 adopted
  // reporter.
  EXPECT_EQ(1u, reporting_controller_.GetBlockingReportersCount());
  EXPECT_EQ(1u, reporting_controller_.GetBlockedReportersCount());
  EXPECT_EQ(1u, reporting_controller_.GetAdoptedReportersCount());

  // At this point no frame has been completed, yet.
  EXPECT_EQ(0u, dropped_counter_.total_frames());
  EXPECT_EQ(0u, dropped_counter_.total_dropped());

  // Start yet another frame that has impl-thread update and submit it, but with
  // failed presentation. The reporter for this frame should become dependent of
  // the main reporter, but should terminated immediately upon presentation
  // failure, hence not adopted by the main reporter.
  SimulateBeginImplFrame();
  reporting_controller_.OnFinishImplFrame(current_id_);
  SubmitInfo submit_info2 = {2u, AdvanceNowByMs(10)};
  reporting_controller_.DidSubmitCompositorFrame(submit_info2, current_id_, {});

  viz::FrameTimingDetails details_2 = {};
  details_2.presentation_feedback.timestamp = AdvanceNowByMs(10);
  details_2.presentation_feedback.flags |= gfx::PresentationFeedback::kFailure;
  reporting_controller_.DidPresentCompositorFrame(2u, details_2);

  // There should be still 1 blocking reporter, but 2 blocked reporters. There
  // should also be only 1 adopted reporter as the new reporter should not be
  // adopted.
  EXPECT_EQ(1u, reporting_controller_.GetBlockingReportersCount());
  EXPECT_EQ(2u, reporting_controller_.GetBlockedReportersCount());
  EXPECT_EQ(1u, reporting_controller_.GetAdoptedReportersCount());

  // At this point 1 frame has been completed and it's a dropped frame.
  EXPECT_EQ(1u, dropped_counter_.total_frames());
  EXPECT_EQ(1u, dropped_counter_.total_dropped());

  reporting_controller_.ResetReporters();
  reporting_controller_.SetDroppedFrameCounter(nullptr);
}

TEST_F(CompositorFrameReportingControllerTest,
       SkippedFramesFromDisplayCompositorAreDropped) {
  // Submit and present two compositor frames.
  SimulatePresentCompositorFrame();
  EXPECT_EQ(1u, dropped_counter_.total_frames());
  EXPECT_EQ(0u, dropped_counter_.total_partial());
  EXPECT_EQ(0u, dropped_counter_.total_dropped());

  SimulatePresentCompositorFrame();
  EXPECT_EQ(2u, dropped_counter_.total_frames());
  EXPECT_EQ(0u, dropped_counter_.total_partial());
  EXPECT_EQ(0u, dropped_counter_.total_dropped());

  // Now skip over a few frames, and submit + present another frame.
  const uint32_t kSkipFrames = 5;
  for (uint32_t i = 0; i < kSkipFrames; ++i)
    IncrementCurrentId();
  SimulatePresentCompositorFrame();
  EXPECT_EQ(3u + kSkipFrames, dropped_counter_.total_frames());
  EXPECT_EQ(0u, dropped_counter_.total_partial());
  EXPECT_EQ(kSkipFrames, dropped_counter_.total_dropped());

  // Stop requesting frames, skip over a few frames, and submit + present
  // another frame. There should no new dropped frames.
  dropped_counter_.Reset();
  reporting_controller_.OnStoppedRequestingBeginFrames();
  for (uint32_t i = 0; i < kSkipFrames; ++i)
    IncrementCurrentId();
  SimulatePresentCompositorFrame();
  EXPECT_EQ(1u, dropped_counter_.total_frames());
  EXPECT_EQ(0u, dropped_counter_.total_partial());
  EXPECT_EQ(0u, dropped_counter_.total_dropped());

  reporting_controller_.ResetReporters();
  reporting_controller_.SetDroppedFrameCounter(nullptr);
}

TEST_F(CompositorFrameReportingControllerTest,
       SkippedFramesFromDisplayCompositorAreDroppedUpToLimit) {
  // Submit and present two compositor frames.
  SimulatePresentCompositorFrame();
  EXPECT_EQ(1u, dropped_counter_.total_frames());
  EXPECT_EQ(0u, dropped_counter_.total_partial());
  EXPECT_EQ(0u, dropped_counter_.total_dropped());

  SimulatePresentCompositorFrame();
  EXPECT_EQ(2u, dropped_counter_.total_frames());
  EXPECT_EQ(0u, dropped_counter_.total_partial());
  EXPECT_EQ(0u, dropped_counter_.total_dropped());

  // Now skip over a 101 frames (It should be ignored as it more than 100)
  // and submit + present another frame.
  const uint32_t kSkipFrames = 101;
  const uint32_t kSkipFramesActual = 0;
  for (uint32_t i = 0; i < kSkipFrames; ++i)
    IncrementCurrentId();
  SimulatePresentCompositorFrame();
  EXPECT_EQ(3u + kSkipFramesActual, dropped_counter_.total_frames());
  EXPECT_EQ(0u, dropped_counter_.total_partial());
  EXPECT_EQ(kSkipFramesActual, dropped_counter_.total_dropped());
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
  EXPECT_EQ(0u, dropped_counter_.total_dropped());
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
  EXPECT_EQ(0u, dropped_counter_.total_partial());
  EXPECT_EQ(0u, dropped_counter_.total_dropped());
  EXPECT_EQ(0u, dropped_counter_.total_frames());

  reporting_controller_.BeginMainFrameAborted(
      args_1.frame_id, CommitEarlyOutReason::kFinishedNoUpdates);
  reporting_controller_.DidNotProduceFrame(args_1.frame_id,
                                           FrameSkippedReason::kNoDamage);
  EXPECT_EQ(0u, dropped_counter_.total_dropped());

  // New reporters replace older reporters
  reporting_controller_.WillBeginImplFrame(args_4);
  reporting_controller_.WillBeginMainFrame(args_4);

  EXPECT_EQ(4u, dropped_counter_.total_frames());
  EXPECT_EQ(0u, dropped_counter_.total_partial());
  EXPECT_EQ(0u, dropped_counter_.total_dropped());
}

TEST_F(CompositorFrameReportingControllerTest,
       SkippedFramesFromDisplayCompositorHaveSmoothThread) {
  auto thread_type_compositor = SmoothEffectDrivingThread::kCompositor;
  reporting_controller_.SetThreadAffectsSmoothness(thread_type_compositor,
                                                   true);
  dropped_counter_.OnFcpReceived();

  // Submit and present two compositor frames.
  SimulatePresentCompositorFrame();
  EXPECT_EQ(1u, dropped_counter_.total_frames());
  EXPECT_EQ(0u, dropped_counter_.total_partial());
  EXPECT_EQ(0u, dropped_counter_.total_dropped());

  SimulatePresentCompositorFrame();
  EXPECT_EQ(2u, dropped_counter_.total_frames());
  EXPECT_EQ(0u, dropped_counter_.total_partial());
  EXPECT_EQ(0u, dropped_counter_.total_dropped());

  // Now skip over a few frames, and submit + present another frame.
  const uint32_t kSkipFrames_1 = 5;
  for (uint32_t i = 0; i < kSkipFrames_1; ++i)
    IncrementCurrentId();
  SimulatePresentCompositorFrame();
  EXPECT_EQ(3u + kSkipFrames_1, dropped_counter_.total_frames());
  EXPECT_EQ(0u, dropped_counter_.total_partial());
  EXPECT_EQ(kSkipFrames_1, dropped_counter_.total_dropped());
  EXPECT_EQ(kSkipFrames_1, dropped_counter_.total_smoothness_dropped());

  // Now skip over a few frames which are not affecting smoothness.
  reporting_controller_.SetThreadAffectsSmoothness(thread_type_compositor,
                                                   false);
  const uint32_t kSkipFrames_2 = 7;
  for (uint32_t i = 0; i < kSkipFrames_2; ++i)
    IncrementCurrentId();
  SimulatePresentCompositorFrame();  // Present another frame.
  EXPECT_EQ(4u + kSkipFrames_1 + kSkipFrames_2,
            dropped_counter_.total_frames());
  EXPECT_EQ(0u, dropped_counter_.total_partial());
  EXPECT_EQ(kSkipFrames_1 + kSkipFrames_2, dropped_counter_.total_dropped());
  EXPECT_EQ(kSkipFrames_1, dropped_counter_.total_smoothness_dropped());

  // Now skip over a few frames more frames which are affecting smoothness.
  reporting_controller_.SetThreadAffectsSmoothness(thread_type_compositor,
                                                   true);
  const uint32_t kSkipFrames_3 = 10;
  for (uint32_t i = 0; i < kSkipFrames_3; ++i)
    IncrementCurrentId();
  SimulatePresentCompositorFrame();  // Present another frame.
  EXPECT_EQ(5u + kSkipFrames_1 + kSkipFrames_2 + kSkipFrames_3,
            dropped_counter_.total_frames());
  EXPECT_EQ(0u, dropped_counter_.total_partial());
  EXPECT_EQ(kSkipFrames_1 + kSkipFrames_2 + kSkipFrames_3,
            dropped_counter_.total_dropped());
  EXPECT_EQ(kSkipFrames_1 + kSkipFrames_3,
            dropped_counter_.total_smoothness_dropped());
}

TEST_F(CompositorFrameReportingControllerTest,
       SkippedFramesFromClientRequestedThrottlingAreDropped) {
  // Submit and present two compositor frames.
  SimulatePresentCompositorFrame();
  EXPECT_EQ(1u, dropped_counter_.total_frames());
  EXPECT_EQ(0u, dropped_counter_.total_partial());
  EXPECT_EQ(0u, dropped_counter_.total_dropped());

  SimulatePresentCompositorFrame();
  EXPECT_EQ(2u, dropped_counter_.total_frames());
  EXPECT_EQ(0u, dropped_counter_.total_partial());
  EXPECT_EQ(0u, dropped_counter_.total_dropped());

  // Now skip over a few frames, and submit + present another frame.
  const uint32_t kTotalFrames = 5;
  const uint64_t kThrottledFrames = 4;
  for (uint32_t i = 0; i < kTotalFrames; ++i)
    IncrementCurrentId();
  args_.frames_throttled_since_last = kThrottledFrames;
  SimulatePresentCompositorFrame();
  EXPECT_EQ(3u + kTotalFrames - kThrottledFrames,
            dropped_counter_.total_frames());
  EXPECT_EQ(0u, dropped_counter_.total_partial());
  EXPECT_EQ(kTotalFrames - kThrottledFrames, dropped_counter_.total_dropped());
}

TEST_F(CompositorFrameReportingControllerTest,
       DroppedFrameCountOnMainFrameAbort) {
  // Start a few begin-main-frames, but abort the main-frames due to no damage.
  for (int i = 0; i < 5; ++i) {
    SimulateBeginImplFrame();
    SimulateBeginMainFrame();
    reporting_controller_.OnFinishImplFrame(current_id_);
    reporting_controller_.BeginMainFrameAborted(
        current_id_, CommitEarlyOutReason::kFinishedNoUpdates);
  }
  EXPECT_EQ(0u, dropped_counter_.total_dropped());

  // Start a few begin-main-frames, but abort the main-frames due to no damage.
  for (int i = 0; i < 5; ++i) {
    SimulateBeginImplFrame();
    SimulateBeginMainFrame();
    reporting_controller_.OnFinishImplFrame(current_id_);
    reporting_controller_.BeginMainFrameAborted(
        current_id_, CommitEarlyOutReason::kAbortedDeferredCommit);
    SimulateSubmitCompositorFrame({});
  }
  SimulatePresentCompositorFrame();
  EXPECT_EQ(5u, dropped_counter_.total_dropped());
}

// Verifies that presentation feedbacks that arrive out of order are handled
// properly. See crbug.com/1195105 for more details.
TEST_F(CompositorFrameReportingControllerTest,
       HandleOutOfOrderPresentationFeedback) {
  // Submit three compositor frames without sending back their presentation
  // feedbacks.
  SimulateSubmitCompositorFrame({});

  SimulateSubmitCompositorFrame({});
  const uint32_t frame_token_2 = *current_token_;

  SimulateSubmitCompositorFrame({});
  const uint32_t frame_token_3 = *current_token_;

  // Send a failed presentation feedback for frame 2. This should only drop
  // frame 2 and leave frame 1 in the queue.
  viz::FrameTimingDetails details_2;
  details_2.presentation_feedback = {AdvanceNowByMs(10), base::TimeDelta(),
                                     gfx::PresentationFeedback::kFailure};
  reporting_controller_.DidPresentCompositorFrame(frame_token_2, details_2);
  DCHECK_EQ(1u, dropped_counter_.total_frames());
  DCHECK_EQ(1u, dropped_counter_.total_dropped());

  // Send a successful presentation feedback for frame 3. This should drop frame
  // 1.
  viz::FrameTimingDetails details_3;
  details_3.presentation_feedback.timestamp = AdvanceNowByMs(10);
  reporting_controller_.DidPresentCompositorFrame(frame_token_3, details_3);
  DCHECK_EQ(3u, dropped_counter_.total_frames());
  DCHECK_EQ(2u, dropped_counter_.total_dropped());
}

TEST_F(CompositorFrameReportingControllerTest,
       NewMainThreadUpdateNotReportedAsDropped) {
  auto thread_type_main = SmoothEffectDrivingThread::kMain;
  reporting_controller_.SetThreadAffectsSmoothness(thread_type_main,
                                                   /*affects_smoothness=*/true);
  dropped_counter_.OnFcpReceived();
  dropped_counter_.SetTimeFcpReceivedForTesting(args_.frame_time);

  SimulateBeginMainFrame();
  reporting_controller_.OnFinishImplFrame(current_id_);
  SubmitInfo submit_info = {1u, AdvanceNowByMs(10)};
  reporting_controller_.DidSubmitCompositorFrame(submit_info, current_id_, {});
  viz::FrameTimingDetails details = {};
  details.presentation_feedback.timestamp = AdvanceNowByMs(10);
  reporting_controller_.DidPresentCompositorFrame(1u, details);
  // Starts a new frame and submit it prior to commit

  SimulateCommit(nullptr);

  const auto previous_id = current_id_;

  SimulateBeginMainFrame();
  DCHECK_NE(previous_id, current_id_);
  reporting_controller_.OnFinishImplFrame(current_id_);

  // Starts a new frame and submit it prior to its commit, but the older frame
  // has new updates which would be activated and submitted now.
  reporting_controller_.WillActivate();
  reporting_controller_.DidActivate();

  SubmitInfo submit_info2 = {2u, AdvanceNowByMs(10)};
  reporting_controller_.DidSubmitCompositorFrame(submit_info2, current_id_,
                                                 previous_id);
  details.presentation_feedback.timestamp = AdvanceNowByMs(10);
  reporting_controller_.DidPresentCompositorFrame(2u, details);

  SimulateCommit(nullptr);
  SimulatePresentCompositorFrame();

  // There are two frames with partial updates
  EXPECT_EQ(2u, dropped_counter_.total_partial());
  // Which one is accompanied with new main thread update so only one affects
  // smoothness
  EXPECT_EQ(1u, dropped_counter_.total_smoothness_dropped());
}

TEST_F(CompositorFrameReportingControllerTest,
       NoUpdateCompositorWithJankyMain) {
  reporting_controller_.SetThreadAffectsSmoothness(
      SmoothEffectDrivingThread::kCompositor, /*affects_smoothness=*/true);
  reporting_controller_.SetThreadAffectsSmoothness(
      SmoothEffectDrivingThread::kMain, /*affects_smoothness=*/false);

  dropped_counter_.OnFcpReceived();
  dropped_counter_.SetTimeFcpReceivedForTesting(args_.frame_time);

  // Start a new frame and take it all the way to start the frame on the main
  // thread (i.e. 'begin main frame').
  SimulateBeginMainFrame();
  EXPECT_EQ(1, reporting_controller_.ActiveReporters());
  EXPECT_EQ(0u, dropped_counter_.total_frames());

  // Terminate the frame without submitting a frame.
  reporting_controller_.OnFinishImplFrame(current_id_);
  reporting_controller_.DidNotProduceFrame(current_id_,
                                           FrameSkippedReason::kWaitingOnMain);
  EXPECT_EQ(0u, dropped_counter_.total_frames());

  // Main thread responds.
  SimulateActivate();
  EXPECT_EQ(1, reporting_controller_.ActiveReporters());
  EXPECT_EQ(0u, dropped_counter_.total_frames());

  // Start and submit a second frame.
  SimulateBeginImplFrame();
  EXPECT_EQ(2, reporting_controller_.ActiveReporters());
  EXPECT_EQ(0u, dropped_counter_.total_frames());

  reporting_controller_.OnFinishImplFrame(current_id_);
  SimulatePresentCompositorFrame();
  EXPECT_EQ(0u, dropped_counter_.total_smoothness_dropped());
  EXPECT_EQ(3u, dropped_counter_.total_frames());
}

TEST_F(CompositorFrameReportingControllerTest, MainFrameBeforeCommit) {
  viz::BeginFrameArgs args1 = SimulateBeginFrameArgs({1, 1});
  viz::BeginFrameArgs args2 = SimulateBeginFrameArgs({1, 2});
  viz::BeginFrameArgs args3 = SimulateBeginFrameArgs({1, 3});
  viz::BeginFrameArgs args4 = SimulateBeginFrameArgs({1, 4});

  // Frame 1
  reporting_controller_.WillBeginImplFrame(args1);
  reporting_controller_.WillBeginMainFrame(args1);
  reporting_controller_.NotifyReadyToCommit(nullptr);
  // Frame 1 is ready to commit, so we can pipeline frame 2.
  reporting_controller_.WillBeginImplFrame(args2);
  reporting_controller_.WillBeginMainFrame(args2);
  EXPECT_EQ(2, reporting_controller_.ActiveReporters());
  EXPECT_TRUE(reporting_controller_.HasReporterAt(
      CompositorFrameReportingController::PipelineStage::kBeginMainFrame));
  EXPECT_TRUE(reporting_controller_.HasReporterAt(
      CompositorFrameReportingController::PipelineStage::kReadyToCommit));

  // Commit frame 1
  reporting_controller_.WillCommit();
  reporting_controller_.DidCommit();
  // Frame 2 ready to commit
  reporting_controller_.NotifyReadyToCommit(nullptr);
  reporting_controller_.WillBeginImplFrame(args3);
  EXPECT_EQ(3, reporting_controller_.ActiveReporters());
  // Pipeline frame 3
  reporting_controller_.WillBeginMainFrame(args3);
  EXPECT_EQ(3, reporting_controller_.ActiveReporters());
  EXPECT_TRUE(reporting_controller_.HasReporterAt(
      CompositorFrameReportingController::PipelineStage::kBeginMainFrame));
  EXPECT_TRUE(reporting_controller_.HasReporterAt(
      CompositorFrameReportingController::PipelineStage::kReadyToCommit));
  EXPECT_TRUE(reporting_controller_.HasReporterAt(
      CompositorFrameReportingController::PipelineStage::kCommit));

  // Activate frame 1
  reporting_controller_.WillActivate();
  reporting_controller_.DidActivate();
  // Commit frame 2
  reporting_controller_.WillCommit();
  reporting_controller_.DidCommit();
  // Frame 3 ready to commit
  reporting_controller_.NotifyReadyToCommit(nullptr);
  reporting_controller_.WillBeginImplFrame(args4);
  EXPECT_EQ(4, reporting_controller_.ActiveReporters());
  // Pipeline frame 4
  reporting_controller_.WillBeginMainFrame(args4);
  EXPECT_EQ(4, reporting_controller_.ActiveReporters());
  EXPECT_TRUE(reporting_controller_.HasReporterAt(
      CompositorFrameReportingController::PipelineStage::kBeginMainFrame));
  EXPECT_TRUE(reporting_controller_.HasReporterAt(
      CompositorFrameReportingController::PipelineStage::kReadyToCommit));
  EXPECT_TRUE(reporting_controller_.HasReporterAt(
      CompositorFrameReportingController::PipelineStage::kCommit));
  EXPECT_TRUE(reporting_controller_.HasReporterAt(
      CompositorFrameReportingController::PipelineStage::kActivate));
}

// Glossary of acronyms used in the tests below.
// AMF - Activate Main Frame
// AbMF - AbortMainFrame
// BF - Begin Impl Frame
// BMF - Begin Main Frame
// CMF - Commit Main Frame
// PF - PresentFrame
// SF - Submit Compositor Frame
//
// This test verifies a compositor scroll scenario where the reporter
// termination is not in order, but we still expect the scroll jank tracker to
// receive the presentation data in order since it gets notified when controller
// receives DidPresentCompositorFrame.
//
// |          R1main           |
// | R1impl | R2impl | R3impl  |
//          | R2main   | (aborted on main)
//                   | R3main         |
//
// The order of events (using the glossary above) is:
// BF1->BMF1->SF1->PF1->BF2->CMF1->BMF2->SF2->AMF1->AbMF2->
// BF3->BMF3->PF2->SF(3+1)->PF(3+1)
TEST_F(CompositorFrameReportingControllerTest,
       ScrollJankMetricsPresentationOrderAbortedMain) {
  base::test::TestTraceProcessor ttp;
  ttp.StartTrace("input");

  std::unique_ptr<EventMetrics> metrics_1 = CreateScrollUpdateEventMetrics(
      ui::ScrollInputType::kWheel, /*is_inertial=*/false,
      ScrollUpdateEventMetrics::ScrollUpdateType::kStarted, std::nullopt);

  std::unique_ptr<EventMetrics> metrics_2 = CreateScrollUpdateEventMetrics(
      ui::ScrollInputType::kWheel, /*is_inertial=*/false,
      ScrollUpdateEventMetrics::ScrollUpdateType::kContinued, std::nullopt);

  std::unique_ptr<EventMetrics> metrics_3 = CreateScrollUpdateEventMetrics(
      ui::ScrollInputType::kWheel, /*is_inertial=*/false,
      ScrollUpdateEventMetrics::ScrollUpdateType::kContinued, std::nullopt);

  SimulateBeginImplFrame();  // BF1
  viz::BeginFrameId bf1_id = current_id_;
  SimulateBeginMainFrame();  // BMF1
  reporting_controller_.OnFinishImplFrame(current_id_);

  // Submit a partial update with update from R1impl.
  EventMetrics::List metrics_list_1;
  metrics_list_1.push_back(std::move(metrics_1));
  ++current_token_;
  SubmitInfo submit_info = {*current_token_, AdvanceNowByMs(10)};
  submit_info.events_metrics = {{}, std::move(metrics_list_1)};
  reporting_controller_.DidSubmitCompositorFrame(submit_info, current_id_,
                                                 {});  // SF1

  // Present the frame with R1impl.
  viz::FrameTimingDetails details_1 = {};
  details_1.presentation_feedback.timestamp = AdvanceNowByMs(10);
  reporting_controller_.DidPresentCompositorFrame(*current_token_,
                                                  details_1);  // PF1

  SimulateBeginImplFrame();  // BF2
  viz::BeginFrameId bf2_id = current_id_;
  reporting_controller_.OnFinishImplFrame(current_id_);
  SimulateCommit(nullptr);   // CMF1
  SimulateBeginMainFrame();  // BMF2

  // Submit partial update containing R2impl, R1main update is only committed
  // yet not activated, so it doesn't go into the frame.
  EventMetrics::List metrics_list_2;
  metrics_list_2.push_back(std::move(metrics_2));
  ++current_token_;
  SubmitInfo submit_info2 = {*current_token_, AdvanceNowByMs(10)};
  submit_info.events_metrics = {{}, std::move(metrics_list_2)};
  reporting_controller_.DidSubmitCompositorFrame(submit_info2, current_id_,
                                                 {});  // SF2
  SimulateActivate();  // AMF1

  // R2main is aborted with no updates desired.
  reporting_controller_.BeginMainFrameAborted(
      bf2_id, CommitEarlyOutReason::kFinishedNoUpdates);  // AbMF2

  SimulateBeginImplFrame();  // BF3
  reporting_controller_.OnFinishImplFrame(current_id_);
  // Begin main frame 3, this replaces the R2main in controller and terminates
  // the reporter. So R2impl won't get adopted by R2main.
  SimulateBeginMainFrame();  // BMF3

  // Present the frame containing R2impl.
  // R2impl gets terminated here immediately and reports its metrics to scroll
  // jank tracker.
  viz::FrameTimingDetails details_2 = {};
  details_2.presentation_feedback.timestamp = AdvanceNowByMs(10);
  reporting_controller_.DidPresentCompositorFrame(*current_token_,
                                                  details_2);  // PF2

  // Submit frame containing R1main and R3impl.
  ++current_token_;
  SubmitInfo submit_info3 = {*current_token_, AdvanceNowByMs(10)};
  reporting_controller_.DidSubmitCompositorFrame(submit_info3, current_id_,
                                                 bf1_id);  // SF(3+1)

  // Present frame containing R1main and R3impl.
  // This is where R1impl will be terminated as well, since it got adopted by
  // R1main.
  viz::FrameTimingDetails details_3 = {};
  details_3.presentation_feedback.timestamp = AdvanceNowByMs(10);
  reporting_controller_.DidPresentCompositorFrame(*current_token_,
                                                  details_3);  // PF(3+1)

  absl::Status status = ttp.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();
  std::string query =
      R"(
      SELECT count(*) as cnt from slice
      where name = 'OutOfOrderTerminatedFrame'
      )";
  auto result = ttp.RunQuery(query);
  ASSERT_TRUE(result.has_value()) << result.error();
  // Even though R2impl gets terminated before R1impl, but we still expect the
  // scroll jank metrics to be reported in order.
  EXPECT_THAT(result.value(),
              ::testing::ElementsAre(std::vector<std::string>{"cnt"},
                                     std::vector<std::string>{"0"}));
}

// This test verifies a main thread scroll scenario where a frame with
// compositor only update gets dropped, and the events should end up in the long
// running main reporter. So we expect the scroll jank dropped frame tracker
// receives data only for the one presented frame corresponding to the long
// running main thread update.
//
// |          R1main           |
// | R1impl | R2impl | R3impl  |
//
// The order of events (using the glossary above) is:
// BF1->BMF1->SF1->PF1->BF2->SF2->PF2(dropped)->AMF1->BF3->SF(3+1)->PF(3+1)
TEST_F(CompositorFrameReportingControllerTest,
       ScrollJankMetricsPresentationOrderDroppedPartialOnMainScroll) {
  base::HistogramTester histogram_tester;

  base::test::TestTraceProcessor ttp;
  ttp.StartTrace("input");

  std::unique_ptr<EventMetrics> metrics_1 = CreateScrollUpdateEventMetrics(
      ui::ScrollInputType::kWheel, /*is_inertial=*/false,
      ScrollUpdateEventMetrics::ScrollUpdateType::kStarted, std::nullopt);
  metrics_1->set_requires_main_thread_update();

  std::unique_ptr<EventMetrics> metrics_2 = CreateScrollUpdateEventMetrics(
      ui::ScrollInputType::kWheel, /*is_inertial=*/false,
      ScrollUpdateEventMetrics::ScrollUpdateType::kContinued, std::nullopt);
  metrics_2->set_requires_main_thread_update();

  SimulateBeginImplFrame();  // BF1
  viz::BeginFrameId bf1_id = current_id_;
  SimulateBeginMainFrame();  // BMF1
  reporting_controller_.OnFinishImplFrame(current_id_);

  // Submit a partial update including only main update from R1impl.
  EventMetrics::List metrics_list_1;
  metrics_list_1.push_back(std::move(metrics_1));
  ++current_token_;
  SubmitInfo submit_info = {*current_token_, AdvanceNowByMs(10)};
  submit_info.events_metrics = {{}, std::move(metrics_list_1)};
  reporting_controller_.DidSubmitCompositorFrame(submit_info, current_id_,
                                                 {});  // SF1

  // Present the partial update.
  viz::FrameTimingDetails details_1 = {};
  details_1.presentation_feedback.timestamp = AdvanceNowByMs(10);
  reporting_controller_.DidPresentCompositorFrame(*current_token_,
                                                  details_1);  // PF1

  SimulateBeginImplFrame();  // BF2
  reporting_controller_.OnFinishImplFrame(current_id_);

  // Submit a partial update including only main update from R2impl.
  EventMetrics::List metrics_list_2;
  metrics_list_2.push_back(std::move(metrics_2));
  ++current_token_;
  SubmitInfo submit_info2 = {*current_token_, AdvanceNowByMs(10)};
  submit_info.events_metrics = {{}, std::move(metrics_list_2)};
  reporting_controller_.DidSubmitCompositorFrame(submit_info2, current_id_,
                                                 {});  // SF2

  // The frame containing R2impl update got dropped.
  viz::FrameTimingDetails details_2 = {};
  details_2.presentation_feedback.timestamp = AdvanceNowByMs(10);
  details_2.presentation_feedback.flags |= gfx::PresentationFeedback::kFailure;
  reporting_controller_.DidPresentCompositorFrame(*current_token_,
                                                  details_2);  // PF2

  // Main thread commits and the tree is activated.
  SimulateCommit(nullptr);
  SimulateActivate();  // AMF1

  SimulateBeginImplFrame();  // BF3
  reporting_controller_.OnFinishImplFrame(current_id_);

  // Submit the frame containing updates from R1main and R3impl.
  ++current_token_;
  SubmitInfo submit_info3 = {*current_token_, AdvanceNowByMs(10)};
  reporting_controller_.DidSubmitCompositorFrame(submit_info3, current_id_,
                                                 bf1_id);  // SF(3+1)

  // Present frame containing update from R1main and R3impl.
  viz::FrameTimingDetails details_3 = {};
  details_3.presentation_feedback.timestamp = AdvanceNowByMs(10);
  reporting_controller_.DidPresentCompositorFrame(*current_token_,
                                                  details_3);  // PF(3+1)

  absl::Status status = ttp.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();
  std::string query =
      R"(
      SELECT count(*) as cnt from slice
      where name = 'OutOfOrderTerminatedFrame'
      )";
  auto result = ttp.RunQuery(query);
  ASSERT_TRUE(result.has_value()) << result.error();
  // R1main should ideally have gotten both the events i.e. from R1impl and
  // R2impl, so we wouldn't expect anything to be out of order with just 1
  // reporter having both inputs.
  EXPECT_THAT(result.value(),
              ::testing::ElementsAre(std::vector<std::string>{"cnt"},
                                     std::vector<std::string>{"0"}));

  histogram_tester.ExpectTotalCount("Event.ScrollJank.MissedVsyncs.PerFrame",
                                    1);
}

// This test verifies events from a dropped impl reporter gets added to
// corresponding impl reporter only.
//
// |     R1main      |
// | R1impl | R2impl |
//
// The order of events (using the glossary above) is:
// BF1->BMF1->SF1->PF1(dropped)->BF2->AMF1->SF(2+1)->PF(2+1)
TEST_F(CompositorFrameReportingControllerTest,
       ScrollJankMetricsPresentationOrderDroppedPartialOnImplScroll) {
  base::HistogramTester histogram_tester;

  base::test::TestTraceProcessor ttp;
  ttp.StartTrace("input");

  std::unique_ptr<EventMetrics> metrics_1 = CreateScrollUpdateEventMetrics(
      ui::ScrollInputType::kWheel, /*is_inertial=*/false,
      ScrollUpdateEventMetrics::ScrollUpdateType::kStarted, std::nullopt);

  std::unique_ptr<EventMetrics> metrics_2 = CreateScrollUpdateEventMetrics(
      ui::ScrollInputType::kWheel, /*is_inertial=*/false,
      ScrollUpdateEventMetrics::ScrollUpdateType::kContinued, std::nullopt);

  SimulateBeginImplFrame();  // BF1
  viz::BeginFrameId bf1_id = current_id_;
  SimulateBeginMainFrame();  // BMF1
  reporting_controller_.OnFinishImplFrame(current_id_);

  // Submit a partial update including only main update from R1impl.
  EventMetrics::List metrics_list_1;
  metrics_list_1.push_back(std::move(metrics_1));
  ++current_token_;
  SubmitInfo submit_info = {*current_token_, AdvanceNowByMs(10)};
  submit_info.events_metrics = {{}, std::move(metrics_list_1)};
  reporting_controller_.DidSubmitCompositorFrame(submit_info, current_id_,
                                                 {});  // SF1

  // Frame 1 is dropped.
  viz::FrameTimingDetails details_1 = {};
  details_1.presentation_feedback.timestamp = AdvanceNowByMs(10);
  details_1.presentation_feedback.flags |= gfx::PresentationFeedback::kFailure;
  reporting_controller_.DidPresentCompositorFrame(*current_token_,
                                                  details_1);  // PF1(dropped)

  SimulateBeginImplFrame();  // BF2
  reporting_controller_.OnFinishImplFrame(current_id_);
  SimulateCommit(nullptr);
  SimulateActivate();  // AMF1

  // Submit a partial update including only main update from R2impl.
  EventMetrics::List metrics_list_2;
  metrics_list_2.push_back(std::move(metrics_2));
  ++current_token_;
  SubmitInfo submit_info2 = {*current_token_, AdvanceNowByMs(10)};
  submit_info2.events_metrics = {{}, std::move(metrics_list_2)};
  reporting_controller_.DidSubmitCompositorFrame(submit_info2, current_id_,
                                                 bf1_id);  // SF(2+1)

  // The frame containing R2impl and R1main is presented.
  viz::FrameTimingDetails details_2 = {};
  details_2.presentation_feedback.timestamp = AdvanceNowByMs(10);
  reporting_controller_.DidPresentCompositorFrame(*current_token_,
                                                  details_2);  // PF(2+1)

  absl::Status status = ttp.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();
  std::string query =
      R"(
      SELECT count(*) as cnt from slice
      where name = 'OutOfOrderTerminatedFrame'
      )";
  auto result = ttp.RunQuery(query);
  ASSERT_TRUE(result.has_value()) << result.error();
  // R2impl should get the events from R1impl so we wouldn't expect anything to
  // be out of order with just 1 reporter having both inputs.
  EXPECT_THAT(result.value(),
              ::testing::ElementsAre(std::vector<std::string>{"cnt"},
                                     std::vector<std::string>{"0"}));

  histogram_tester.ExpectTotalCount("Event.ScrollJank.MissedVsyncs.PerFrame",
                                    1);
}

// This test verifies the events from dropped frames doesn't get passed to
// adopter if the impl only update was successful. As a result we expect only
// two frames to report scroll jank metrics i.e. the frames corresponding to
// R3impl and R4impl+R2main.
//
// |     R1main      |
// | R1impl | R2impl | R3impl | R4impl |
//          |          R2main          |
//
// The order of events (using the glossary above) is:
// BF1->BMF1->SF1->PF1->AMF1->BF2->BMF2->SF(2+1)->PF(2+1)(dropped)->
// BF3->SF3->PF3->AMF2->BF4->SF(4+2)->PF(4+2)
TEST_F(CompositorFrameReportingControllerTest,
       ScrollJankMetricsPresentationOrderReceivedDroppedEventsAreNotPassed) {
  base::HistogramTester histogram_tester;

  base::test::TestTraceProcessor ttp;
  ttp.StartTrace("input");

  std::unique_ptr<EventMetrics> metrics_1 = CreateScrollUpdateEventMetrics(
      ui::ScrollInputType::kWheel, /*is_inertial=*/false,
      ScrollUpdateEventMetrics::ScrollUpdateType::kStarted, std::nullopt);
  metrics_1->set_requires_main_thread_update();

  std::unique_ptr<EventMetrics> metrics_2 = CreateScrollUpdateEventMetrics(
      ui::ScrollInputType::kWheel, /*is_inertial=*/false,
      ScrollUpdateEventMetrics::ScrollUpdateType::kContinued, std::nullopt);
  metrics_2->set_requires_main_thread_update();

  std::unique_ptr<EventMetrics> metrics_3 = CreateScrollUpdateEventMetrics(
      ui::ScrollInputType::kWheel, /*is_inertial=*/false,
      ScrollUpdateEventMetrics::ScrollUpdateType::kContinued, std::nullopt);
  metrics_3->set_requires_main_thread_update();

  SimulateBeginImplFrame();  // BF1
  viz::BeginFrameId bf1_id = current_id_;
  SimulateBeginMainFrame();  // BMF1
  reporting_controller_.OnFinishImplFrame(current_id_);

  // Submit a partial update containing R1impl.
  EventMetrics::List metrics_list_1;
  metrics_list_1.push_back(std::move(metrics_1));
  ++current_token_;
  SubmitInfo submit_info = {*current_token_, AdvanceNowByMs(10)};
  submit_info.events_metrics = {{}, std::move(metrics_list_1)};
  reporting_controller_.DidSubmitCompositorFrame(submit_info, current_id_,
                                                 {});  // SF1

  // Present frame containing update from R1impl.
  viz::FrameTimingDetails details_1 = {};
  details_1.presentation_feedback.timestamp = AdvanceNowByMs(10);
  reporting_controller_.DidPresentCompositorFrame(*current_token_,
                                                  details_1);  // PF1

  // Commit and activate main update from R1main.
  SimulateCommit(nullptr);
  SimulateActivate();  // AMF1

  SimulateBeginImplFrame();  // BF2
  viz::BeginFrameId bf2_id = current_id_;
  reporting_controller_.OnFinishImplFrame(current_id_);
  SimulateBeginMainFrame();  // BMF2

  // Submit frame containing update from R1main and R2impl.
  EventMetrics::List metrics_list_2;
  metrics_list_2.push_back(std::move(metrics_2));
  ++current_token_;
  SubmitInfo submit_info2 = {*current_token_, AdvanceNowByMs(10)};
  submit_info2.events_metrics = {{}, std::move(metrics_list_2)};
  reporting_controller_.DidSubmitCompositorFrame(submit_info2, current_id_,
                                                 bf1_id);  // SF(2+1)

  // The frame containing R1main and R2impl was dropped.
  viz::FrameTimingDetails details_2 = {};
  details_2.presentation_feedback.timestamp = AdvanceNowByMs(10);
  details_2.presentation_feedback.flags |= gfx::PresentationFeedback::kFailure;
  reporting_controller_.DidPresentCompositorFrame(*current_token_,
                                                  details_2);  // PF(2+1)

  SimulateBeginImplFrame();  // BF3
  reporting_controller_.OnFinishImplFrame(current_id_);

  // Submit partial update containing R3impl.
  EventMetrics::List metrics_list_3;
  metrics_list_3.push_back(std::move(metrics_3));
  ++current_token_;
  SubmitInfo submit_info3 = {*current_token_, AdvanceNowByMs(10)};
  submit_info3.events_metrics = {{}, std::move(metrics_list_3)};
  reporting_controller_.DidSubmitCompositorFrame(submit_info3, current_id_,
                                                 bf1_id);  // SF3

  viz::FrameTimingDetails details_3 = {};
  details_3.presentation_feedback.timestamp = AdvanceNowByMs(10);
  reporting_controller_.DidPresentCompositorFrame(*current_token_,
                                                  details_3);  // PF3

  // Commit and activate main update from R2main.
  SimulateCommit(nullptr);
  SimulateActivate();  // AMF2

  SimulateBeginImplFrame();  // BF4
  reporting_controller_.OnFinishImplFrame(current_id_);

  // Submit frame containing update from R2main and R4impl.
  ++current_token_;
  SubmitInfo submit_info4 = {*current_token_, AdvanceNowByMs(10)};
  reporting_controller_.DidSubmitCompositorFrame(submit_info4, current_id_,
                                                 bf2_id);  // SF(4+2)

  // Present frame containing R2main and R4impl.
  viz::FrameTimingDetails details_4 = {};
  details_4.presentation_feedback.timestamp = AdvanceNowByMs(10);
  reporting_controller_.DidPresentCompositorFrame(*current_token_,
                                                  details_4);  // PF(4+2)

  absl::Status status = ttp.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();
  std::string query =
      R"(
      SELECT count(*) as cnt from slice
      where name = 'OutOfOrderTerminatedFrame'
      )";
  auto result = ttp.RunQuery(query);
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_THAT(result.value(),
              ::testing::ElementsAre(std::vector<std::string>{"cnt"},
                                     std::vector<std::string>{"0"}));

  // Expect reporters R3impl, R2main to report data to scroll jank tracker.
  // R3impl - The events it received from previously dropped frames.
  // R2main - The events associated with the reporter itself.
  histogram_tester.ExpectTotalCount("Event.ScrollJank.MissedVsyncs.PerFrame",
                                    2);
}

TEST_F(CompositorFrameReportingControllerTest, EmitsEventLatencyId) {
  base::test::TestTraceProcessor ttp;
  ttp.StartTrace("input");

  std::unique_ptr<EventMetrics> metrics_1 = CreateScrollUpdateEventMetrics(
      ui::ScrollInputType::kWheel, /*is_inertial=*/false,
      ScrollUpdateEventMetrics::ScrollUpdateType::kStarted,
      base::IdType64<class ui::LatencyInfo>(14));

  std::unique_ptr<EventMetrics> metrics_2 = CreateEventMetrics(
      ui::EventType::kTouchPressed, base::IdType64<class ui::LatencyInfo>(15));

  EventMetrics::List metrics_list_1;
  metrics_list_1.push_back(std::move(metrics_1));
  metrics_list_1.push_back(std::move(metrics_2));
  SimulateSubmitCompositorFrame({{}, std::move(metrics_list_1)});
  SimulatePresentCompositorFrame();

  absl::Status status = ttp.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();
  std::string query =
      R"(
      SELECT count(*) AS cnt
      FROM slice
      WHERE name = 'EventLatency'
      AND (EXTRACT_ARG(arg_set_id, 'event_latency.event_latency_id') = 14
      OR EXTRACT_ARG(arg_set_id, 'event_latency.event_latency_id') = 15)
      )";
  auto result = ttp.RunQuery(query);
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_THAT(result.value(),
              ::testing::ElementsAre(std::vector<std::string>{"cnt"},
                                     std::vector<std::string>{"2"}));
}

/*
Test if we emit is_janky_scrolled_frame argument.
vsync                   v0        v1   v2
                        |    |    |    |
input  GSU1    GSU2    non-GSU
        |       |       |
F1:     |---------------|
F2:             |-----------------|
F3:                     |--------------|
F1 should have is_janky_scrolled_frame set to false while F2 should have it set
to true and F3 should not have a value for the argument.
*/
TEST_F(CompositorFrameReportingControllerTest, JankyScrolledFrameArg) {
  base::test::TestTraceProcessor ttp;
  ttp.StartTrace("input");

  std::unique_ptr<EventMetrics> metrics_1 = CreateScrollUpdateEventMetrics(
      ui::ScrollInputType::kWheel, /*is_inertial=*/false,
      ScrollUpdateEventMetrics::ScrollUpdateType::kStarted, std::nullopt);
  base::TimeTicks event1_generation_ts = metrics_1->GetDispatchStageTimestamp(
      EventMetrics::DispatchStage::kGenerated);

  std::unique_ptr<EventMetrics> metrics_2 = CreateScrollUpdateEventMetrics(
      ui::ScrollInputType::kWheel, /*is_inertial=*/false,
      ScrollUpdateEventMetrics::ScrollUpdateType::kContinued, std::nullopt);
  base::TimeTicks event2_generation_ts = metrics_2->GetDispatchStageTimestamp(
      EventMetrics::DispatchStage::kGenerated);

  std::unique_ptr<EventMetrics> non_scroll_event =
      CreateEventMetrics(ui::EventType::kTouchPressed, std::nullopt);

  base::TimeDelta vsync_interval = event2_generation_ts - event1_generation_ts;
  args_.interval = vsync_interval;

  SimulateBeginImplFrame();  // BF1
  reporting_controller_.OnFinishImplFrame(current_id_);
  EventMetrics::List metrics_list_1;
  metrics_list_1.push_back(std::move(metrics_1));
  SimulateSubmitCompositorFrame({{}, std::move(metrics_list_1)});

  viz::FrameTimingDetails details_1 = {};
  details_1.presentation_feedback.timestamp =
      event1_generation_ts + base::Microseconds(200);
  reporting_controller_.DidPresentCompositorFrame(*current_token_,
                                                  details_1);  // PF1

  SimulateBeginImplFrame();  // BF2
  reporting_controller_.OnFinishImplFrame(current_id_);
  EventMetrics::List metrics_list_2;
  metrics_list_2.push_back(std::move(metrics_2));
  SimulateSubmitCompositorFrame({{}, std::move(metrics_list_2)});

  viz::FrameTimingDetails details_2 = {};
  details_2.presentation_feedback.timestamp =
      event2_generation_ts + base::Microseconds(200) + args_.interval;
  reporting_controller_.DidPresentCompositorFrame(*current_token_,
                                                  details_2);  // PF2

  SimulateBeginImplFrame();  // BF3
  reporting_controller_.OnFinishImplFrame(current_id_);
  EventMetrics::List metrics_list_3;
  metrics_list_3.push_back(std::move(non_scroll_event));
  SimulateSubmitCompositorFrame({{}, std::move(metrics_list_3)});

  viz::FrameTimingDetails details_3 = {};
  details_3.presentation_feedback.timestamp =
      details_2.presentation_feedback.timestamp + args_.interval;
  reporting_controller_.DidPresentCompositorFrame(*current_token_,
                                                  details_3);  // PF3

  absl::Status status = ttp.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();
  constexpr char query[] =
      R"(
      SELECT COUNT(*) AS cnt
      FROM slice
      WHERE name = 'EventLatency'
      AND EXTRACT_ARG(slice.arg_set_id,
             'event_latency.is_janky_scrolled_frame') %s
      )";
  auto result = ttp.RunQuery(base::StringPrintf(query, "= FALSE"));
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_THAT(result.value(),
              ::testing::ElementsAre(std::vector<std::string>{"cnt"},
                                     std::vector<std::string>{"1"}));

  result = ttp.RunQuery(base::StringPrintf(query, "= TRUE"));
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_THAT(result.value(),
              ::testing::ElementsAre(std::vector<std::string>{"cnt"},
                                     std::vector<std::string>{"1"}));

  result = ttp.RunQuery(base::StringPrintf(query, "IS NULL"));
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_THAT(result.value(),
              ::testing::ElementsAre(std::vector<std::string>{"cnt"},
                                     std::vector<std::string>{"1"}));
}

// A simple test that ensures the vsync_interval is copied onto the
// EventLatency.
TEST_F(CompositorFrameReportingControllerTest, VsyncIntervalArg) {
  base::test::TestTraceProcessor ttp;
  ttp.StartTrace("input");

  std::unique_ptr<EventMetrics> metrics_1 = CreateScrollUpdateEventMetrics(
      ui::ScrollInputType::kWheel, /*is_inertial=*/false,
      ScrollUpdateEventMetrics::ScrollUpdateType::kStarted, std::nullopt);

  std::unique_ptr<EventMetrics> metrics_2 = CreateScrollUpdateEventMetrics(
      ui::ScrollInputType::kWheel, /*is_inertial=*/false,
      ScrollUpdateEventMetrics::ScrollUpdateType::kContinued, std::nullopt);

  std::unique_ptr<EventMetrics> non_scroll_event =
      CreateEventMetrics(ui::EventType::kTouchPressed, std::nullopt);

  // First BeginFrame with a 32ms interval.
  args_.interval = base::Milliseconds(32);
  SimulateBeginImplFrame();  // BF1
  reporting_controller_.OnFinishImplFrame(current_id_);
  EventMetrics::List metrics_list_1;
  metrics_list_1.push_back(std::move(metrics_1));
  SimulateSubmitCompositorFrame({{}, std::move(metrics_list_1)});

  // Presentation of the first BeginFrame.
  SimulatePresentCompositorFrame();

  // Second BeginFrame with an 8ms interval.
  args_.interval = base::Milliseconds(8);
  SimulateBeginImplFrame();  // BF2
  reporting_controller_.OnFinishImplFrame(current_id_);
  EventMetrics::List metrics_list_2;
  metrics_list_2.push_back(std::move(metrics_2));
  SimulateSubmitCompositorFrame({{}, std::move(metrics_list_2)});

  // Presentation of the second BeginFrame.
  SimulatePresentCompositorFrame();

  // Query and ensure we see both intervals on different events.
  absl::Status status = ttp.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();
  constexpr char kQuery[] =
      R"(
      SELECT
        EXTRACT_ARG(slice.arg_set_id, 'event_latency.vsync_interval_ms') AS interval, COUNT(*) AS cnt
      FROM slice
      WHERE name = 'EventLatency'
      GROUP BY 1
      ORDER BY 1 ASC
      )";
  auto result = ttp.RunQuery(kQuery);
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_THAT(result.value(), ::testing::ElementsAre(
                                  std::vector<std::string>{"interval", "cnt"},
                                  std::vector<std::string>{"8", "1"},
                                  std::vector<std::string>{"32", "1"}));
}


}  // namespace
}  // namespace cc
