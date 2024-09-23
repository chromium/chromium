// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/frame_sequence_tracker.h"

#include <utility>

#include "base/feature_list.h"
#include "cc/base/features.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"
#include "ui/gfx/presentation_feedback.h"

namespace cc {

const char* FrameSequenceTracker::GetFrameSequenceTrackerTypeName(
    FrameSequenceTrackerType type) {
  switch (type) {
    case FrameSequenceTrackerType::kCompositorAnimation:
      return "CompositorAnimation";
    case FrameSequenceTrackerType::kMainThreadAnimation:
      return "MainThreadAnimation";
    case FrameSequenceTrackerType::kPinchZoom:
      return "PinchZoom";
    case FrameSequenceTrackerType::kRAF:
      return "RAF";
    case FrameSequenceTrackerType::kTouchScroll:
      return "TouchScroll";
    case FrameSequenceTrackerType::kVideo:
      return "Video";
    case FrameSequenceTrackerType::kWheelScroll:
      return "WheelScroll";
    case FrameSequenceTrackerType::kScrollbarScroll:
      return "ScrollbarScroll";
    case FrameSequenceTrackerType::kCustom:
      return "Custom";
    case FrameSequenceTrackerType::kCanvasAnimation:
      return "CanvasAnimation";
    case FrameSequenceTrackerType::kJSAnimation:
      return "JSAnimation";
    case FrameSequenceTrackerType::kSETMainThreadAnimation:
      return "SETMainThreadAnimation";
    case FrameSequenceTrackerType::kSETCompositorAnimation:
      return "SETCompositorAnimation";
    case FrameSequenceTrackerType::kMaxType:
      return "";
  }
}

FrameSequenceTracker::FrameSequenceTracker(FrameSequenceTrackerType type)
    : custom_sequence_id_(-1),
      metrics_(std::make_unique<FrameSequenceMetrics>(type)) {
  DCHECK_LT(type, FrameSequenceTrackerType::kMaxType);
  DCHECK(type != FrameSequenceTrackerType::kCustom);
}

FrameSequenceTracker::FrameSequenceTracker(
    int custom_sequence_id,
    FrameSequenceMetrics::CustomReporter custom_reporter)
    : custom_sequence_id_(custom_sequence_id),
      metrics_(std::make_unique<FrameSequenceMetrics>(
          FrameSequenceTrackerType::kCustom)) {
  DCHECK_GT(custom_sequence_id_, 0);
  metrics_->SetCustomReporter(std::move(custom_reporter));
}

FrameSequenceTracker::~FrameSequenceTracker() {
  CleanUp();
}

void FrameSequenceTracker::ScheduleTerminate() {
  // If the last frame has ended and there is no frame awaiting presentation,
  // then it is ready to terminate.
  if (!is_inside_frame_ && last_ended_frame_id_.sequence_number <=
                               last_sorted_frame_id_.sequence_number) {
    termination_status_ = TerminationStatus::kReadyForTermination;
  } else {
    termination_status_ = TerminationStatus::kScheduledForTermination;
  }
}

void FrameSequenceTracker::ReportBeginImplFrame(
    const viz::BeginFrameArgs& args) {
  if (termination_status_ != TerminationStatus::kActive)
    return;

  if (ShouldIgnoreBeginFrameSource(args.frame_id.source_id))
    return;

  is_inside_frame_ = true;
  ResetAllStateIfPaused();

  begin_impl_frame_data_.previous_source = args.frame_id.source_id;
  begin_impl_frame_data_.previous_sequence = args.frame_id.sequence_number;

  if (!first_begin_frame_args_.IsValid()) {
    first_begin_frame_args_ = args;
  }
}

void FrameSequenceTracker::ReportFrameEnd(
    const viz::BeginFrameArgs& args,
    const viz::BeginFrameArgs& main_args) {
  DCHECK_NE(termination_status_, TerminationStatus::kReadyForTermination);
  if (ShouldIgnoreBeginFrameSource(args.frame_id.source_id)) {
    return;
  }

  bool should_ignore_sequence =
      args.frame_id.sequence_number != begin_impl_frame_data_.previous_sequence;
  ResetAllStateIfPaused();
  is_inside_frame_ = false;

  if (should_ignore_sequence) {
    return;
  }

  if (termination_status_ == TerminationStatus::kActive) {
    last_ended_frame_id_ = args.frame_id;
  }
}

void FrameSequenceTracker::PauseFrameProduction() {
  // The states need to be reset, so that the tracker ignores the vsyncs until
  // the next received begin-frame. However, defer doing that until the frame
  // ends (or a new frame starts), so that in case a frame is in-progress,
  // subsequent notifications for that frame can be handled correctly.
  reset_all_state_ = true;
}

bool FrameSequenceTracker::ShouldIgnoreBeginFrameSource(
    uint64_t source_id) const {
  if (begin_impl_frame_data_.previous_source == 0)
    return source_id == viz::BeginFrameArgs::kManualSourceId;
  return source_id != begin_impl_frame_data_.previous_source;
}

void FrameSequenceTracker::ResetAllStateIfPaused() {
  // TODO(crbug.com/40200408): With FrameSequenceMetrics handling
  // FrameInfo::FrameFinalState::kNoUpdateDesired we likely do not need this
  // anymore.
  if (!reset_all_state_) {
    return;
  }
  begin_impl_frame_data_ = {};
  last_ended_frame_id_ = viz::BeginFrameId();
  reset_all_state_ = false;
}

bool FrameSequenceTracker::ShouldReportMetricsNow(
    const viz::BeginFrameArgs& args) const {
  return metrics_->HasEnoughDataForReporting() &&
         first_begin_frame_args_.IsValid() &&
         args.frame_time - first_begin_frame_args_.frame_time >=
             time_delta_to_report_;
}

std::unique_ptr<FrameSequenceMetrics> FrameSequenceTracker::TakeMetrics() {
  return std::move(metrics_);
}

void FrameSequenceTracker::CleanUp() {
  if (metrics_)
    metrics_->ReportLeftoverData();
}

void FrameSequenceTracker::AddSortedFrame(const viz::BeginFrameArgs& args,
                                          const FrameInfo& frame_info) {
  // We will begin receiving sorted frames upon creation. Ignore those that are
  // from before the `first_begin_frame_args_` as they are not a part of this
  // sequence.
  if (!first_begin_frame_args_.IsValid() ||
      args.frame_id.sequence_number <
          first_begin_frame_args_.frame_id.sequence_number) {
    return;
  }

  // Do not process any frames once we are terminated.
  if (termination_status_ == TerminationStatus::kReadyForTermination) {
    return;
  }
  last_sorted_frame_id_ = args.frame_id;
  // For trackers that scheduled for termination, only proceed to update
  // metrics for the frame if it is before the last ended frame.
  if (termination_status_ == TerminationStatus::kScheduledForTermination) {
    if (!last_ended_frame_id_.IsSequenceValid() ||
        last_ended_frame_id_.source_id != args.frame_id.source_id) {
      // Frame source changed so we will no longer receive updates for frames
      // we submitted to the old source.
      termination_status_ = TerminationStatus::kReadyForTermination;
      return;
    }
    // We only terminate when the content is no longer dropped.
    if (frame_info.final_state != FrameInfo::FrameFinalState::kDropped) {
      if (last_ended_frame_id_.sequence_number <
          args.frame_id.sequence_number) {
        termination_status_ = TerminationStatus::kReadyForTermination;
        return;
      } else if (last_ended_frame_id_.sequence_number ==
                 args.frame_id.sequence_number) {
        // We still report the final `sequence_number`, but need to mark for
        // termination.
        termination_status_ = TerminationStatus::kReadyForTermination;
      }
    } else if (!base::FeatureList::IsEnabled(
                   ::features::kMetricsBackfillAdjustmentHoldback) &&
               last_ended_frame_id_.sequence_number <
                   args.frame_id.sequence_number) {
      // Don't count drops from after the sequence has begun termination.
      return;
    }
  }

  // When we are not scheduled for termination we can receive frames whose
  // `sequence_number` is newer than the `last_ended_frame_id_.sequence_number`.
  // These can be backfills for VSyncs for which we did not attempt to produce a
  // frame, due to delays in the previous `sequence_number`, but for which
  // content was still expected.
  if (base::FeatureList::IsEnabled(
          ::features::kMetricsBackfillAdjustmentHoldback) &&
      frame_info.final_state == FrameInfo::FrameFinalState::kDropped &&
      last_ended_frame_id_.sequence_number < args.frame_id.sequence_number) {
    return;
  }
  if (metrics_)
    metrics_->AddSortedFrame(args, frame_info);
}

}  // namespace cc
