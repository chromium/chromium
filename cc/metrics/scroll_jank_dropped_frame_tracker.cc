// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/scroll_jank_dropped_frame_tracker.h"

#include <algorithm>

#include "base/metrics/histogram_macros.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_id_helper.h"
#include "base/trace_event/typed_macros.h"

namespace cc {

ScrollJankDroppedFrameTracker::ScrollJankDroppedFrameTracker() = default;
ScrollJankDroppedFrameTracker::~ScrollJankDroppedFrameTracker() = default;

void ScrollJankDroppedFrameTracker::EmitHistogramsAndResetCounters() {
  DCHECK_EQ(num_presented_frames_, kHistogramEmitFrequency);

  UMA_HISTOGRAM_PERCENTAGE(kDelayedFramesHistogram,
                           (100 * missed_frames_) / kHistogramEmitFrequency);
  UMA_HISTOGRAM_CUSTOM_COUNTS(kMissedVsyncsHistogram, missed_vsyncs_, 1, 50,
                              25);

  missed_frames_ = 0;
  missed_vsyncs_ = 0;
  // We don't need to reset it to -1 because after the first window we always
  // have a valid previous frame data to compare the first frame of window.
  num_presented_frames_ = 0;
}

void ScrollJankDroppedFrameTracker::ReportLatestPresentationData(
    base::TimeTicks first_input_generation_ts,
    base::TimeTicks last_input_generation_ts,
    base::TimeTicks presentation_ts,
    base::TimeDelta vsync_interval) {
  if ((last_input_generation_ts < first_input_generation_ts) ||
      (presentation_ts <= last_input_generation_ts)) {
    // TODO(crbug/1447358): Investigate when these edge cases can be triggered
    // in field and web tests. We have already seen this triggered in field, and
    // some web tests where an event with null(0) timestamp gets coalesced with
    // a "normal" input.
    return;
  }
  // TODO(b/276722271) : Analyze and reduce these cases of out of order
  // frame termination.
  if (presentation_ts <= prev_presentation_ts_) {
    TRACE_EVENT_INSTANT("input", "OutOfOrderTerminatedFrame");
    return;
  }

  // The presentation delta is usually 16.6ms for 60 Hz devices,
  // but sometimes random errors result in a delta of up to 20ms
  // as observed in traces. This adds an error margin of 1/2 a
  // vsync before considering the Vsync missed.
  bool missed_frame = (presentation_ts - prev_presentation_ts_) >
                      (vsync_interval + vsync_interval / 2);
  bool input_available =
      (first_input_generation_ts - prev_last_input_generation_ts_) <
      (vsync_interval + vsync_interval / 2);
  if (missed_frame && input_available) {
    missed_frames_++;
    missed_vsyncs_ +=
        (presentation_ts - prev_presentation_ts_ - (vsync_interval / 2)) /
        vsync_interval;
    TRACE_EVENT_INSTANT("input", "MissedFrame", "missed_frames_",
                        missed_frames_, "missed_vsyncs_", missed_vsyncs_,
                        "vsync_interval", vsync_interval);
  }

  ++num_presented_frames_;
  if (num_presented_frames_ == kHistogramEmitFrequency) {
    EmitHistogramsAndResetCounters();
  }
  DCHECK_LT(num_presented_frames_, kHistogramEmitFrequency);

  prev_presentation_ts_ = presentation_ts;
  prev_last_input_generation_ts_ = last_input_generation_ts;
}

}  // namespace cc
