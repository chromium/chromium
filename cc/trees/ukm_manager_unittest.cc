// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/ukm_manager.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
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
const int64_t kTestSourceId2 = 200;

const char kUserInteraction[] = "Compositor.UserInteraction";
const char kRendering[] = "Compositor.Rendering";

const char kCheckerboardArea[] = "CheckerboardedContentArea";
const char kCheckerboardAreaRatio[] = "CheckerboardedContentAreaRatio";
const char kMissingTiles[] = "NumMissingTiles";
const char kCheckerboardedImagesCount[] = "CheckerboardedImagesCount";

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
const char kBlinkBreakdownCompositingAssignments[] =
    "SendBeginMainFrameToCommit.CompositingAssignments";
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
const char kVizBreakdownSubmitToReceiveCompositorFrame[] =
    "SubmitCompositorFrameToPresentationCompositorFrame."
    "SubmitToReceiveCompositorFrame";
const char kVizBreakdownReceivedCompositorFrameToStartDraw[] =
    "SubmitCompositorFrameToPresentationCompositorFrame."
    "ReceivedCompositorFrameToStartDraw";
const char kVizBreakdownStartDrawToSwapStart[] =
    "SubmitCompositorFrameToPresentationCompositorFrame.StartDrawToSwapStart";
const char kVizBreakdownSwapStartToSwapEnd[] =
    "SubmitCompositorFrameToPresentationCompositorFrame.SwapStartToSwapEnd";
const char kVizBreakdownSwapStartToBufferAvailable[] =
    "SubmitCompositorFrameToPresentationCompositorFrame."
    "SwapStartToBufferAvailable";
const char kVizBreakdownBufferAvailableToBufferReady[] =
    "SubmitCompositorFrameToPresentationCompositorFrame."
    "BufferAvailableToBufferReady";
const char kVizBreakdownBufferReadyToLatch[] =
    "SubmitCompositorFrameToPresentationCompositorFrame.BufferReadyToLatch";
const char kVizBreakdownLatchToSwapEnd[] =
    "SubmitCompositorFrameToPresentationCompositorFrame.LatchToSwapEnd";
const char kVizBreakdownSwapEndToPresentationCompositorFrame[] =
    "SubmitCompositorFrameToPresentationCompositorFrame."
    "SwapEndToPresentationCompositorFrame";
const char kTotalLatencyToSwapBegin[] = "TotalLatencyToSwapBegin";
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

class UkmManagerTest : public testing::Test {
 public:
  UkmManagerTest() {
    auto recorder = std::make_unique<ukm::TestUkmRecorder>();
    test_ukm_recorder_ = recorder.get();
    manager_ = std::make_unique<UkmManager>(std::move(recorder));

    // In production, new UKM Source would have been already created, so
    // manager only needs to know the source id.
    test_ukm_recorder_->UpdateSourceURL(kTestSourceId1, GURL(kTestUrl));
    manager_->SetSourceId(kTestSourceId1);
  }

  ~UkmManagerTest() override = default;

 protected:
  base::TimeTicks AdvanceNowByMs(int advance_ms) {
    test_tick_clock_.Advance(base::TimeDelta::FromMicroseconds(advance_ms));
    return test_tick_clock_.NowTicks();
  }

  std::unique_ptr<EventMetrics> CreateEventMetrics(
      ui::EventType type,
      base::Optional<EventMetrics::ScrollUpdateType> scroll_update_type,
      base::Optional<ui::ScrollInputType> scroll_input_type) {
    base::TimeTicks event_time = AdvanceNowByMs(10);
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
      AdvanceNowByMs(10);
      metrics->SetDispatchStageTimestamp(
          EventMetrics::DispatchStage::kRendererMainStarted);
      AdvanceNowByMs(10);
      metrics->SetDispatchStageTimestamp(
          EventMetrics::DispatchStage::kRendererMainFinished);
    }
    return metrics;
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
    std::transform(
        events_metrics.cbegin(), events_metrics.cend(),
        std::back_inserter(event_times), [](const auto& event_metrics) {
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
    breakdown.handle_input_events = base::TimeDelta::FromMicroseconds(10);
    breakdown.animate = base::TimeDelta::FromMicroseconds(9);
    breakdown.style_update = base::TimeDelta::FromMicroseconds(8);
    breakdown.layout_update = base::TimeDelta::FromMicroseconds(7);
    breakdown.compositing_inputs = base::TimeDelta::FromMicroseconds(6);
    breakdown.prepaint = base::TimeDelta::FromMicroseconds(5);
    breakdown.compositing_assignments = base::TimeDelta::FromMicroseconds(4);
    breakdown.paint = base::TimeDelta::FromMicroseconds(3);
    breakdown.composite_commit = base::TimeDelta::FromMicroseconds(2);
    breakdown.update_layers = base::TimeDelta::FromMicroseconds(1);

    // Advance now by the sum of the breakdowns.
    AdvanceNowByMs(10 + 9 + 8 + 7 + 6 + 5 + 4 + 3 + 2 + 1);

    return breakdown;
  }

  viz::FrameTimingDetails BuildVizBreakdown() {
    viz::FrameTimingDetails breakdown;
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

  ukm::TestUkmRecorder* test_ukm_recorder_;
  std::unique_ptr<UkmManager> manager_;
  base::SimpleTestTickClock test_tick_clock_;
};

TEST_F(UkmManagerTest, Basic) {
  manager_->SetUserInteractionInProgress(true);
  manager_->AddCheckerboardStatsForFrame(5, 1, 10);
  manager_->AddCheckerboardStatsForFrame(15, 3, 30);
  manager_->AddCheckerboardedImages(6);
  manager_->SetUserInteractionInProgress(false);

  // We should see a single entry for the interaction above.
  const auto& entries = test_ukm_recorder_->GetEntriesByName(kUserInteraction);
  ukm::SourceId original_id = ukm::kInvalidSourceId;
  EXPECT_EQ(1u, entries.size());
  for (const auto* entry : entries) {
    original_id = entry->source_id;
    EXPECT_NE(ukm::kInvalidSourceId, entry->source_id);
    test_ukm_recorder_->ExpectEntrySourceHasUrl(entry, GURL(kTestUrl));
    test_ukm_recorder_->ExpectEntryMetric(entry, kCheckerboardArea, 10);
    test_ukm_recorder_->ExpectEntryMetric(entry, kMissingTiles, 2);
    test_ukm_recorder_->ExpectEntryMetric(entry, kCheckerboardAreaRatio, 50);
    test_ukm_recorder_->ExpectEntryMetric(entry, kCheckerboardedImagesCount, 6);
  }
  test_ukm_recorder_->Purge();

  // Try pushing some stats while no user interaction is happening. No entries
  // should be pushed.
  manager_->AddCheckerboardStatsForFrame(6, 1, 10);
  manager_->AddCheckerboardStatsForFrame(99, 3, 100);
  EXPECT_EQ(0u, test_ukm_recorder_->entries_count());
  manager_->SetUserInteractionInProgress(true);
  EXPECT_EQ(0u, test_ukm_recorder_->entries_count());

  // Record a few entries and change the source before the interaction ends. The
  // stats collected up till this point should be recorded before the source is
  // swapped.
  manager_->AddCheckerboardStatsForFrame(10, 1, 100);
  manager_->AddCheckerboardStatsForFrame(30, 5, 100);

  manager_->SetSourceId(kTestSourceId2);

  const auto& entries2 = test_ukm_recorder_->GetEntriesByName(kUserInteraction);
  EXPECT_EQ(1u, entries2.size());
  for (const auto* entry : entries2) {
    EXPECT_EQ(original_id, entry->source_id);
    test_ukm_recorder_->ExpectEntryMetric(entry, kCheckerboardArea, 20);
    test_ukm_recorder_->ExpectEntryMetric(entry, kMissingTiles, 3);
    test_ukm_recorder_->ExpectEntryMetric(entry, kCheckerboardAreaRatio, 20);
    test_ukm_recorder_->ExpectEntryMetric(entry, kCheckerboardedImagesCount, 0);
  }

  // An entry for rendering is emitted when the URL changes.
  const auto& entries_rendering =
      test_ukm_recorder_->GetEntriesByName(kRendering);
  EXPECT_EQ(1u, entries_rendering.size());
  for (const auto* entry : entries_rendering) {
    EXPECT_EQ(original_id, entry->source_id);
    test_ukm_recorder_->ExpectEntryMetric(entry, kCheckerboardedImagesCount, 6);
  }
}

class UkmManagerCompositorLatencyTest
    : public UkmManagerTest,
      public testing::WithParamInterface<
          CompositorFrameReporter::FrameReportType> {
 public:
  UkmManagerCompositorLatencyTest() : report_type_(GetParam()) {}
  ~UkmManagerCompositorLatencyTest() override = default;

 protected:
  CompositorFrameReporter::FrameReportType report_type() const {
    return report_type_;
  }

 private:
  CompositorFrameReporter::FrameReportType report_type_;
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
  const base::TimeTicks begin_impl_time = AdvanceNowByMs(10);
  const base::TimeTicks begin_main_time = AdvanceNowByMs(10);
  const base::TimeTicks begin_main_start_time = AdvanceNowByMs(10);

  BeginMainFrameMetrics blink_breakdown = BuildBlinkBreakdown();

  const base::TimeTicks begin_commit_time = AdvanceNowByMs(10);
  const base::TimeTicks end_commit_time = AdvanceNowByMs(10);
  const base::TimeTicks begin_activate_time = AdvanceNowByMs(10);
  const base::TimeTicks end_activate_time = AdvanceNowByMs(10);
  const base::TimeTicks submit_time = AdvanceNowByMs(10);

  viz::FrameTimingDetails viz_breakdown = BuildVizBreakdown();

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
          CompositorFrameReporter::StageType::
              kEndActivateToSubmitCompositorFrame,
          end_activate_time,
          submit_time,
      },
      {
          CompositorFrameReporter::StageType::
              kSubmitCompositorFrameToPresentationCompositorFrame,
          submit_time,
          viz_breakdown.presentation_feedback.timestamp,
      },
      {
          CompositorFrameReporter::StageType::kTotalLatency,
          begin_impl_time,
          viz_breakdown.presentation_feedback.timestamp,
      },
  };

  CompositorFrameReporter::ActiveTrackers active_trackers;
  active_trackers.set(
      static_cast<size_t>(FrameSequenceTrackerType::kScrollbarScroll));
  active_trackers.set(
      static_cast<size_t>(FrameSequenceTrackerType::kTouchScroll));
  active_trackers.set(
      static_cast<size_t>(FrameSequenceTrackerType::kCompositorAnimation));

  CompositorFrameReporter::ProcessedBlinkBreakdown processed_blink_breakdown(
      begin_main_time, begin_main_start_time, blink_breakdown);
  CompositorFrameReporter::ProcessedVizBreakdown processed_viz_breakdown(
      submit_time, viz_breakdown);
  manager_->RecordCompositorLatencyUKM(
      report_type(), stage_history, active_trackers, processed_blink_breakdown,
      processed_viz_breakdown);

  const auto& entries =
      test_ukm_recorder_->GetEntriesByName(kCompositorLatency);
  EXPECT_EQ(1u, entries.size());
  const auto* entry = entries[0];

  EXPECT_NE(ukm::kInvalidSourceId, entry->source_id);
  test_ukm_recorder_->ExpectEntrySourceHasUrl(entry, GURL(kTestUrl));

  if (report_type() ==
      CompositorFrameReporter::FrameReportType::kDroppedFrame) {
    test_ukm_recorder_->ExpectEntryMetric(entry, kMissedFrame, true);
  } else {
    EXPECT_FALSE(test_ukm_recorder_->EntryHasMetric(entry, kMissedFrame));
  }

  test_ukm_recorder_->ExpectEntryMetric(
      entry, kBeginImplFrameToSendBeginMainFrame,
      (begin_main_time - begin_impl_time).InMicroseconds());
  test_ukm_recorder_->ExpectEntryMetric(
      entry, kSendBeginMainFrameToCommit,
      (begin_commit_time - begin_main_time).InMicroseconds());
  test_ukm_recorder_->ExpectEntryMetric(
      entry, kBlinkBreakdownHandleInputEvents,
      blink_breakdown.handle_input_events.InMicroseconds());
  test_ukm_recorder_->ExpectEntryMetric(
      entry, kBlinkBreakdownAnimate, blink_breakdown.animate.InMicroseconds());
  test_ukm_recorder_->ExpectEntryMetric(
      entry, kBlinkBreakdownStyleUpdate,
      blink_breakdown.style_update.InMicroseconds());
  test_ukm_recorder_->ExpectEntryMetric(
      entry, kBlinkBreakdownLayoutUpdate,
      blink_breakdown.layout_update.InMicroseconds());
  test_ukm_recorder_->ExpectEntryMetric(
      entry, kBlinkBreakdownPrepaint,
      blink_breakdown.prepaint.InMicroseconds());
  test_ukm_recorder_->ExpectEntryMetric(
      entry, kBlinkBreakdownCompositingInputs,
      blink_breakdown.compositing_inputs.InMicroseconds());
  test_ukm_recorder_->ExpectEntryMetric(
      entry, kBlinkBreakdownCompositingAssignments,
      blink_breakdown.compositing_assignments.InMicroseconds());
  test_ukm_recorder_->ExpectEntryMetric(entry, kBlinkBreakdownPaint,
                                        blink_breakdown.paint.InMicroseconds());
  test_ukm_recorder_->ExpectEntryMetric(
      entry, kBlinkBreakdownCompositeCommit,
      blink_breakdown.composite_commit.InMicroseconds());
  test_ukm_recorder_->ExpectEntryMetric(
      entry, kBlinkBreakdownUpdateLayers,
      blink_breakdown.update_layers.InMicroseconds());
  test_ukm_recorder_->ExpectEntryMetric(
      entry, kBlinkBreakdownBeginMainSentToStarted,
      (begin_main_start_time - begin_main_time).InMicroseconds());
  test_ukm_recorder_->ExpectEntryMetric(
      entry, kCommit, (end_commit_time - begin_commit_time).InMicroseconds());
  test_ukm_recorder_->ExpectEntryMetric(
      entry, kEndCommitToActivation,
      (begin_activate_time - end_commit_time).InMicroseconds());
  test_ukm_recorder_->ExpectEntryMetric(
      entry, kActivation,
      (end_activate_time - begin_activate_time).InMicroseconds());
  test_ukm_recorder_->ExpectEntryMetric(
      entry, kEndActivateToSubmitCompositorFrame,
      (submit_time - end_activate_time).InMicroseconds());
  test_ukm_recorder_->ExpectEntryMetric(
      entry, kSubmitCompositorFrameToPresentationCompositorFrame,
      (viz_breakdown.presentation_feedback.timestamp - submit_time)
          .InMicroseconds());
  test_ukm_recorder_->ExpectEntryMetric(
      entry, kVizBreakdownSubmitToReceiveCompositorFrame,
      (viz_breakdown.received_compositor_frame_timestamp - submit_time)
          .InMicroseconds());
  test_ukm_recorder_->ExpectEntryMetric(
      entry, kVizBreakdownReceivedCompositorFrameToStartDraw,
      (viz_breakdown.draw_start_timestamp -
       viz_breakdown.received_compositor_frame_timestamp)
          .InMicroseconds());
  test_ukm_recorder_->ExpectEntryMetric(entry,
                                        kVizBreakdownStartDrawToSwapStart,
                                        (viz_breakdown.swap_timings.swap_start -
                                         viz_breakdown.draw_start_timestamp)
                                            .InMicroseconds());
  test_ukm_recorder_->ExpectEntryMetric(entry, kVizBreakdownSwapStartToSwapEnd,
                                        (viz_breakdown.swap_timings.swap_end -
                                         viz_breakdown.swap_timings.swap_start)
                                            .InMicroseconds());
  test_ukm_recorder_->ExpectEntryMetric(
      entry, kVizBreakdownSwapStartToBufferAvailable,
      (viz_breakdown.presentation_feedback.available_timestamp -
       viz_breakdown.swap_timings.swap_start)
          .InMicroseconds());
  test_ukm_recorder_->ExpectEntryMetric(
      entry, kVizBreakdownBufferAvailableToBufferReady,
      (viz_breakdown.presentation_feedback.ready_timestamp -
       viz_breakdown.presentation_feedback.available_timestamp)
          .InMicroseconds());
  test_ukm_recorder_->ExpectEntryMetric(
      entry, kVizBreakdownBufferReadyToLatch,
      (viz_breakdown.presentation_feedback.latch_timestamp -
       viz_breakdown.presentation_feedback.ready_timestamp)
          .InMicroseconds());
  test_ukm_recorder_->ExpectEntryMetric(
      entry, kVizBreakdownLatchToSwapEnd,
      (viz_breakdown.swap_timings.swap_end -
       viz_breakdown.presentation_feedback.latch_timestamp)
          .InMicroseconds());
  test_ukm_recorder_->ExpectEntryMetric(
      entry, kVizBreakdownSwapEndToPresentationCompositorFrame,
      (viz_breakdown.presentation_feedback.timestamp -
       viz_breakdown.swap_timings.swap_end)
          .InMicroseconds());
  test_ukm_recorder_->ExpectEntryMetric(
      entry, kTotalLatency,
      (viz_breakdown.presentation_feedback.timestamp - begin_impl_time)
          .InMicroseconds());

  test_ukm_recorder_->ExpectEntryMetric(entry, kCompositorAnimation, true);
  test_ukm_recorder_->ExpectEntryMetric(entry, kTouchScroll, true);
  test_ukm_recorder_->ExpectEntryMetric(entry, kScrollbarScroll, true);
  EXPECT_FALSE(test_ukm_recorder_->EntryHasMetric(entry, kMainThreadAnimation));
  EXPECT_FALSE(test_ukm_recorder_->EntryHasMetric(entry, kPinchZoom));
  EXPECT_FALSE(test_ukm_recorder_->EntryHasMetric(entry, kRAF));
  EXPECT_FALSE(test_ukm_recorder_->EntryHasMetric(entry, kVideo));
  EXPECT_FALSE(test_ukm_recorder_->EntryHasMetric(entry, kWheelScroll));
}

TEST_F(UkmManagerTest, EventLatency) {
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
  const base::TimeTicks end_activate_time = AdvanceNowByMs(10);
  const base::TimeTicks submit_time = AdvanceNowByMs(10);

  viz::FrameTimingDetails viz_breakdown = BuildVizBreakdown();

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
          CompositorFrameReporter::StageType::
              kEndActivateToSubmitCompositorFrame,
          end_activate_time,
          submit_time,
      },
      {
          CompositorFrameReporter::StageType::
              kSubmitCompositorFrameToPresentationCompositorFrame,
          submit_time,
          viz_breakdown.presentation_feedback.timestamp,
      },
      {
          CompositorFrameReporter::StageType::kTotalLatency,
          begin_impl_time,
          viz_breakdown.presentation_feedback.timestamp,
      },
  };

  CompositorFrameReporter::ProcessedBlinkBreakdown processed_blink_breakdown(
      begin_main_time, begin_main_start_time, blink_breakdown);
  CompositorFrameReporter::ProcessedVizBreakdown processed_viz_breakdown(
      submit_time, viz_breakdown);
  manager_->RecordEventLatencyUKM(events_metrics, stage_history,
                                  processed_blink_breakdown,
                                  processed_viz_breakdown);

  const auto& entries = test_ukm_recorder_->GetEntriesByName(kEventLatency);
  EXPECT_EQ(3u, entries.size());
  for (size_t i = 0; i < entries.size(); i++) {
    const auto* entry = entries[i];
    const auto* event_metrics = events_metrics[i].get();

    EXPECT_NE(ukm::kInvalidSourceId, entry->source_id);
    test_ukm_recorder_->ExpectEntrySourceHasUrl(entry, GURL(kTestUrl));

    test_ukm_recorder_->ExpectEntryMetric(
        entry, kEventType, static_cast<int64_t>(event_metrics->type()));
    test_ukm_recorder_->ExpectEntryMetric(
        entry, kScrollInputType,
        static_cast<int64_t>(*event_metrics->scroll_type()));

    test_ukm_recorder_->ExpectEntryMetric(
        entry, kGenerationToRendererCompositor,
        (event_dispatch_times[i].arrived_in_renderer -
         event_dispatch_times[i].generated)
            .InMicroseconds());
    test_ukm_recorder_->ExpectEntryMetric(
        entry, kRendererCompositorQueueingDelay,
        (event_dispatch_times[i].renderer_compositor_started -
         event_dispatch_times[i].arrived_in_renderer)
            .InMicroseconds());
    test_ukm_recorder_->ExpectEntryMetric(
        entry, kRendererCompositorProcessing,
        (event_dispatch_times[i].renderer_compositor_finished -
         event_dispatch_times[i].renderer_compositor_started)
            .InMicroseconds());
    test_ukm_recorder_->ExpectEntryMetric(
        entry, kRendererCompositorToMain,
        (event_dispatch_times[i].renderer_main_started -
         event_dispatch_times[i].renderer_compositor_finished)
            .InMicroseconds());
    test_ukm_recorder_->ExpectEntryMetric(
        entry, kRendererMainProcessing,
        (event_dispatch_times[i].renderer_main_finished -
         event_dispatch_times[i].renderer_main_started)
            .InMicroseconds());
    test_ukm_recorder_->ExpectEntryMetric(
        entry, kRendererMainFinishedToBeginImplFrame,
        (begin_impl_time - event_dispatch_times[i].renderer_main_finished)
            .InMicroseconds());
    test_ukm_recorder_->ExpectEntryMetric(
        entry, kBeginImplFrameToSendBeginMainFrame,
        (begin_main_time - begin_impl_time).InMicroseconds());
    test_ukm_recorder_->ExpectEntryMetric(
        entry, kSendBeginMainFrameToCommit,
        (begin_commit_time - begin_main_time).InMicroseconds());
    test_ukm_recorder_->ExpectEntryMetric(
        entry, kBlinkBreakdownHandleInputEvents,
        blink_breakdown.handle_input_events.InMicroseconds());
    test_ukm_recorder_->ExpectEntryMetric(
        entry, kBlinkBreakdownAnimate,
        blink_breakdown.animate.InMicroseconds());
    test_ukm_recorder_->ExpectEntryMetric(
        entry, kBlinkBreakdownStyleUpdate,
        blink_breakdown.style_update.InMicroseconds());
    test_ukm_recorder_->ExpectEntryMetric(
        entry, kBlinkBreakdownLayoutUpdate,
        blink_breakdown.layout_update.InMicroseconds());
    test_ukm_recorder_->ExpectEntryMetric(
        entry, kBlinkBreakdownPrepaint,
        blink_breakdown.prepaint.InMicroseconds());
    test_ukm_recorder_->ExpectEntryMetric(
        entry, kBlinkBreakdownCompositingInputs,
        blink_breakdown.compositing_inputs.InMicroseconds());
    test_ukm_recorder_->ExpectEntryMetric(
        entry, kBlinkBreakdownCompositingAssignments,
        blink_breakdown.compositing_assignments.InMicroseconds());
    test_ukm_recorder_->ExpectEntryMetric(
        entry, kBlinkBreakdownPaint, blink_breakdown.paint.InMicroseconds());
    test_ukm_recorder_->ExpectEntryMetric(
        entry, kBlinkBreakdownCompositeCommit,
        blink_breakdown.composite_commit.InMicroseconds());
    test_ukm_recorder_->ExpectEntryMetric(
        entry, kBlinkBreakdownUpdateLayers,
        blink_breakdown.update_layers.InMicroseconds());
    test_ukm_recorder_->ExpectEntryMetric(
        entry, kBlinkBreakdownBeginMainSentToStarted,
        (begin_main_start_time - begin_main_time).InMicroseconds());
    test_ukm_recorder_->ExpectEntryMetric(
        entry, kCommit, (end_commit_time - begin_commit_time).InMicroseconds());
    test_ukm_recorder_->ExpectEntryMetric(
        entry, kEndCommitToActivation,
        (begin_activate_time - end_commit_time).InMicroseconds());
    test_ukm_recorder_->ExpectEntryMetric(
        entry, kActivation,
        (end_activate_time - begin_activate_time).InMicroseconds());
    test_ukm_recorder_->ExpectEntryMetric(
        entry, kEndActivateToSubmitCompositorFrame,
        (submit_time - end_activate_time).InMicroseconds());
    test_ukm_recorder_->ExpectEntryMetric(
        entry, kSubmitCompositorFrameToPresentationCompositorFrame,
        (viz_breakdown.presentation_feedback.timestamp - submit_time)
            .InMicroseconds());
    test_ukm_recorder_->ExpectEntryMetric(
        entry, kVizBreakdownSubmitToReceiveCompositorFrame,
        (viz_breakdown.received_compositor_frame_timestamp - submit_time)
            .InMicroseconds());
    test_ukm_recorder_->ExpectEntryMetric(
        entry, kVizBreakdownReceivedCompositorFrameToStartDraw,
        (viz_breakdown.draw_start_timestamp -
         viz_breakdown.received_compositor_frame_timestamp)
            .InMicroseconds());
    test_ukm_recorder_->ExpectEntryMetric(
        entry, kVizBreakdownStartDrawToSwapStart,
        (viz_breakdown.swap_timings.swap_start -
         viz_breakdown.draw_start_timestamp)
            .InMicroseconds());
    test_ukm_recorder_->ExpectEntryMetric(
        entry, kVizBreakdownSwapStartToSwapEnd,
        (viz_breakdown.swap_timings.swap_end -
         viz_breakdown.swap_timings.swap_start)
            .InMicroseconds());
    test_ukm_recorder_->ExpectEntryMetric(
        entry, kVizBreakdownSwapStartToBufferAvailable,
        (viz_breakdown.presentation_feedback.available_timestamp -
         viz_breakdown.swap_timings.swap_start)
            .InMicroseconds());
    test_ukm_recorder_->ExpectEntryMetric(
        entry, kVizBreakdownBufferAvailableToBufferReady,
        (viz_breakdown.presentation_feedback.ready_timestamp -
         viz_breakdown.presentation_feedback.available_timestamp)
            .InMicroseconds());
    test_ukm_recorder_->ExpectEntryMetric(
        entry, kVizBreakdownBufferReadyToLatch,
        (viz_breakdown.presentation_feedback.latch_timestamp -
         viz_breakdown.presentation_feedback.ready_timestamp)
            .InMicroseconds());
    test_ukm_recorder_->ExpectEntryMetric(
        entry, kVizBreakdownLatchToSwapEnd,
        (viz_breakdown.swap_timings.swap_end -
         viz_breakdown.presentation_feedback.latch_timestamp)
            .InMicroseconds());
    test_ukm_recorder_->ExpectEntryMetric(
        entry, kVizBreakdownSwapEndToPresentationCompositorFrame,
        (viz_breakdown.presentation_feedback.timestamp -
         viz_breakdown.swap_timings.swap_end)
            .InMicroseconds());
    test_ukm_recorder_->ExpectEntryMetric(
        entry, kVizBreakdownSubmitToReceiveCompositorFrame,
        (viz_breakdown.received_compositor_frame_timestamp - submit_time)
            .InMicroseconds());
    test_ukm_recorder_->ExpectEntryMetric(
        entry, kVizBreakdownReceivedCompositorFrameToStartDraw,
        (viz_breakdown.draw_start_timestamp -
         viz_breakdown.received_compositor_frame_timestamp)
            .InMicroseconds());
    test_ukm_recorder_->ExpectEntryMetric(
        entry, kVizBreakdownStartDrawToSwapStart,
        (viz_breakdown.swap_timings.swap_start -
         viz_breakdown.draw_start_timestamp)
            .InMicroseconds());
    test_ukm_recorder_->ExpectEntryMetric(
        entry, kVizBreakdownSwapStartToSwapEnd,
        (viz_breakdown.swap_timings.swap_end -
         viz_breakdown.swap_timings.swap_start)
            .InMicroseconds());
    test_ukm_recorder_->ExpectEntryMetric(
        entry, kVizBreakdownSwapStartToBufferAvailable,
        (viz_breakdown.presentation_feedback.available_timestamp -
         viz_breakdown.swap_timings.swap_start)
            .InMicroseconds());
    test_ukm_recorder_->ExpectEntryMetric(
        entry, kVizBreakdownBufferAvailableToBufferReady,
        (viz_breakdown.presentation_feedback.ready_timestamp -
         viz_breakdown.presentation_feedback.available_timestamp)
            .InMicroseconds());
    test_ukm_recorder_->ExpectEntryMetric(
        entry, kVizBreakdownBufferReadyToLatch,
        (viz_breakdown.presentation_feedback.latch_timestamp -
         viz_breakdown.presentation_feedback.ready_timestamp)
            .InMicroseconds());
    test_ukm_recorder_->ExpectEntryMetric(
        entry, kVizBreakdownLatchToSwapEnd,
        (viz_breakdown.swap_timings.swap_end -
         viz_breakdown.presentation_feedback.latch_timestamp)
            .InMicroseconds());
    test_ukm_recorder_->ExpectEntryMetric(
        entry, kVizBreakdownSwapEndToPresentationCompositorFrame,
        (viz_breakdown.presentation_feedback.timestamp -
         viz_breakdown.swap_timings.swap_end)
            .InMicroseconds());
    test_ukm_recorder_->ExpectEntryMetric(
        entry, kTotalLatencyToSwapBegin,
        (viz_breakdown.swap_timings.swap_start -
         event_dispatch_times[i].generated)
            .InMicroseconds());
    test_ukm_recorder_->ExpectEntryMetric(
        entry, kTotalLatency,
        (viz_breakdown.presentation_feedback.timestamp -
         event_dispatch_times[i].generated)
            .InMicroseconds());
  }
}

}  // namespace
}  // namespace cc
