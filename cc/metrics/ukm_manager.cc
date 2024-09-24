// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/ukm_manager.h"

#include <utility>

#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "cc/metrics/compositor_frame_reporter.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace cc {

namespace {

// The combination of ScrollType and EventType which we are interested in the
// TotalLatency of.
enum class InputMetricEvent {
  NONE,
  SCROLL_BEGIN_TOUCH,
  SCROLL_UPDATE_TOUCH,
  SCROLL_BEGIN_WHEEL,
  SCROLL_UPDATE_WHEEL,
  INPUT_METRIC_EVENT_MAX = SCROLL_UPDATE_WHEEL
};

InputMetricEvent GetInputMetricEvent(ScrollEventMetrics::ScrollType scroll_type,
                                     EventMetrics::EventType event_type) {
  if (scroll_type == ScrollEventMetrics::ScrollType::kTouchscreen) {
    if (event_type == EventMetrics::EventType::kFirstGestureScrollUpdate) {
      return InputMetricEvent::SCROLL_BEGIN_TOUCH;
    } else if (event_type == EventMetrics::EventType::kGestureScrollUpdate) {
      return InputMetricEvent::SCROLL_UPDATE_TOUCH;
    }
  } else if (scroll_type == ScrollEventMetrics::ScrollType::kWheel) {
    if (event_type == EventMetrics::EventType::kFirstGestureScrollUpdate) {
      return InputMetricEvent::SCROLL_BEGIN_WHEEL;
    } else if (event_type == EventMetrics::EventType::kGestureScrollUpdate) {
      return InputMetricEvent::SCROLL_UPDATE_WHEEL;
    }
  }
  return InputMetricEvent::NONE;
}

}  // namespace

UkmManager::UkmManager(std::unique_ptr<ukm::UkmRecorder> recorder)
    : recorder_(std::move(recorder)) {
  DCHECK(recorder_);
}

UkmManager::~UkmManager() = default;

void UkmManager::SetSourceId(ukm::SourceId source_id) {
  source_id_ = source_id;
}

void UkmManager::RecordCompositorLatencyUKM(
    const CompositorFrameReporter::FrameReportTypes& report_types,
    const std::vector<CompositorFrameReporter::StageData>& stage_history,
    const ActiveTrackers& active_trackers,
    const CompositorFrameReporter::ProcessedBlinkBreakdown&
        processed_blink_breakdown,
    const CompositorFrameReporter::ProcessedVizBreakdown&
        processed_viz_breakdown) const {
  using StageType = CompositorFrameReporter::StageType;

  ukm::builders::Graphics_Smoothness_Latency builder(source_id_);

  if (report_types.test(static_cast<size_t>(
          CompositorFrameReporter::FrameReportType::kDroppedFrame))) {
    builder.SetMissedFrame(true);
  }

  // Record each stage.
  for (const CompositorFrameReporter::StageData& stage : stage_history) {
    switch (stage.stage_type) {
#define CASE_FOR_STAGE(name)                                                 \
  case StageType::k##name:                                                   \
    builder.Set##name((stage.end_time - stage.start_time).InMicroseconds()); \
    break;
      CASE_FOR_STAGE(BeginImplFrameToSendBeginMainFrame);
      CASE_FOR_STAGE(SendBeginMainFrameToCommit);
      CASE_FOR_STAGE(Commit);
      CASE_FOR_STAGE(EndCommitToActivation);
      CASE_FOR_STAGE(Activation);
      CASE_FOR_STAGE(EndActivateToSubmitCompositorFrame);
      CASE_FOR_STAGE(SubmitCompositorFrameToPresentationCompositorFrame);
      CASE_FOR_STAGE(TotalLatency);
#undef CASE_FOR_STAGE
      case StageType::kStageTypeCount:
        NOTREACHED();
    }
  }

  // Record Blink breakdowns.
  for (auto it = processed_blink_breakdown.CreateIterator(); it.IsValid();
       it.Advance()) {
    switch (it.GetBreakdown()) {
#define CASE_FOR_BLINK_BREAKDOWN(name)                   \
  case CompositorFrameReporter::BlinkBreakdown::k##name: \
    builder.SetSendBeginMainFrameToCommit_##name(        \
        it.GetLatency().InMicroseconds());               \
    break;
      CASE_FOR_BLINK_BREAKDOWN(HandleInputEvents);
      CASE_FOR_BLINK_BREAKDOWN(Animate);
      CASE_FOR_BLINK_BREAKDOWN(StyleUpdate);
      CASE_FOR_BLINK_BREAKDOWN(LayoutUpdate);
      CASE_FOR_BLINK_BREAKDOWN(Accessibility);
      CASE_FOR_BLINK_BREAKDOWN(Prepaint);
      CASE_FOR_BLINK_BREAKDOWN(CompositingInputs);
      CASE_FOR_BLINK_BREAKDOWN(Paint);
      CASE_FOR_BLINK_BREAKDOWN(CompositeCommit);
      CASE_FOR_BLINK_BREAKDOWN(UpdateLayers);
      CASE_FOR_BLINK_BREAKDOWN(BeginMainSentToStarted);
#undef CASE_FOR_BLINK_BREAKDOWN
      case CompositorFrameReporter::BlinkBreakdown::kBreakdownCount:
        NOTREACHED();
    }
  }

  // Record Viz breakdowns.
  for (auto it = processed_viz_breakdown.CreateIterator(false); it.IsValid();
       it.Advance()) {
    switch (it.GetBreakdown()) {
#define CASE_FOR_VIZ_BREAKDOWN(name)                                      \
  case CompositorFrameReporter::VizBreakdown::k##name:                    \
    builder.SetSubmitCompositorFrameToPresentationCompositorFrame_##name( \
        it.GetDuration().InMicroseconds());                               \
    break;
      CASE_FOR_VIZ_BREAKDOWN(SubmitToReceiveCompositorFrame);
      CASE_FOR_VIZ_BREAKDOWN(ReceivedCompositorFrameToStartDraw);
      CASE_FOR_VIZ_BREAKDOWN(StartDrawToSwapStart);
      CASE_FOR_VIZ_BREAKDOWN(SwapStartToSwapEnd);
      CASE_FOR_VIZ_BREAKDOWN(SwapEndToPresentationCompositorFrame);
      CASE_FOR_VIZ_BREAKDOWN(SwapStartToBufferAvailable);
      CASE_FOR_VIZ_BREAKDOWN(BufferAvailableToBufferReady);
      CASE_FOR_VIZ_BREAKDOWN(BufferReadyToLatch);
      CASE_FOR_VIZ_BREAKDOWN(LatchToSwapEnd);
#undef CASE_FOR_VIZ_BREAKDOWN
      case CompositorFrameReporter::VizBreakdown::kBreakdownCount:
        NOTREACHED();
    }
  }

  // Record the active trackers.
  for (size_t type = 0; type < active_trackers.size(); ++type) {
    if (!active_trackers.test(type))
      continue;
    const auto frame_sequence_tracker_type =
        static_cast<FrameSequenceTrackerType>(type);
    switch (frame_sequence_tracker_type) {
#define CASE_FOR_TRACKER(name)            \
  case FrameSequenceTrackerType::k##name: \
    builder.Set##name(true);              \
    break;
      CASE_FOR_TRACKER(CompositorAnimation);
      CASE_FOR_TRACKER(MainThreadAnimation);
      CASE_FOR_TRACKER(PinchZoom);
      CASE_FOR_TRACKER(RAF);
      CASE_FOR_TRACKER(ScrollbarScroll);
      CASE_FOR_TRACKER(TouchScroll);
      CASE_FOR_TRACKER(Video);
      CASE_FOR_TRACKER(WheelScroll);
      CASE_FOR_TRACKER(CanvasAnimation);
      CASE_FOR_TRACKER(JSAnimation);
#undef CASE_FOR_TRACKER
      case FrameSequenceTrackerType::kSETCompositorAnimation:
      case FrameSequenceTrackerType::kSETMainThreadAnimation:
        break;
      case FrameSequenceTrackerType::kCustom:
      case FrameSequenceTrackerType::kMaxType:
        NOTREACHED();
    }
  }

  builder.Record(recorder_.get());
}

void UkmManager::RecordEventLatencyUKM(
    const EventMetrics::List& events_metrics,
    const std::vector<CompositorFrameReporter::StageData>& stage_history,
    const CompositorFrameReporter::ProcessedBlinkBreakdown&
        processed_blink_breakdown,
    const CompositorFrameReporter::ProcessedVizBreakdown&
        processed_viz_breakdown) const {
  using StageType = CompositorFrameReporter::StageType;

  for (const auto& event_metrics : events_metrics) {
    ukm::builders::Graphics_Smoothness_EventLatency builder(source_id_);

    builder.SetEventType(static_cast<int64_t>(event_metrics->type()));

    base::TimeTicks generated_timestamp =
        event_metrics->GetDispatchStageTimestamp(
            EventMetrics::DispatchStage::kGenerated);

    std::optional<ScrollEventMetrics::ScrollType> scroll_type;
    if (ScrollEventMetrics* scroll_metrics = event_metrics->AsScroll()) {
      scroll_type = scroll_metrics->scroll_type();
      builder.SetScrollInputType(
          static_cast<int64_t>(scroll_metrics->scroll_type()));
    } else if (PinchEventMetrics* pinch_metrics = event_metrics->AsPinch()) {
      builder.SetPinchInputType(
          static_cast<int64_t>(pinch_metrics->pinch_type()));
    }

    // Record event dispatch metrics.
    EventMetrics::DispatchStage dispatch_stage =
        EventMetrics::DispatchStage::kGenerated;
    base::TimeTicks dispatch_timestamp = generated_timestamp;
    while (dispatch_stage != EventMetrics::DispatchStage::kMaxValue) {
      DCHECK(!dispatch_timestamp.is_null());
      int dispatch_index = static_cast<int>(dispatch_stage);

      // Find the end dispatch stage.
      auto end_stage =
          static_cast<EventMetrics::DispatchStage>(dispatch_index + 1);
      base::TimeTicks end_timestamp =
          event_metrics->GetDispatchStageTimestamp(end_stage);
      while (end_timestamp.is_null() &&
             end_stage != EventMetrics::DispatchStage::kMaxValue) {
        end_stage = static_cast<EventMetrics::DispatchStage>(
            static_cast<int>(end_stage) + 1);
        end_timestamp = event_metrics->GetDispatchStageTimestamp(end_stage);
      }
      if (end_timestamp.is_null())
        break;

      const int64_t dispatch_latency =
          (end_timestamp - dispatch_timestamp).InMicroseconds();
      switch (dispatch_stage) {
        case EventMetrics::DispatchStage::kGenerated:
          switch (end_stage) {
            case EventMetrics::DispatchStage::
                kScrollsBlockingTouchDispatchedToRenderer:
            case EventMetrics::DispatchStage::kArrivedInBrowserMain:
              // Will build the `GenerationToRendererCompositor` metric on the
              // `kArrivedInBrowserMain` stage.
              break;
            case EventMetrics::DispatchStage::kArrivedInRendererCompositor:
              builder.SetGenerationToRendererCompositor(dispatch_latency);
              break;
            default:
              NOTREACHED();
          }
          break;
        case EventMetrics::DispatchStage::
            kScrollsBlockingTouchDispatchedToRenderer:
          break;
        case EventMetrics::DispatchStage::kArrivedInBrowserMain:
          DCHECK_EQ(end_stage,
                    EventMetrics::DispatchStage::kArrivedInRendererCompositor);
          // TODO(b/224960731): Add new UKM metrics and then split kGenerated
          // with kArrivedInBrowserMain breakdown.
          builder.SetGenerationToRendererCompositor(
              (end_timestamp - generated_timestamp).InMicroseconds());
          break;
        case EventMetrics::DispatchStage::kArrivedInRendererCompositor:
          switch (end_stage) {
            case EventMetrics::DispatchStage::kRendererCompositorStarted:
              builder.SetRendererCompositorQueueingDelay(dispatch_latency);
              break;
            case EventMetrics::DispatchStage::kRendererMainStarted:
              builder.SetRendererCompositorToMain(dispatch_latency);
              break;
            default:
              NOTREACHED();
          }
          break;
        case EventMetrics::DispatchStage::kRendererCompositorStarted:
          DCHECK_EQ(end_stage,
                    EventMetrics::DispatchStage::kRendererCompositorFinished);
          builder.SetRendererCompositorProcessing(dispatch_latency);
          break;
        case EventMetrics::DispatchStage::kRendererCompositorFinished:
          DCHECK_EQ(end_stage,
                    EventMetrics::DispatchStage::kRendererMainStarted);
          builder.SetRendererCompositorToMain(dispatch_latency);
          break;
        case EventMetrics::DispatchStage::kRendererMainStarted:
          DCHECK_EQ(end_stage,
                    EventMetrics::DispatchStage::kRendererMainFinished);
          builder.SetRendererMainProcessing(dispatch_latency);
          break;
        case EventMetrics::DispatchStage::kRendererMainFinished:
          NOTREACHED();
      }

      dispatch_stage = end_stage;
      dispatch_timestamp = end_timestamp;
    }

    // It is possible for an event to be handled on the renderer in the middle
    // of a frame (e.g. the browser received the event *after* renderer received
    // a begin-impl, and the event was handled on the renderer before that frame
    // ended). To handle such cases, find the first stage that happens after the
    // event's processing finished on the renderer.
    auto stage_it = base::ranges::lower_bound(
        stage_history, dispatch_timestamp, {},
        &CompositorFrameReporter::StageData::start_time);
    // TODO(crbug.com/40843545): Ideally, at least the start time of
    // SubmitCompositorFrameToPresentationCompositorFrame stage should be
    // greater than or equal to the final event dispatch timestamp, but
    // apparently, this is not always the case (see crbug.com/1330903). Skip
    // recording compositor stages for now until we investigate the issue.
    if (stage_it == stage_history.end())
      continue;

    switch (dispatch_stage) {
      case EventMetrics::DispatchStage::kRendererCompositorFinished:
        switch (stage_it->stage_type) {
#define CASE_FOR_STAGE(stage_name, metrics_suffix)                     \
  case StageType::k##stage_name:                                       \
    builder.SetRendererCompositorFinishedTo##metrics_suffix(           \
        (stage_it->start_time - dispatch_timestamp).InMicroseconds()); \
    break;
          CASE_FOR_STAGE(BeginImplFrameToSendBeginMainFrame, BeginImplFrame);
          CASE_FOR_STAGE(SendBeginMainFrameToCommit, SendBeginMainFrame);
          CASE_FOR_STAGE(Commit, Commit);
          CASE_FOR_STAGE(EndCommitToActivation, EndCommit);
          CASE_FOR_STAGE(Activation, Activation);
          CASE_FOR_STAGE(EndActivateToSubmitCompositorFrame, EndActivate);
          CASE_FOR_STAGE(SubmitCompositorFrameToPresentationCompositorFrame,
                         SubmitCompositorFrame);
#undef CASE_FOR_STAGE
          case StageType::kTotalLatency:
          case StageType::kStageTypeCount:
            NOTREACHED();
        }
        break;
      case EventMetrics::DispatchStage::kRendererMainFinished:
        switch (stage_it->stage_type) {
#define CASE_FOR_STAGE(stage_name, metrics_suffix)                     \
  case StageType::k##stage_name:                                       \
    builder.SetRendererMainFinishedTo##metrics_suffix(                 \
        (stage_it->start_time - dispatch_timestamp).InMicroseconds()); \
    break;
          CASE_FOR_STAGE(BeginImplFrameToSendBeginMainFrame, BeginImplFrame);
          CASE_FOR_STAGE(SendBeginMainFrameToCommit, SendBeginMainFrame);
          CASE_FOR_STAGE(Commit, Commit);
          CASE_FOR_STAGE(EndCommitToActivation, EndCommit);
          CASE_FOR_STAGE(Activation, Activation);
          CASE_FOR_STAGE(EndActivateToSubmitCompositorFrame, EndActivate);
          CASE_FOR_STAGE(SubmitCompositorFrameToPresentationCompositorFrame,
                         SubmitCompositorFrame);
#undef CASE_FOR_STAGE
          case StageType::kTotalLatency:
          case StageType::kStageTypeCount:
            NOTREACHED();
        }
        break;
      default:
        NOTREACHED();
    }
    for (; stage_it != stage_history.end(); ++stage_it) {
      // Total latency is calculated since the event timestamp.
      const base::TimeTicks start_time =
          stage_it->stage_type == StageType::kTotalLatency
              ? generated_timestamp
              : stage_it->start_time;

      // We want to separate out Touch and Wheel scroll events, along with their
      // Begin and Update components. Reporting the TotalLatency for each. As
      // they all have different performance characteristics which are blurred
      // when analysing the aggregate TotalLatency.
      if (stage_it->stage_type == StageType::kTotalLatency) {
        auto delta = (stage_it->end_time - start_time);
        if (scroll_type.has_value()) {
          switch (
              GetInputMetricEvent(scroll_type.value(), event_metrics->type())) {
            case InputMetricEvent::NONE:
              break;
            case InputMetricEvent::SCROLL_BEGIN_TOUCH:
              builder.SetScrollBegin_Touch(delta.InMicroseconds());
              break;
            case InputMetricEvent::SCROLL_UPDATE_TOUCH:
              builder.SetScrollUpdate_Touch(delta.InMicroseconds());
              break;
            case InputMetricEvent::SCROLL_BEGIN_WHEEL:
              builder.SetScrollBegin_Wheel(delta.InMicroseconds());
              break;
            case InputMetricEvent::SCROLL_UPDATE_WHEEL:
              builder.SetScrollUpdate_Wheel(delta.InMicroseconds());
              break;
          }
        }
      }

      switch (stage_it->stage_type) {
#define CASE_FOR_STAGE(name)                                               \
  case StageType::k##name:                                                 \
    builder.Set##name((stage_it->end_time - start_time).InMicroseconds()); \
    break;
        CASE_FOR_STAGE(BeginImplFrameToSendBeginMainFrame);
        CASE_FOR_STAGE(SendBeginMainFrameToCommit);
        CASE_FOR_STAGE(Commit);
        CASE_FOR_STAGE(EndCommitToActivation);
        CASE_FOR_STAGE(Activation);
        CASE_FOR_STAGE(EndActivateToSubmitCompositorFrame);
        CASE_FOR_STAGE(SubmitCompositorFrameToPresentationCompositorFrame);
        CASE_FOR_STAGE(TotalLatency);
#undef CASE_FOR_STAGE
        case StageType::kStageTypeCount:
          NOTREACHED();
      }
    }

    // Record Blink breakdowns.
    for (auto it = processed_blink_breakdown.CreateIterator(); it.IsValid();
         it.Advance()) {
      switch (it.GetBreakdown()) {
#define CASE_FOR_BLINK_BREAKDOWN(name)                   \
  case CompositorFrameReporter::BlinkBreakdown::k##name: \
    builder.SetSendBeginMainFrameToCommit_##name(        \
        it.GetLatency().InMicroseconds());               \
    break;
        CASE_FOR_BLINK_BREAKDOWN(HandleInputEvents);
        CASE_FOR_BLINK_BREAKDOWN(Animate);
        CASE_FOR_BLINK_BREAKDOWN(StyleUpdate);
        CASE_FOR_BLINK_BREAKDOWN(LayoutUpdate);
        CASE_FOR_BLINK_BREAKDOWN(Accessibility);
        CASE_FOR_BLINK_BREAKDOWN(Prepaint);
        CASE_FOR_BLINK_BREAKDOWN(CompositingInputs);
        CASE_FOR_BLINK_BREAKDOWN(Paint);
        CASE_FOR_BLINK_BREAKDOWN(CompositeCommit);
        CASE_FOR_BLINK_BREAKDOWN(UpdateLayers);
        CASE_FOR_BLINK_BREAKDOWN(BeginMainSentToStarted);
#undef CASE_FOR_BLINK_BREAKDOWN
        case CompositorFrameReporter::BlinkBreakdown::kBreakdownCount:
          NOTREACHED();
      }
    }

    // Record Viz breakdowns.
    for (auto it = processed_viz_breakdown.CreateIterator(false); it.IsValid();
         it.Advance()) {
      switch (it.GetBreakdown()) {
#define CASE_FOR_VIZ_BREAKDOWN(name)                                      \
  case CompositorFrameReporter::VizBreakdown::k##name:                    \
    builder.SetSubmitCompositorFrameToPresentationCompositorFrame_##name( \
        it.GetDuration().InMicroseconds());                               \
    break;
        CASE_FOR_VIZ_BREAKDOWN(SubmitToReceiveCompositorFrame);
        CASE_FOR_VIZ_BREAKDOWN(ReceivedCompositorFrameToStartDraw);
        CASE_FOR_VIZ_BREAKDOWN(StartDrawToSwapStart);
        CASE_FOR_VIZ_BREAKDOWN(SwapStartToSwapEnd);
        CASE_FOR_VIZ_BREAKDOWN(SwapEndToPresentationCompositorFrame);
        CASE_FOR_VIZ_BREAKDOWN(SwapStartToBufferAvailable);
        CASE_FOR_VIZ_BREAKDOWN(BufferAvailableToBufferReady);
        CASE_FOR_VIZ_BREAKDOWN(BufferReadyToLatch);
        CASE_FOR_VIZ_BREAKDOWN(LatchToSwapEnd);
#undef CASE_FOR_VIZ_BREAKDOWN
        case CompositorFrameReporter::VizBreakdown::kBreakdownCount:
          NOTREACHED();
      }
    }

    builder.Record(recorder_.get());
  }
}

}  // namespace cc
