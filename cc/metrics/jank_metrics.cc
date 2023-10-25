// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/jank_metrics.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/trace_event/trace_event.h"
#include "cc/metrics/frame_sequence_tracker.h"

namespace cc {

namespace {

constexpr uint64_t kMaxNoUpdateFrameQueueLength = 100;
constexpr int kBuiltinSequenceNum =
    static_cast<int>(FrameSequenceTrackerType::kMaxType) + 1;
constexpr int kMaximumStaleHistogramIndex = kBuiltinSequenceNum;

constexpr base::TimeDelta kStaleHistogramMin = base::Microseconds(1);
constexpr base::TimeDelta kStaleHistogramMax = base::Milliseconds(1000);
constexpr int kStaleHistogramBucketCount = 200;

constexpr bool IsValidJankThreadType(
    FrameInfo::SmoothEffectDrivingThread type) {
  return type == FrameInfo::SmoothEffectDrivingThread::kCompositor ||
         type == FrameInfo::SmoothEffectDrivingThread::kMain;
}

int GetIndexForStaleMetric(FrameSequenceTrackerType type) {
  return static_cast<int>(type);
}

std::string GetStaleHistogramName(FrameSequenceTrackerType type) {
  return base::StrCat(
      {"Graphics.Smoothness.Stale.",
       FrameSequenceTracker::GetFrameSequenceTrackerTypeName(type)});
}

std::string GetMaxStaleHistogramName(FrameSequenceTrackerType type) {
  return base::StrCat(
      {"Graphics.Smoothness.MaxStale.",
       FrameSequenceTracker::GetFrameSequenceTrackerTypeName(type)});
}

}  // namespace

JankMetrics::JankMetrics(FrameSequenceTrackerType tracker_type,
                         FrameInfo::SmoothEffectDrivingThread effective_thread)
    : tracker_type_(tracker_type), effective_thread_(effective_thread) {
  DCHECK(IsValidJankThreadType(effective_thread));
}
JankMetrics::~JankMetrics() = default;

void JankMetrics::AddSubmitFrame(uint32_t frame_token,
                                 uint32_t sequence_number) {
  // When a frame is submitted, record its |frame_token| and its associated
  // |sequence_number|. This pushed item will be removed when this frame is
  // presented.
  queue_frame_token_and_id_.push({frame_token, sequence_number});
}

void JankMetrics::AddFrameWithNoUpdate(uint32_t sequence_number,
                                       base::TimeDelta frame_interval) {
  DCHECK_LE(queue_frame_id_and_interval_.size(), kMaxNoUpdateFrameQueueLength);

  // If a frame does not cause an increase in expected frames, it will be
  // recorded here and later subtracted from the presentation interval that
  // includes this frame.
  queue_frame_id_and_interval_.push({sequence_number, frame_interval});

  // This prevents the no-update frame queue from growing infinitely on an idle
  // page.
  if (queue_frame_id_and_interval_.size() > kMaxNoUpdateFrameQueueLength)
    queue_frame_id_and_interval_.pop();
}

void JankMetrics::AddPresentedFrame(
    uint32_t presented_frame_token,
    base::TimeTicks current_presentation_timestamp,
    base::TimeDelta frame_interval) {
  uint32_t presented_frame_id = 0;

  // Find the main_sequence_number of the presented_frame_token
  while (!queue_frame_token_and_id_.empty()) {
    auto token_and_id = queue_frame_token_and_id_.front();

    if (token_and_id.first > presented_frame_token) {
      // The submitting of this presented frame was not recorded (e.g. the
      // submitting might have occurred before JankMetrics starts recording).
      // In that case, do not use this frame presentation for jank detection.
      return;
    }
    queue_frame_token_and_id_.pop();

    if (token_and_id.first == presented_frame_token) {
      // Found information about the submit of this presented frame;
      // retrieve the frame's sequence number.
      presented_frame_id = token_and_id.second;
      break;
    }
  }
  // If for any reason the sequence number associated with the
  // presented_frame_token cannot be identified, then ignore this frame
  // presentation.
  if (presented_frame_id == 0)
    return;

  base::TimeDelta no_update_time;  // The frame time spanned by the frames that
                                   // have no updates

  // If |queue_frame_id_and_interval_| contains an excessive amount of no-update
  // frames, it indicates that the current presented frame is most likely the
  // first presentation after a long idle period. Such frames are excluded from
  // jank/stale calculation because they usually have little impact on
  // smoothness perception, and |queue_frame_id_and_interval_| does not hold
  // enough data to accurately estimate the effective frame delta.
  bool will_ignore_current_frame =
      queue_frame_id_and_interval_.size() == kMaxNoUpdateFrameQueueLength;

  // Compute the presentation delay contributed by no-update frames that
  // began BEFORE (i.e. have smaller sequence number than) the current
  // presented frame.
  while (!queue_frame_id_and_interval_.empty() &&
         queue_frame_id_and_interval_.front().first < presented_frame_id) {
    auto id_and_interval = queue_frame_id_and_interval_.front();
    if (id_and_interval.first >= last_presentation_frame_id_) {
      // Only count no-update frames that began SINCE (i.e. have a greater [or
      // equal] sequence number than) the beginning of previous presented frame.
      // If, in rare cases, there are still no-update frames that began BEFORE
      // the beginning of previous presented frame left in the queue, those
      // frames will simply be discarded and not counted into |no_update_time|.
      no_update_time += id_and_interval.second;
    }
    queue_frame_id_and_interval_.pop();
  }

  // Exclude the presentation delay introduced by no-update frames. If this
  // exclusion results in negative frame delta, treat the frame delta as 0.
  const base::TimeDelta zero_delta = base::Milliseconds(0);

  // Setting the current_frame_delta to zero conveniently excludes the current
  // frame to be ignored from jank/stale calculation.
  base::TimeDelta current_frame_delta = (will_ignore_current_frame)
                                            ? zero_delta
                                            : current_presentation_timestamp -
                                                  last_presentation_timestamp_ -
                                                  no_update_time;

  // Guard against the situation when the physical presentation interval is
  // shorter than |no_update_time|. For example, consider two BeginFrames A and
  // B separated by 5 vsync cycles of no-updates (i.e. |no_update_time| = 5
  // vsync cycles); the Presentation of A occurs 2 vsync cycles after BeginFrame
  // A, whereas Presentation B occurs in the same vsync cycle as BeginFrame B.
  // In this situation, the physical presentation interval is shorter than 5
  // vsync cycles and will result in a negative |current_frame_delta|.
  if (current_frame_delta < zero_delta)
    current_frame_delta = zero_delta;

  // Only start tracking jank if this function has already been
  // called at least once (so that |last_presentation_timestamp_|
  // and |prev_frame_delta_| have been set).
  //
  // The presentation interval is typically a multiple of VSync
  // intervals (i.e. 16.67ms, 33.33ms, 50ms ... on a 60Hz display)
  // with small fluctuations. The 0.5 * |frame_interval| criterion
  // is chosen so that the jank detection is robust to those
  // fluctuations.
  if (!last_presentation_timestamp_.is_null()) {
    base::TimeDelta staleness = current_frame_delta - frame_interval;
    if (staleness < zero_delta)
      staleness = zero_delta;

    if (tracker_type_ != FrameSequenceTrackerType::kCustom) {
      STATIC_HISTOGRAM_POINTER_GROUP(
          GetStaleHistogramName(tracker_type_),
          GetIndexForStaleMetric(tracker_type_), kMaximumStaleHistogramIndex,
          AddTimeMillisecondsGranularity(staleness),
          base::Histogram::FactoryTimeGet(
              GetStaleHistogramName(tracker_type_), kStaleHistogramMin,
              kStaleHistogramMax, kStaleHistogramBucketCount,
              base::HistogramBase::kUmaTargetedHistogramFlag));
      if (staleness > max_staleness_)
        max_staleness_ = staleness;
    }

    if (!prev_frame_delta_.is_zero() &&
        current_frame_delta > prev_frame_delta_ + 0.5 * frame_interval) {
      jank_count_++;
    }
  }
  last_presentation_timestamp_ = current_presentation_timestamp;
  last_presentation_frame_id_ = presented_frame_id;
  prev_frame_delta_ = current_frame_delta;
}

void JankMetrics::ReportJankMetrics(int frames_expected) {
  if (tracker_type_ == FrameSequenceTrackerType::kCustom)
    return;

  // Report the max staleness metrics
  STATIC_HISTOGRAM_POINTER_GROUP(
      GetMaxStaleHistogramName(tracker_type_),
      GetIndexForStaleMetric(tracker_type_), kMaximumStaleHistogramIndex,
      AddTimeMillisecondsGranularity(max_staleness_),
      base::Histogram::FactoryTimeGet(
          GetMaxStaleHistogramName(tracker_type_), kStaleHistogramMin,
          kStaleHistogramMax, kStaleHistogramBucketCount,
          base::HistogramBase::kUmaTargetedHistogramFlag));

  // Reset counts to avoid duplicated reporting.
  Reset();
}

void JankMetrics::Reset() {
  jank_count_ = 0;
  max_staleness_ = {};
}

void JankMetrics::Merge(std::unique_ptr<JankMetrics> jank_metrics) {
  if (jank_metrics) {
    jank_count_ += jank_metrics->jank_count_;
    max_staleness_ = std::max(max_staleness_, jank_metrics->max_staleness_);
  }
}

}  // namespace cc
