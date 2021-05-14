// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/compositor_frame_reporter.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_tick_clock.h"
#include "cc/metrics/compositor_frame_reporting_controller.h"
#include "cc/metrics/dropped_frame_counter.h"
#include "cc/metrics/event_metrics.h"
#include "cc/metrics/total_frame_counter.h"
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
  CompositorFrameReporterTest() : pipeline_reporter_(CreatePipelineReporter()) {
    AdvanceNowByMs(1);
    dropped_frame_counter_.set_total_counter(&total_frame_counter_);
  }

 protected:
  base::TimeTicks AdvanceNowByMs(int advance_ms) {
    test_tick_clock_.Advance(base::TimeDelta::FromMicroseconds(advance_ms));
    return test_tick_clock_.NowTicks();
  }

  base::TimeTicks Now() { return test_tick_clock_.NowTicks(); }

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

  std::unique_ptr<EventMetrics> CreateEventMetrics(
      ui::EventType type,
      absl::optional<EventMetrics::ScrollUpdateType> scroll_update_type,
      absl::optional<ui::ScrollInputType> scroll_input_type) {
    const base::TimeTicks event_time = AdvanceNowByMs(3);
    AdvanceNowByMs(3);
    std::unique_ptr<EventMetrics> metrics = EventMetrics::CreateForTesting(
        type, scroll_update_type, scroll_input_type, event_time,
        &test_tick_clock_);
    if (metrics) {
      AdvanceNowByMs(3);
      metrics->SetDispatchStageTimestamp(
          EventMetrics::DispatchStage::kRendererCompositorStarted);
      AdvanceNowByMs(3);
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

  std::unique_ptr<CompositorFrameReporter> CreatePipelineReporter() {
    auto reporter = std::make_unique<CompositorFrameReporter>(
        ActiveTrackers(), viz::BeginFrameArgs(),
        /*latency_ukm_reporter=*/nullptr,
        /*should_report_metrics=*/true,
        CompositorFrameReporter::SmoothThread::kSmoothBoth,
        FrameSequenceMetrics::ThreadType::kUnknown,
        /*layer_tree_host_id=*/1, &dropped_frame_counter_);
    reporter->set_tick_clock(&test_tick_clock_);
    return reporter;
  }

  // This should be defined before |pipeline_reporter_| so it is created before
  // and destroyed after that.
  base::SimpleTestTickClock test_tick_clock_;

  DroppedFrameCounter dropped_frame_counter_;
  TotalFrameCounter total_frame_counter_;
  std::unique_ptr<CompositorFrameReporter> pipeline_reporter_;
};

TEST_F(CompositorFrameReporterTest, MainFrameAbortedReportingTest) {
  base::HistogramTester histogram_tester;

  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kBeginImplFrameToSendBeginMainFrame,
      Now());
  EXPECT_EQ(0, pipeline_reporter_->stage_history_size_for_testing());

  AdvanceNowByMs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kSendBeginMainFrameToCommit, Now());
  EXPECT_EQ(1, pipeline_reporter_->stage_history_size_for_testing());

  AdvanceNowByMs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kEndActivateToSubmitCompositorFrame,
      Now());
  EXPECT_EQ(2, pipeline_reporter_->stage_history_size_for_testing());

  AdvanceNowByMs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::
          kSubmitCompositorFrameToPresentationCompositorFrame,
      Now());
  EXPECT_EQ(3, pipeline_reporter_->stage_history_size_for_testing());

  AdvanceNowByMs(3);
  pipeline_reporter_->TerminateFrame(
      CompositorFrameReporter::FrameTerminationStatus::kPresentedFrame, Now());
  EXPECT_EQ(4, pipeline_reporter_->stage_history_size_for_testing());

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
  EXPECT_EQ(0, pipeline_reporter_->stage_history_size_for_testing());

  AdvanceNowByMs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kEndCommitToActivation, Now());
  EXPECT_EQ(1, pipeline_reporter_->stage_history_size_for_testing());

  AdvanceNowByMs(2);
  pipeline_reporter_->TerminateFrame(
      CompositorFrameReporter::FrameTerminationStatus::kReplacedByNewReporter,
      Now());
  EXPECT_EQ(2, pipeline_reporter_->stage_history_size_for_testing());

  pipeline_reporter_ = nullptr;
  histogram_tester.ExpectTotalCount("CompositorLatency.Commit", 0);
  histogram_tester.ExpectTotalCount("CompositorLatency.EndCommitToActivation",
                                    0);
}

TEST_F(CompositorFrameReporterTest, SubmittedFrameReportingTest) {
  base::HistogramTester histogram_tester;

  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kActivation, Now());
  EXPECT_EQ(0, pipeline_reporter_->stage_history_size_for_testing());

  AdvanceNowByMs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kEndActivateToSubmitCompositorFrame,
      Now());
  EXPECT_EQ(1, pipeline_reporter_->stage_history_size_for_testing());

  AdvanceNowByMs(2);
  pipeline_reporter_->TerminateFrame(
      CompositorFrameReporter::FrameTerminationStatus::kPresentedFrame, Now());
  EXPECT_EQ(2, pipeline_reporter_->stage_history_size_for_testing());

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
  EXPECT_EQ(0, pipeline_reporter_->stage_history_size_for_testing());

  AdvanceNowByMs(3);
  pipeline_reporter_->StartStage(CompositorFrameReporter::StageType::kCommit,
                                 Now());
  EXPECT_EQ(1, pipeline_reporter_->stage_history_size_for_testing());

  AdvanceNowByMs(2);
  pipeline_reporter_->TerminateFrame(
      CompositorFrameReporter::FrameTerminationStatus::kDidNotPresentFrame,
      Now());
  EXPECT_EQ(2, pipeline_reporter_->stage_history_size_for_testing());

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

  std::unique_ptr<EventMetrics> event_metrics_ptrs[] = {
      CreateEventMetrics(ui::ET_TOUCH_PRESSED, absl::nullopt, absl::nullopt),
      CreateEventMetrics(ui::ET_TOUCH_MOVED, absl::nullopt, absl::nullopt),
      CreateEventMetrics(ui::ET_TOUCH_MOVED, absl::nullopt, absl::nullopt),
  };
  EXPECT_THAT(event_metrics_ptrs, Each(NotNull()));
  EventMetrics::List events_metrics(
      std::make_move_iterator(std::begin(event_metrics_ptrs)),
      std::make_move_iterator(std::end(event_metrics_ptrs)));
  std::vector<base::TimeTicks> event_times = GetEventTimestamps(events_metrics);

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
  pipeline_reporter_->AddEventsMetrics(std::move(events_metrics));

  const base::TimeTicks presentation_time = AdvanceNowByMs(3);
  pipeline_reporter_->TerminateFrame(
      CompositorFrameReporter::FrameTerminationStatus::kPresentedFrame,
      presentation_time);

  pipeline_reporter_ = nullptr;

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

// Tests that when a frame is presented to the user, total scroll event latency
// metrics are reported properly.
TEST_F(CompositorFrameReporterTest,
       EventLatencyScrollTotalForPresentedFrameReported) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<EventMetrics> event_metrics_ptrs[] = {
      CreateEventMetrics(ui::ET_GESTURE_SCROLL_BEGIN, absl::nullopt,
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
  pipeline_reporter_->AddEventsMetrics(std::move(events_metrics));

  AdvanceNowByMs(3);
  viz::FrameTimingDetails viz_breakdown = BuildVizBreakdown();
  pipeline_reporter_->SetVizBreakdown(viz_breakdown);
  pipeline_reporter_->TerminateFrame(
      CompositorFrameReporter::FrameTerminationStatus::kPresentedFrame,
      viz_breakdown.presentation_feedback.timestamp);

  pipeline_reporter_ = nullptr;

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
      viz_breakdown.presentation_feedback.timestamp;
  const base::TimeTicks swap_begin_time = viz_breakdown.swap_timings.swap_start;
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

// Tests that when the frame is not presented to the user, event latency metrics
// are not reported.
TEST_F(CompositorFrameReporterTest,
       EventLatencyForDidNotPresentFrameNotReported) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<EventMetrics> event_metrics_ptrs[] = {
      CreateEventMetrics(ui::ET_TOUCH_PRESSED, absl::nullopt, absl::nullopt),
      CreateEventMetrics(ui::ET_TOUCH_MOVED, absl::nullopt, absl::nullopt),
      CreateEventMetrics(ui::ET_TOUCH_MOVED, absl::nullopt, absl::nullopt),
  };
  EXPECT_THAT(event_metrics_ptrs, Each(NotNull()));
  EventMetrics::List events_metrics(
      std::make_move_iterator(std::begin(event_metrics_ptrs)),
      std::make_move_iterator(std::end(event_metrics_ptrs)));

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
  pipeline_reporter_->AddEventsMetrics(std::move(events_metrics));

  AdvanceNowByMs(3);
  pipeline_reporter_->TerminateFrame(
      CompositorFrameReporter::FrameTerminationStatus::kDidNotPresentFrame,
      Now());

  pipeline_reporter_ = nullptr;

  EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix("EventLaterncy."),
              IsEmpty());
}

// Verifies that partial update dependent queues are working as expected when
// they reach their maximum capacity.
TEST_F(CompositorFrameReporterTest, PartialUpdateDependentQueues) {
  // This constant should match the constant with the same name in
  // compositor_frame_reporter.cc.
  const size_t kMaxOwnedPartialUpdateDependents = 300u;

  // The first three dependent reporters for the front of the queue.
  std::unique_ptr<CompositorFrameReporter> deps[] = {
      CreatePipelineReporter(),
      CreatePipelineReporter(),
      CreatePipelineReporter(),
  };

  // Set `deps[0]` as a dependent of the main reporter and adopt it at the same
  // time. This should enqueue it in both non-owned and owned dependents queues.
  deps[0]->SetPartialUpdateDecider(pipeline_reporter_.get());
  pipeline_reporter_->AdoptReporter(std::move(deps[0]));
  DCHECK_EQ(1u,
            pipeline_reporter_->partial_update_dependents_size_for_testing());
  DCHECK_EQ(
      1u,
      pipeline_reporter_->owned_partial_update_dependents_size_for_testing());

  // Set `deps[1]` as a dependent of the main reporter, but don't adopt it yet.
  // This should enqueue it in non-owned dependents queue only.
  deps[1]->SetPartialUpdateDecider(pipeline_reporter_.get());
  DCHECK_EQ(2u,
            pipeline_reporter_->partial_update_dependents_size_for_testing());
  DCHECK_EQ(
      1u,
      pipeline_reporter_->owned_partial_update_dependents_size_for_testing());

  // Set `deps[2]` as a dependent of the main reporter and adopt it at the same
  // time. This should enqueue it in both non-owned and owned dependents queues.
  deps[2]->SetPartialUpdateDecider(pipeline_reporter_.get());
  pipeline_reporter_->AdoptReporter(std::move(deps[2]));
  DCHECK_EQ(3u,
            pipeline_reporter_->partial_update_dependents_size_for_testing());
  DCHECK_EQ(
      2u,
      pipeline_reporter_->owned_partial_update_dependents_size_for_testing());

  // Now adopt `deps[1]` to enqueue it in the owned dependents queue.
  pipeline_reporter_->AdoptReporter(std::move(deps[1]));
  DCHECK_EQ(3u,
            pipeline_reporter_->partial_update_dependents_size_for_testing());
  DCHECK_EQ(
      3u,
      pipeline_reporter_->owned_partial_update_dependents_size_for_testing());

  // Fill the queues with more dependent reporters until the capacity is
  // reached. After this, the queues should look like this (assuming n equals
  // `kMaxOwnedPartialUpdateDependents`):
  //   Partial Update Dependents:       [0, 1, 2, 3, 4, ..., n-1]
  //   Owned Partial Update Dependents: [0, 2, 1, 3, 4, ..., n-1]
  while (
      pipeline_reporter_->owned_partial_update_dependents_size_for_testing() <
      kMaxOwnedPartialUpdateDependents) {
    std::unique_ptr<CompositorFrameReporter> dependent =
        CreatePipelineReporter();
    dependent->SetPartialUpdateDecider(pipeline_reporter_.get());
    pipeline_reporter_->AdoptReporter(std::move(dependent));
  }
  DCHECK_EQ(kMaxOwnedPartialUpdateDependents,
            pipeline_reporter_->partial_update_dependents_size_for_testing());
  DCHECK_EQ(
      kMaxOwnedPartialUpdateDependents,
      pipeline_reporter_->owned_partial_update_dependents_size_for_testing());

  // Enqueue a new dependent reporter. This should pop `deps[0]` from the front
  // of the owned dependents queue and destroy it. Since the same one is in
  // front of the non-owned dependents queue, it will be popped out of that
  // queue, too. The queues will look like this:
  //   Partial Update Dependents:       [1, 2, 3, 4, ..., n]
  //   Owned Partial Update Dependents: [2, 1, 3, 4, ..., n]
  auto new_dep = CreatePipelineReporter();
  new_dep->SetPartialUpdateDecider(pipeline_reporter_.get());
  pipeline_reporter_->AdoptReporter(std::move(new_dep));
  DCHECK_EQ(kMaxOwnedPartialUpdateDependents,
            pipeline_reporter_->partial_update_dependents_size_for_testing());
  DCHECK_EQ(
      kMaxOwnedPartialUpdateDependents,
      pipeline_reporter_->owned_partial_update_dependents_size_for_testing());

  // Enqueue another new dependent reporter. This should pop `deps[2]` from the
  // front of the owned dependents queue and destroy it. Since another reporter
  // is in front of the non-owned dependents queue it won't be popped out of
  // that queue. The queues will look like this:
  //   Partial Update Dependents:       [2, 3, 4, ..., n+1]
  //   Owned Partial Update Dependents: [2, nullptr, 3, 4, ..., n+1]
  new_dep = CreatePipelineReporter();
  new_dep->SetPartialUpdateDecider(pipeline_reporter_.get());
  pipeline_reporter_->AdoptReporter(std::move(new_dep));
  DCHECK_EQ(kMaxOwnedPartialUpdateDependents + 1,
            pipeline_reporter_->partial_update_dependents_size_for_testing());
  DCHECK_EQ(
      kMaxOwnedPartialUpdateDependents,
      pipeline_reporter_->owned_partial_update_dependents_size_for_testing());

  // Enqueue yet another new dependent reporter. This should pop `deps[1]` from
  // the front of the owned dependents queue and destroy it. Since the same one
  // is in front of the non-owned dependents queue followed by `deps[2]` which
  // was destroyed in the previous step, they will be popped out of that queue,
  // too. The queues will look like this:
  //   Partial Update Dependents:       [3, 4, ..., n+2]
  //   Owned Partial Update Dependents: [3, 4, ..., n+2]
  new_dep = CreatePipelineReporter();
  new_dep->SetPartialUpdateDecider(pipeline_reporter_.get());
  pipeline_reporter_->AdoptReporter(std::move(new_dep));
  DCHECK_EQ(kMaxOwnedPartialUpdateDependents,
            pipeline_reporter_->partial_update_dependents_size_for_testing());
  DCHECK_EQ(
      kMaxOwnedPartialUpdateDependents,
      pipeline_reporter_->owned_partial_update_dependents_size_for_testing());
}

}  // namespace
}  // namespace cc
