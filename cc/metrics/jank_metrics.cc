// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/jank_metrics.h"

#include <memory>
#include <string>
#include <utility>

#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/trace_event/trace_event.h"
#include "cc/metrics/frame_sequence_tracker.h"

namespace cc {

namespace {

constexpr int kBuiltinSequenceNum =
    static_cast<int>(FrameSequenceTrackerType::kMaxType) + 1;
constexpr int kMaximumJankHistogramIndex = 2 * kBuiltinSequenceNum;

constexpr bool IsValidJankThreadType(FrameSequenceMetrics::ThreadType type) {
  return type == FrameSequenceMetrics::ThreadType::kCompositor ||
         type == FrameSequenceMetrics::ThreadType::kMain;
}

const char* GetJankThreadTypeName(FrameSequenceMetrics::ThreadType type) {
  DCHECK(IsValidJankThreadType(type));

  switch (type) {
    case FrameSequenceMetrics::ThreadType::kCompositor:
      return "Compositor";
    case FrameSequenceMetrics::ThreadType::kMain:
      return "Main";
    default:
      NOTREACHED();
      return "";
  }
}

int GetIndexForJankMetric(FrameSequenceMetrics::ThreadType thread_type,
                          FrameSequenceTrackerType type) {
  DCHECK(IsValidJankThreadType(thread_type));
  if (thread_type == FrameSequenceMetrics::ThreadType::kMain)
    return static_cast<int>(type);

  DCHECK_EQ(thread_type, FrameSequenceMetrics::ThreadType::kCompositor);
  return static_cast<int>(type) + kBuiltinSequenceNum;
}

std::string GetJankHistogramName(FrameSequenceTrackerType type,
                                 const char* thread_name) {
  return base::StrCat(
      {"Graphics.Smoothness.Jank.", thread_name, ".",
       FrameSequenceTracker::GetFrameSequenceTrackerTypeName(type)});
}

}  // namespace

JankMetrics::JankMetrics(FrameSequenceTrackerType tracker_type,
                         FrameSequenceMetrics::ThreadType effective_thread)
    : tracker_type_(tracker_type), effective_thread_(effective_thread) {
  DCHECK(IsValidJankThreadType(effective_thread));
}
JankMetrics::~JankMetrics() = default;

void JankMetrics::AddPresentedFrame(
    base::TimeTicks current_presentation_timestamp,
    base::TimeDelta frame_interval) {
  base::TimeDelta current_frame_delta =
      current_presentation_timestamp - last_presentation_timestamp_;

  // Only start tracking jank if this function has been called (so that
  // |last_presentation_timestamp_| and |prev_frame_delta_| have been set).
  //
  // The presentation interval is typically a multiple of VSync intervals (i.e.
  // 16.67ms, 33.33ms, 50ms ... on a 60Hz display) with small fluctuations. The
  // 0.5 * |frame_interval| criterion is chosen so that the jank detection is
  // robust to those fluctuations.
  if (!last_presentation_timestamp_.is_null() && !prev_frame_delta_.is_zero() &&
      current_frame_delta > prev_frame_delta_ + 0.5 * frame_interval) {
    jank_count_++;

    TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP1(
        "cc,benchmark", "Jank", TRACE_ID_LOCAL(this),
        last_presentation_timestamp_, "thread-type",
        GetJankThreadTypeName(effective_thread_));
    TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP1(
        "cc,benchmark", "Jank", TRACE_ID_LOCAL(this),
        current_presentation_timestamp, "tracker-type",
        FrameSequenceTracker::GetFrameSequenceTrackerTypeName(tracker_type_));
  }
  last_presentation_timestamp_ = current_presentation_timestamp;

  prev_frame_delta_ = current_frame_delta;
}

void JankMetrics::ReportJankMetrics(int frames_expected) {
  if (tracker_type_ == FrameSequenceTrackerType::kCustom)
    return;

  int jank_percent = static_cast<int>(100 * jank_count_ / frames_expected);

  const char* jank_thread_name = GetJankThreadTypeName(effective_thread_);

  STATIC_HISTOGRAM_POINTER_GROUP(
      GetJankHistogramName(tracker_type_, jank_thread_name),
      GetIndexForJankMetric(effective_thread_, tracker_type_),
      kMaximumJankHistogramIndex, Add(jank_percent),
      base::LinearHistogram::FactoryGet(
          GetJankHistogramName(tracker_type_, jank_thread_name), 1, 100, 101,
          base::HistogramBase::kUmaTargetedHistogramFlag));
}

void JankMetrics::Merge(std::unique_ptr<JankMetrics> jank_metrics) {
  if (jank_metrics)
    jank_count_ += jank_metrics->jank_count_;
}

}  // namespace cc
