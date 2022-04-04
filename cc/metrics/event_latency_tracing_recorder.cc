// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/event_latency_tracing_recorder.h"

#include "base/notreached.h"
#include "base/time/time.h"
#include "base/trace_event/trace_id_helper.h"
#include "base/trace_event/typed_macros.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"
#include "cc/metrics/event_metrics.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

namespace cc {
namespace {

constexpr char kTracingCategory[] = "cc,benchmark,input";

// Returns the name of the event dispatch breakdown of EventLatency trace events
// between `start_stage` and `end_stage`.
constexpr const char* GetDispatchBreakdownName(
    EventMetrics::DispatchStage start_stage,
    EventMetrics::DispatchStage end_stage) {
  switch (start_stage) {
    case EventMetrics::DispatchStage::kGenerated:
      DCHECK_EQ(end_stage,
                EventMetrics::DispatchStage::kArrivedInRendererCompositor);
      return "GenerationToRendererCompositor";
    case EventMetrics::DispatchStage::kArrivedInRendererCompositor:
      switch (end_stage) {
        case EventMetrics::DispatchStage::kRendererCompositorStarted:
          return "RendererCompositorQueueingDelay";
        case EventMetrics::DispatchStage::kRendererMainStarted:
          return "RendererCompositorToMain";
        default:
          NOTREACHED();
          return nullptr;
      }
    case EventMetrics::DispatchStage::kRendererCompositorStarted:
      DCHECK_EQ(end_stage,
                EventMetrics::DispatchStage::kRendererCompositorFinished);
      return "RendererCompositorProcessing";
    case EventMetrics::DispatchStage::kRendererCompositorFinished:
      DCHECK_EQ(end_stage, EventMetrics::DispatchStage::kRendererMainStarted);
      return "RendererCompositorToMain";
    case EventMetrics::DispatchStage::kRendererMainStarted:
      DCHECK_EQ(end_stage, EventMetrics::DispatchStage::kRendererMainFinished);
      return "RendererMainProcessing";
    case EventMetrics::DispatchStage::kRendererMainFinished:
      NOTREACHED();
      return nullptr;
  }
}

// Returns the name of EventLatency breakdown between `dispatch_stage` and
// `compositor_stage`.
constexpr const char* GetDispatchToCompositorBreakdownName(
    EventMetrics::DispatchStage dispatch_stage,
    CompositorFrameReporter::StageType compositor_stage) {
  switch (dispatch_stage) {
    case EventMetrics::DispatchStage::kRendererCompositorFinished:
      switch (compositor_stage) {
        case CompositorFrameReporter::StageType::
            kBeginImplFrameToSendBeginMainFrame:
          return "RendererCompositorFinishedToBeginImplFrame";
        case CompositorFrameReporter::StageType::kSendBeginMainFrameToCommit:
          return "RendererCompositorFinishedToSendBeginMainFrame";
        case CompositorFrameReporter::StageType::kCommit:
          return "RendererCompositorFinishedToCommit";
        case CompositorFrameReporter::StageType::kEndCommitToActivation:
          return "RendererCompositorFinishedToEndCommit";
        case CompositorFrameReporter::StageType::kActivation:
          return "RendererCompositorFinishedToActivation";
        case CompositorFrameReporter::StageType::
            kEndActivateToSubmitCompositorFrame:
          return "RendererCompositorFinishedToEndActivate";
        case CompositorFrameReporter::StageType::
            kSubmitCompositorFrameToPresentationCompositorFrame:
          return "RendererCompositorFinishedToSubmitCompositorFrame";
        default:
          NOTREACHED();
          return nullptr;
      }
    case EventMetrics::DispatchStage::kRendererMainFinished:
      switch (compositor_stage) {
        case CompositorFrameReporter::StageType::
            kBeginImplFrameToSendBeginMainFrame:
          return "RendererMainFinishedToBeginImplFrame";
        case CompositorFrameReporter::StageType::kSendBeginMainFrameToCommit:
          return "RendererMainFinishedToSendBeginMainFrame";
        case CompositorFrameReporter::StageType::kCommit:
          return "RendererMainFinishedToCommit";
        case CompositorFrameReporter::StageType::kEndCommitToActivation:
          return "RendererMainFinishedToEndCommit";
        case CompositorFrameReporter::StageType::kActivation:
          return "RendererMainFinishedToActivation";
        case CompositorFrameReporter::StageType::
            kEndActivateToSubmitCompositorFrame:
          return "RendererMainFinishedToEndActivate";
        case CompositorFrameReporter::StageType::
            kSubmitCompositorFrameToPresentationCompositorFrame:
          return "RendererMainFinishedToSubmitCompositorFrame";
        default:
          NOTREACHED();
          return nullptr;
      }
    default:
      NOTREACHED();
      return nullptr;
  }
}

// Returns the name of EventLatency breakdown between `dispatch_stage` and
// termination for events not associated with a frame update.
constexpr const char* GetDispatchToTerminationBreakdownName(
    EventMetrics::DispatchStage dispatch_stage) {
  switch (dispatch_stage) {
    case EventMetrics::DispatchStage::kArrivedInRendererCompositor:
      return "ArrivedInRendererCompositorToTermination";
    case EventMetrics::DispatchStage::kRendererCompositorStarted:
      return "RendererCompositorStartedToTermination";
    case EventMetrics::DispatchStage::kRendererCompositorFinished:
      return "RendererCompositorFinishedToTermination";
    case EventMetrics::DispatchStage::kRendererMainStarted:
      return "RendererMainStartedToTermination";
    case EventMetrics::DispatchStage::kRendererMainFinished:
      return "RendererMainFinishedToTermination";
    default:
      NOTREACHED();
      return nullptr;
  }
}

constexpr perfetto::protos::pbzero::EventLatency::EventType ToProtoEnum(
    EventMetrics::EventType event_type) {
#define CASE(event_type, proto_event_type)  \
  case EventMetrics::EventType::event_type: \
    return perfetto::protos::pbzero::EventLatency::proto_event_type
  switch (event_type) {
    CASE(kMousePressed, MOUSE_PRESSED);
    CASE(kMouseReleased, MOUSE_RELEASED);
    CASE(kMouseWheel, MOUSE_WHEEL);
    CASE(kKeyPressed, KEY_PRESSED);
    CASE(kKeyReleased, KEY_RELEASED);
    CASE(kTouchPressed, TOUCH_PRESSED);
    CASE(kTouchReleased, TOUCH_RELEASED);
    CASE(kTouchMoved, TOUCH_MOVED);
    CASE(kGestureScrollBegin, GESTURE_SCROLL_BEGIN);
    CASE(kGestureScrollUpdate, GESTURE_SCROLL_UPDATE);
    CASE(kGestureScrollEnd, GESTURE_SCROLL_END);
    CASE(kGestureDoubleTap, GESTURE_DOUBLE_TAP);
    CASE(kGestureLongPress, GESTURE_LONG_PRESS);
    CASE(kGestureLongTap, GESTURE_LONG_TAP);
    CASE(kGestureShowPress, GESTURE_SHOW_PRESS);
    CASE(kGestureTap, GESTURE_TAP);
    CASE(kGestureTapCancel, GESTURE_TAP_CANCEL);
    CASE(kGestureTapDown, GESTURE_TAP_DOWN);
    CASE(kGestureTapUnconfirmed, GESTURE_TAP_UNCONFIRMED);
    CASE(kGestureTwoFingerTap, GESTURE_TWO_FINGER_TAP);
    CASE(kFirstGestureScrollUpdate, FIRST_GESTURE_SCROLL_UPDATE);
    CASE(kMouseDragged, MOUSE_DRAGGED);
    CASE(kGesturePinchBegin, GESTURE_PINCH_BEGIN);
    CASE(kGesturePinchEnd, GESTURE_PINCH_END);
    CASE(kGesturePinchUpdate, GESTURE_PINCH_UPDATE);
    CASE(kInertialGestureScrollUpdate, INERTIAL_GESTURE_SCROLL_UPDATE);
  }
}

}  // namespace

// static
void EventLatencyTracingRecorder::RecordEventLatencyTraceEvent(
    EventMetrics* event_metrics,
    base::TimeTicks termination_time,
    const std::vector<CompositorFrameReporter::StageData>* stage_history,
    const CompositorFrameReporter::ProcessedVizBreakdown* viz_breakdown) {
  DCHECK(event_metrics);
  DCHECK(!event_metrics->is_tracing_recorded());
  event_metrics->set_tracing_recorded();

  const base::TimeTicks generated_timestamp =
      event_metrics->GetDispatchStageTimestamp(
          EventMetrics::DispatchStage::kGenerated);

  const auto trace_track =
      perfetto::Track(base::trace_event::GetNextGlobalTraceId());
  TRACE_EVENT_BEGIN(
      kTracingCategory, "EventLatency", trace_track, generated_timestamp,
      [&](perfetto::EventContext context) {
        auto* event =
            context.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* event_latency = event->set_event_latency();
        event_latency->set_event_type(ToProtoEnum(event_metrics->type()));
      });

  // Event dispatch stages.
  EventMetrics::DispatchStage dispatch_stage =
      EventMetrics::DispatchStage::kGenerated;
  base::TimeTicks dispatch_timestamp =
      event_metrics->GetDispatchStageTimestamp(dispatch_stage);
  while (dispatch_stage != EventMetrics::DispatchStage::kMaxValue) {
    DCHECK(!dispatch_timestamp.is_null());

    // Find the end dispatch stage.
    auto end_stage = static_cast<EventMetrics::DispatchStage>(
        static_cast<int>(dispatch_stage) + 1);
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

    const char* breakdown_name =
        GetDispatchBreakdownName(dispatch_stage, end_stage);
    TRACE_EVENT_BEGIN(kTracingCategory, perfetto::StaticString{breakdown_name},
                      trace_track, dispatch_timestamp);
    TRACE_EVENT_END(kTracingCategory, trace_track, end_timestamp);

    dispatch_stage = end_stage;
    dispatch_timestamp = end_timestamp;
  }
  if (stage_history) {
    DCHECK(viz_breakdown);
    // Find the first compositor stage that happens after the final dispatch
    // stage.
    auto stage_it = std::find_if(
        stage_history->begin(), stage_history->end(),
        [dispatch_timestamp](const CompositorFrameReporter::StageData& stage) {
          return stage.start_time > dispatch_timestamp;
        });
    // TODO(crbug.com/1079116): Ideally, at least the start time of
    // SubmitCompositorFrameToPresentationCompositorFrame stage should be
    // greater than the final event dispatch timestamp, but apparently, this is
    // not always the case (see crbug.com/1093698). For now, skip to the next
    // event in such cases. Hopefully, the work to reduce discrepancies between
    // the new EventLatency and the old Event.Latency metrics would fix this
    // issue. If not, we need to reconsider investigating this issue.
    if (stage_it == stage_history->end())
      return;

    DCHECK(dispatch_stage ==
               EventMetrics::DispatchStage::kRendererCompositorFinished ||
           dispatch_stage ==
               EventMetrics::DispatchStage::kRendererMainFinished);

    const char* d2c_breakdown_name = GetDispatchToCompositorBreakdownName(
        dispatch_stage, stage_it->stage_type);
    TRACE_EVENT_BEGIN(kTracingCategory,
                      perfetto::StaticString{d2c_breakdown_name}, trace_track,
                      dispatch_timestamp);
    TRACE_EVENT_END(kTracingCategory, trace_track, stage_it->start_time);

    // Compositor stages.
    for (; stage_it != stage_history->end(); ++stage_it) {
      if (stage_it->start_time >= termination_time)
        break;
      DCHECK_GE(stage_it->end_time, stage_it->start_time);
      if (stage_it->start_time == stage_it->end_time)
        continue;
      const char* stage_name =
          CompositorFrameReporter::GetStageName(stage_it->stage_type);

      TRACE_EVENT_BEGIN(kTracingCategory, perfetto::StaticString{stage_name},
                        trace_track, stage_it->start_time);

      if (stage_it->stage_type ==
          CompositorFrameReporter::StageType::
              kSubmitCompositorFrameToPresentationCompositorFrame) {
        DCHECK(viz_breakdown);
        for (auto it = viz_breakdown->CreateIterator(true); it.IsValid();
             it.Advance()) {
          base::TimeTicks start_time = it.GetStartTime();
          base::TimeTicks end_time = it.GetEndTime();
          if (start_time >= end_time)
            continue;
          const char* breakdown_name =
              CompositorFrameReporter::GetVizBreakdownName(it.GetBreakdown());
          TRACE_EVENT_BEGIN(kTracingCategory,
                            perfetto::StaticString{breakdown_name}, trace_track,
                            start_time);
          TRACE_EVENT_END(kTracingCategory, trace_track, end_time);
        }
      }

      TRACE_EVENT_END(kTracingCategory, trace_track, stage_it->end_time);
    }
  } else {
    DCHECK(!viz_breakdown);
    const char* d2t_breakdown_name =
        GetDispatchToTerminationBreakdownName(dispatch_stage);
    TRACE_EVENT_BEGIN(kTracingCategory,
                      perfetto::StaticString{d2t_breakdown_name}, trace_track,
                      dispatch_timestamp);
    TRACE_EVENT_END(kTracingCategory, trace_track, termination_time);
  }
  TRACE_EVENT_END(kTracingCategory, trace_track, termination_time);
}

}  // namespace cc
