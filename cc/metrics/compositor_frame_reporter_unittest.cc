// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/compositor_frame_reporter.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
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
    AdvanceNowByUs(1);
    dropped_frame_counter_.set_total_counter(&total_frame_counter_);
  }

 protected:
  base::TimeTicks AdvanceNowByUs(int advance_ms) {
    test_tick_clock_.Advance(base::Microseconds(advance_ms));
    return test_tick_clock_.NowTicks();
  }

  base::TimeTicks Now() { return test_tick_clock_.NowTicks(); }

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
    AdvanceNowByUs(10 + 9 + 8 + 7 + 6 + 5 + 3 + 2 + 1);

    return breakdown;
  }

  viz::FrameTimingDetails BuildVizBreakdown() {
    viz::FrameTimingDetails viz_breakdown;
    viz_breakdown.received_compositor_frame_timestamp = AdvanceNowByUs(1);
    viz_breakdown.draw_start_timestamp = AdvanceNowByUs(2);
    viz_breakdown.swap_timings.swap_start = AdvanceNowByUs(3);
    viz_breakdown.swap_timings.swap_end = AdvanceNowByUs(4);
    viz_breakdown.presentation_feedback.timestamp = AdvanceNowByUs(5);
    return viz_breakdown;
  }

  std::unique_ptr<EventMetrics> SetupEventMetrics(
      std::unique_ptr<EventMetrics> metrics) {
    if (metrics) {
      AdvanceNowByUs(3);
      metrics->SetDispatchStageTimestamp(
          EventMetrics::DispatchStage::kRendererCompositorStarted);
      AdvanceNowByUs(3);
      metrics->SetDispatchStageTimestamp(
          EventMetrics::DispatchStage::kRendererCompositorFinished);
    }
    return metrics;
  }

  // Sets up the dispatch durations of each EventMetrics according to
  // stage_durations. Stages with a duration of -1 will be skipped.
  std::unique_ptr<EventMetrics> SetupEventMetricsWithDispatchTimes(
      std::unique_ptr<EventMetrics> metrics,
      const std::vector<int>& stage_durations) {
    if (metrics) {
      int num_stages = stage_durations.size();
      int max_num_stages =
          static_cast<int>(EventMetrics::DispatchStage::kMaxValue) + 1;
      CHECK(num_stages <= max_num_stages)
          << num_stages << " > " << max_num_stages;
      // Start indexing from 2 because the 0th index held duration from
      // kGenerated to kArrivedInRendererCompositor, which was already used in
      // when the EventMetrics was created.
      for (int i = 2; i < num_stages; i++) {
        if (stage_durations[i] >= 0) {
          AdvanceNowByUs(stage_durations[i]);
          metrics->SetDispatchStageTimestamp(
              EventMetrics::DispatchStage(i + 2));
        }
      }
    }
    return metrics;
  }

  std::unique_ptr<EventMetrics> CreateEventMetrics(ui::EventType type) {
    const base::TimeTicks event_time = AdvanceNowByUs(3);
    const base::TimeTicks arrived_in_browser_main_timestamp = AdvanceNowByUs(2);
    AdvanceNowByUs(3);
    return SetupEventMetrics(EventMetrics::CreateForTesting(
        type, event_time, arrived_in_browser_main_timestamp, &test_tick_clock_,
        std::nullopt));
  }

  // Creates EventMetrics with elements in stage_durations representing each
  // dispatch stage's desired duration respectively, with the 0th index
  // representing the duration from kGenerated to kArrivedInRendererCompositor.
  // stage_durations must have at least 1 element for the first required stage
  // Use -1 for stages that want to be skipped.
  std::unique_ptr<EventMetrics> CreateScrollUpdateEventMetricsWithDispatchTimes(
      bool is_inertial,
      ScrollUpdateEventMetrics::ScrollUpdateType scroll_update_type,
      const std::vector<int>& stage_durations) {
    CHECK_GE(stage_durations.size(), 2u);

    const base::TimeTicks event_time = AdvanceNowByUs(3);

    // kGenerated -> kArrivedInBrowserMain
    int begin_rwh_latency_ms = stage_durations[0];
    const base::TimeTicks arrived_in_browser_main_timestamp =
        AdvanceNowByUs(begin_rwh_latency_ms);

    // kArrivedInBrowserMain -> kArrivedInRendererCompositor
    AdvanceNowByUs(stage_durations[1]);

    // Creates a kGestureScrollUpdate event.
    return SetupEventMetricsWithDispatchTimes(
        ScrollUpdateEventMetrics::CreateForTesting(
            ui::EventType::kGestureScrollUpdate, ui::ScrollInputType::kWheel,
            is_inertial, scroll_update_type, /*delta=*/10.0f, event_time,
            arrived_in_browser_main_timestamp, &test_tick_clock_, std::nullopt),
        stage_durations);
  }

  std::unique_ptr<EventMetrics> CreateScrollBeginMetrics(
      ui::ScrollInputType input_type) {
    const base::TimeTicks event_time = AdvanceNowByUs(3);
    const base::TimeTicks arrived_in_browser_main_timestamp = AdvanceNowByUs(2);
    AdvanceNowByUs(3);
    return SetupEventMetrics(ScrollEventMetrics::CreateForTesting(
        ui::EventType::kGestureScrollBegin, input_type,
        /*is_inertial=*/false, event_time, arrived_in_browser_main_timestamp,
        &test_tick_clock_));
  }

  std::unique_ptr<EventMetrics> CreateScrollUpdateEventMetrics(
      ui::ScrollInputType input_type,
      bool is_inertial,
      ScrollUpdateEventMetrics::ScrollUpdateType scroll_update_type) {
    const base::TimeTicks event_time = AdvanceNowByUs(3);
    const base::TimeTicks arrived_in_browser_main_timestamp = AdvanceNowByUs(2);
    AdvanceNowByUs(3);
    return SetupEventMetrics(ScrollUpdateEventMetrics::CreateForTesting(
        ui::EventType::kGestureScrollUpdate, input_type, is_inertial,
        scroll_update_type, /*delta=*/10.0f, event_time,
        arrived_in_browser_main_timestamp, &test_tick_clock_, std::nullopt));
  }

  std::unique_ptr<EventMetrics> CreatePinchEventMetrics(
      ui::EventType type,
      ui::ScrollInputType input_type) {
    const base::TimeTicks event_time = AdvanceNowByUs(3);
    AdvanceNowByUs(3);
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

  std::unique_ptr<CompositorFrameReporter> CreatePipelineReporter() {
    GlobalMetricsTrackers trackers{&dropped_frame_counter_, nullptr, nullptr,
                                   nullptr, nullptr};
    auto reporter = std::make_unique<CompositorFrameReporter>(
        ActiveTrackers(), viz::BeginFrameArgs(),
        /*should_report_metrics=*/true,
        CompositorFrameReporter::SmoothThread::kSmoothBoth,
        FrameInfo::SmoothEffectDrivingThread::kUnknown,
        /*layer_tree_host_id=*/1, trackers);
    reporter->set_tick_clock(&test_tick_clock_);
    return reporter;
  }

  void IntToTimeDeltaVector(std::vector<base::TimeDelta>& timedelta_vector,
                            std::vector<int> int_vector) {
    size_t vector_size = int_vector.size();
    for (size_t i = 0; i < vector_size; i++) {
      timedelta_vector[i] = base::Microseconds(int_vector[i]);
    }
  }

  void VerifyLatencyInfo(
      const CompositorFrameReporter::CompositorLatencyInfo& expected_info,
      const CompositorFrameReporter::CompositorLatencyInfo& actual_info) {
    EXPECT_EQ(expected_info.top_level_stages, actual_info.top_level_stages);
    EXPECT_EQ(expected_info.blink_breakdown_stages,
              actual_info.blink_breakdown_stages);
    EXPECT_EQ(expected_info.viz_breakdown_stages,
              actual_info.viz_breakdown_stages);
    EXPECT_EQ(expected_info.total_latency, actual_info.total_latency);
    EXPECT_EQ(expected_info.total_blink_latency,
              actual_info.total_blink_latency);
    EXPECT_EQ(expected_info.total_viz_latency, actual_info.total_viz_latency);
  }

  // Disable sub-sampling to deterministically record histograms under test.
  base::MetricsSubSampler::ScopedAlwaysSampleForTesting no_subsampling_;

  // This should be defined before |pipeline_reporter_| so it is created before
  // and destroyed after that.
  base::SimpleTestTickClock test_tick_clock_;

  DroppedFrameCounter dropped_frame_counter_;
  TotalFrameCounter total_frame_counter_;
  std::unique_ptr<CompositorFrameReporter> pipeline_reporter_;

  // Number of breakdown stages of the current PipelineReporter
  const int kNumOfCompositorStages =
      static_cast<int>(CompositorFrameReporter::StageType::kStageTypeCount) - 1;
  const int kNumDispatchStages =
      static_cast<int>(EventMetrics::DispatchStage::kMaxValue);
  const base::TimeDelta kLatencyPredictionDeviationThreshold =
      base::Milliseconds(8.33);
};

TEST_F(CompositorFrameReporterTest, MainFrameAbortedReportingTest) {
  base::HistogramTester histogram_tester;

  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kBeginImplFrameToSendBeginMainFrame,
      Now());
  EXPECT_EQ(0u, pipeline_reporter_->stage_history_size_for_testing());

  AdvanceNowByUs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kSendBeginMainFrameToCommit, Now());
  EXPECT_EQ(1u, pipeline_reporter_->stage_history_size_for_testing());

  AdvanceNowByUs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kEndActivateToSubmitCompositorFrame,
      Now());
  EXPECT_EQ(2u, pipeline_reporter_->stage_history_size_for_testing());

  AdvanceNowByUs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::
          kSubmitCompositorFrameToPresentationCompositorFrame,
      Now());
  EXPECT_EQ(3u, pipeline_reporter_->stage_history_size_for_testing());

  AdvanceNowByUs(3);
  pipeline_reporter_->TerminateFrame(
      CompositorFrameReporter::FrameTerminationStatus::kPresentedFrame, Now());
  EXPECT_EQ(4u, pipeline_reporter_->stage_history_size_for_testing());

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
  EXPECT_EQ(0u, pipeline_reporter_->stage_history_size_for_testing());

  AdvanceNowByUs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kEndCommitToActivation, Now());
  EXPECT_EQ(1u, pipeline_reporter_->stage_history_size_for_testing());

  AdvanceNowByUs(2);
  pipeline_reporter_->TerminateFrame(
      CompositorFrameReporter::FrameTerminationStatus::kReplacedByNewReporter,
      Now());
  EXPECT_EQ(2u, pipeline_reporter_->stage_history_size_for_testing());

  pipeline_reporter_ = nullptr;
  histogram_tester.ExpectTotalCount("CompositorLatency.Commit", 0);
  histogram_tester.ExpectTotalCount("CompositorLatency.EndCommitToActivation",
                                    0);
}

TEST_F(CompositorFrameReporterTest, SubmittedFrameReportingTest) {
  base::HistogramTester histogram_tester;

  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kActivation, Now());
  EXPECT_EQ(0u, pipeline_reporter_->stage_history_size_for_testing());

  AdvanceNowByUs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kEndActivateToSubmitCompositorFrame,
      Now());
  EXPECT_EQ(1u, pipeline_reporter_->stage_history_size_for_testing());

  AdvanceNowByUs(2);
  pipeline_reporter_->TerminateFrame(
      CompositorFrameReporter::FrameTerminationStatus::kPresentedFrame, Now());
  EXPECT_EQ(2u, pipeline_reporter_->stage_history_size_for_testing());

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
  EXPECT_EQ(0u, pipeline_reporter_->stage_history_size_for_testing());

  AdvanceNowByUs(3);
  pipeline_reporter_->StartStage(CompositorFrameReporter::StageType::kCommit,
                                 Now());
  EXPECT_EQ(1u, pipeline_reporter_->stage_history_size_for_testing());

  AdvanceNowByUs(2);
  pipeline_reporter_->TerminateFrame(
      CompositorFrameReporter::FrameTerminationStatus::kDidNotPresentFrame,
      Now());
  EXPECT_EQ(2u, pipeline_reporter_->stage_history_size_for_testing());

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
      CreateEventMetrics(ui::EventType::kTouchPressed),
      CreateEventMetrics(ui::EventType::kTouchMoved),
      CreateEventMetrics(ui::EventType::kTouchMoved),
  };
  EXPECT_THAT(event_metrics_ptrs, Each(NotNull()));
  EventMetrics::List events_metrics(
      std::make_move_iterator(std::begin(event_metrics_ptrs)),
      std::make_move_iterator(std::end(event_metrics_ptrs)));
  std::vector<base::TimeTicks> event_times = GetEventTimestamps(events_metrics);

  AdvanceNowByUs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kBeginImplFrameToSendBeginMainFrame,
      Now());

  AdvanceNowByUs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kEndActivateToSubmitCompositorFrame,
      Now());

  AdvanceNowByUs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::
          kSubmitCompositorFrameToPresentationCompositorFrame,
      Now());
  pipeline_reporter_->AddEventsMetrics(std::move(events_metrics));

  const base::TimeTicks presentation_time = AdvanceNowByUs(3);
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

// Tests that when a frame is presented to the user, total scroll event latency
// metrics are reported properly.
TEST_F(CompositorFrameReporterTest,
       EventLatencyScrollTotalForPresentedFrameReported) {
  base::HistogramTester histogram_tester;

  const bool kScrollIsInertial = true;
  const bool kScrollIsNotInertial = false;
  std::unique_ptr<EventMetrics> event_metrics_ptrs[] = {
      CreateScrollBeginMetrics(ui::ScrollInputType::kWheel),
      CreateScrollUpdateEventMetrics(
          ui::ScrollInputType::kWheel, kScrollIsNotInertial,
          ScrollUpdateEventMetrics::ScrollUpdateType::kStarted),
      CreateScrollUpdateEventMetrics(
          ui::ScrollInputType::kWheel, kScrollIsNotInertial,
          ScrollUpdateEventMetrics::ScrollUpdateType::kContinued),
      CreateScrollUpdateEventMetrics(
          ui::ScrollInputType::kWheel, kScrollIsInertial,
          ScrollUpdateEventMetrics::ScrollUpdateType::kContinued),
      CreateScrollBeginMetrics(ui::ScrollInputType::kTouchscreen),
      CreateScrollUpdateEventMetrics(
          ui::ScrollInputType::kTouchscreen, kScrollIsNotInertial,
          ScrollUpdateEventMetrics::ScrollUpdateType::kStarted),
      CreateScrollUpdateEventMetrics(
          ui::ScrollInputType::kTouchscreen, kScrollIsNotInertial,
          ScrollUpdateEventMetrics::ScrollUpdateType::kContinued),
      CreateScrollUpdateEventMetrics(
          ui::ScrollInputType::kTouchscreen, kScrollIsInertial,
          ScrollUpdateEventMetrics::ScrollUpdateType::kContinued),
  };
  EXPECT_THAT(event_metrics_ptrs, Each(NotNull()));
  EventMetrics::List events_metrics(
      std::make_move_iterator(std::begin(event_metrics_ptrs)),
      std::make_move_iterator(std::end(event_metrics_ptrs)));
  std::vector<base::TimeTicks> event_times = GetEventTimestamps(events_metrics);

  AdvanceNowByUs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kBeginImplFrameToSendBeginMainFrame,
      Now());

  AdvanceNowByUs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kEndActivateToSubmitCompositorFrame,
      Now());

  AdvanceNowByUs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::
          kSubmitCompositorFrameToPresentationCompositorFrame,
      Now());
  pipeline_reporter_->AddEventsMetrics(std::move(events_metrics));

  AdvanceNowByUs(3);
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
      viz_breakdown.presentation_feedback.timestamp;
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

// Tests that when a frame is presented to the user, total pinch event latency
// metrics are reported properly.
TEST_F(CompositorFrameReporterTest,
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

  AdvanceNowByUs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kBeginImplFrameToSendBeginMainFrame,
      Now());

  AdvanceNowByUs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kEndActivateToSubmitCompositorFrame,
      Now());

  AdvanceNowByUs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::
          kSubmitCompositorFrameToPresentationCompositorFrame,
      Now());
  pipeline_reporter_->AddEventsMetrics(std::move(events_metrics));

  AdvanceNowByUs(3);
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
      {"EventLatency.GesturePinchBegin.Touchscreen.TotalLatency", 1},
      {"EventLatency.GesturePinchUpdate.Touchscreen.TotalLatency", 1},
      {"EventLatency.GesturePinchBegin.Touchpad.TotalLatency", 1},
      {"EventLatency.GesturePinchUpdate.Touchpad.TotalLatency", 1},
      {"EventLatency.TotalLatency", 4},
  };
  for (const auto& expected_count : expected_counts) {
    histogram_tester.ExpectTotalCount(expected_count.name,
                                      expected_count.count);
  }

  const base::TimeTicks presentation_time =
      viz_breakdown.presentation_feedback.timestamp;
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

// Tests that when the frame is not presented to the user, event latency metrics
// are not reported.
TEST_F(CompositorFrameReporterTest,
       EventLatencyForDidNotPresentFrameNotReported) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<EventMetrics> event_metrics_ptrs[] = {
      CreateEventMetrics(ui::EventType::kTouchPressed),
      CreateEventMetrics(ui::EventType::kTouchMoved),
      CreateEventMetrics(ui::EventType::kTouchMoved),
  };
  EXPECT_THAT(event_metrics_ptrs, Each(NotNull()));
  EventMetrics::List events_metrics(
      std::make_move_iterator(std::begin(event_metrics_ptrs)),
      std::make_move_iterator(std::end(event_metrics_ptrs)));

  AdvanceNowByUs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kBeginImplFrameToSendBeginMainFrame,
      Now());

  AdvanceNowByUs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kEndActivateToSubmitCompositorFrame,
      Now());

  AdvanceNowByUs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::
          kSubmitCompositorFrameToPresentationCompositorFrame,
      Now());
  pipeline_reporter_->AddEventsMetrics(std::move(events_metrics));

  AdvanceNowByUs(3);
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
  // front of the owned dependents queue and destroy it. It should be removed
  // from the non-owned dependents queue as well.
  //   Partial Update Dependents:       [2, 3, 4, ..., n+1]
  //   Owned Partial Update Dependents: [2, 3, 4, ..., n+1]
  new_dep = CreatePipelineReporter();
  new_dep->SetPartialUpdateDecider(pipeline_reporter_.get());
  pipeline_reporter_->AdoptReporter(std::move(new_dep));
  DCHECK_EQ(kMaxOwnedPartialUpdateDependents,
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

TEST_F(CompositorFrameReporterTest, StageLatencyGeneralPrediction) {
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kBeginImplFrameToSendBeginMainFrame,
      Now());

  AdvanceNowByUs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kSendBeginMainFrameToCommit, Now());
  AdvanceNowByUs(4);
  base::TimeTicks begin_main_frame_start_time = Now();
  std::unique_ptr<BeginMainFrameMetrics> blink_breakdown =
      BuildBlinkBreakdown();
  pipeline_reporter_->SetBlinkBreakdown(std::move(blink_breakdown),
                                        begin_main_frame_start_time);

  pipeline_reporter_->StartStage(CompositorFrameReporter::StageType::kCommit,
                                 Now());

  AdvanceNowByUs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kEndCommitToActivation, Now());

  AdvanceNowByUs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kActivation, Now());

  AdvanceNowByUs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kEndActivateToSubmitCompositorFrame,
      Now());

  AdvanceNowByUs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::
          kSubmitCompositorFrameToPresentationCompositorFrame,
      Now());
  viz::FrameTimingDetails viz_breakdown = BuildVizBreakdown();
  pipeline_reporter_->SetVizBreakdown(viz_breakdown);

  pipeline_reporter_->TerminateFrame(
      CompositorFrameReporter::FrameTerminationStatus::kPresentedFrame,
      viz_breakdown.presentation_feedback.timestamp);

  // predictions when this is the very first prediction
  CompositorFrameReporter::CompositorLatencyInfo expected_latency_predictions1;
  expected_latency_predictions1.top_level_stages = {
      base::Microseconds(3), base::Microseconds(55), base::Microseconds(3),
      base::Microseconds(3), base::Microseconds(3),  base::Microseconds(3),
      base::Microseconds(15)};
  expected_latency_predictions1.blink_breakdown_stages = {
      base::Microseconds(10), base::Microseconds(9), base::Microseconds(8),
      base::Microseconds(7),  base::Microseconds(0), base::Microseconds(5),
      base::Microseconds(6),  base::Microseconds(3), base::Microseconds(2),
      base::Microseconds(1),  base::Microseconds(4)};
  expected_latency_predictions1.viz_breakdown_stages = {
      base::Microseconds(1), base::Microseconds(2), base::Microseconds(3),
      base::Microseconds(4), base::Microseconds(0), base::Microseconds(0),
      base::Microseconds(0), base::Microseconds(0), base::Microseconds(5)};
  expected_latency_predictions1.total_latency = base::Microseconds(85);
  expected_latency_predictions1.total_blink_latency = base::Microseconds(55);
  expected_latency_predictions1.total_viz_latency = base::Microseconds(15);

  // predictions when there exists a previous prediction
  CompositorFrameReporter::CompositorLatencyInfo expected_latency_predictions2(
      base::Microseconds(0));
  expected_latency_predictions2.top_level_stages = {
      base::Microseconds(1), base::Microseconds(13), base::Microseconds(3),
      base::Microseconds(0), base::Microseconds(2),  base::Microseconds(3),
      base::Microseconds(3)};
  expected_latency_predictions2.blink_breakdown_stages = {
      base::Microseconds(10), base::Microseconds(9), base::Microseconds(8),
      base::Microseconds(7),  base::Microseconds(0), base::Microseconds(5),
      base::Microseconds(6),  base::Microseconds(3), base::Microseconds(2),
      base::Microseconds(1),  base::Microseconds(4)};
  expected_latency_predictions2.viz_breakdown_stages = {
      base::Microseconds(1), base::Microseconds(2), base::Microseconds(3),
      base::Microseconds(4), base::Microseconds(0), base::Microseconds(0),
      base::Microseconds(0), base::Microseconds(0), base::Microseconds(5)};
  expected_latency_predictions2.total_latency = base::Microseconds(28);
  expected_latency_predictions2.total_blink_latency = base::Microseconds(55);
  expected_latency_predictions2.total_viz_latency = base::Microseconds(15);

  // expected attribution for all 3 cases above
  std::vector<std::string> expected_latency_attributions = {};

  CompositorFrameReporter::CompositorLatencyInfo actual_latency_predictions1(
      base::Microseconds(-1));
  pipeline_reporter_->CalculateCompositorLatencyPrediction(
      actual_latency_predictions1, kLatencyPredictionDeviationThreshold);
  std::vector<std::string> actual_latency_attributions1 =
      pipeline_reporter_->high_latency_substages_for_testing();
  pipeline_reporter_->ClearHighLatencySubstagesForTesting();

  CompositorFrameReporter::CompositorLatencyInfo actual_latency_predictions2(
      base::Microseconds(0));
  actual_latency_predictions2.top_level_stages = {
      base::Microseconds(1), base::Microseconds(0), base::Microseconds(4),
      base::Microseconds(0), base::Microseconds(2), base::Microseconds(3),
      base::Microseconds(0)};
  actual_latency_predictions2.total_latency = base::Microseconds(10);
  pipeline_reporter_->CalculateCompositorLatencyPrediction(
      actual_latency_predictions2, kLatencyPredictionDeviationThreshold);
  std::vector<std::string> actual_latency_attributions2 =
      pipeline_reporter_->high_latency_substages_for_testing();
  pipeline_reporter_->ClearHighLatencySubstagesForTesting();

  VerifyLatencyInfo(expected_latency_predictions1, actual_latency_predictions1);
  VerifyLatencyInfo(expected_latency_predictions2, actual_latency_predictions2);

  EXPECT_EQ(expected_latency_attributions, actual_latency_attributions1);
  EXPECT_EQ(expected_latency_attributions, actual_latency_attributions2);

  pipeline_reporter_ = nullptr;
}

TEST_F(CompositorFrameReporterTest, StageLatencyAllZeroPrediction) {
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kBeginImplFrameToSendBeginMainFrame,
      Now());

  AdvanceNowByUs(0);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kSendBeginMainFrameToCommit, Now());

  AdvanceNowByUs(0);
  pipeline_reporter_->StartStage(CompositorFrameReporter::StageType::kCommit,
                                 Now());

  AdvanceNowByUs(0);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kEndCommitToActivation, Now());

  AdvanceNowByUs(0);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kActivation, Now());

  AdvanceNowByUs(0);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kEndActivateToSubmitCompositorFrame,
      Now());

  AdvanceNowByUs(0);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::
          kSubmitCompositorFrameToPresentationCompositorFrame,
      Now());

  AdvanceNowByUs(0);
  pipeline_reporter_->TerminateFrame(
      CompositorFrameReporter::FrameTerminationStatus::kDidNotProduceFrame,
      Now());

  // predictions when this is the very first prediction
  CompositorFrameReporter::CompositorLatencyInfo expected_latency_predictions1(
      base::Microseconds(-1));

  // predictions when there exists a previous prediction
  CompositorFrameReporter::CompositorLatencyInfo expected_latency_predictions2(
      base::Microseconds(0));
  expected_latency_predictions2.top_level_stages = {
      base::Microseconds(1), base::Microseconds(0), base::Microseconds(4),
      base::Microseconds(0), base::Microseconds(2), base::Microseconds(3),
      base::Microseconds(0)};
  expected_latency_predictions2.total_latency = base::Microseconds(10);

  // expected attribution for all 3 cases above
  std::vector<std::string> expected_latency_attributions = {};

  CompositorFrameReporter::CompositorLatencyInfo actual_latency_predictions1(
      base::Microseconds(-1));
  pipeline_reporter_->CalculateCompositorLatencyPrediction(
      actual_latency_predictions1, kLatencyPredictionDeviationThreshold);
  std::vector<std::string> actual_latency_attributions1 =
      pipeline_reporter_->high_latency_substages_for_testing();
  pipeline_reporter_->ClearHighLatencySubstagesForTesting();

  CompositorFrameReporter::CompositorLatencyInfo actual_latency_predictions2(
      base::Microseconds(0));
  actual_latency_predictions2.top_level_stages = {
      base::Microseconds(1), base::Microseconds(0), base::Microseconds(4),
      base::Microseconds(0), base::Microseconds(2), base::Microseconds(3),
      base::Microseconds(0)};
  actual_latency_predictions2.total_latency = base::Microseconds(10);

  pipeline_reporter_->CalculateCompositorLatencyPrediction(
      actual_latency_predictions2, kLatencyPredictionDeviationThreshold);
  std::vector<std::string> actual_latency_attributions2 =
      pipeline_reporter_->high_latency_substages_for_testing();
  pipeline_reporter_->ClearHighLatencySubstagesForTesting();

  VerifyLatencyInfo(expected_latency_predictions1, actual_latency_predictions1);
  VerifyLatencyInfo(expected_latency_predictions2, actual_latency_predictions2);

  EXPECT_EQ(expected_latency_attributions, actual_latency_attributions1);
  EXPECT_EQ(expected_latency_attributions, actual_latency_attributions2);

  pipeline_reporter_ = nullptr;
}

TEST_F(CompositorFrameReporterTest, StageLatencyLargeDurationPrediction) {
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kBeginImplFrameToSendBeginMainFrame,
      Now());

  AdvanceNowByUs(10000000);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kSendBeginMainFrameToCommit, Now());
  AdvanceNowByUs(400000);
  base::TimeTicks begin_main_frame_start_time = Now();

  auto blink_breakdown = std::make_unique<BeginMainFrameMetrics>();
  blink_breakdown->handle_input_events = base::Microseconds(1000000);
  blink_breakdown->animate = base::Microseconds(900000);
  blink_breakdown->style_update = base::Microseconds(800000);
  blink_breakdown->layout_update = base::Microseconds(300000);
  blink_breakdown->accessibility = base::Microseconds(400000);
  blink_breakdown->prepaint = base::Microseconds(500000);
  blink_breakdown->compositing_inputs = base::Microseconds(600000);
  blink_breakdown->paint = base::Microseconds(300000);
  blink_breakdown->composite_commit = base::Microseconds(200000);
  blink_breakdown->update_layers = base::Microseconds(100000);
  AdvanceNowByUs(5100000);

  pipeline_reporter_->SetBlinkBreakdown(std::move(blink_breakdown),
                                        begin_main_frame_start_time);

  pipeline_reporter_->StartStage(CompositorFrameReporter::StageType::kCommit,
                                 Now());

  AdvanceNowByUs(6000000);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kEndCommitToActivation, Now());

  AdvanceNowByUs(10000000);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kActivation, Now());

  AdvanceNowByUs(0);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kEndActivateToSubmitCompositorFrame,
      Now());

  AdvanceNowByUs(2000000);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::
          kSubmitCompositorFrameToPresentationCompositorFrame,
      Now());

  viz::FrameTimingDetails viz_breakdown;
  viz_breakdown.received_compositor_frame_timestamp = AdvanceNowByUs(1000000);
  viz_breakdown.draw_start_timestamp = AdvanceNowByUs(2000000);
  viz_breakdown.swap_timings.swap_start = AdvanceNowByUs(3000000);
  viz_breakdown.presentation_feedback.available_timestamp =
      AdvanceNowByUs(15000000);
  viz_breakdown.presentation_feedback.ready_timestamp = AdvanceNowByUs(700000);
  viz_breakdown.presentation_feedback.latch_timestamp = AdvanceNowByUs(800000);
  viz_breakdown.swap_timings.swap_end = AdvanceNowByUs(1000000);
  viz_breakdown.presentation_feedback.timestamp = AdvanceNowByUs(5000000);

  pipeline_reporter_->SetVizBreakdown(viz_breakdown);

  pipeline_reporter_->TerminateFrame(
      CompositorFrameReporter::FrameTerminationStatus::kPresentedFrame, Now());

  // predictions when this is the very first prediction
  CompositorFrameReporter::CompositorLatencyInfo expected_latency_predictions1;
  expected_latency_predictions1.top_level_stages = {
      base::Microseconds(10000000), base::Microseconds(5500000),
      base::Microseconds(6000000),  base::Microseconds(10000000),
      base::Microseconds(0),        base::Microseconds(2000000),
      base::Microseconds(28500000)};
  expected_latency_predictions1.blink_breakdown_stages = {
      base::Microseconds(1000000), base::Microseconds(900000),
      base::Microseconds(800000),  base::Microseconds(300000),
      base::Microseconds(400000),  base::Microseconds(500000),
      base::Microseconds(600000),  base::Microseconds(300000),
      base::Microseconds(200000),  base::Microseconds(100000),
      base::Microseconds(400000)};
  expected_latency_predictions1.viz_breakdown_stages = {
      base::Microseconds(1000000),  base::Microseconds(2000000),
      base::Microseconds(3000000),  base::Microseconds(0),
      base::Microseconds(15000000), base::Microseconds(700000),
      base::Microseconds(800000),   base::Microseconds(1000000),
      base::Microseconds(5000000)};
  expected_latency_predictions1.total_latency = base::Microseconds(62000000);
  expected_latency_predictions1.total_blink_latency =
      base::Microseconds(5500000);
  expected_latency_predictions1.total_viz_latency =
      base::Microseconds(28500000);

  // predictions when there exists a previous prediction
  CompositorFrameReporter::CompositorLatencyInfo expected_latency_predictions2;
  expected_latency_predictions2.top_level_stages = {
      base::Microseconds(8500000), base::Microseconds(4375000),
      base::Microseconds(4875000), base::Microseconds(5252650),
      base::Microseconds(750000),  base::Microseconds(1850000),
      base::Microseconds(18375000)};
  expected_latency_predictions2.blink_breakdown_stages = {
      base::Microseconds(1000000), base::Microseconds(225000),
      base::Microseconds(500000),  base::Microseconds(75000),
      base::Microseconds(250000),  base::Microseconds(350000),
      base::Microseconds(150000),  base::Microseconds(750000),
      base::Microseconds(650000),  base::Microseconds(250000),
      base::Microseconds(175000)};
  expected_latency_predictions2.viz_breakdown_stages = {
      base::Microseconds(625000),  base::Microseconds(875000),
      base::Microseconds(1500000), base::Microseconds(0),
      base::Microseconds(9750000), base::Microseconds(925000),
      base::Microseconds(1100000), base::Microseconds(1893925),
      base::Microseconds(1706075)};
  expected_latency_predictions2.total_latency = base::Microseconds(43977650);
  expected_latency_predictions2.total_blink_latency =
      base::Microseconds(4375000);
  expected_latency_predictions2.total_viz_latency =
      base::Microseconds(18375000);

  // expected attribution for cases 1 above
  std::vector<std::string> expected_latency_attributions1 = {};

  // expected attribution for case 2 above
  std::vector<std::string> expected_latency_attributions2 = {
      "EndCommitToActivation",
      "SubmitCompositorFrameToPresentationCompositorFrame."
      "SwapEndToPresentationCompositorFrame"};

  CompositorFrameReporter::CompositorLatencyInfo actual_latency_predictions1(
      base::Microseconds(-1));
  pipeline_reporter_->CalculateCompositorLatencyPrediction(
      actual_latency_predictions1, kLatencyPredictionDeviationThreshold);
  std::vector<std::string> actual_latency_attributions1 =
      pipeline_reporter_->high_latency_substages_for_testing();
  pipeline_reporter_->ClearHighLatencySubstagesForTesting();

  CompositorFrameReporter::CompositorLatencyInfo actual_latency_predictions2;
  actual_latency_predictions2.top_level_stages = {
      base::Microseconds(8000000), base::Microseconds(4000000),
      base::Microseconds(4500000), base::Microseconds(3670200),
      base::Microseconds(1000000), base::Microseconds(1800000),
      base::Microseconds(15000000)};
  actual_latency_predictions2.blink_breakdown_stages = {
      base::Microseconds(1000000), base::Microseconds(0),
      base::Microseconds(400000),  base::Microseconds(0),
      base::Microseconds(200000),  base::Microseconds(300000),
      base::Microseconds(0),       base::Microseconds(900000),
      base::Microseconds(800000),  base::Microseconds(300000),
      base::Microseconds(100000)};
  actual_latency_predictions2.viz_breakdown_stages = {
      base::Microseconds(500000),  base::Microseconds(500000),
      base::Microseconds(1000000), base::Microseconds(0),
      base::Microseconds(8000000), base::Microseconds(1000000),
      base::Microseconds(1200000), base::Microseconds(2191900),
      base::Microseconds(608100)};
  actual_latency_predictions2.total_latency = base::Microseconds(37970200);
  actual_latency_predictions2.total_blink_latency = base::Microseconds(4000000);
  actual_latency_predictions2.total_viz_latency = base::Microseconds(15000000);

  pipeline_reporter_->CalculateCompositorLatencyPrediction(
      actual_latency_predictions2, kLatencyPredictionDeviationThreshold);
  std::vector<std::string> actual_latency_attributions2 =
      pipeline_reporter_->high_latency_substages_for_testing();
  pipeline_reporter_->ClearHighLatencySubstagesForTesting();

  VerifyLatencyInfo(expected_latency_predictions1, actual_latency_predictions1);
  VerifyLatencyInfo(expected_latency_predictions2, actual_latency_predictions2);

  EXPECT_EQ(expected_latency_attributions1, actual_latency_attributions1);
  EXPECT_EQ(expected_latency_attributions2, actual_latency_attributions2);

  pipeline_reporter_ = nullptr;
}

TEST_F(CompositorFrameReporterTest, StageLatencyMultiplePrediction) {
  CompositorFrameReporter::CompositorLatencyInfo actual_latency_predictions(
      base::Microseconds(-1));
  CompositorFrameReporter::CompositorLatencyInfo expected_latency_predictions(
      base::Microseconds(-1));

  // First compositor reporter (general)
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kBeginImplFrameToSendBeginMainFrame,
      Now());

  AdvanceNowByUs(16000);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kEndActivateToSubmitCompositorFrame,
      Now());

  AdvanceNowByUs(1500);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::
          kSubmitCompositorFrameToPresentationCompositorFrame,
      Now());

  viz::FrameTimingDetails viz_breakdown;
  viz_breakdown.received_compositor_frame_timestamp = AdvanceNowByUs(330000);
  viz_breakdown.draw_start_timestamp = AdvanceNowByUs(23000);
  viz_breakdown.swap_timings.swap_start = AdvanceNowByUs(170000);
  viz_breakdown.swap_timings.swap_end = AdvanceNowByUs(280000);
  viz_breakdown.presentation_feedback.timestamp = AdvanceNowByUs(30000);

  pipeline_reporter_->SetVizBreakdown(viz_breakdown);

  pipeline_reporter_->TerminateFrame(
      CompositorFrameReporter::FrameTerminationStatus::kPresentedFrame, Now());

  expected_latency_predictions.top_level_stages = {
      base::Microseconds(16000), base::Microseconds(0),
      base::Microseconds(0),     base::Microseconds(0),
      base::Microseconds(0),     base::Microseconds(1500),
      base::Microseconds(833000)};
  expected_latency_predictions.blink_breakdown_stages = {
      base::Microseconds(0), base::Microseconds(0), base::Microseconds(0),
      base::Microseconds(0), base::Microseconds(0), base::Microseconds(0),
      base::Microseconds(0), base::Microseconds(0), base::Microseconds(0),
      base::Microseconds(0), base::Microseconds(0)};
  expected_latency_predictions.viz_breakdown_stages = {
      base::Microseconds(330000), base::Microseconds(23000),
      base::Microseconds(170000), base::Microseconds(280000),
      base::Microseconds(0),      base::Microseconds(0),
      base::Microseconds(0),      base::Microseconds(0),
      base::Microseconds(30000)};
  expected_latency_predictions.total_latency = base::Microseconds(850500);
  expected_latency_predictions.total_blink_latency = base::Microseconds(0);
  expected_latency_predictions.total_viz_latency = base::Microseconds(833000);

  pipeline_reporter_->CalculateCompositorLatencyPrediction(
      actual_latency_predictions, kLatencyPredictionDeviationThreshold);

  VerifyLatencyInfo(expected_latency_predictions, actual_latency_predictions);

  // Second compositor reporter (without subtmit stage)
  pipeline_reporter_ = CreatePipelineReporter();
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kBeginImplFrameToSendBeginMainFrame,
      Now());

  AdvanceNowByUs(16000);
  pipeline_reporter_->TerminateFrame(
      CompositorFrameReporter::FrameTerminationStatus::kDidNotProduceFrame,
      Now());

  expected_latency_predictions.top_level_stages = {
      base::Microseconds(16000), base::Microseconds(0),
      base::Microseconds(0),     base::Microseconds(0),
      base::Microseconds(0),     base::Microseconds(1500),
      base::Microseconds(833000)};
  expected_latency_predictions.blink_breakdown_stages = {
      base::Microseconds(0), base::Microseconds(0), base::Microseconds(0),
      base::Microseconds(0), base::Microseconds(0), base::Microseconds(0),
      base::Microseconds(0), base::Microseconds(0), base::Microseconds(0),
      base::Microseconds(0), base::Microseconds(0)};
  expected_latency_predictions.viz_breakdown_stages = {
      base::Microseconds(330000), base::Microseconds(23000),
      base::Microseconds(170000), base::Microseconds(280000),
      base::Microseconds(0),      base::Microseconds(0),
      base::Microseconds(0),      base::Microseconds(0),
      base::Microseconds(30000)};
  expected_latency_predictions.total_latency = base::Microseconds(850500);
  expected_latency_predictions.total_blink_latency = base::Microseconds(0);
  expected_latency_predictions.total_viz_latency = base::Microseconds(833000);

  pipeline_reporter_->CalculateCompositorLatencyPrediction(
      actual_latency_predictions, kLatencyPredictionDeviationThreshold);

  VerifyLatencyInfo(expected_latency_predictions, actual_latency_predictions);

  // Third compositor reporter (prediction and actual latency does not differ
  // by 8)
  pipeline_reporter_ = CreatePipelineReporter();
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kBeginImplFrameToSendBeginMainFrame,
      Now());

  AdvanceNowByUs(16500);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kEndActivateToSubmitCompositorFrame,
      Now());

  AdvanceNowByUs(2000);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::
          kSubmitCompositorFrameToPresentationCompositorFrame,
      Now());

  viz_breakdown.received_compositor_frame_timestamp = AdvanceNowByUs(330000);
  viz_breakdown.draw_start_timestamp = AdvanceNowByUs(23000);
  viz_breakdown.swap_timings.swap_start = AdvanceNowByUs(170000);
  viz_breakdown.swap_timings.swap_end = AdvanceNowByUs(280000);
  viz_breakdown.presentation_feedback.timestamp = AdvanceNowByUs(30000);

  pipeline_reporter_->SetVizBreakdown(viz_breakdown);

  pipeline_reporter_->TerminateFrame(
      CompositorFrameReporter::FrameTerminationStatus::kPresentedFrame, Now());

  expected_latency_predictions.top_level_stages = {
      base::Microseconds(16125), base::Microseconds(0),
      base::Microseconds(0),     base::Microseconds(0),
      base::Microseconds(0),     base::Microseconds(1625),
      base::Microseconds(833000)};
  expected_latency_predictions.blink_breakdown_stages = {
      base::Microseconds(0), base::Microseconds(0), base::Microseconds(0),
      base::Microseconds(0), base::Microseconds(0), base::Microseconds(0),
      base::Microseconds(0), base::Microseconds(0), base::Microseconds(0),
      base::Microseconds(0), base::Microseconds(0)};
  expected_latency_predictions.viz_breakdown_stages = {
      base::Microseconds(330000), base::Microseconds(23000),
      base::Microseconds(170000), base::Microseconds(280000),
      base::Microseconds(0),      base::Microseconds(0),
      base::Microseconds(0),      base::Microseconds(0),
      base::Microseconds(30000)};
  expected_latency_predictions.total_latency = base::Microseconds(850750);
  expected_latency_predictions.total_blink_latency = base::Microseconds(0);
  expected_latency_predictions.total_viz_latency = base::Microseconds(833000);

  pipeline_reporter_->CalculateCompositorLatencyPrediction(
      actual_latency_predictions, kLatencyPredictionDeviationThreshold);

  VerifyLatencyInfo(expected_latency_predictions, actual_latency_predictions);

  // Fourth compositor reporter (total duration is 0)
  pipeline_reporter_ = CreatePipelineReporter();
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kBeginImplFrameToSendBeginMainFrame,
      Now());

  AdvanceNowByUs(0);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kSendBeginMainFrameToCommit, Now());

  AdvanceNowByUs(0);
  pipeline_reporter_->StartStage(CompositorFrameReporter::StageType::kCommit,
                                 Now());

  AdvanceNowByUs(0);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kEndCommitToActivation, Now());

  AdvanceNowByUs(0);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::
          kSubmitCompositorFrameToPresentationCompositorFrame,
      Now());

  AdvanceNowByUs(0);
  pipeline_reporter_->TerminateFrame(
      CompositorFrameReporter::FrameTerminationStatus::kDidNotProduceFrame,
      Now());

  expected_latency_predictions.top_level_stages = {
      base::Microseconds(16125), base::Microseconds(0),
      base::Microseconds(0),     base::Microseconds(0),
      base::Microseconds(0),     base::Microseconds(1625),
      base::Microseconds(833000)};
  expected_latency_predictions.blink_breakdown_stages = {
      base::Microseconds(0), base::Microseconds(0), base::Microseconds(0),
      base::Microseconds(0), base::Microseconds(0), base::Microseconds(0),
      base::Microseconds(0), base::Microseconds(0), base::Microseconds(0),
      base::Microseconds(0), base::Microseconds(0)};
  expected_latency_predictions.viz_breakdown_stages = {
      base::Microseconds(330000), base::Microseconds(23000),
      base::Microseconds(170000), base::Microseconds(280000),
      base::Microseconds(0),      base::Microseconds(0),
      base::Microseconds(0),      base::Microseconds(0),
      base::Microseconds(30000)};
  expected_latency_predictions.total_latency = base::Microseconds(850750);
  expected_latency_predictions.total_blink_latency = base::Microseconds(0);
  expected_latency_predictions.total_viz_latency = base::Microseconds(833000);

  pipeline_reporter_->CalculateCompositorLatencyPrediction(
      actual_latency_predictions, kLatencyPredictionDeviationThreshold);

  VerifyLatencyInfo(expected_latency_predictions, actual_latency_predictions);

  // Fifth compositor reporter (prediction and actual latency differ by a lot)
  pipeline_reporter_ = CreatePipelineReporter();
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kBeginImplFrameToSendBeginMainFrame,
      Now());

  AdvanceNowByUs(16000);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kSendBeginMainFrameToCommit, Now());

  AdvanceNowByUs(4000);
  base::TimeTicks begin_main_frame_start_time = Now();

  auto blink_breakdown = std::make_unique<BeginMainFrameMetrics>();
  blink_breakdown->handle_input_events = base::Microseconds(12000);
  blink_breakdown->animate = base::Microseconds(3000);
  blink_breakdown->style_update = base::Microseconds(7000);
  blink_breakdown->layout_update = base::Microseconds(19000);
  blink_breakdown->accessibility = base::Microseconds(800);
  blink_breakdown->prepaint = base::Microseconds(4100);
  blink_breakdown->compositing_inputs = base::Microseconds(5100);
  blink_breakdown->paint = base::Microseconds(1500);
  blink_breakdown->composite_commit = base::Microseconds(1500);
  blink_breakdown->update_layers = base::Microseconds(2000);
  AdvanceNowByUs(56000);

  pipeline_reporter_->SetBlinkBreakdown(std::move(blink_breakdown),
                                        begin_main_frame_start_time);

  pipeline_reporter_->StartStage(CompositorFrameReporter::StageType::kCommit,
                                 Now());

  AdvanceNowByUs(6000);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kEndCommitToActivation, Now());

  AdvanceNowByUs(3000);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kActivation, Now());

  AdvanceNowByUs(300);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kEndActivateToSubmitCompositorFrame,
      Now());

  AdvanceNowByUs(39000);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::
          kSubmitCompositorFrameToPresentationCompositorFrame,
      Now());

  viz_breakdown.received_compositor_frame_timestamp = AdvanceNowByUs(340000);
  viz_breakdown.draw_start_timestamp = AdvanceNowByUs(20000);
  viz_breakdown.swap_timings.swap_start = AdvanceNowByUs(160000);
  viz_breakdown.swap_timings.swap_end = AdvanceNowByUs(283000);
  viz_breakdown.presentation_feedback.timestamp = AdvanceNowByUs(30000);

  pipeline_reporter_->SetVizBreakdown(viz_breakdown);

  pipeline_reporter_->TerminateFrame(
      CompositorFrameReporter::FrameTerminationStatus::kPresentedFrame, Now());

  expected_latency_predictions.top_level_stages = {
      base::Microseconds(16093), base::Microseconds(15000),
      base::Microseconds(1500),  base::Microseconds(750),
      base::Microseconds(75),    base::Microseconds(10968),
      base::Microseconds(833000)};
  expected_latency_predictions.blink_breakdown_stages = {
      base::Microseconds(12000), base::Microseconds(3000),
      base::Microseconds(7000),  base::Microseconds(19000),
      base::Microseconds(800),   base::Microseconds(4100),
      base::Microseconds(5100),  base::Microseconds(1500),
      base::Microseconds(1500),  base::Microseconds(2000),
      base::Microseconds(4000)};
  expected_latency_predictions.viz_breakdown_stages = {
      base::Microseconds(332500), base::Microseconds(22250),
      base::Microseconds(167500), base::Microseconds(280750),
      base::Microseconds(0),      base::Microseconds(0),
      base::Microseconds(0),      base::Microseconds(0),
      base::Microseconds(30000)};
  expected_latency_predictions.total_latency = base::Microseconds(877387);
  expected_latency_predictions.total_blink_latency = base::Microseconds(60000);
  expected_latency_predictions.total_viz_latency = base::Microseconds(833000);

  std::vector<std::string> expected_latency_attributions = {
      "EndActivateToSubmitCompositorFrame"};

  pipeline_reporter_->CalculateCompositorLatencyPrediction(
      actual_latency_predictions, kLatencyPredictionDeviationThreshold);
  std::vector<std::string> actual_latency_attributions =
      pipeline_reporter_->high_latency_substages_for_testing();

  VerifyLatencyInfo(expected_latency_predictions, actual_latency_predictions);
  EXPECT_EQ(expected_latency_attributions, actual_latency_attributions);

  pipeline_reporter_ = nullptr;
}

// Tests that when a frame is presented to the user, event latency predictions
// are reported properly.
TEST_F(CompositorFrameReporterTest, EventLatencyDispatchPredictions) {
  base::HistogramTester histogram_tester;
  std::vector<int> dispatch_times = {
      /*[kGenerated, kArrivedInBrowserMain]=*/300,
      /*[kArrivedInBrowserMain, kArrivedInRendererCompositor]=*/300,
      /*[kArrivedInRendererCompositor, kRendererCompositorStarted]=*/300,
      /*[kRendererCompositorStarted, kRendererCompositorFinished]=*/300,
      /*[kRendererCompositorFinished, kRendererMainStarted]=*/300,
      /*[kRendererMainStarted, kRendererMainFinished]=*/300};
  std::unique_ptr<EventMetrics> event_metrics_ptrs[] = {
      CreateScrollUpdateEventMetricsWithDispatchTimes(
          false, ScrollUpdateEventMetrics::ScrollUpdateType::kContinued,
          dispatch_times)};
  EXPECT_THAT(event_metrics_ptrs, Each(NotNull()));
  EventMetrics::List events_metrics = {
      std::make_move_iterator(std::begin(event_metrics_ptrs)),
      std::make_move_iterator(std::end(event_metrics_ptrs))};

  AdvanceNowByUs(300);  // Transition time
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kEndActivateToSubmitCompositorFrame,
      Now());

  AdvanceNowByUs(300);  // kEndActivateToSubmitCompositorFrame stage duration
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::
          kSubmitCompositorFrameToPresentationCompositorFrame,
      Now());

  pipeline_reporter_->AddEventsMetrics(std::move(events_metrics));
  pipeline_reporter_->TerminateFrame(
      CompositorFrameReporter::FrameTerminationStatus::kPresentedFrame, Now());

  // Test with no previous stage predictions.
  std::vector<base::TimeDelta> expected_predictions1(kNumDispatchStages,
                                                     base::Microseconds(-1));
  IntToTimeDeltaVector(
      expected_predictions1,
      std::vector<int>{/*kScrollsBlockingTouchDispatchedToRenderer=*/-1,
                       /*kArrivedInBrowserMain=*/300,
                       /*kArrivedInRendererCompositor=*/300,
                       /*kRendererCompositorStarted=*/300,
                       /*kRendererCompositorFinished=*/300,
                       /*kRendererMainStarted=*/300,
                       /*kRendererMainFinished=*/300});
  base::TimeDelta expected_transition1 = base::Microseconds(300);
  base::TimeDelta expected_total1 = base::Microseconds(2400);
  CompositorFrameReporter::EventLatencyInfo actual_predictions1 =
      CompositorFrameReporter::EventLatencyInfo(kNumDispatchStages,
                                                kNumOfCompositorStages);
  pipeline_reporter_->CalculateEventLatencyPrediction(
      actual_predictions1, kLatencyPredictionDeviationThreshold);

  // Test with all previous stage predictions.
  std::vector<base::TimeDelta> expected_predictions2(kNumDispatchStages,
                                                     base::Microseconds(-1));
  IntToTimeDeltaVector(expected_predictions2,
                       std::vector<int>{300, 262, 262, 300, 412, 225, 450});
  base::TimeDelta expected_transition2 = base::Microseconds(390);
  base::TimeDelta expected_total2 = base::Microseconds(2901);
  CompositorFrameReporter::EventLatencyInfo actual_predictions2 =
      CompositorFrameReporter::EventLatencyInfo(kNumDispatchStages,
                                                kNumOfCompositorStages);
  IntToTimeDeltaVector(actual_predictions2.dispatch_durations,
                       std::vector<int>{300, 250, 250, 300, 450, 200, 500});
  actual_predictions2.transition_duration = base::Microseconds(420);
  pipeline_reporter_->CalculateEventLatencyPrediction(
      actual_predictions2, kLatencyPredictionDeviationThreshold);

  // Test with some previous stage predictions.
  std::vector<base::TimeDelta> expected_predictions3(kNumDispatchStages,
                                                     base::Microseconds(-1));
  IntToTimeDeltaVector(expected_predictions3,
                       std::vector<int>{300, 300, 375, 450, 300, 300, 300});
  base::TimeDelta expected_transition3 = base::Microseconds(270);
  base::TimeDelta expected_total3 = base::Microseconds(2895);
  CompositorFrameReporter::EventLatencyInfo actual_predictions3 =
      CompositorFrameReporter::EventLatencyInfo(kNumDispatchStages,
                                                kNumOfCompositorStages);
  IntToTimeDeltaVector(actual_predictions3.dispatch_durations,
                       std::vector<int>{300, -1, 400, 500, 300, -1, -1});
  actual_predictions3.transition_duration = base::Microseconds(260);
  pipeline_reporter_->CalculateEventLatencyPrediction(
      actual_predictions3, kLatencyPredictionDeviationThreshold);

  for (int i = 0; i < kNumDispatchStages; i++) {
    EXPECT_EQ(expected_predictions1[i],
              actual_predictions1.dispatch_durations[i]);
    EXPECT_EQ(expected_predictions2[i],
              actual_predictions2.dispatch_durations[i]);
    EXPECT_EQ(expected_predictions3[i],
              actual_predictions3.dispatch_durations[i]);
  }
  EXPECT_EQ(expected_transition1, actual_predictions1.transition_duration);
  EXPECT_EQ(expected_total1, actual_predictions1.total_duration);
  EXPECT_EQ(expected_transition2, actual_predictions2.transition_duration);
  EXPECT_EQ(expected_total2, actual_predictions2.total_duration);
  EXPECT_EQ(expected_transition3, actual_predictions3.transition_duration);
  EXPECT_EQ(expected_total3, actual_predictions3.total_duration);

  pipeline_reporter_ = nullptr;
  histogram_tester.ExpectTotalCount(
      "EventLatency.GestureScrollUpdate.GenerationToBrowserMain", 1);
}

// Tests that when a new frame with missing dispatch stages is presented to
// the user, event latency predictions are reported properly.
TEST_F(CompositorFrameReporterTest,
       EventLatencyDispatchPredictionsWithMissingStages) {
  base::HistogramTester histogram_tester;
  // Invalid EventLatency stage durations will cause program to crash, validity
  // checked in event_latency_tracing_recorder.cc.
  std::vector<int> dispatch_times = {
      /*[kGenerated, kArrivedInBrowserMain]=*/200,
      /*[kArrivedInBrowserMain, kArrivedInRendererCompositor]=*/400,
      /*[kArrivedInRendererCompositor, kRendererCompositorStarted]=*/600,
      /*[kRendererCompositorStarted, kRendererCompositorFinished]=*/700,
      /*[kRendererCompositorFinished, kRendererMainStarted]=*/-1,
      /*[kRendererMainStarted, kRendererMainFinished]=*/-1};
  std::unique_ptr<EventMetrics> event_metrics_ptrs[] = {
      CreateScrollUpdateEventMetricsWithDispatchTimes(
          false, ScrollUpdateEventMetrics::ScrollUpdateType::kContinued,
          dispatch_times)};
  EXPECT_THAT(event_metrics_ptrs, Each(NotNull()));
  EventMetrics::List events_metrics = {
      std::make_move_iterator(std::begin(event_metrics_ptrs)),
      std::make_move_iterator(std::end(event_metrics_ptrs))};

  AdvanceNowByUs(470);  // Transition time
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kEndActivateToSubmitCompositorFrame,
      Now());

  AdvanceNowByUs(300);  // kEndActivateToSubmitCompositorFrame stage duration
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::
          kSubmitCompositorFrameToPresentationCompositorFrame,
      Now());

  pipeline_reporter_->AddEventsMetrics(std::move(events_metrics));
  pipeline_reporter_->TerminateFrame(
      CompositorFrameReporter::FrameTerminationStatus::kPresentedFrame, Now());

  // Test with no previous stage predictions.
  std::vector<base::TimeDelta> expected_predictions1(kNumDispatchStages,
                                                     base::Microseconds(-1));
  IntToTimeDeltaVector(expected_predictions1,
                       std::vector<int>{-1, 200, 400, 600, 700, -1, -1});
  base::TimeDelta expected_transition1 = base::Microseconds(470);
  base::TimeDelta expected_total1 = base::Microseconds(2670);
  CompositorFrameReporter::EventLatencyInfo actual_predictions1 =
      CompositorFrameReporter::EventLatencyInfo(kNumDispatchStages,
                                                kNumOfCompositorStages);
  pipeline_reporter_->CalculateEventLatencyPrediction(
      actual_predictions1, kLatencyPredictionDeviationThreshold);

  // Test with all previous stage predictions.
  std::vector<base::TimeDelta> expected_predictions2(kNumDispatchStages,
                                                     base::Microseconds(-1));
  IntToTimeDeltaVector(expected_predictions2,
                       std::vector<int>{100, 125, 250, 375, 475, 200, 500});
  base::TimeDelta expected_transition2 = base::Microseconds(402);
  base::TimeDelta expected_total2 = base::Microseconds(2727);
  CompositorFrameReporter::EventLatencyInfo actual_predictions2 =
      CompositorFrameReporter::EventLatencyInfo(kNumDispatchStages,
                                                kNumOfCompositorStages);
  IntToTimeDeltaVector(actual_predictions2.dispatch_durations,
                       std::vector<int>{100, 100, 200, 300, 400, 200, 500});
  actual_predictions2.transition_duration = base::Microseconds(380);
  pipeline_reporter_->CalculateEventLatencyPrediction(
      actual_predictions2, kLatencyPredictionDeviationThreshold);

  // Test with some previous stage predictions.
  std::vector<base::TimeDelta> expected_predictions3(kNumDispatchStages,
                                                     base::Microseconds(-1));
  IntToTimeDeltaVector(expected_predictions3,
                       std::vector<int>{125, 143, 400, 525, 745, -1, -1});
  base::TimeDelta expected_transition3 = base::Microseconds(492);
  base::TimeDelta expected_total3 = base::Microseconds(2730);
  CompositorFrameReporter::EventLatencyInfo actual_predictions3 =
      CompositorFrameReporter::EventLatencyInfo(kNumDispatchStages,
                                                kNumOfCompositorStages);
  IntToTimeDeltaVector(actual_predictions3.dispatch_durations,
                       std::vector<int>{125, 125, 400, 500, 760, -1, -1});
  actual_predictions3.transition_duration = base::Microseconds(500);
  pipeline_reporter_->CalculateEventLatencyPrediction(
      actual_predictions3, kLatencyPredictionDeviationThreshold);

  for (int i = 0; i < kNumDispatchStages; i++) {
    EXPECT_EQ(expected_predictions1[i],
              actual_predictions1.dispatch_durations[i]);
    EXPECT_EQ(expected_predictions2[i],
              actual_predictions2.dispatch_durations[i]);
    EXPECT_EQ(expected_predictions3[i],
              actual_predictions3.dispatch_durations[i]);
  }
  EXPECT_EQ(expected_transition1, actual_predictions1.transition_duration);
  EXPECT_EQ(expected_total1, actual_predictions1.total_duration);
  EXPECT_EQ(expected_transition2, actual_predictions2.transition_duration);
  EXPECT_EQ(expected_total2, actual_predictions2.total_duration);
  EXPECT_EQ(expected_transition3, actual_predictions3.transition_duration);
  EXPECT_EQ(expected_total3, actual_predictions3.total_duration);

  pipeline_reporter_ = nullptr;
  histogram_tester.ExpectTotalCount(
      "EventLatency.GestureScrollUpdate.GenerationToBrowserMain", 1);
}

// Tests that when a frame is presented to the user, event latency predictions
// are reported properly.
TEST_F(CompositorFrameReporterTest, EventLatencyCompositorPredictions) {
  base::HistogramTester histogram_tester;
  std::vector<int> dispatch_times = {
      /*[kGenerated, kArrivedInBrowserMain]=*/300,
      /*[kArrivedInBrowserMain, kArrivedInRendererCompositor]=*/300,
      /*[kArrivedInRendererCompositor, kRendererCompositorStarted]=*/300,
      /*[kRendererCompositorStarted, kRendererCompositorFinished]=*/300,
      /*[kRendererCompositorFinished, kRendererMainStarted]=*/300,
      /*[kRendererMainStarted, kRendererMainFinished]=*/300};

  std::unique_ptr<EventMetrics> event_metrics_ptrs[] = {
      CreateScrollUpdateEventMetricsWithDispatchTimes(
          false, ScrollUpdateEventMetrics::ScrollUpdateType::kContinued,
          dispatch_times)};
  EXPECT_THAT(event_metrics_ptrs, Each(NotNull()));
  EventMetrics::List events_metrics = {
      std::make_move_iterator(std::begin(event_metrics_ptrs)),
      std::make_move_iterator(std::end(event_metrics_ptrs))};

  AdvanceNowByUs(300);  // Transition time
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kBeginImplFrameToSendBeginMainFrame,
      Now());

  // For this test there are only 3 compositor substages updated which reflects
  // a more realistic scenario.

  AdvanceNowByUs(300);  // kBeginImplFrameToSendBeginMainFrame duration
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kEndActivateToSubmitCompositorFrame,
      Now());

  AdvanceNowByUs(300);  // kEndActivateToSubmitCompositorFrame duration
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::
          kSubmitCompositorFrameToPresentationCompositorFrame,
      Now());

  // kSubmitCompositorFrameToPresentationCompositorFrame duration
  AdvanceNowByUs(300);
  pipeline_reporter_->TerminateFrame(
      CompositorFrameReporter::FrameTerminationStatus::kPresentedFrame, Now());

  pipeline_reporter_->AddEventsMetrics(std::move(events_metrics));

  // Test with no previous stage predictions.
  std::vector<base::TimeDelta> expected_dispatch1(kNumDispatchStages,
                                                  base::Microseconds(-1));
  IntToTimeDeltaVector(
      expected_dispatch1,
      std::vector<int>{/*kScrollsBlockingTouchDispatchedToRenderer=*/-1,
                       /*kArrivedInBrowserMain=*/300,
                       /*kArrivedInRendererCompositor=*/300,
                       /*kRendererCompositorStarted=*/300,
                       /*kRendererCompositorFinished=*/300,
                       /*kRendererMainStarted=*/300,
                       /*kRendererMainFinished=*/300});
  base::TimeDelta expected_transition1 = base::Microseconds(300);
  std::vector<base::TimeDelta> expected_compositor1(kNumOfCompositorStages,
                                                    base::Microseconds(-1));
  IntToTimeDeltaVector(expected_compositor1,
                       std::vector<int>{300, -1, -1, -1, -1, 300, 300});
  base::TimeDelta expected_total1 = base::Microseconds(3000);
  CompositorFrameReporter::EventLatencyInfo actual_predictions1 =
      CompositorFrameReporter::EventLatencyInfo(kNumDispatchStages,
                                                kNumOfCompositorStages);
  pipeline_reporter_->CalculateEventLatencyPrediction(
      actual_predictions1, kLatencyPredictionDeviationThreshold);

  // Test with all previous stage predictions.
  std::vector<base::TimeDelta> expected_dispatch2(kNumDispatchStages,
                                                  base::Microseconds(-1));
  IntToTimeDeltaVector(expected_dispatch2,
                       std::vector<int>{250, 262, 262, 300, 412, 225, 450});
  base::TimeDelta expected_transition2 = base::Microseconds(390);
  std::vector<base::TimeDelta> expected_compositor2(kNumOfCompositorStages,
                                                    base::Microseconds(-1));
  IntToTimeDeltaVector(expected_compositor2,
                       std::vector<int>{465, 500, 90, 720, 410, 742, 390});
  base::TimeDelta expected_total2 = base::Microseconds(5868);
  CompositorFrameReporter::EventLatencyInfo actual_predictions2 =
      CompositorFrameReporter::EventLatencyInfo(kNumDispatchStages,
                                                kNumOfCompositorStages);
  IntToTimeDeltaVector(actual_predictions2.dispatch_durations,
                       std::vector<int>{250, 250, 250, 300, 450, 200, 500});
  actual_predictions2.transition_duration = base::Microseconds(420);
  IntToTimeDeltaVector(actual_predictions2.compositor_durations,
                       std::vector<int>{520, 500, 90, 720, 410, 890, 420});
  pipeline_reporter_->CalculateEventLatencyPrediction(
      actual_predictions2, kLatencyPredictionDeviationThreshold);

  // Test with some previous stage predictions.
  std::vector<base::TimeDelta> expected_dispatch3(kNumDispatchStages,
                                                  base::Microseconds(-1));
  IntToTimeDeltaVector(expected_dispatch3,
                       std::vector<int>{400, 375, 375, 450, 300, 300, 300});
  base::TimeDelta expected_transition3 = base::Microseconds(270);
  std::vector<base::TimeDelta> expected_compositor3(kNumOfCompositorStages,
                                                    base::Microseconds(-1));
  IntToTimeDeltaVector(expected_compositor3,
                       std::vector<int>{300, 500, -1, -1, 410, 742, 390});
  base::TimeDelta expected_total3 = base::Microseconds(5112);
  CompositorFrameReporter::EventLatencyInfo actual_predictions3 =
      CompositorFrameReporter::EventLatencyInfo(kNumDispatchStages,
                                                kNumOfCompositorStages);
  IntToTimeDeltaVector(actual_predictions3.dispatch_durations,
                       std::vector<int>{400, 400, 400, 500, 300, -1, -1});
  actual_predictions3.transition_duration = base::Microseconds(260);
  IntToTimeDeltaVector(actual_predictions3.compositor_durations,
                       std::vector<int>{-1, 500, -1, -1, 410, 890, 420});
  pipeline_reporter_->CalculateEventLatencyPrediction(
      actual_predictions3, kLatencyPredictionDeviationThreshold);

  for (int i = 0; i < kNumDispatchStages; i++) {
    EXPECT_EQ(expected_dispatch1[i], actual_predictions1.dispatch_durations[i]);
    EXPECT_EQ(expected_dispatch2[i], actual_predictions2.dispatch_durations[i]);
    EXPECT_EQ(expected_dispatch3[i], actual_predictions3.dispatch_durations[i]);
  }
  for (int i = 0; i < kNumOfCompositorStages; i++) {
    EXPECT_EQ(expected_compositor1[i],
              actual_predictions1.compositor_durations[i]);
    EXPECT_EQ(expected_compositor2[i],
              actual_predictions2.compositor_durations[i]);
    EXPECT_EQ(expected_compositor3[i],
              actual_predictions3.compositor_durations[i]);
  }
  EXPECT_EQ(expected_transition1, actual_predictions1.transition_duration);
  EXPECT_EQ(expected_total1, actual_predictions1.total_duration);
  EXPECT_EQ(expected_transition2, actual_predictions2.transition_duration);
  EXPECT_EQ(expected_total2, actual_predictions2.total_duration);
  EXPECT_EQ(expected_transition3, actual_predictions3.transition_duration);
  EXPECT_EQ(expected_total3, actual_predictions3.total_duration);

  pipeline_reporter_ = nullptr;
  histogram_tester.ExpectTotalCount(
      "EventLatency.GestureScrollUpdate.GenerationToBrowserMain", 1);
}

// Tests that when a frame is presented to the user, event latency predictions
// are reported properly for filtered EventTypes.
TEST_F(CompositorFrameReporterTest, EventLatencyMultipleEventTypePredictions) {
  base::HistogramTester histogram_tester;
  std::vector<int> dispatch_times = {
      /*[kGenerated, kArrivedInBrowserMain]=*/300,
      /*[kArrivedInBrowserMain, kArrivedInRendererCompositor]=*/300,
      /*[kArrivedInRendererCompositor, kRendererCompositorStarted]=*/300,
      /*[kRendererCompositorStarted, kRendererCompositorFinished]=*/300,
      /*[kRendererCompositorFinished, kRendererMainStarted]=*/300,
      /*[kRendererMainStarted, kRendererMainFinished]=*/300};
  // The prediction should only be updated with the ScrollUpdateType event,
  // other EventType metrics should be ignored.
  std::unique_ptr<EventMetrics> event_metrics_ptrs[] = {
      CreateEventMetrics(ui::EventType::kTouchPressed),
      CreateEventMetrics(ui::EventType::kTouchMoved),
      CreateScrollUpdateEventMetricsWithDispatchTimes(
          false, ScrollUpdateEventMetrics::ScrollUpdateType::kContinued,
          dispatch_times),
      CreateEventMetrics(ui::EventType::kTouchMoved)};
  // The last EventType::kTouchMoved event above adds 12 us to transition time.
  const base::TimeDelta kTouchEventTransition = base::Microseconds(12);
  EXPECT_THAT(event_metrics_ptrs, Each(NotNull()));
  EventMetrics::List events_metrics = {
      std::make_move_iterator(std::begin(event_metrics_ptrs)),
      std::make_move_iterator(std::end(event_metrics_ptrs))};

  AdvanceNowByUs(300);
  // Total transition time is 312 us because of the extra 3 us from the
  // EventType::kTouchMoved event.
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kBeginImplFrameToSendBeginMainFrame,
      Now());

  // For this test there are only 3 compositor substages updated which reflects
  // a more realistic scenario.

  AdvanceNowByUs(300);  // kBeginImplFrameToSendBeginMainFrame duration
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kEndActivateToSubmitCompositorFrame,
      Now());

  AdvanceNowByUs(300);  // kEndActivateToSubmitCompositorFrame duration
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::
          kSubmitCompositorFrameToPresentationCompositorFrame,
      Now());

  // kSubmitCompositorFrameToPresentationCompositorFrame duration
  AdvanceNowByUs(300);
  pipeline_reporter_->TerminateFrame(
      CompositorFrameReporter::FrameTerminationStatus::kPresentedFrame, Now());

  pipeline_reporter_->AddEventsMetrics(std::move(events_metrics));

  // Test with no previous stage predictions.
  std::vector<base::TimeDelta> expected_dispatch1(kNumDispatchStages,
                                                  base::Microseconds(-1));
  IntToTimeDeltaVector(
      expected_dispatch1,
      std::vector<int>{/*kScrollsBlockingTouchDispatchedToRenderer=*/-1,
                       /*kArrivedInBrowserMain=*/300,
                       /*kArrivedInRendererCompositor=*/300,
                       /*kRendererCompositorStarted=*/300,
                       /*kRendererCompositorFinished=*/300,
                       /*kRendererMainStarted=*/300,
                       /*kRenderePrMainFinished=*/300});
  base::TimeDelta expected_transition1 =
      base::Microseconds(302) + kTouchEventTransition;
  std::vector<base::TimeDelta> expected_compositor1(kNumOfCompositorStages,
                                                    base::Microseconds(-1));
  IntToTimeDeltaVector(expected_compositor1,
                       std::vector<int>{300, -1, -1, -1, -1, 300, 300});
  base::TimeDelta expected_total1 =
      base::Microseconds(3002) + kTouchEventTransition;
  CompositorFrameReporter::EventLatencyInfo actual_predictions1 =
      CompositorFrameReporter::EventLatencyInfo(kNumDispatchStages,
                                                kNumOfCompositorStages);
  pipeline_reporter_->CalculateEventLatencyPrediction(
      actual_predictions1, kLatencyPredictionDeviationThreshold);

  // Test with all previous stage predictions.
  std::vector<base::TimeDelta> expected_dispatch2(kNumDispatchStages,
                                                  base::Microseconds(-1));
  IntToTimeDeltaVector(expected_dispatch2,
                       std::vector<int>{250, 262, 262, 300, 412, 225, 450});
  base::TimeDelta expected_transition2 = base::Microseconds(393);
  std::vector<base::TimeDelta> expected_compositor2(kNumOfCompositorStages,
                                                    base::Microseconds(-1));
  IntToTimeDeltaVector(expected_compositor2,
                       std::vector<int>{465, 500, 90, 720, 410, 742, 390});
  base::TimeDelta expected_total2 = base::Microseconds(5871);
  CompositorFrameReporter::EventLatencyInfo actual_predictions2 =
      CompositorFrameReporter::EventLatencyInfo(kNumDispatchStages,
                                                kNumOfCompositorStages);
  IntToTimeDeltaVector(actual_predictions2.dispatch_durations,
                       std::vector<int>{250, 250, 250, 300, 450, 200, 500});
  actual_predictions2.transition_duration = base::Microseconds(420);
  IntToTimeDeltaVector(actual_predictions2.compositor_durations,
                       std::vector<int>{520, 500, 90, 720, 410, 890, 420});
  pipeline_reporter_->CalculateEventLatencyPrediction(
      actual_predictions2, kLatencyPredictionDeviationThreshold);

  for (int i = 0; i < kNumDispatchStages; i++) {
    EXPECT_EQ(expected_dispatch1[i], actual_predictions1.dispatch_durations[i]);
    EXPECT_EQ(expected_dispatch2[i], actual_predictions2.dispatch_durations[i]);
  }
  for (int i = 0; i < kNumOfCompositorStages; i++) {
    EXPECT_EQ(expected_compositor1[i],
              actual_predictions1.compositor_durations[i]);
    EXPECT_EQ(expected_compositor2[i],
              actual_predictions2.compositor_durations[i]);
  }
  EXPECT_EQ(expected_transition1, actual_predictions1.transition_duration);
  EXPECT_EQ(expected_total1, actual_predictions1.total_duration);
  EXPECT_EQ(expected_transition2, actual_predictions2.transition_duration);
  EXPECT_EQ(expected_total2, actual_predictions2.total_duration);

  pipeline_reporter_ = nullptr;
  histogram_tester.ExpectTotalCount(
      "EventLatency.GestureScrollUpdate.GenerationToBrowserMain", 1);
}

// Tests that when a frame is presented to the user, high latency attribution
// for EventLatency is reported properly for filtered EventTypes.
TEST_F(CompositorFrameReporterTest, EventLatencyAttributionPredictions) {
  base::HistogramTester histogram_tester;
  std::vector<int> dispatch_times = {
      /*[kGenerated, kArrivedInBrowserMain]=*/300,
      /*[kArrivedInBrowserMain, kArrivedInRendererCompositor]=*/300,
      /*[kArrivedInRendererCompositor, kRendererCompositorStarted]=*/300,
      /*[kRendererCompositorStarted, kRendererCompositorFinished]=*/300,
      /*[kRendererCompositorFinished, kRendererMainStarted]=*/50000,
      /*[kRendererMainStarted, kRendererMainFinished]=*/300};
  // The prediction should only be updated with the ScrollUpdateType event,
  // other EventType metrics should be ignored.
  std::unique_ptr<EventMetrics> event_metrics_ptrs[] = {
      CreateScrollUpdateEventMetricsWithDispatchTimes(
          false, ScrollUpdateEventMetrics::ScrollUpdateType::kContinued,
          dispatch_times)};
  EXPECT_THAT(event_metrics_ptrs, Each(NotNull()));
  EventMetrics::List events_metrics = {
      std::make_move_iterator(std::begin(event_metrics_ptrs)),
      std::make_move_iterator(std::end(event_metrics_ptrs))};

  AdvanceNowByUs(300);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kBeginImplFrameToSendBeginMainFrame,
      Now());

  // For this test there are only 3 compositor substages updated which reflects
  // a more realistic scenario.

  AdvanceNowByUs(300);  // kBeginImplFrameToSendBeginMainFrame duration
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kEndActivateToSubmitCompositorFrame,
      Now());

  AdvanceNowByUs(50000);  // kEndActivateToSubmitCompositorFrame duration
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::
          kSubmitCompositorFrameToPresentationCompositorFrame,
      Now());

  // kSubmitCompositorFrameToPresentationCompositorFrame duration
  AdvanceNowByUs(300);
  pipeline_reporter_->TerminateFrame(
      CompositorFrameReporter::FrameTerminationStatus::kPresentedFrame, Now());

  pipeline_reporter_->AddEventsMetrics(std::move(events_metrics));

  // Test with no high latency attribution.
  CompositorFrameReporter::EventLatencyInfo expected_predictions1 =
      CompositorFrameReporter::EventLatencyInfo(kNumDispatchStages,
                                                kNumOfCompositorStages);
  IntToTimeDeltaVector(expected_predictions1.dispatch_durations,
                       std::vector<int>{-1, 300, 300, 300, 300, 50000, 300});
  expected_predictions1.transition_duration = base::Microseconds(300);
  IntToTimeDeltaVector(expected_predictions1.compositor_durations,
                       std::vector<int>{300, -1, -1, -1, -1, 50000, 300});
  expected_predictions1.total_duration = base::Microseconds(102400);

  CompositorFrameReporter::EventLatencyInfo actual_predictions1 =
      CompositorFrameReporter::EventLatencyInfo(kNumDispatchStages,
                                                kNumOfCompositorStages);
  pipeline_reporter_->CalculateEventLatencyPrediction(
      actual_predictions1, kLatencyPredictionDeviationThreshold);

  std::unique_ptr<EventMetrics>& event_metrics =
      pipeline_reporter_->events_metrics_for_testing()[0];
  std::vector<std::string> attribution = event_metrics->GetHighLatencyStages();
  EXPECT_EQ(0u, attribution.size());
  event_metrics->ClearHighLatencyStagesForTesting();

  // Test with 1 high latency stage attributed.
  CompositorFrameReporter::EventLatencyInfo expected_predictions2 =
      CompositorFrameReporter::EventLatencyInfo(kNumDispatchStages,
                                                kNumOfCompositorStages);
  IntToTimeDeltaVector(expected_predictions2.dispatch_durations,
                       std::vector<int>{300, 300, 300, 300, 300, 12725, 300});
  expected_predictions2.transition_duration = base::Microseconds(300);
  IntToTimeDeltaVector(expected_predictions2.compositor_durations,
                       std::vector<int>{300, -1, -1, -1, -1, 50000, 300});
  expected_predictions2.total_duration = base::Microseconds(65425);

  CompositorFrameReporter::EventLatencyInfo actual_predictions2 =
      CompositorFrameReporter::EventLatencyInfo(kNumDispatchStages,
                                                kNumOfCompositorStages);
  IntToTimeDeltaVector(actual_predictions2.dispatch_durations,
                       std::vector<int>{300, 300, 300, 300, 300, 300, 300});
  actual_predictions2.transition_duration = base::Microseconds(300);
  IntToTimeDeltaVector(actual_predictions2.compositor_durations,
                       std::vector<int>{300, -1, -1, -1, -1, 50000, 300});
  actual_predictions2.total_duration = base::Microseconds(5200);
  pipeline_reporter_->CalculateEventLatencyPrediction(
      actual_predictions2, kLatencyPredictionDeviationThreshold);

  attribution = event_metrics->GetHighLatencyStages();
  EXPECT_EQ(1u, attribution.size());
  EXPECT_EQ("RendererCompositorToMain", attribution[0]);
  event_metrics->ClearHighLatencyStagesForTesting();

  // Test with more than 1 high latency stage attributed.
  CompositorFrameReporter::EventLatencyInfo expected_predictions3 =
      CompositorFrameReporter::EventLatencyInfo(kNumDispatchStages,
                                                kNumOfCompositorStages);
  IntToTimeDeltaVector(expected_predictions3.dispatch_durations,
                       std::vector<int>{300, 300, 300, 300, 300, 12725, 300});
  expected_predictions3.transition_duration = base::Microseconds(300);
  IntToTimeDeltaVector(expected_predictions3.compositor_durations,
                       std::vector<int>{300, -1, -1, -1, -1, 12725, 300});
  expected_predictions3.total_duration = base::Microseconds(28150);

  CompositorFrameReporter::EventLatencyInfo actual_predictions3 =
      CompositorFrameReporter::EventLatencyInfo(kNumDispatchStages,
                                                kNumOfCompositorStages);
  IntToTimeDeltaVector(actual_predictions3.dispatch_durations,
                       std::vector<int>{300, 300, 300, 300, 300, 300, 300});
  actual_predictions3.transition_duration = base::Microseconds(300);
  IntToTimeDeltaVector(actual_predictions3.compositor_durations,
                       std::vector<int>{300, -1, -1, -1, -1, 300, 300});
  actual_predictions3.total_duration = base::Microseconds(2700);
  pipeline_reporter_->CalculateEventLatencyPrediction(
      actual_predictions3, kLatencyPredictionDeviationThreshold);

  attribution = event_metrics->GetHighLatencyStages();
  EXPECT_EQ(2u, attribution.size());
  EXPECT_EQ("RendererCompositorToMain", attribution[0]);
  EXPECT_EQ("EndActivateToSubmitCompositorFrame", attribution[1]);

  // Check that all prediction values are accurate.
  for (int i = 0; i < kNumDispatchStages; i++) {
    EXPECT_EQ(expected_predictions1.dispatch_durations[i],
              actual_predictions1.dispatch_durations[i]);
    EXPECT_EQ(expected_predictions2.dispatch_durations[i],
              actual_predictions2.dispatch_durations[i]);
    EXPECT_EQ(expected_predictions3.dispatch_durations[i],
              actual_predictions3.dispatch_durations[i]);
  }
  for (int i = 0; i < kNumOfCompositorStages; i++) {
    EXPECT_EQ(expected_predictions1.compositor_durations[i],
              actual_predictions1.compositor_durations[i]);
    EXPECT_EQ(expected_predictions2.compositor_durations[i],
              actual_predictions2.compositor_durations[i]);
    EXPECT_EQ(expected_predictions3.compositor_durations[i],
              actual_predictions3.compositor_durations[i]);
  }
  EXPECT_EQ(expected_predictions1.transition_duration,
            actual_predictions1.transition_duration);
  EXPECT_EQ(expected_predictions1.total_duration,
            actual_predictions1.total_duration);
  EXPECT_EQ(expected_predictions2.transition_duration,
            actual_predictions2.transition_duration);
  EXPECT_EQ(expected_predictions2.total_duration,
            actual_predictions2.total_duration);
  EXPECT_EQ(expected_predictions3.transition_duration,
            actual_predictions3.transition_duration);
  EXPECT_EQ(expected_predictions3.total_duration,
            actual_predictions3.total_duration);

  pipeline_reporter_ = nullptr;
  histogram_tester.ExpectTotalCount(
      "EventLatency.GestureScrollUpdate.GenerationToBrowserMain", 1);
}

// Tests that when a frame is presented to the user, high latency attribution
// for EventLatency is reported properly for filtered EventTypes.
TEST_F(CompositorFrameReporterTest, EventLatencyAttributionChangePredictions) {
  base::HistogramTester histogram_tester;
  std::vector<int> dispatch_times = {
      /*[kGenerated, kArrivedInBrowserMain]=*/40000,
      /*[kArrivedInBrowserMain, kArrivedInRendererCompositor]=*/150,
      /*[kArrivedInRendererCompositor, kRendererCompositorStarted]=*/-1,
      /*[kRendererCompositorStarted, kRendererCompositorFinished]=*/-1,
      /*[kRendererCompositorFinished, kRendererMainStarted]=*/150,
      /*[kRendererMainStarted, kRendererMainFinished]=*/50000};

  // The prediction should only be updated with the ScrollUpdateType event,
  // other EventType metrics should be ignored.
  std::unique_ptr<EventMetrics> event_metrics_ptrs[] = {
      CreateScrollUpdateEventMetricsWithDispatchTimes(
          false, ScrollUpdateEventMetrics::ScrollUpdateType::kContinued,
          dispatch_times)};
  EXPECT_THAT(event_metrics_ptrs, Each(NotNull()));
  EventMetrics::List events_metrics = {
      std::make_move_iterator(std::begin(event_metrics_ptrs)),
      std::make_move_iterator(std::end(event_metrics_ptrs))};

  AdvanceNowByUs(300);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kBeginImplFrameToSendBeginMainFrame,
      Now());

  // For this test there are only 3 compositor substages updated which reflects
  // a more realistic scenario.

  AdvanceNowByUs(300);  // kBeginImplFrameToSendBeginMainFrame duration
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kEndActivateToSubmitCompositorFrame,
      Now());

  AdvanceNowByUs(50000);  // kEndActivateToSubmitCompositorFrame duration
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::
          kSubmitCompositorFrameToPresentationCompositorFrame,
      Now());

  // kSubmitCompositorFrameToPresentationCompositorFrame duration
  AdvanceNowByUs(300);
  pipeline_reporter_->TerminateFrame(
      CompositorFrameReporter::FrameTerminationStatus::kPresentedFrame, Now());

  pipeline_reporter_->AddEventsMetrics(std::move(events_metrics));

  // Test 1
  CompositorFrameReporter::EventLatencyInfo expected_predictions1 =
      CompositorFrameReporter::EventLatencyInfo(kNumDispatchStages,
                                                kNumOfCompositorStages);
  IntToTimeDeltaVector(expected_predictions1.dispatch_durations,
                       std::vector<int>{-1, 10300, 262, -1, -1, 262, 42500});
  expected_predictions1.transition_duration = base::Microseconds(300);
  IntToTimeDeltaVector(expected_predictions1.compositor_durations,
                       std::vector<int>{300, -1, -1, -1, -1, 15200, 300});
  expected_predictions1.total_duration = base::Microseconds(69424);

  CompositorFrameReporter::EventLatencyInfo actual_predictions1 =
      CompositorFrameReporter::EventLatencyInfo(kNumDispatchStages,
                                                kNumOfCompositorStages);
  IntToTimeDeltaVector(actual_predictions1.dispatch_durations,
                       std::vector<int>{-1, 400, 300, -1, -1, 300, 40000});
  actual_predictions1.transition_duration = base::Microseconds(300);
  IntToTimeDeltaVector(actual_predictions1.compositor_durations,
                       std::vector<int>{300, -1, -1, -1, -1, 3600, 300});
  actual_predictions1.total_duration = base::Microseconds(45500);
  pipeline_reporter_->CalculateEventLatencyPrediction(
      actual_predictions1, kLatencyPredictionDeviationThreshold);

  std::unique_ptr<EventMetrics>& event_metrics =
      pipeline_reporter_->events_metrics_for_testing()[0];
  std::vector<std::string> attribution = event_metrics->GetHighLatencyStages();
  EXPECT_EQ(2, static_cast<int>(attribution.size()));
  EXPECT_EQ("GenerationToBrowserMain", attribution[0]);
  EXPECT_EQ("EndActivateToSubmitCompositorFrame", attribution[1]);
  event_metrics->ClearHighLatencyStagesForTesting();

  // Test 2
  CompositorFrameReporter::EventLatencyInfo expected_predictions2 =
      CompositorFrameReporter::EventLatencyInfo(kNumDispatchStages,
                                                kNumOfCompositorStages);
  IntToTimeDeltaVector(expected_predictions2.dispatch_durations,
                       std::vector<int>{300, 10225, 262, -1, -1, 262, 12725});
  expected_predictions2.transition_duration = base::Microseconds(300);

  IntToTimeDeltaVector(expected_predictions2.compositor_durations,
                       std::vector<int>{300, -1, -1, -1, -1, 12725, 300});
  expected_predictions2.total_duration = base::Microseconds(37399);

  CompositorFrameReporter::EventLatencyInfo actual_predictions2 =
      CompositorFrameReporter::EventLatencyInfo(kNumDispatchStages,
                                                kNumOfCompositorStages);
  IntToTimeDeltaVector(actual_predictions2.dispatch_durations,
                       std::vector<int>{300, 300, 300, -1, -1, 300, 300});
  actual_predictions2.transition_duration = base::Microseconds(300);
  IntToTimeDeltaVector(actual_predictions2.compositor_durations,
                       std::vector<int>{300, -1, -1, -1, -1, 300, 300});
  actual_predictions2.total_duration = base::Microseconds(2100);
  pipeline_reporter_->CalculateEventLatencyPrediction(
      actual_predictions2, kLatencyPredictionDeviationThreshold);

  attribution = event_metrics->GetHighLatencyStages();
  EXPECT_EQ(2u, attribution.size());
  EXPECT_EQ("RendererMainProcessing", attribution[0]);
  EXPECT_EQ("EndActivateToSubmitCompositorFrame", attribution[1]);

  // Check that all prediction values are accurate.
  for (int i = 0; i < kNumDispatchStages; i++) {
    EXPECT_EQ(expected_predictions1.dispatch_durations[i],
              actual_predictions1.dispatch_durations[i]);
    EXPECT_EQ(expected_predictions2.dispatch_durations[i],
              actual_predictions2.dispatch_durations[i]);
  }
  for (int i = 0; i < kNumOfCompositorStages; i++) {
    EXPECT_EQ(expected_predictions1.compositor_durations[i],
              actual_predictions1.compositor_durations[i]);
    EXPECT_EQ(expected_predictions2.compositor_durations[i],
              actual_predictions2.compositor_durations[i]);
  }
  EXPECT_EQ(expected_predictions1.transition_duration,
            actual_predictions1.transition_duration);
  EXPECT_EQ(expected_predictions1.total_duration,
            actual_predictions1.total_duration);
  EXPECT_EQ(expected_predictions2.transition_duration,
            actual_predictions2.transition_duration);
  EXPECT_EQ(expected_predictions2.total_duration,
            actual_predictions2.total_duration);

  pipeline_reporter_ = nullptr;
  histogram_tester.ExpectTotalCount(
      "EventLatency.GestureScrollUpdate.GenerationToBrowserMain", 1);
}

}  // namespace
}  // namespace cc
