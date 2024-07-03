// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/scroll_jank_ukm_reporter.h"

#include "base/trace_event/trace_id_helper.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"
#include "cc/metrics/ukm_manager.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace cc {

ScrollJankUkmReporter::ScrollJankUkmReporter() = default;
ScrollJankUkmReporter::~ScrollJankUkmReporter() {
  EmitScrollJankUkm();
  ResetPredictorMetrics();
  ukm_manager_ = nullptr;
}

void ScrollJankUkmReporter::IncrementFrameCount() {
  num_frames_++;
}

void ScrollJankUkmReporter::IncrementDelayedFrameCount() {
  num_delayed_frames_++;
}

void ScrollJankUkmReporter::AddVsyncs(int vsyncs) {
  num_vsyncs_ += vsyncs;
}

void ScrollJankUkmReporter::AddMissedVsyncs(int missed_vsyncs) {
  num_missed_vsyncs_ += missed_vsyncs;
}

void ScrollJankUkmReporter::IncrementPredictorJankyFrames() {
  predictor_jank_frames_++;
}

void ScrollJankUkmReporter::SetEarliestScrollEvent(
    ScrollUpdateEventMetrics& earliest_event) {
  first_frame_timestamp_ = earliest_event.GetDispatchStageTimestamp(
      EventMetrics::DispatchStage::kGenerated);
}

void ScrollJankUkmReporter::EmitScrollJankUkm() {
  if (first_frame_timestamp_ == base::TimeTicks::Min() ||
      final_frame_presentation_timestamp_ == base::TimeTicks::Min()) {
    return;
  }

  WriteScrollTraceEvent();

  if (ukm_manager_) {
    ukm::builders::Event_Scroll builder(ukm_manager_->source_id());
    builder.SetFrameCount(
        ukm::GetExponentialBucketMinForCounts1000(num_frames_));
    builder.SetVsyncCount(
        ukm::GetExponentialBucketMinForCounts1000(num_vsyncs_));
    builder.SetScrollJank_MissedVsyncsMax(
        ukm::GetExponentialBucketMinForCounts1000(max_missed_vsyncs_));
    builder.SetScrollJank_MissedVsyncsSum(
        ukm::GetExponentialBucketMinForCounts1000(num_missed_vsyncs_));
    builder.SetScrollJank_DelayedFrameCount(
        ukm::GetExponentialBucketMinForCounts1000(num_delayed_frames_));
    builder.SetPredictorJankyFrameCount(
        ukm::GetExponentialBucketMinForCounts1000(predictor_jank_frames_));
    builder.Record(ukm_manager_->recorder());
  }

  // Reset metrics.
  num_frames_ = 0;
  num_vsyncs_ = 0;
  num_missed_vsyncs_ = 0;
  max_missed_vsyncs_ = 0;
  num_delayed_frames_ = 0;
  predictor_jank_frames_ = 0;
}

void ScrollJankUkmReporter::UpdateLatestFrameAndEmitPredictorJank(
    base::TimeTicks latest_timestamp) {
  final_frame_presentation_timestamp_ = latest_timestamp;
  bool should_report =
      frame_with_missed_vsync_ != 0 || frame_with_no_missed_vsync_ != 0;
  if (ukm_manager_ && should_report) {
    ukm::builders::Event_ScrollJank_PredictorJank builder(
        ukm_manager_->source_id());
    builder.SetMaxDelta(ukm::GetExponentialBucketMinForCounts1000(max_delta_));
    builder.SetScrollUpdate_MissedVsync_FrameAboveJankyThreshold2(
        ukm::GetExponentialBucketMinForCounts1000(frame_with_missed_vsync_));
    builder.SetScrollUpdate_NoMissedVsync_FrameAboveJankyThreshold2(
        ukm::GetExponentialBucketMinForCounts1000(frame_with_no_missed_vsync_));
    builder.Record(ukm_manager_->recorder());
  }

  ResetPredictorMetrics();
}

void ScrollJankUkmReporter::ResetPredictorMetrics() {
  max_delta_ = 0;
  frame_with_missed_vsync_ = 0;
  frame_with_no_missed_vsync_ = 0;
}

void ScrollJankUkmReporter::WriteScrollTraceEvent() {
  const auto trace_track =
      perfetto::Track(base::trace_event::GetNextGlobalTraceId());
  TRACE_EVENT_BEGIN(
      "interactions,input.scrolling", "Scroll", trace_track,
      first_frame_timestamp_, [&](perfetto::EventContext& ctx) {
        auto* scroll = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>()
                           ->set_scroll_metrics();
        scroll->set_frame_count(num_frames_);
        scroll->set_vsync_count(num_vsyncs_);
        scroll->set_missed_vsync_max(max_missed_vsyncs_);
        scroll->set_missed_vsync_sum(num_missed_vsyncs_);
        scroll->set_delayed_frame_count(num_delayed_frames_);
        scroll->set_predictor_janky_frame_count(predictor_jank_frames_);
      });

  TRACE_EVENT_END("interactions,input.scrolling", trace_track,
                  final_frame_presentation_timestamp_);

  // Reset tracing variables.
  first_frame_timestamp_ = base::TimeTicks::Min();
  final_frame_presentation_timestamp_ = base::TimeTicks::Min();
}

}  // namespace cc
