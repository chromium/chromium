// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/ukm_manager.h"

#include <utility>
#include <vector>

#include "base/ranges/algorithm.h"
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
    base::ranges::transform(
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
      submit_time, viz_breakdown);
  manager_->RecordCompositorLatencyUKM(
      report_types(), stage_history, active_trackers, processed_blink_breakdown,
      processed_viz_breakdown);

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
  recorder()->ExpectEntryMetric(
      entry, kEndActivateToSubmitCompositorFrame,
      (submit_time - end_activate_time).InMicroseconds());
  recorder()->ExpectEntryMetric(
      entry, kSubmitCompositorFrameToPresentationCompositorFrame,
      (viz_breakdown.presentation_feedback.timestamp - submit_time)
          .InMicroseconds());
  recorder()->ExpectEntryMetric(
      entry, kVizBreakdownSubmitToReceiveCompositorFrame,
      (viz_breakdown.received_compositor_frame_timestamp - submit_time)
          .InMicroseconds());
  recorder()->ExpectEntryMetric(
      entry, kVizBreakdownReceivedCompositorFrameToStartDraw,
      (viz_breakdown.draw_start_timestamp -
       viz_breakdown.received_compositor_frame_timestamp)
          .InMicroseconds());
  recorder()->ExpectEntryMetric(entry, kVizBreakdownStartDrawToSwapStart,
                                (viz_breakdown.swap_timings.swap_start -
                                 viz_breakdown.draw_start_timestamp)
                                    .InMicroseconds());
  recorder()->ExpectEntryMetric(entry, kVizBreakdownSwapStartToSwapEnd,
                                (viz_breakdown.swap_timings.swap_end -
                                 viz_breakdown.swap_timings.swap_start)
                                    .InMicroseconds());
  recorder()->ExpectEntryMetric(
      entry, kVizBreakdownSwapStartToBufferAvailable,
      (viz_breakdown.presentation_feedback.available_timestamp -
       viz_breakdown.swap_timings.swap_start)
          .InMicroseconds());
  recorder()->ExpectEntryMetric(
      entry, kVizBreakdownBufferAvailableToBufferReady,
      (viz_breakdown.presentation_feedback.ready_timestamp -
       viz_breakdown.presentation_feedback.available_timestamp)
          .InMicroseconds());
  recorder()->ExpectEntryMetric(
      entry, kVizBreakdownBufferReadyToLatch,
      (viz_breakdown.presentation_feedback.latch_timestamp -
       viz_breakdown.presentation_feedback.ready_timestamp)
          .InMicroseconds());
  recorder()->ExpectEntryMetric(
      entry, kVizBreakdownLatchToSwapEnd,
      (viz_breakdown.swap_timings.swap_end -
       viz_breakdown.presentation_feedback.latch_timestamp)
          .InMicroseconds());
  recorder()->ExpectEntryMetric(
      entry, kVizBreakdownSwapEndToPresentationCompositorFrame,
      (viz_breakdown.presentation_feedback.timestamp -
       viz_breakdown.swap_timings.swap_end)
          .InMicroseconds());
  recorder()->ExpectEntryMetric(
      entry, kTotalLatency,
      (viz_breakdown.presentation_feedback.timestamp - begin_impl_time)
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

  const auto& entries = recorder()->GetEntriesByName(kEventLatency);
  EXPECT_EQ(4u, entries.size());
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
    recorder()->ExpectEntryMetric(
        entry, kEndActivateToSubmitCompositorFrame,
        (submit_time - end_activate_time).InMicroseconds());
    recorder()->ExpectEntryMetric(
        entry, kSubmitCompositorFrameToPresentationCompositorFrame,
        (viz_breakdown.presentation_feedback.timestamp - submit_time)
            .InMicroseconds());
    recorder()->ExpectEntryMetric(
        entry, kVizBreakdownSubmitToReceiveCompositorFrame,
        (viz_breakdown.received_compositor_frame_timestamp - submit_time)
            .InMicroseconds());
    recorder()->ExpectEntryMetric(
        entry, kVizBreakdownReceivedCompositorFrameToStartDraw,
        (viz_breakdown.draw_start_timestamp -
         viz_breakdown.received_compositor_frame_timestamp)
            .InMicroseconds());
    recorder()->ExpectEntryMetric(entry, kVizBreakdownStartDrawToSwapStart,
                                  (viz_breakdown.swap_timings.swap_start -
                                   viz_breakdown.draw_start_timestamp)
                                      .InMicroseconds());
    recorder()->ExpectEntryMetric(entry, kVizBreakdownSwapStartToSwapEnd,
                                  (viz_breakdown.swap_timings.swap_end -
                                   viz_breakdown.swap_timings.swap_start)
                                      .InMicroseconds());
    recorder()->ExpectEntryMetric(
        entry, kVizBreakdownSwapStartToBufferAvailable,
        (viz_breakdown.presentation_feedback.available_timestamp -
         viz_breakdown.swap_timings.swap_start)
            .InMicroseconds());
    recorder()->ExpectEntryMetric(
        entry, kVizBreakdownBufferAvailableToBufferReady,
        (viz_breakdown.presentation_feedback.ready_timestamp -
         viz_breakdown.presentation_feedback.available_timestamp)
            .InMicroseconds());
    recorder()->ExpectEntryMetric(
        entry, kVizBreakdownBufferReadyToLatch,
        (viz_breakdown.presentation_feedback.latch_timestamp -
         viz_breakdown.presentation_feedback.ready_timestamp)
            .InMicroseconds());
    recorder()->ExpectEntryMetric(
        entry, kVizBreakdownLatchToSwapEnd,
        (viz_breakdown.swap_timings.swap_end -
         viz_breakdown.presentation_feedback.latch_timestamp)
            .InMicroseconds());
    recorder()->ExpectEntryMetric(
        entry, kVizBreakdownSwapEndToPresentationCompositorFrame,
        (viz_breakdown.presentation_feedback.timestamp -
         viz_breakdown.swap_timings.swap_end)
            .InMicroseconds());
    recorder()->ExpectEntryMetric(
        entry, kVizBreakdownSubmitToReceiveCompositorFrame,
        (viz_breakdown.received_compositor_frame_timestamp - submit_time)
            .InMicroseconds());
    recorder()->ExpectEntryMetric(
        entry, kVizBreakdownReceivedCompositorFrameToStartDraw,
        (viz_breakdown.draw_start_timestamp -
         viz_breakdown.received_compositor_frame_timestamp)
            .InMicroseconds());
    recorder()->ExpectEntryMetric(entry, kVizBreakdownStartDrawToSwapStart,
                                  (viz_breakdown.swap_timings.swap_start -
                                   viz_breakdown.draw_start_timestamp)
                                      .InMicroseconds());
    recorder()->ExpectEntryMetric(entry, kVizBreakdownSwapStartToSwapEnd,
                                  (viz_breakdown.swap_timings.swap_end -
                                   viz_breakdown.swap_timings.swap_start)
                                      .InMicroseconds());
    recorder()->ExpectEntryMetric(
        entry, kVizBreakdownSwapStartToBufferAvailable,
        (viz_breakdown.presentation_feedback.available_timestamp -
         viz_breakdown.swap_timings.swap_start)
            .InMicroseconds());
    recorder()->ExpectEntryMetric(
        entry, kVizBreakdownBufferAvailableToBufferReady,
        (viz_breakdown.presentation_feedback.ready_timestamp -
         viz_breakdown.presentation_feedback.available_timestamp)
            .InMicroseconds());
    recorder()->ExpectEntryMetric(
        entry, kVizBreakdownBufferReadyToLatch,
        (viz_breakdown.presentation_feedback.latch_timestamp -
         viz_breakdown.presentation_feedback.ready_timestamp)
            .InMicroseconds());
    recorder()->ExpectEntryMetric(
        entry, kVizBreakdownLatchToSwapEnd,
        (viz_breakdown.swap_timings.swap_end -
         viz_breakdown.presentation_feedback.latch_timestamp)
            .InMicroseconds());
    recorder()->ExpectEntryMetric(
        entry, kVizBreakdownSwapEndToPresentationCompositorFrame,
        (viz_breakdown.presentation_feedback.timestamp -
         viz_breakdown.swap_timings.swap_end)
            .InMicroseconds());
    recorder()->ExpectEntryMetric(
        entry, kTotalLatency,
        (viz_breakdown.presentation_feedback.timestamp -
         event_dispatch_times[i].generated)
            .InMicroseconds());
  }
}

}  // namespace
}  // namespace cc
