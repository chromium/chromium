// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/ukm_manager.h"

#include <algorithm>
#include <utility>

#include "cc/metrics/compositor_frame_reporter.h"
#include "cc/metrics/throughput_ukm_reporter.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace cc {

UkmManager::UkmManager(std::unique_ptr<ukm::UkmRecorder> recorder)
    : recorder_(std::move(recorder)) {
  DCHECK(recorder_);
}

UkmManager::~UkmManager() {
  RecordCheckerboardUkm();
  RecordRenderingUkm();
}

void UkmManager::SetSourceId(ukm::SourceId source_id) {
  // If we accumulated any metrics, record them before resetting the source.
  RecordCheckerboardUkm();
  RecordRenderingUkm();

  source_id_ = source_id;
}

void UkmManager::SetUserInteractionInProgress(bool in_progress) {
  if (user_interaction_in_progress_ == in_progress)
    return;

  user_interaction_in_progress_ = in_progress;
  if (!user_interaction_in_progress_)
    RecordCheckerboardUkm();
}

void UkmManager::AddCheckerboardStatsForFrame(int64_t checkerboard_area,
                                              int64_t num_missing_tiles,
                                              int64_t total_visible_area) {
  DCHECK_GE(total_visible_area, checkerboard_area);
  if (source_id_ == ukm::kInvalidSourceId || !user_interaction_in_progress_)
    return;

  checkerboarded_content_area_ += checkerboard_area;
  num_missing_tiles_ += num_missing_tiles;
  total_visible_area_ += total_visible_area;
  num_of_frames_++;
}

void UkmManager::AddCheckerboardedImages(int num_of_checkerboarded_images) {
  if (user_interaction_in_progress_) {
    num_of_images_checkerboarded_during_interaction_ +=
        num_of_checkerboarded_images;
  }
  total_num_of_checkerboarded_images_ += num_of_checkerboarded_images;
}

void UkmManager::RecordCheckerboardUkm() {
  // Only make a recording if there was any visible area from PictureLayers,
  // which can be checkerboarded.
  if (num_of_frames_ > 0 && total_visible_area_ > 0) {
    DCHECK_NE(source_id_, ukm::kInvalidSourceId);
    ukm::builders::Compositor_UserInteraction(source_id_)
        .SetCheckerboardedContentArea(checkerboarded_content_area_ /
                                      num_of_frames_)
        .SetNumMissingTiles(num_missing_tiles_ / num_of_frames_)
        .SetCheckerboardedContentAreaRatio(
            (checkerboarded_content_area_ * 100) / total_visible_area_)
        .SetCheckerboardedImagesCount(
            num_of_images_checkerboarded_during_interaction_)
        .Record(recorder_.get());
  }

  checkerboarded_content_area_ = 0;
  num_missing_tiles_ = 0;
  num_of_frames_ = 0;
  total_visible_area_ = 0;
  num_of_images_checkerboarded_during_interaction_ = 0;
}

void UkmManager::RecordRenderingUkm() {
  if (source_id_ == ukm::kInvalidSourceId)
    return;

  ukm::builders::Compositor_Rendering(source_id_)
      .SetCheckerboardedImagesCount(total_num_of_checkerboarded_images_)
      .Record(recorder_.get());
  total_num_of_checkerboarded_images_ = 0;
}

void UkmManager::RecordThroughputUKM(
    FrameSequenceTrackerType tracker_type,
    FrameSequenceMetrics::ThreadType thread_type,
    int64_t throughput) const {
  ukm::builders::Graphics_Smoothness_PercentDroppedFrames builder(source_id_);
  switch (thread_type) {
    case FrameSequenceMetrics::ThreadType::kMain: {
      switch (tracker_type) {
#define CASE_FOR_MAIN_THREAD_TRACKER(name)    \
  case FrameSequenceTrackerType::k##name:     \
    builder.SetMainThread_##name(throughput); \
    break;
        CASE_FOR_MAIN_THREAD_TRACKER(CompositorAnimation);
        CASE_FOR_MAIN_THREAD_TRACKER(MainThreadAnimation);
        CASE_FOR_MAIN_THREAD_TRACKER(PinchZoom);
        CASE_FOR_MAIN_THREAD_TRACKER(RAF);
        CASE_FOR_MAIN_THREAD_TRACKER(ScrollbarScroll);
        CASE_FOR_MAIN_THREAD_TRACKER(TouchScroll);
        CASE_FOR_MAIN_THREAD_TRACKER(Video);
        CASE_FOR_MAIN_THREAD_TRACKER(WheelScroll);
#undef CASE_FOR_MAIN_THREAD_TRACKER
        default:
          NOTREACHED();
          break;
      }

      break;
    }

    case FrameSequenceMetrics::ThreadType::kCompositor: {
      switch (tracker_type) {
#define CASE_FOR_COMPOSITOR_THREAD_TRACKER(name)    \
  case FrameSequenceTrackerType::k##name:           \
    builder.SetCompositorThread_##name(throughput); \
    break;
        CASE_FOR_COMPOSITOR_THREAD_TRACKER(CompositorAnimation);
        CASE_FOR_COMPOSITOR_THREAD_TRACKER(MainThreadAnimation);
        CASE_FOR_COMPOSITOR_THREAD_TRACKER(PinchZoom);
        CASE_FOR_COMPOSITOR_THREAD_TRACKER(RAF);
        CASE_FOR_COMPOSITOR_THREAD_TRACKER(ScrollbarScroll);
        CASE_FOR_COMPOSITOR_THREAD_TRACKER(TouchScroll);
        CASE_FOR_COMPOSITOR_THREAD_TRACKER(Video);
        CASE_FOR_COMPOSITOR_THREAD_TRACKER(WheelScroll);
#undef CASE_FOR_COMPOSITOR_THREAD_TRACKER
        default:
          NOTREACHED();
          break;
      }
      break;
    }

    case FrameSequenceMetrics::ThreadType::kUnknown:
      NOTREACHED();
      break;
  }
  builder.Record(recorder_.get());
}

void UkmManager::RecordAggregateThroughput(AggregationType aggregation_type,
                                           int64_t throughput_percent) const {
  ukm::builders::Graphics_Smoothness_PercentDroppedFrames builder(source_id_);
  switch (aggregation_type) {
    case AggregationType::kAllAnimations:
      builder.SetAllAnimations(throughput_percent);
      break;
    case AggregationType::kAllInteractions:
      builder.SetAllInteractions(throughput_percent);
      break;
    case AggregationType::kAllSequences:
      builder.SetAllSequences(throughput_percent);
      break;
  }
  builder.Record(recorder_.get());
}

void UkmManager::RecordCompositorLatencyUKM(
    CompositorFrameReporter::FrameReportType report_type,
    const std::vector<CompositorFrameReporter::StageData>& stage_history,
    const CompositorFrameReporter::ActiveTrackers& active_trackers,
    const viz::FrameTimingDetails& viz_breakdown) const {
  using StageType = CompositorFrameReporter::StageType;

  ukm::builders::Graphics_Smoothness_Latency builder(source_id_);

  if (report_type == CompositorFrameReporter::FrameReportType::kDroppedFrame) {
    builder.SetMissedFrame(true);
  }

  // Record each stage
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
      CASE_FOR_STAGE(TotalLatency);
#undef CASE_FOR_STAGE
      // Break out kSubmitCompositorFrameToPresentationCompositorFrame to report
      // the viz breakdown.
      case StageType::kSubmitCompositorFrameToPresentationCompositorFrame:
        builder.SetSubmitCompositorFrameToPresentationCompositorFrame(
            (stage.end_time - stage.start_time).InMicroseconds());
        if (viz_breakdown.received_compositor_frame_timestamp.is_null())
          break;
        builder
            .SetSubmitCompositorFrameToPresentationCompositorFrame_SubmitToReceiveCompositorFrame(
                (viz_breakdown.received_compositor_frame_timestamp -
                 stage.start_time)
                    .InMicroseconds());
        if (viz_breakdown.draw_start_timestamp.is_null())
          break;
        builder
            .SetSubmitCompositorFrameToPresentationCompositorFrame_ReceivedCompositorFrameToStartDraw(
                (viz_breakdown.draw_start_timestamp -
                 viz_breakdown.received_compositor_frame_timestamp)
                    .InMicroseconds());
        if (viz_breakdown.swap_timings.is_null())
          break;
        builder
            .SetSubmitCompositorFrameToPresentationCompositorFrame_StartDrawToSwapStart(
                (viz_breakdown.swap_timings.swap_start -
                 viz_breakdown.draw_start_timestamp)
                    .InMicroseconds());
        builder
            .SetSubmitCompositorFrameToPresentationCompositorFrame_SwapStartToSwapEnd(
                (viz_breakdown.swap_timings.swap_end -
                 viz_breakdown.swap_timings.swap_start)
                    .InMicroseconds());
        builder
            .SetSubmitCompositorFrameToPresentationCompositorFrame_SwapEndToPresentationCompositorFrame(
                (viz_breakdown.presentation_feedback.timestamp -
                 viz_breakdown.swap_timings.swap_end)
                    .InMicroseconds());
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  // Record the active trackers
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
#undef CASE_FOR_TRACKER
      default:
        NOTREACHED();
        break;
    }
  }

  builder.Record(recorder_.get());
}

void UkmManager::RecordEventLatencyUKM(
    const std::vector<EventMetrics>& events_metrics,
    const std::vector<CompositorFrameReporter::StageData>& stage_history,
    const viz::FrameTimingDetails& viz_breakdown) const {
  using StageType = CompositorFrameReporter::StageType;

  for (const EventMetrics& event_metrics : events_metrics) {
    ukm::builders::Graphics_Smoothness_EventLatency builder(source_id_);

    builder.SetEventType(static_cast<int64_t>(event_metrics.type()));

    if (event_metrics.scroll_type()) {
      builder.SetScrollInputType(
          static_cast<int64_t>(*event_metrics.scroll_type()));

      if (!viz_breakdown.swap_timings.is_null()) {
        builder.SetTotalLatencyToSwapBegin(
            (viz_breakdown.swap_timings.swap_start - event_metrics.time_stamp())
                .InMicroseconds());
      }
    }

    // It is possible for an event to arrive in the compositor in the middle of
    // a frame (e.g. the browser received the event *after* renderer received a
    // begin-impl, and the event reached the compositor before that frame
    // ended). To handle such cases, find the first stage that happens after the
    // event's arrival in the browser.
    auto stage_it = std::find_if(
        stage_history.begin(), stage_history.end(),
        [&event_metrics](const CompositorFrameReporter::StageData& stage) {
          return stage.start_time > event_metrics.time_stamp();
        });
    // TODO(crbug.com/1079116): Ideally, at least the start time of
    // SubmitCompositorFrameToPresentationCompositorFrame stage should be
    // greater than the event time stamp, but apparently, this is not always the
    // case (see crbug.com/1093698). For now, skip to the next event in such
    // cases. Hopefully, the work to reduce discrepancies between the new
    // EventLatency and the old Event.Latency metrics would fix this issue. If
    // not, we need to reconsider investigating this issue.
    if (stage_it == stage_history.end())
      continue;

    builder.SetBrowserToRendererCompositor(
        (stage_it->start_time - event_metrics.time_stamp()).InMicroseconds());

    for (; stage_it != stage_history.end(); ++stage_it) {
      // Total latency is calculated since the event timestamp.
      const base::TimeTicks start_time =
          stage_it->stage_type == StageType::kTotalLatency
              ? event_metrics.time_stamp()
              : stage_it->start_time;

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
        default:
          NOTREACHED();
          break;
      }
    }

    builder.Record(recorder_.get());
  }
}

}  // namespace cc
