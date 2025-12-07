// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/ukm_manager.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "cc/base/features.h"
#include "cc/metrics/begin_main_frame_metrics.h"
#include "cc/metrics/compositor_frame_reporter.h"
#include "cc/metrics/event_metrics.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/viz/common/frame_timing_details.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace cc {
namespace {

const char kTestUrl[] = "https://example.com/foo";
const int64_t kTestSourceId1 = 100;

// Names of compositor/event latency UKM events.
const char kCompositorLatency[] = "Graphics.Smoothness.Latency";
const char kEventLatency[] = "Graphics.Smoothness.EventLatency";

// Names of enum metrics used in compositor/event latency UKM metrics.
const char kMissedFrame[] = "MissedFrame";
const char kEventType[] = "EventType";
const char kScrollInputType[] = "ScrollInputType";

// Names of compositor stages and substages used in compositor/event latency UKM
// metrics.
const char kGenerationToRendererCompositor[] = "GenerationToRendererCompositor";
const char kRendererCompositorQueueingDelay[] =
    "RendererCompositorQueueingDelay";
const char kRendererCompositorProcessing[] = "RendererCompositorProcessing";
const char kRendererCompositorToMain[] = "RendererCompositorToMain";
const char kRendererMainProcessing[] = "RendererMainProcessing";
const char kRendererMainFinishedToBeginImplFrame[] =
    "RendererMainFinishedToBeginImplFrame";
const char kBeginImplFrameToSendBeginMainFrame[] =
    "BeginImplFrameToSendBeginMainFrame";
const char kSendBeginMainFrameToCommit[] = "SendBeginMainFrameToCommit";
const char kBlinkBreakdownHandleInputEvents[] =
    "SendBeginMainFrameToCommit.HandleInputEvents";
const char kBlinkBreakdownAnimate[] = "SendBeginMainFrameToCommit.Animate";
const char kBlinkBreakdownStyleUpdate[] =
    "SendBeginMainFrameToCommit.StyleUpdate";
const char kBlinkBreakdownLayoutUpdate[] =
    "SendBeginMainFrameToCommit.LayoutUpdate";
const char kBlinkBreakdownPrepaint[] = "SendBeginMainFrameToCommit.Prepaint";
const char kBlinkBreakdownCompositingInputs[] =
    "SendBeginMainFrameToCommit.CompositingInputs";
const char kBlinkBreakdownPaint[] = "SendBeginMainFrameToCommit.Paint";
const char kBlinkBreakdownCompositeCommit[] =
    "SendBeginMainFrameToCommit.CompositeCommit";
const char kBlinkBreakdownUpdateLayers[] =
    "SendBeginMainFrameToCommit.UpdateLayers";
const char kBlinkBreakdownBeginMainSentToStarted[] =
    "SendBeginMainFrameToCommit.BeginMainSentToStarted";
const char kCommit[] = "Commit";
const char kEndCommitToActivation[] = "EndCommitToActivation";
const char kActivation[] = "Activation";
const char kEndActivateToSubmitCompositorFrame[] =
    "EndActivateToSubmitCompositorFrame";
const char kSubmitCompositorFrameToPresentationCompositorFrame[] =
    "SubmitCompositorFrameToPresentationCompositorFrame";
const char kEndActivateToSubmitUpdateDisplayTree[] =
    "EndActivateToSubmitUpdateDisplayTree";
const char kSubmitUpdateDisplayTreeToPresentationCompositorFrame[] =
    "SubmitUpdateDisplayTreeToPresentationCompositorFrame";
// TreesInViz specific substages
const char kEndActivateToDrawLayers[] = "EndActivateToDrawLayers";
const char kDrawLayersToSubmitUpdateDisplayTree[] =
    "DrawLayersToSubmitUpdateDisplayTree";
const char kSendUpdateDisplayTreeToReceiveUpdateDisplayTree[] =
    "SendUpdateDisplayTreeToReceiveUpdateDisplayTree";
const char kReceiveUpdateDisplayTreeToStartPrepareToDraw[] =
    "ReceiveUpdateDisplayTreeToStartPrepareToDraw";
const char kStartPrepareToDrawToStartDrawLayers[] =
    "StartPrepareToDrawToStartDrawLayers";
const char kStartDrawLayersToSubmitCompositorFrame[] =
    "StartDrawLayersToSubmitCompositorFrame";
const char kVizBreakdownSubmitToReceiveCompositorFrame[] =
    "SubmitToReceiveCompositorFrame";
const char kVizBreakdownReceivedCompositorFrameToStartDraw[] =
    "ReceivedCompositorFrameToStartDraw";
const char kVizBreakdownStartDrawToSwapStart[] = "StartDrawToSwapStart";
const char kVizBreakdownSwapStartToSwapEnd[] = "SwapStartToSwapEnd";
const char kVizBreakdownSwapStartToBufferAvailable[] =
    "SwapStartToBufferAvailable";
const char kVizBreakdownBufferAvailableToBufferReady[] =
    "BufferAvailableToBufferReady";
const char kVizBreakdownBufferReadyToLatch[] = "BufferReadyToLatch";
const char kVizBreakdownLatchToSwapEnd[] = "LatchToSwapEnd";
const char kVizBreakdownSwapEndToPresentationCompositorFrame[] =
    "SwapEndToPresentationCompositorFrame";
const char kTotalLatency[] = "TotalLatency";

// Names of frame sequence types use in compositor latency UKM metrics (see
// FrameSequenceTrackerType enum).
const char kCompositorAnimation[] = "CompositorAnimation";
const char kMainThreadAnimation[] = "MainThreadAnimation";
const char kPinchZoom[] = "PinchZoom";
const char kRAF[] = "RAF";
const char kScrollbarScroll[] = "ScrollbarScroll";
const char kTouchScroll[] = "TouchScroll";
const char kVideo[] = "Video";
const char kWheelScroll[] = "WheelScroll";

std::string SubstageName(std::string& stage_name, std::string substage_name) {
  return base::StrCat({stage_name, ".", substage_name});
}

class UkmManagerTest : public testing::Test {
 public:
  UkmManagerTest() {
    auto recorder = std::make_unique<ukm::TestUkmRecorder>();
    recorder->UpdateSourceURL(kTestSourceId1, GURL(kTestUrl));
    manager_ = std::make_unique<UkmManager>(std::move(recorder));

    // In production, new UKM Source would have been already created, so
    // manager only needs to know the source id.

    manager_->SetSourceId(kTestSourceId1);
  }

  ~UkmManagerTest() override = default;

 protected:
  base::TimeTicks AdvanceNowByMs(int advance_ms) {
    test_tick_clock_.Advance(base::Microseconds(advance_ms));
    return test_tick_clock_.NowTicks();
  }

  ukm::TestUkmRecorder* recorder() {
    return static_cast<ukm::TestUkmRecorder*>(manager_->recorder());
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
      AdvanceNowByMs(10);
      metrics->SetDispatchStageTimestamp(
          EventMetrics::DispatchStage::kRendererMainStarted);
      AdvanceNowByMs(10);
      metrics->SetDispatchStageTimestamp(
          EventMetrics::DispatchStage::kRendererMainFinished);
    }
    return metrics;
  }

  std::unique_ptr<EventMetrics> CreateScrollBeginEventMetrics() {
    base::TimeTicks event_time = AdvanceNowByMs(10);
    base::TimeTicks arrived_in_browser_main_timestamp = AdvanceNowByMs(5);
    AdvanceNowByMs(10);
    return SetupEventMetrics(ScrollEventMetrics::CreateForTesting(
        ui::EventType::kGestureScrollBegin, ui::ScrollInputType::kWheel,
        /*is_inertial=*/false, event_time, arrived_in_browser_main_timestamp,
        &test_tick_clock_));
  }

  std::unique_ptr<EventMetrics> CreateScrollUpdateEventMetrics(
      bool is_inertial,
      ScrollUpdateEventMetrics::ScrollUpdateType scroll_update_type) {
    base::TimeTicks event_time = AdvanceNowByMs(10);
    base::TimeTicks arrived_in_browser_main_timestamp = AdvanceNowByMs(5);
    AdvanceNowByMs(10);
    return SetupEventMetrics(ScrollUpdateEventMetrics::CreateForTesting(
        ui::EventType::kGestureScrollUpdate, ui::ScrollInputType::kWheel,
        is_inertial, scroll_update_type, /*delta=*/10.0f, event_time,
        arrived_in_browser_main_timestamp, &test_tick_clock_, std::nullopt));
  }

  struct DispatchTimestamps {
    base::TimeTicks generated;
    base::TimeTicks arrived_in_renderer;
    base::TimeTicks renderer_compositor_started;
    base::TimeTicks renderer_compositor_finished;
    base::TimeTicks renderer_main_started;
    base::TimeTicks renderer_main_finished;
  };
  std::vector<DispatchTimestamps> GetEventDispatchTimestamps(
      const EventMetrics::List& events_metrics) {
    std::vector<DispatchTimestamps> event_times;
    event_times.reserve(events_metrics.size());
    std::ranges::transform(
        events_metrics, std::back_inserter(event_times),
        [](const auto& event_metrics) {
          return DispatchTimestamps{
              event_metrics->GetDispatchStageTimestamp(
                  EventMetrics::DispatchStage::kGenerated),
              event_metrics->GetDispatchStageTimestamp(
                  EventMetrics::DispatchStage::kArrivedInRendererCompositor),
              event_metrics->GetDispatchStageTimestamp(
                  EventMetrics::DispatchStage::kRendererCompositorStarted),
              event_metrics->GetDispatchStageTimestamp(
                  EventMetrics::DispatchStage::kRendererCompositorFinished),
              event_metrics->GetDispatchStageTimestamp(
                  EventMetrics::DispatchStage::kRendererMainStarted),
              event_metrics->GetDispatchStageTimestamp(
                  EventMetrics::DispatchStage::kRendererMainFinished),
          };
        });
    return event_times;
  }

  BeginMainFrameMetrics BuildBlinkBreakdown() {
    BeginMainFrameMetrics breakdown;
    breakdown.handle_input_events = base::Microseconds(10);
    breakdown.animate = base::Microseconds(9);
    breakdown.style_update = base::Microseconds(8);
    breakdown.layout_update = base::Microseconds(7);
    breakdown.compositing_inputs = base::Microseconds(6);
    breakdown.prepaint = base::Microseconds(5);
    breakdown.paint = base::Microseconds(3);
    breakdown.composite_commit = base::Microseconds(2);
    breakdown.update_layers = base::Microseconds(1);

    // Advance now by the sum of the breakdowns.
    AdvanceNowByMs(10 + 9 + 8 + 7 + 6 + 5 + 3 + 2 + 1);

    return breakdown;
  }

  viz::FrameTimingDetails BuildTreesInVizBreakdown() {
    viz::FrameTimingDetails breakdown;
    // Trees-in-viz relevant timestamps
    breakdown.start_update_display_tree = AdvanceNowByMs(1);
    breakdown.start_prepare_to_draw = AdvanceNowByMs(2);
    breakdown.start_draw_layers = AdvanceNowByMs(3);
    breakdown.submit_compositor_frame = AdvanceNowByMs(4);

    return breakdown;
  }

  viz::FrameTimingDetails BuildVizBreakdown(bool trees_in_viz_enabled) {
    viz::FrameTimingDetails breakdown;
    if (trees_in_viz_enabled) {
      breakdown = BuildTreesInVizBreakdown();
    }
    breakdown.received_compositor_frame_timestamp = AdvanceNowByMs(1);
    breakdown.draw_start_timestamp = AdvanceNowByMs(2);
    breakdown.swap_timings.swap_start = AdvanceNowByMs(3);
    breakdown.presentation_feedback.available_timestamp = AdvanceNowByMs(1);
    breakdown.presentation_feedback.ready_timestamp = AdvanceNowByMs(1);
    breakdown.presentation_feedback.latch_timestamp = AdvanceNowByMs(1);
    breakdown.swap_timings.swap_end = AdvanceNowByMs(1);
    breakdown.presentation_feedback.timestamp = AdvanceNowByMs(5);
    return breakdown;
  }
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<UkmManager> manager_;
  base::SimpleTestTickClock test_tick_clock_;
};

class UkmManagerCompositorLatencyTest
    : public UkmManagerTest,
      public testing::WithParamInterface<
          CompositorFrameReporter::FrameReportType> {
 public:
  UkmManagerCompositorLatencyTest() {
    report_types_.set(static_cast<size_t>(GetParam()));
  }
  ~UkmManagerCompositorLatencyTest() override = default;

 protected:
  CompositorFrameReporter::FrameReportType report_type() const {
    for (size_t type = 0; type < report_types_.size(); ++type) {
      if (!report_types_.test(type))
        continue;
      return static_cast<CompositorFrameReporter::FrameReportType>(type);
    }
    return CompositorFrameReporter::FrameReportType::kNonDroppedFrame;
  }
  const CompositorFrameReporter::FrameReportTypes& report_types() const {
    return report_types_;
  }

 private:
  CompositorFrameReporter::FrameReportTypes report_types_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    UkmManagerCompositorLatencyTest,
    testing::Values(
        CompositorFrameReporter::FrameReportType::kNonDroppedFrame,
        CompositorFrameReporter::FrameReportType::kMissedDeadlineFrame,
        CompositorFrameReporter::FrameReportType::kDroppedFrame,
        CompositorFrameReporter::FrameReportType::kCompositorOnlyFrame));

TEST_P(UkmManagerCompositorLatencyTest, CompositorLatency) {
  bool trees_in_viz_mode = base::FeatureList::IsEnabled(features::kTreesInViz);
  const base::TimeTicks begin_impl_time = AdvanceNowByMs(10);
  const base::TimeTicks begin_main_time = AdvanceNowByMs(10);
  const base::TimeTicks begin_main_start_time = AdvanceNowByMs(10);

  BeginMainFrameMetrics blink_breakdown = BuildBlinkBreakdown();

  const base::TimeTicks begin_commit_time = AdvanceNowByMs(10);
  const base::TimeTicks end_commit_time = AdvanceNowByMs(10);
  const base::TimeTicks begin_activate_time = AdvanceNowByMs(10);
  const base::TimeTicks end_activate_time = AdvanceNowByMs(
      10);  // For trees in viz, this will be trees_in_viz_activate_time_
  const base::TimeTicks submit_time =
      AdvanceNowByMs(10);  // For TreesInViz, this will be branch time.
  // Extra stage for TreesinViz
  const base::TimeTicks trees_in_viz_viz_start_time = AdvanceNowByMs(11);

  viz::FrameTimingDetails frame_timing_details =
      BuildVizBreakdown(trees_in_viz_mode);

  std::vector<CompositorFrameReporter::StageData> stage_history = {
      {
          CompositorFrameReporter::StageType::
              kBeginImplFrameToSendBeginMainFrame,
          begin_impl_time,
          begin_main_time,
      },
      {
          CompositorFrameReporter::StageType::kSendBeginMainFrameToCommit,
          begin_main_time,
          begin_commit_time,
      },
      {
          CompositorFrameReporter::StageType::kCommit,
          begin_commit_time,
          end_commit_time,
      },
      {
          CompositorFrameReporter::StageType::kEndCommitToActivation,
          end_commit_time,
          begin_activate_time,
      },
      {
          CompositorFrameReporter::StageType::kActivation,
          begin_activate_time,
          end_activate_time,
      },
      {
          CompositorFrameReporter::StageType::kTotalLatency,
          begin_impl_time,
          frame_timing_details.presentation_feedback.timestamp,
      },
  };

  if (trees_in_viz_mode) {
    // TreesInViz branch
    stage_history.emplace_back(CompositorFrameReporter::StageType::
                                   kEndActivateToSubmitUpdateDisplayTree,
                               end_activate_time, trees_in_viz_viz_start_time);
    stage_history.emplace_back(
        CompositorFrameReporter::StageType::
            kSubmitUpdateDisplayTreeToPresentationCompositorFrame,
        trees_in_viz_viz_start_time,
        frame_timing_details.presentation_feedback.timestamp);
  } else {
    stage_history.emplace_back(  // Normal branch
        CompositorFrameReporter::StageType::kEndActivateToSubmitCompositorFrame,
        end_activate_time, submit_time);
    stage_history.emplace_back(
        CompositorFrameReporter::StageType::
            kSubmitCompositorFrameToPresentationCompositorFrame,
        submit_time, frame_timing_details.presentation_feedback.timestamp);
  }

  ActiveTrackers active_trackers;
  active_trackers.set(
      static_cast<size_t>(FrameSequenceTrackerType::kScrollbarScroll));
  active_trackers.set(
      static_cast<size_t>(FrameSequenceTrackerType::kTouchScroll));
  active_trackers.set(
      static_cast<size_t>(FrameSequenceTrackerType::kCompositorAnimation));

  CompositorFrameReporter::ProcessedBlinkBreakdown processed_blink_breakdown(
      begin_main_time, begin_main_start_time, blink_breakdown);
  CompositorFrameReporter::ProcessedVizBreakdown processed_viz_breakdown(
      submit_time, frame_timing_details);
  CompositorFrameReporter::ProcessedTreesInVizBreakdown
      processed_trees_in_viz_breakdown(end_activate_time, submit_time,
                                       trees_in_viz_viz_start_time,
                                       frame_timing_details);
  manager_->RecordCompositorLatencyUKM(
      report_types(), stage_history, active_trackers, processed_blink_breakdown,
      processed_viz_breakdown, processed_trees_in_viz_breakdown);

  const auto& entries = recorder()->GetEntriesByName(kCompositorLatency);
  EXPECT_EQ(1u, entries.size());
  const auto* entry = entries[0].get();

  EXPECT_NE(ukm::kInvalidSourceId, entry->source_id);
  recorder()->ExpectEntrySourceHasUrl(entry, GURL(kTestUrl));

  if (report_type() ==
      CompositorFrameReporter::FrameReportType::kDroppedFrame) {
    recorder()->ExpectEntryMetric(entry, kMissedFrame, true);
  } else {
    EXPECT_FALSE(recorder()->EntryHasMetric(entry, kMissedFrame));
  }

  recorder()->ExpectEntryMetric(
      entry, kBeginImplFrameToSendBeginMainFrame,
      (begin_main_time - begin_impl_time).InMicroseconds());
  recorder()->ExpectEntryMetric(
      entry, kSendBeginMainFrameToCommit,
      (begin_commit_time - begin_main_time).InMicroseconds());
  recorder()->ExpectEntryMetric(
      entry, kBlinkBreakdownHandleInputEvents,
      blink_breakdown.handle_input_events.InMicroseconds());
  recorder()->ExpectEntryMetric(entry, kBlinkBreakdownAnimate,
                                blink_breakdown.animate.InMicroseconds());
  recorder()->ExpectEntryMetric(entry, kBlinkBreakdownStyleUpdate,
                                blink_breakdown.style_update.InMicroseconds());
  recorder()->ExpectEntryMetric(entry, kBlinkBreakdownLayoutUpdate,
                                blink_breakdown.layout_update.InMicroseconds());
  recorder()->ExpectEntryMetric(entry, kBlinkBreakdownPrepaint,
                                blink_breakdown.prepaint.InMicroseconds());
  recorder()->ExpectEntryMetric(
      entry, kBlinkBreakdownCompositingInputs,
      blink_breakdown.compositing_inputs.InMicroseconds());
  recorder()->ExpectEntryMetric(entry, kBlinkBreakdownPaint,
                                blink_breakdown.paint.InMicroseconds());
  recorder()->ExpectEntryMetric(
      entry, kBlinkBreakdownCompositeCommit,
      blink_breakdown.composite_commit.InMicroseconds());
  recorder()->ExpectEntryMetric(entry, kBlinkBreakdownUpdateLayers,
                                blink_breakdown.update_layers.InMicroseconds());
  recorder()->ExpectEntryMetric(
      entry, kBlinkBreakdownBeginMainSentToStarted,
      (begin_main_start_time - begin_main_time).InMicroseconds());
  recorder()->ExpectEntryMetric(
      entry, kCommit, (end_commit_time - begin_commit_time).InMicroseconds());
  recorder()->ExpectEntryMetric(
      entry, kEndCommitToActivation,
      (begin_activate_time - end_commit_time).InMicroseconds());
  recorder()->ExpectEntryMetric(
      entry, kActivation,
      (end_activate_time - begin_activate_time).InMicroseconds());

  // After Activate, CompositorFrameReporter stages branch
  std::string activate_stage =
      (trees_in_viz_mode ? kEndActivateToSubmitUpdateDisplayTree
                         : kEndActivateToSubmitCompositorFrame);
  std::string submit_stage =
      (trees_in_viz_mode ? kSubmitUpdateDisplayTreeToPresentationCompositorFrame
                         : kSubmitCompositorFrameToPresentationCompositorFrame);

  base::TimeTicks rpc_submit_time =
      (trees_in_viz_mode ? trees_in_viz_viz_start_time : submit_time);
  recorder()->ExpectEntryMetric(
      entry, activate_stage,
      (rpc_submit_time - end_activate_time).InMicroseconds());
  recorder()->ExpectEntryMetric(
      entry, submit_stage,
      (frame_timing_details.presentation_feedback.timestamp - rpc_submit_time)
          .InMicroseconds());

  // Tests for new substages introduced by TreesInViz
  if (trees_in_viz_mode) {
    // In CC
    recorder()->ExpectEntryMetric(
        entry, SubstageName(activate_stage, kEndActivateToDrawLayers),
        (submit_time - end_activate_time).InMicroseconds());

    recorder()->ExpectEntryMetric(
        entry,
        SubstageName(activate_stage, kDrawLayersToSubmitUpdateDisplayTree),
        (trees_in_viz_viz_start_time - submit_time).InMicroseconds());
    // CC -> Viz
    recorder()->ExpectEntryMetric(
        entry,
        SubstageName(submit_stage,
                     kSendUpdateDisplayTreeToReceiveUpdateDisplayTree),
        (frame_timing_details.start_update_display_tree -
         trees_in_viz_viz_start_time)
            .InMicroseconds());
    // Viz
    recorder()->ExpectEntryMetric(
        entry,
        SubstageName(submit_stage,
                     kReceiveUpdateDisplayTreeToStartPrepareToDraw),
        (frame_timing_details.start_prepare_to_draw -
         frame_timing_details.start_update_display_tree)
            .InMicroseconds());
    recorder()->ExpectEntryMetric(
        entry, SubstageName(submit_stage, kStartPrepareToDrawToStartDrawLayers),
        (frame_timing_details.start_draw_layers -
         frame_timing_details.start_prepare_to_draw)
            .InMicroseconds());
    recorder()->ExpectEntryMetric(
        entry,
        SubstageName(submit_stage, kStartDrawLayersToSubmitCompositorFrame),
        (frame_timing_details.submit_compositor_frame -
         frame_timing_details.start_draw_layers)
            .InMicroseconds());
  }
  base::TimeTicks compositor_frame_submit_time =
      (trees_in_viz_mode ? frame_timing_details.submit_compositor_frame
                         : submit_time);
  // Stages present in both paths
  recorder()->ExpectEntryMetric(
      entry,
      SubstageName(submit_stage, kVizBreakdownSubmitToReceiveCompositorFrame),
      (frame_timing_details.received_compositor_frame_timestamp -
       compositor_frame_submit_time)
          .InMicroseconds());
  recorder()->ExpectEntryMetric(
      entry,
      SubstageName(submit_stage,
                   kVizBreakdownReceivedCompositorFrameToStartDraw),
      (frame_timing_details.draw_start_timestamp -
       frame_timing_details.received_compositor_frame_timestamp)
          .InMicroseconds());
  recorder()->ExpectEntryMetric(
      entry, SubstageName(submit_stage, kVizBreakdownStartDrawToSwapStart),
      (frame_timing_details.swap_timings.swap_start -
       frame_timing_details.draw_start_timestamp)
          .InMicroseconds());
  recorder()->ExpectEntryMetric(
      entry, SubstageName(submit_stage, kVizBreakdownSwapStartToSwapEnd),
      (frame_timing_details.swap_timings.swap_end -
       frame_timing_details.swap_timings.swap_start)
          .InMicroseconds());
  recorder()->ExpectEntryMetric(
      entry,
      SubstageName(submit_stage, kVizBreakdownSwapStartToBufferAvailable),
      (frame_timing_details.presentation_feedback.available_timestamp -
       frame_timing_details.swap_timings.swap_start)
          .InMicroseconds());
  recorder()->ExpectEntryMetric(
      entry,
      SubstageName(submit_stage, kVizBreakdownBufferAvailableToBufferReady),
      (frame_timing_details.presentation_feedback.ready_timestamp -
       frame_timing_details.presentation_feedback.available_timestamp)
          .InMicroseconds());
  recorder()->ExpectEntryMetric(
      entry, SubstageName(submit_stage, kVizBreakdownBufferReadyToLatch),
      (frame_timing_details.presentation_feedback.latch_timestamp -
       frame_timing_details.presentation_feedback.ready_timestamp)
          .InMicroseconds());
  recorder()->ExpectEntryMetric(
      entry, SubstageName(submit_stage, kVizBreakdownLatchToSwapEnd),
      (frame_timing_details.swap_timings.swap_end -
       frame_timing_details.presentation_feedback.latch_timestamp)
          .InMicroseconds());
  recorder()->ExpectEntryMetric(
      entry,
      SubstageName(submit_stage,
                   kVizBreakdownSwapEndToPresentationCompositorFrame),
      (frame_timing_details.presentation_feedback.timestamp -
       frame_timing_details.swap_timings.swap_end)
          .InMicroseconds());
  recorder()->ExpectEntryMetric(
      entry, kTotalLatency,
      (frame_timing_details.presentation_feedback.timestamp - begin_impl_time)
          .InMicroseconds());

  recorder()->ExpectEntryMetric(entry, kCompositorAnimation, true);
  recorder()->ExpectEntryMetric(entry, kTouchScroll, true);
  recorder()->ExpectEntryMetric(entry, kScrollbarScroll, true);
  EXPECT_FALSE(recorder()->EntryHasMetric(entry, kMainThreadAnimation));
  EXPECT_FALSE(recorder()->EntryHasMetric(entry, kPinchZoom));
  EXPECT_FALSE(recorder()->EntryHasMetric(entry, kRAF));
  EXPECT_FALSE(recorder()->EntryHasMetric(entry, kVideo));
  EXPECT_FALSE(recorder()->EntryHasMetric(entry, kWheelScroll));
}

// TODO(crbug.com/443785891): EventLatency metrics support TreesInViz stages.
TEST_F(UkmManagerTest, EventLatency) {
  const bool kScrollIsInertial = true;
  const bool kScrollIsNotInertial = false;
  std::unique_ptr<EventMetrics> event_metrics_ptrs[] = {
      CreateScrollBeginEventMetrics(),
      CreateScrollUpdateEventMetrics(
          kScrollIsNotInertial,
          ScrollUpdateEventMetrics::ScrollUpdateType::kStarted),
      CreateScrollUpdateEventMetrics(
          kScrollIsNotInertial,
          ScrollUpdateEventMetrics::ScrollUpdateType::kContinued),
      CreateScrollUpdateEventMetrics(
          kScrollIsInertial,
          ScrollUpdateEventMetrics::ScrollUpdateType::kContinued),
  };
  EXPECT_THAT(event_metrics_ptrs, ::testing::Each(::testing::NotNull()));
  EventMetrics::List events_metrics(
      std::make_move_iterator(std::begin(event_metrics_ptrs)),
      std::make_move_iterator(std::end(event_metrics_ptrs)));
  std::vector<DispatchTimestamps> event_dispatch_times =
      GetEventDispatchTimestamps(events_metrics);

  const base::TimeTicks begin_impl_time = AdvanceNowByMs(10);
  const base::TimeTicks begin_main_time = AdvanceNowByMs(10);
  const base::TimeTicks begin_main_start_time = AdvanceNowByMs(10);

  BeginMainFrameMetrics blink_breakdown = BuildBlinkBreakdown();

  const base::TimeTicks begin_commit_time = AdvanceNowByMs(10);
  const base::TimeTicks end_commit_time = AdvanceNowByMs(10);
  const base::TimeTicks begin_activate_time = AdvanceNowByMs(10);
  const base::TimeTicks end_activate_time = AdvanceNowByMs(
      10);  // For trees in viz, this will be trees_in_viz_activate_time_
  const base::TimeTicks submit_time =
      AdvanceNowByMs(10);  // For TreesInViz, this will be branch time.

  // Extra stages for TreesinViz
  const base::TimeTicks trees_in_viz_viz_start_time = AdvanceNowByMs(11);

  viz::FrameTimingDetails frame_timing_details =
      BuildVizBreakdown(base::FeatureList::IsEnabled(features::kTreesInViz));

  std::vector<CompositorFrameReporter::StageData> stage_history = {
      {
          CompositorFrameReporter::StageType::
              kBeginImplFrameToSendBeginMainFrame,
          begin_impl_time,
          begin_main_time,
      },
      {
          CompositorFrameReporter::StageType::kSendBeginMainFrameToCommit,
          begin_main_time,
          begin_commit_time,
      },
      {
          CompositorFrameReporter::StageType::kCommit,
          begin_commit_time,
          end_commit_time,
      },
      {
          CompositorFrameReporter::StageType::kEndCommitToActivation,
          end_commit_time,
          begin_activate_time,
      },
      {
          CompositorFrameReporter::StageType::kActivation,
          begin_activate_time,
          end_activate_time,
      },
      {
          CompositorFrameReporter::StageType::kTotalLatency,
          begin_impl_time,
          frame_timing_details.presentation_feedback.timestamp,
      },
  };

  bool trees_in_viz_mode = base::FeatureList::IsEnabled(features::kTreesInViz);
  if (trees_in_viz_mode) {
    // TreesInViz branch
    stage_history.emplace_back(CompositorFrameReporter::StageType::
                                   kEndActivateToSubmitUpdateDisplayTree,
                               end_activate_time, trees_in_viz_viz_start_time);
    stage_history.emplace_back(
        CompositorFrameReporter::StageType::
            kSubmitUpdateDisplayTreeToPresentationCompositorFrame,
        trees_in_viz_viz_start_time,
        frame_timing_details.presentation_feedback.timestamp);

  } else {
    stage_history.emplace_back(  // Normal branch
        CompositorFrameReporter::StageType::kEndActivateToSubmitCompositorFrame,
        end_activate_time, submit_time);
    stage_history.emplace_back(
        CompositorFrameReporter::StageType::
            kSubmitCompositorFrameToPresentationCompositorFrame,
        submit_time, frame_timing_details.presentation_feedback.timestamp);
  }

  CompositorFrameReporter::ProcessedBlinkBreakdown processed_blink_breakdown(
      begin_main_time, begin_main_start_time, blink_breakdown);
  CompositorFrameReporter::ProcessedVizBreakdown processed_viz_breakdown(
      submit_time, frame_timing_details);
  CompositorFrameReporter::ProcessedTreesInVizBreakdown
      processed_trees_in_viz_breakdown(end_activate_time, submit_time,
                                       trees_in_viz_viz_start_time,
                                       frame_timing_details);
  manager_->RecordEventLatencyUKM(events_metrics, stage_history,
                                  processed_blink_breakdown,
                                  processed_viz_breakdown);

  const auto& entries = recorder()->GetEntriesByName(kEventLatency);
  EXPECT_EQ(4u, entries.size());

  std::string submit_stage =
      (trees_in_viz_mode ? kSubmitUpdateDisplayTreeToPresentationCompositorFrame
                         : kSubmitCompositorFrameToPresentationCompositorFrame);

  for (size_t i = 0; i < entries.size(); i++) {
    const auto* entry = entries[i].get();
    const auto* event_metrics = events_metrics[i].get();

    EXPECT_NE(ukm::kInvalidSourceId, entry->source_id);
    recorder()->ExpectEntrySourceHasUrl(entry, GURL(kTestUrl));

    recorder()->ExpectEntryMetric(entry, kEventType,
                                  static_cast<int64_t>(event_metrics->type()));
    recorder()->ExpectEntryMetric(
        entry, kScrollInputType,
        static_cast<int64_t>(event_metrics->AsScroll()->scroll_type()));

    recorder()->ExpectEntryMetric(entry, kGenerationToRendererCompositor,
                                  (event_dispatch_times[i].arrived_in_renderer -
                                   event_dispatch_times[i].generated)
                                      .InMicroseconds());
    recorder()->ExpectEntryMetric(
        entry, kRendererCompositorQueueingDelay,
        (event_dispatch_times[i].renderer_compositor_started -
         event_dispatch_times[i].arrived_in_renderer)
            .InMicroseconds());
    recorder()->ExpectEntryMetric(
        entry, kRendererCompositorProcessing,
        (event_dispatch_times[i].renderer_compositor_finished -
         event_dispatch_times[i].renderer_compositor_started)
            .InMicroseconds());
    recorder()->ExpectEntryMetric(
        entry, kRendererCompositorToMain,
        (event_dispatch_times[i].renderer_main_started -
         event_dispatch_times[i].renderer_compositor_finished)
            .InMicroseconds());
    recorder()->ExpectEntryMetric(
        entry, kRendererMainProcessing,
        (event_dispatch_times[i].renderer_main_finished -
         event_dispatch_times[i].renderer_main_started)
            .InMicroseconds());
    recorder()->ExpectEntryMetric(
        entry, kRendererMainFinishedToBeginImplFrame,
        (begin_impl_time - event_dispatch_times[i].renderer_main_finished)
            .InMicroseconds());
    recorder()->ExpectEntryMetric(
        entry, kBeginImplFrameToSendBeginMainFrame,
        (begin_main_time - begin_impl_time).InMicroseconds());
    recorder()->ExpectEntryMetric(
        entry, kSendBeginMainFrameToCommit,
        (begin_commit_time - begin_main_time).InMicroseconds());
    recorder()->ExpectEntryMetric(
        entry, kBlinkBreakdownHandleInputEvents,
        blink_breakdown.handle_input_events.InMicroseconds());
    recorder()->ExpectEntryMetric(entry, kBlinkBreakdownAnimate,
                                  blink_breakdown.animate.InMicroseconds());
    recorder()->ExpectEntryMetric(
        entry, kBlinkBreakdownStyleUpdate,
        blink_breakdown.style_update.InMicroseconds());
    recorder()->ExpectEntryMetric(
        entry, kBlinkBreakdownLayoutUpdate,
        blink_breakdown.layout_update.InMicroseconds());
    recorder()->ExpectEntryMetric(entry, kBlinkBreakdownPrepaint,
                                  blink_breakdown.prepaint.InMicroseconds());
    recorder()->ExpectEntryMetric(
        entry, kBlinkBreakdownCompositingInputs,
        blink_breakdown.compositing_inputs.InMicroseconds());
    recorder()->ExpectEntryMetric(entry, kBlinkBreakdownPaint,
                                  blink_breakdown.paint.InMicroseconds());
    recorder()->ExpectEntryMetric(
        entry, kBlinkBreakdownCompositeCommit,
        blink_breakdown.composite_commit.InMicroseconds());
    recorder()->ExpectEntryMetric(
        entry, kBlinkBreakdownUpdateLayers,
        blink_breakdown.update_layers.InMicroseconds());
    recorder()->ExpectEntryMetric(
        entry, kBlinkBreakdownBeginMainSentToStarted,
        (begin_main_start_time - begin_main_time).InMicroseconds());
    recorder()->ExpectEntryMetric(
        entry, kCommit, (end_commit_time - begin_commit_time).InMicroseconds());
    recorder()->ExpectEntryMetric(
        entry, kEndCommitToActivation,
        (begin_activate_time - end_commit_time).InMicroseconds());
    recorder()->ExpectEntryMetric(
        entry, kActivation,
        (end_activate_time - begin_activate_time).InMicroseconds());
    // Normal branch
    if (!trees_in_viz_mode) {
      recorder()->ExpectEntryMetric(
          entry, kEndActivateToSubmitCompositorFrame,
          (submit_time - end_activate_time).InMicroseconds());
      recorder()->ExpectEntryMetric(
          entry, kSubmitCompositorFrameToPresentationCompositorFrame,
          (frame_timing_details.presentation_feedback.timestamp - submit_time)
              .InMicroseconds());
      recorder()->ExpectEntryMetric(
          entry,
          SubstageName(submit_stage,
                       kVizBreakdownSubmitToReceiveCompositorFrame),
          (frame_timing_details.received_compositor_frame_timestamp -
           submit_time)
              .InMicroseconds());
      recorder()->ExpectEntryMetric(
          entry,
          SubstageName(submit_stage,
                       kVizBreakdownReceivedCompositorFrameToStartDraw),
          (frame_timing_details.draw_start_timestamp -
           frame_timing_details.received_compositor_frame_timestamp)
              .InMicroseconds());
      recorder()->ExpectEntryMetric(
          entry, SubstageName(submit_stage, kVizBreakdownStartDrawToSwapStart),
          (frame_timing_details.swap_timings.swap_start -
           frame_timing_details.draw_start_timestamp)
              .InMicroseconds());
      recorder()->ExpectEntryMetric(
          entry, SubstageName(submit_stage, kVizBreakdownSwapStartToSwapEnd),
          (frame_timing_details.swap_timings.swap_end -
           frame_timing_details.swap_timings.swap_start)
              .InMicroseconds());
      recorder()->ExpectEntryMetric(
          entry,
          SubstageName(submit_stage, kVizBreakdownSwapStartToBufferAvailable),
          (frame_timing_details.presentation_feedback.available_timestamp -
           frame_timing_details.swap_timings.swap_start)
              .InMicroseconds());
      recorder()->ExpectEntryMetric(
          entry,
          SubstageName(submit_stage, kVizBreakdownBufferAvailableToBufferReady),
          (frame_timing_details.presentation_feedback.ready_timestamp -
           frame_timing_details.presentation_feedback.available_timestamp)
              .InMicroseconds());
      recorder()->ExpectEntryMetric(
          entry, SubstageName(submit_stage, kVizBreakdownBufferReadyToLatch),
          (frame_timing_details.presentation_feedback.latch_timestamp -
           frame_timing_details.presentation_feedback.ready_timestamp)
              .InMicroseconds());
      recorder()->ExpectEntryMetric(
          entry, SubstageName(submit_stage, kVizBreakdownLatchToSwapEnd),
          (frame_timing_details.swap_timings.swap_end -
           frame_timing_details.presentation_feedback.latch_timestamp)
              .InMicroseconds());
      recorder()->ExpectEntryMetric(
          entry,
          SubstageName(submit_stage,
                       kVizBreakdownSwapEndToPresentationCompositorFrame),
          (frame_timing_details.presentation_feedback.timestamp -
           frame_timing_details.swap_timings.swap_end)
              .InMicroseconds());
    }
    recorder()->ExpectEntryMetric(
        entry, kTotalLatency,
        (frame_timing_details.presentation_feedback.timestamp -
         event_dispatch_times[i].generated)
            .InMicroseconds());
  }
}

}  // namespace
}  // namespace cc
