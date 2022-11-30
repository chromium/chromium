// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/frame_sequence_tracker.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"
#include "ui/gfx/presentation_feedback.h"

// This macro is used with DCHECK to provide addition debug info.
#if DCHECK_IS_ON()
#define TRACKER_TRACE_STREAM frame_sequence_trace_
#define TRACKER_DCHECK_MSG                                      \
  " in " << GetFrameSequenceTrackerTypeName(this->type())       \
         << " tracker: " << frame_sequence_trace_.str() << " (" \
         << frame_sequence_trace_.str().size() << ")";
#else
#define TRACKER_TRACE_STREAM EAT_STREAM_PARAMETERS
#define TRACKER_DCHECK_MSG ""
#endif

namespace cc {

namespace {

constexpr char kTraceCategory[] =
    "cc,benchmark," TRACE_DISABLED_BY_DEFAULT("devtools.timeline.frame");

}  // namespace

using ThreadType = FrameInfo::SmoothEffectDrivingThread;

// In the |TRACKER_TRACE_STREAM|, we mod the numbers such as frame sequence
// number, or frame token, such that the debug string is not too long.
constexpr int kDebugStrMod = 1000;

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

FrameSequenceTracker::FrameSequenceTracker(
    FrameSequenceTrackerType type,
    ThroughputUkmReporter* throughput_ukm_reporter)
    : custom_sequence_id_(-1),
      metrics_(
          std::make_unique<FrameSequenceMetrics>(type,
                                                 throughput_ukm_reporter)) {
  DCHECK_LT(type, FrameSequenceTrackerType::kMaxType);
  DCHECK(type != FrameSequenceTrackerType::kCustom);
  // TODO(crbug.com/1158439): remove the trace event once the validation is
  // completed.
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP1(
      kTraceCategory, "TrackerValidation", TRACE_ID_LOCAL(this),
      base::TimeTicks::Now(), "name", GetFrameSequenceTrackerTypeName(type));
}

FrameSequenceTracker::FrameSequenceTracker(
    int custom_sequence_id,
    FrameSequenceMetrics::CustomReporter custom_reporter)
    : custom_sequence_id_(custom_sequence_id),
      metrics_(std::make_unique<FrameSequenceMetrics>(
          FrameSequenceTrackerType::kCustom,
          /*ukm_reporter=*/nullptr)) {
  DCHECK_GT(custom_sequence_id_, 0);
  metrics_->SetCustomReporter(std::move(custom_reporter));
}

FrameSequenceTracker::~FrameSequenceTracker() {
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP2(
      kTraceCategory, "TrackerValidation", TRACE_ID_LOCAL(this),
      base::TimeTicks::Now(), "aborted_main", aborted_main_frame_,
      "no_damage_main", no_damage_draw_main_frames_);
  CleanUp();
}

void FrameSequenceTracker::ScheduleTerminate() {
  // If the last frame has ended and there is no frame awaiting presentation,
  // then it is ready to terminate.
  if (!is_inside_frame_ && last_submitted_frame_ == 0)
    termination_status_ = TerminationStatus::kReadyForTermination;
  else
    termination_status_ = TerminationStatus::kScheduledForTermination;
}

void FrameSequenceTracker::ReportBeginImplFrame(
    const viz::BeginFrameArgs& args) {
  if (termination_status_ != TerminationStatus::kActive)
    return;

  if (ShouldIgnoreBeginFrameSource(args.frame_id.source_id))
    return;

  TRACKER_TRACE_STREAM << "b(" << args.frame_id.sequence_number % kDebugStrMod
                       << ")";

  DCHECK(!is_inside_frame_) << TRACKER_DCHECK_MSG;
  is_inside_frame_ = true;
#if DCHECK_IS_ON()
  if (args.type == viz::BeginFrameArgs::NORMAL)
    impl_frames_.insert(args.frame_id);
#endif

  DCHECK_EQ(last_started_impl_sequence_, 0u) << TRACKER_DCHECK_MSG;
  last_started_impl_sequence_ = args.frame_id.sequence_number;
  if (reset_all_state_) {
    begin_impl_frame_data_ = {};
    begin_main_frame_data_ = {};
    reset_all_state_ = false;
  }

  DCHECK(!frame_had_no_compositor_damage_) << TRACKER_DCHECK_MSG;
  DCHECK(!compositor_frame_submitted_) << TRACKER_DCHECK_MSG;

  UpdateTrackedFrameData(&begin_impl_frame_data_, args.frame_id.source_id,
                         args.frame_id.sequence_number,
                         args.frames_throttled_since_last);
  impl_throughput().frames_expected +=
      begin_impl_frame_data_.previous_sequence_delta;
#if DCHECK_IS_ON()
  ++impl_throughput().frames_received;
#endif

  if (first_frame_timestamp_.is_null())
    first_frame_timestamp_ = args.frame_time;
}

void FrameSequenceTracker::ReportBeginMainFrame(
    const viz::BeginFrameArgs& args) {
  if (termination_status_ != TerminationStatus::kActive)
    return;

  if (ShouldIgnoreBeginFrameSource(args.frame_id.source_id))
    return;

  TRACKER_TRACE_STREAM << "B("
                       << begin_main_frame_data_.previous_sequence %
                              kDebugStrMod
                       << "," << args.frame_id.sequence_number % kDebugStrMod
                       << ")";

  if (first_received_main_sequence_ &&
      first_received_main_sequence_ > args.frame_id.sequence_number) {
    return;
  }

  if (!first_received_main_sequence_ &&
      ShouldIgnoreSequence(args.frame_id.sequence_number)) {
    return;
  }

#if DCHECK_IS_ON()
  if (args.type == viz::BeginFrameArgs::NORMAL) {
    DCHECK(impl_frames_.contains(args.frame_id)) << TRACKER_DCHECK_MSG;
  }
#endif

  last_processed_main_sequence_latency_ = 0;
  pending_main_sequences_.push_back(args.frame_id.sequence_number);

  UpdateTrackedFrameData(&begin_main_frame_data_, args.frame_id.source_id,
                         args.frame_id.sequence_number,
                         args.frames_throttled_since_last);
  if (!first_received_main_sequence_ ||
      first_received_main_sequence_ <= last_no_main_damage_sequence_) {
    first_received_main_sequence_ = args.frame_id.sequence_number;
  }
  main_throughput().frames_expected +=
      begin_main_frame_data_.previous_sequence_delta;
  previous_begin_main_sequence_ = current_begin_main_sequence_;
  current_begin_main_sequence_ = args.frame_id.sequence_number;
}

void FrameSequenceTracker::ReportMainFrameProcessed(
    const viz::BeginFrameArgs& args) {
  if (termination_status_ != TerminationStatus::kActive)
    return;

  if (ShouldIgnoreBeginFrameSource(args.frame_id.source_id))
    return;

  TRACKER_TRACE_STREAM << "E(" << args.frame_id.sequence_number % kDebugStrMod
                       << ")";

  const bool previous_main_frame_submitted_or_no_damage =
      previous_begin_main_sequence_ != 0 &&
      (last_submitted_main_sequence_ == previous_begin_main_sequence_ ||
       last_no_main_damage_sequence_ == previous_begin_main_sequence_);
  if (last_processed_main_sequence_ != 0 &&
      !had_impl_frame_submitted_between_commits_ &&
      !previous_main_frame_submitted_or_no_damage) {
    DCHECK_GE(main_throughput().frames_expected,
              begin_main_frame_data_.previous_sequence_delta)
        << TRACKER_DCHECK_MSG;
    main_throughput().frames_expected -=
        begin_main_frame_data_.previous_sequence_delta;
    last_no_main_damage_sequence_ = previous_begin_main_sequence_;
  }
  had_impl_frame_submitted_between_commits_ = false;

  if (first_received_main_sequence_ &&
      args.frame_id.sequence_number >= first_received_main_sequence_) {
    DCHECK_EQ(last_processed_main_sequence_latency_, 0u) << TRACKER_DCHECK_MSG;
    last_processed_main_sequence_ = args.frame_id.sequence_number;
    last_processed_main_sequence_latency_ =
        std::max(last_started_impl_sequence_, last_processed_impl_sequence_) -
        args.frame_id.sequence_number;
  }
}

void FrameSequenceTracker::ReportSubmitFrame(
    uint32_t frame_token,
    bool has_missing_content,
    const viz::BeginFrameAck& ack,
    const viz::BeginFrameArgs& origin_args) {
  DCHECK_NE(termination_status_, TerminationStatus::kReadyForTermination);

  // TODO(crbug.com/1072482): find a proper way to terminate a tracker.
  // Right now, we define a magical number |frames_to_terminate_tracker| = 3,
  // which means that if this frame_token is more than 3 frames compared with
  // the last submitted frame, then we assume that the last submitted frame is
  // not going to be presented, and thus terminate this tracker.
  const uint32_t frames_to_terminate_tracker = 3;
  if (termination_status_ == TerminationStatus::kScheduledForTermination &&
      last_submitted_frame_ != 0 &&
      viz::FrameTokenGT(frame_token,
                        last_submitted_frame_ + frames_to_terminate_tracker)) {
    termination_status_ = TerminationStatus::kReadyForTermination;
    return;
  }

  if (ShouldIgnoreBeginFrameSource(ack.frame_id.source_id) ||
      ShouldIgnoreSequence(ack.frame_id.sequence_number)) {
    ignored_frame_tokens_.insert(frame_token);
    return;
  }

#if DCHECK_IS_ON()
  DCHECK(is_inside_frame_) << TRACKER_DCHECK_MSG;
  DCHECK_LT(impl_throughput().frames_processed,
            impl_throughput().frames_received)
      << TRACKER_DCHECK_MSG;
  ++impl_throughput().frames_processed;
#endif

  last_processed_impl_sequence_ = ack.frame_id.sequence_number;
  if (first_submitted_frame_ == 0)
    first_submitted_frame_ = frame_token;
  last_submitted_frame_ = frame_token;
  compositor_frame_submitted_ = true;

  TRACKER_TRACE_STREAM << "s(" << frame_token % kDebugStrMod << ")";
  had_impl_frame_submitted_between_commits_ = true;
  metrics()->NotifySubmitForJankReporter(
      FrameInfo::SmoothEffectDrivingThread::kCompositor, frame_token,
      ack.frame_id.sequence_number);

  const bool main_changes_after_sequence_started =
      first_received_main_sequence_ &&
      origin_args.frame_id.sequence_number >= first_received_main_sequence_;
  const bool main_changes_include_new_changes =
      last_submitted_main_sequence_ == 0 ||
      origin_args.frame_id.sequence_number > last_submitted_main_sequence_;
  const bool main_change_had_no_damage =
      last_no_main_damage_sequence_ != 0 &&
      origin_args.frame_id.sequence_number == last_no_main_damage_sequence_;
  const bool origin_args_is_valid = origin_args.frame_id.sequence_number <=
                                    begin_main_frame_data_.previous_sequence;

  if (!ShouldIgnoreBeginFrameSource(origin_args.frame_id.source_id) &&
      origin_args_is_valid) {
    if (main_changes_after_sequence_started &&
        main_changes_include_new_changes && !main_change_had_no_damage) {
      submitted_frame_had_new_main_content_ = true;
      TRACKER_TRACE_STREAM << "S("
                           << origin_args.frame_id.sequence_number %
                                  kDebugStrMod
                           << ")";
      metrics()->NotifySubmitForJankReporter(
          FrameInfo::SmoothEffectDrivingThread::kMain, frame_token,
          origin_args.frame_id.sequence_number);

      last_submitted_main_sequence_ = origin_args.frame_id.sequence_number;
      main_frames_.push_back(frame_token);
      DCHECK_GE(main_throughput().frames_expected, main_frames_.size())
          << TRACKER_DCHECK_MSG;
    }
  }

  if (has_missing_content) {
    checkerboarding_.frames.push_back(frame_token);
  }
}

void FrameSequenceTracker::ReportFrameEnd(
    const viz::BeginFrameArgs& args,
    const viz::BeginFrameArgs& main_args) {
  DCHECK_NE(termination_status_, TerminationStatus::kReadyForTermination);

  if (ShouldIgnoreBeginFrameSource(args.frame_id.source_id))
    return;

  // We only update the `pending_main_sequences` when the frame has successfully
  // submitted, or when we determine that it has no damage. See
  // ReportMainFrameCausedNoDamage. We do not do this in
  // NotifyMainFrameProcessed, as that can occur during Commit, and we may yet
  // determine at Draw that there was no damage.
  while (!pending_main_sequences_.empty() &&
         pending_main_sequences_.front() <=
             main_args.frame_id.sequence_number) {
    pending_main_sequences_.pop_front();
  }
  TRACKER_TRACE_STREAM << "e(" << args.frame_id.sequence_number % kDebugStrMod
                       << ","
                       << main_args.frame_id.sequence_number % kDebugStrMod
                       << ")";

  bool should_ignore_sequence =
      ShouldIgnoreSequence(args.frame_id.sequence_number);
  if (reset_all_state_) {
    begin_impl_frame_data_ = {};
    begin_main_frame_data_ = {};
    reset_all_state_ = false;
  }

  if (should_ignore_sequence) {
    is_inside_frame_ = false;
    return;
  }

  if (compositor_frame_submitted_ && submitted_frame_had_new_main_content_ &&
      last_processed_main_sequence_latency_) {
    // If a compositor frame was submitted with new content from the
    // main-thread, then make sure the latency gets accounted for.
    main_throughput().frames_expected += last_processed_main_sequence_latency_;
  }

  // It is possible that the compositor claims there was no damage from the
  // compositor, but before the frame ends, it submits a compositor frame (e.g.
  // with some damage from main). In such cases, the compositor is still
  // responsible for processing the update, and therefore the 'no damage' claim
  // is ignored.
  if (frame_had_no_compositor_damage_ && !compositor_frame_submitted_) {
    DCHECK_GT(impl_throughput().frames_expected, 0u) << TRACKER_DCHECK_MSG;
    DCHECK_GT(impl_throughput().frames_expected,
              impl_throughput().frames_produced)
        << TRACKER_DCHECK_MSG;
    DCHECK_GE(impl_throughput().frames_produced,
              impl_throughput().frames_ontime)
        << TRACKER_DCHECK_MSG;
    --impl_throughput().frames_expected;
    metrics()->NotifyNoUpdateForJankReporter(
        FrameInfo::SmoothEffectDrivingThread::kCompositor,
        args.frame_id.sequence_number, args.interval);
#if DCHECK_IS_ON()
    ++impl_throughput().frames_processed;
    // If these two are the same, it means that each impl frame is either
    // no-damage or submitted. That's expected, so we don't need those in the
    // output of DCHECK.
    if (impl_throughput().frames_processed == impl_throughput().frames_received)
      ignored_trace_char_count_ = frame_sequence_trace_.str().size();
    else
      NOTREACHED() << TRACKER_DCHECK_MSG;
#endif
    begin_impl_frame_data_.previous_sequence = 0;
  }
  // last_submitted_frame_ == 0 means the last impl frame has been presented.
  if (termination_status_ == TerminationStatus::kScheduledForTermination &&
      last_submitted_frame_ == 0)
    termination_status_ = TerminationStatus::kReadyForTermination;

  frame_had_no_compositor_damage_ = false;
  compositor_frame_submitted_ = false;
  submitted_frame_had_new_main_content_ = false;
  last_processed_main_sequence_latency_ = 0;

  DCHECK(is_inside_frame_) << TRACKER_DCHECK_MSG;
  is_inside_frame_ = false;

  DCHECK_EQ(last_started_impl_sequence_, last_processed_impl_sequence_)
      << TRACKER_DCHECK_MSG;
  last_started_impl_sequence_ = 0;
}

void FrameSequenceTracker::ReportFramePresented(
    uint32_t frame_token,
    const gfx::PresentationFeedback& feedback) {
  // TODO(xidachen): We should early exit if |last_submitted_frame_| = 0, as it
  // means that we are presenting the same frame_token again.
  const bool submitted_frame_since_last_presentation = !!last_submitted_frame_;
  // !viz::FrameTokenGT(a, b) is equivalent to b >= a.
  const bool frame_token_acks_last_frame =
      !viz::FrameTokenGT(last_submitted_frame_, frame_token);

  // Even if the presentation timestamp is null, we set last_submitted_frame_ to
  // 0 such that the tracker can be terminated.
  if (last_submitted_frame_ && frame_token_acks_last_frame)
    last_submitted_frame_ = 0;
  // Update termination status if this is scheduled for termination, and it is
  // not waiting for any frames, or it has received the presentation-feedback
  // for the latest frame it is tracking.
  //
  // We should always wait for an impl frame to end, that is, ReportFrameEnd.
  if (termination_status_ == TerminationStatus::kScheduledForTermination &&
      last_submitted_frame_ == 0 && !is_inside_frame_) {
    termination_status_ = TerminationStatus::kReadyForTermination;
  }

  if (first_submitted_frame_ == 0 ||
      viz::FrameTokenGT(first_submitted_frame_, frame_token)) {
    // We are getting presentation feedback for frames that were submitted
    // before this sequence started. So ignore these.
    return;
  }

  TRACKER_TRACE_STREAM << "P(" << frame_token % kDebugStrMod << ")";

  base::EraseIf(ignored_frame_tokens_, [frame_token](const uint32_t& token) {
    return viz::FrameTokenGT(frame_token, token);
  });
  if (ignored_frame_tokens_.contains(frame_token))
    return;

  const auto vsync_interval =
      (feedback.interval.is_zero() ? viz::BeginFrameArgs::DefaultInterval()
                                   : feedback.interval);
  DCHECK(!vsync_interval.is_zero()) << TRACKER_DCHECK_MSG;
  base::TimeTicks safe_deadline_for_frame =
      last_frame_presentation_timestamp_ + vsync_interval * 1.5;

  const bool was_presented = !feedback.failed();
  if (was_presented && submitted_frame_since_last_presentation) {
    if (!last_frame_presentation_timestamp_.is_null() &&
        (safe_deadline_for_frame < feedback.timestamp)) {
      DCHECK_LE(impl_throughput().frames_ontime,
                impl_throughput().frames_produced)
          << TRACKER_DCHECK_MSG;
      ++impl_throughput().frames_ontime;
    }

    DCHECK_LT(impl_throughput().frames_produced,
              impl_throughput().frames_expected)
        << TRACKER_DCHECK_MSG;
    ++impl_throughput().frames_produced;
    if (metrics()->GetEffectiveThread() == ThreadType::kCompositor) {
      metrics()->AdvanceTrace(feedback.timestamp);
    }

    metrics()->ComputeJank(FrameInfo::SmoothEffectDrivingThread::kCompositor,
                           frame_token, feedback.timestamp, vsync_interval);
  }

  if (was_presented) {
    // This presentation includes the visual update from all main frame tokens
    // <= |frame_token|.
    const unsigned size_before_erase = main_frames_.size();
    while (!main_frames_.empty() &&
           !viz::FrameTokenGT(main_frames_.front(), frame_token)) {
      main_frames_.pop_front();
    }
    if (main_frames_.size() < size_before_erase) {
      DCHECK_LT(main_throughput().frames_produced,
                main_throughput().frames_expected)
          << TRACKER_DCHECK_MSG;
      ++main_throughput().frames_produced;
      if (metrics()->GetEffectiveThread() == ThreadType::kMain) {
        metrics()->AdvanceTrace(feedback.timestamp);
      }

      metrics()->ComputeJank(FrameInfo::SmoothEffectDrivingThread::kMain,
                             frame_token, feedback.timestamp, vsync_interval);
    }
    if (main_frames_.size() < size_before_erase) {
      if (!last_frame_presentation_timestamp_.is_null() &&
          (safe_deadline_for_frame < feedback.timestamp)) {
        DCHECK_LE(main_throughput().frames_ontime,
                  main_throughput().frames_produced)
            << TRACKER_DCHECK_MSG;
        ++main_throughput().frames_ontime;
      }
    }
    last_frame_presentation_timestamp_ = feedback.timestamp;

    if (checkerboarding_.last_frame_had_checkerboarding) {
      DCHECK(!checkerboarding_.last_frame_timestamp.is_null())
          << TRACKER_DCHECK_MSG;
      DCHECK(!feedback.timestamp.is_null()) << TRACKER_DCHECK_MSG;

      // |feedback.timestamp| is the timestamp when the latest frame was
      // presented. |checkerboarding_.last_frame_timestamp| is the timestamp
      // when the previous frame (which had checkerboarding) was presented. Use
      // |feedback.interval| to compute the number of vsyncs that have passed
      // between the two frames (since that is how many times the user saw that
      // checkerboarded frame).
      base::TimeDelta difference =
          feedback.timestamp - checkerboarding_.last_frame_timestamp;
      const auto& interval = feedback.interval.is_zero()
                                 ? viz::BeginFrameArgs::DefaultInterval()
                                 : feedback.interval;
      DCHECK(!interval.is_zero()) << TRACKER_DCHECK_MSG;
      constexpr base::TimeDelta kEpsilon = base::Milliseconds(1);
      int64_t frames = (difference + kEpsilon).IntDiv(interval);
      metrics_->add_checkerboarded_frames(frames);
    }

    const bool frame_had_checkerboarding =
        base::Contains(checkerboarding_.frames, frame_token);
    checkerboarding_.last_frame_had_checkerboarding = frame_had_checkerboarding;
    checkerboarding_.last_frame_timestamp = feedback.timestamp;
  }

  while (!checkerboarding_.frames.empty() &&
         !viz::FrameTokenGT(checkerboarding_.frames.front(), frame_token)) {
    checkerboarding_.frames.pop_front();
  }
}

void FrameSequenceTracker::ReportImplFrameCausedNoDamage(
    const viz::BeginFrameAck& ack) {
  DCHECK_NE(termination_status_, TerminationStatus::kReadyForTermination);

  if (ShouldIgnoreBeginFrameSource(ack.frame_id.source_id))
    return;

  TRACKER_TRACE_STREAM << "n(" << ack.frame_id.sequence_number % kDebugStrMod
                       << ")";

  // This tracker would be scheduled to terminate, and this frame doesn't belong
  // to that tracker.
  if (ShouldIgnoreSequence(ack.frame_id.sequence_number))
    return;

  last_processed_impl_sequence_ = ack.frame_id.sequence_number;
  // If there is no damage for this frame (and no frame is submitted), then the
  // impl-sequence needs to be reset. However, this should be done after the
  // processing the frame is complete (i.e. in ReportFrameEnd()), so that other
  // notifications (e.g. 'no main damage' etc.) can be handled correctly.
  DCHECK_EQ(begin_impl_frame_data_.previous_sequence,
            ack.frame_id.sequence_number);
  frame_had_no_compositor_damage_ = true;
}

void FrameSequenceTracker::ReportMainFrameCausedNoDamage(
    const viz::BeginFrameArgs& args,
    bool aborted) {
  if (termination_status_ != TerminationStatus::kActive)
    return;

  if (ShouldIgnoreBeginFrameSource(args.frame_id.source_id))
    return;

  TRACKER_TRACE_STREAM << "N("
                       << begin_main_frame_data_.previous_sequence %
                              kDebugStrMod
                       << "," << args.frame_id.sequence_number % kDebugStrMod
                       << ")";

  if (!first_received_main_sequence_ ||
      first_received_main_sequence_ > args.frame_id.sequence_number) {
    return;
  }

  if (last_no_main_damage_sequence_ == args.frame_id.sequence_number)
    return;

  auto initial_pending_size = pending_main_sequences_.size();
  while (!pending_main_sequences_.empty() &&
         pending_main_sequences_.front() <= args.frame_id.sequence_number) {
    pending_main_sequences_.pop_front();
  }
  // If we didn't remove any `pending_main_sequences`, then we have previously
  // submitted a CompositorFrame with damage for `args.frame_id.sequence_number`
  // and the sequence is being re-used on a subsequent Impl frame. Which just
  // happens to have no damage.
  //
  // This can occur when there is a Compositor Animation that is offscreen, and
  // when we are awaiting the next BeginMainFrame to be Committed and Activated.
  //
  // We do not change the `main_throughput` expectations when the sequence is
  // re-used.
  if (pending_main_sequences_.size() == initial_pending_size)
    return;

  if (aborted)
    ++aborted_main_frame_;
  else
    ++no_damage_draw_main_frames_;

  DCHECK_GT(main_throughput().frames_expected, 0u) << TRACKER_DCHECK_MSG;
  DCHECK_GT(main_throughput().frames_expected,
            main_throughput().frames_produced)
      << TRACKER_DCHECK_MSG;
  DCHECK_GE(main_throughput().frames_produced, main_throughput().frames_ontime)
      << TRACKER_DCHECK_MSG;
  last_no_main_damage_sequence_ = args.frame_id.sequence_number;
  --main_throughput().frames_expected;
  metrics()->NotifyNoUpdateForJankReporter(
      FrameInfo::SmoothEffectDrivingThread::kMain,
      args.frame_id.sequence_number, args.interval);

  DCHECK_GE(main_throughput().frames_expected, main_frames_.size())
      << TRACKER_DCHECK_MSG;

  // Could be 0 if there were a pause frame production.
  if (begin_main_frame_data_.previous_sequence != 0) {
    DCHECK_GE(begin_main_frame_data_.previous_sequence,
              args.frame_id.sequence_number)
        << TRACKER_DCHECK_MSG;
  }
  begin_main_frame_data_.previous_sequence = 0;
}

void FrameSequenceTracker::PauseFrameProduction() {
  // The states need to be reset, so that the tracker ignores the vsyncs until
  // the next received begin-frame. However, defer doing that until the frame
  // ends (or a new frame starts), so that in case a frame is in-progress,
  // subsequent notifications for that frame can be handled correctly.
  TRACKER_TRACE_STREAM << 'R';
  reset_all_state_ = true;
}

void FrameSequenceTracker::UpdateTrackedFrameData(
    TrackedFrameData* frame_data,
    uint64_t source_id,
    uint64_t sequence_number,
    uint64_t throttled_frame_count) {
  if (frame_data->previous_sequence &&
      frame_data->previous_source == source_id) {
    uint32_t current_latency =
        sequence_number - frame_data->previous_sequence - throttled_frame_count;
    DCHECK_GT(current_latency, 0u) << TRACKER_DCHECK_MSG;
    frame_data->previous_sequence_delta = current_latency;
  } else {
    frame_data->previous_sequence_delta = 1;
  }
  frame_data->previous_source = source_id;
  frame_data->previous_sequence = sequence_number;
}

bool FrameSequenceTracker::ShouldIgnoreBeginFrameSource(
    uint64_t source_id) const {
  if (begin_impl_frame_data_.previous_source == 0)
    return source_id == viz::BeginFrameArgs::kManualSourceId;
  return source_id != begin_impl_frame_data_.previous_source;
}

// This check handles two cases:
// 1. When there is a call to ReportBeginMainFrame, or ReportSubmitFrame, or
// ReportFramePresented, there must be a ReportBeginImplFrame for that sequence.
// Otherwise, the begin_impl_frame_data_.previous_sequence would be 0.
// 2. A tracker is scheduled to terminate, then any new request to handle a new
// impl frame whose sequence_number > begin_impl_frame_data_.previous_sequence
// should be ignored.
// Note that sequence_number < begin_impl_frame_data_.previous_sequence cannot
// happen.
bool FrameSequenceTracker::ShouldIgnoreSequence(
    uint64_t sequence_number) const {
  return sequence_number != begin_impl_frame_data_.previous_sequence;
}

bool FrameSequenceTracker::ShouldReportMetricsNow(
    const viz::BeginFrameArgs& args) const {
  return metrics_->HasEnoughDataForReporting() &&
         !first_frame_timestamp_.is_null() &&
         args.frame_time - first_frame_timestamp_ >= time_delta_to_report_;
}

std::unique_ptr<FrameSequenceMetrics> FrameSequenceTracker::TakeMetrics() {
#if DCHECK_IS_ON()
  DCHECK_EQ(impl_throughput().frames_received,
            impl_throughput().frames_processed)
      << frame_sequence_trace_.str().substr(ignored_trace_char_count_);
#endif
  return std::move(metrics_);
}

void FrameSequenceTracker::CleanUp() {
  if (metrics_)
    metrics_->ReportLeftoverData();
}

void FrameSequenceTracker::AddSortedFrame(const viz::BeginFrameArgs& args,
                                          const FrameInfo& frame_info) {
  if (metrics_)
    metrics_->AddSortedFrame(args, frame_info);
}

FrameSequenceTracker::CheckerboardingData::CheckerboardingData() = default;
FrameSequenceTracker::CheckerboardingData::~CheckerboardingData() = default;

}  // namespace cc
