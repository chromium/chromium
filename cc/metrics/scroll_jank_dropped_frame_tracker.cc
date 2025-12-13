// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/scroll_jank_dropped_frame_tracker.h"

#include <optional>
#include <utility>

#include "base/check_op.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "cc/metrics/event_metrics.h"

namespace cc {

namespace {

// Histogram min, max and no. of buckets.
constexpr int kVsyncCountsMin = 1;
constexpr int kVsyncCountsMax = 50;
constexpr int kVsyncCountsBuckets = 25;

}  // namespace

ScrollJankDroppedFrameTracker::ScrollJankDroppedFrameTracker() {
  // Not initializing with 0 because the first frame in first window will be
  // always deemed non-janky which makes the metric slightly biased. Setting
  // it to -1 essentially ignores first frame.
  fixed_window_.num_presented_frames = -1;
}

ScrollJankDroppedFrameTracker::~ScrollJankDroppedFrameTracker() {
  if (per_scroll_.has_value()) {
    // Per scroll metrics for a given scroll are emitted at the start of next
    // scroll. Emittimg from here makes sure we don't loose the data for last
    // scroll.
    EmitPerScrollHistogramsAndResetCounters();
  }
}

void ScrollJankDroppedFrameTracker::EmitPerScrollHistogramsAndResetCounters() {
  if (!per_scroll_.has_value()) {
    return;
  }

  // There should be at least one presented frame given the method is only
  // called after we have a successful presentation.
  if (per_scroll_->num_presented_frames == 0) {
    // TODO(crbug.com/40067426): Debug cases where we can have 0 presented
    // frames.
    TRACE_EVENT_INSTANT("input", "NoPresentedFramesInScroll");
    per_scroll_ = std::nullopt;
    return;
  }
  int delayed_frames_percentage =
      (100 * per_scroll_->missed_frames) / per_scroll_->num_presented_frames;
  UMA_HISTOGRAM_PERCENTAGE(kDelayedFramesPerScrollHistogram,
                           delayed_frames_percentage);
  UMA_HISTOGRAM_CUSTOM_COUNTS(kMissedVsyncsMaxPerScrollHistogram,
                              per_scroll_->max_missed_vsyncs, kVsyncCountsMin,
                              kVsyncCountsMax, kVsyncCountsBuckets);
  UMA_HISTOGRAM_CUSTOM_COUNTS(kMissedVsyncsSumPerScrollHistogram,
                              per_scroll_->missed_vsyncs, kVsyncCountsMin,
                              kVsyncCountsMax, kVsyncCountsBuckets);

  per_scroll_ = std::nullopt;
}

void ScrollJankDroppedFrameTracker::EmitPerWindowHistogramsAndResetCounters() {
  DCHECK_EQ(fixed_window_.num_presented_frames, kHistogramEmitFrequency);

  UMA_HISTOGRAM_PERCENTAGE(
      kDelayedFramesWindowHistogram,
      (100 * fixed_window_.missed_frames) / kHistogramEmitFrequency);
  UMA_HISTOGRAM_CUSTOM_COUNTS(kMissedVsyncsSumInWindowHistogram,
                              fixed_window_.missed_vsyncs, kVsyncCountsMin,
                              kVsyncCountsMax, kVsyncCountsBuckets);
  UMA_HISTOGRAM_CUSTOM_COUNTS(kMissedVsyncsMaxInWindowHistogram,
                              fixed_window_.max_missed_vsyncs, kVsyncCountsMin,
                              kVsyncCountsMax, kVsyncCountsBuckets);

  fixed_window_.missed_frames = 0;
  fixed_window_.missed_vsyncs = 0;
  fixed_window_.max_missed_vsyncs = 0;
  // We don't need to reset it to -1 because after the first window we always
  // have a valid previous frame data to compare the first frame of window.
  fixed_window_.num_presented_frames = 0;
}

void ScrollJankDroppedFrameTracker::ReportLatestPresentationData(
    ScrollUpdateEventMetrics& latest_event,
    base::TimeTicks last_input_generation_ts,
    base::TimeTicks presentation_ts,
    base::TimeDelta vsync_interval) {
  base::TimeTicks first_input_generation_ts =
      latest_event.GetDispatchStageTimestamp(
          EventMetrics::DispatchStage::kGenerated);
  if ((last_input_generation_ts < first_input_generation_ts) ||
      (presentation_ts <= last_input_generation_ts)) {
    // TODO(crbug.com/40913586): Investigate when these edge cases can be
    // triggered in field and web tests. We have already seen this triggered in
    // field, and some web tests where an event with null(0) timestamp gets
    // coalesced with a "normal" input.
    return;
  }
  // TODO(b/276722271) : Analyze and reduce these cases of out of order
  // frame termination.
  if (presentation_ts <= prev_presentation_ts_) {
    TRACE_EVENT_INSTANT("input", "OutOfOrderTerminatedFrame");
    return;
  }

  // `per_scroll_` is initialized in OnScrollStarted when we see
  // FIRST_GESTURE_SCROLL_UPDATE event. But in some rare scenarios we don't see
  // the FIRST_GESTURE_SCROLL_UPDATE events on scroll start.
  if (!per_scroll_.has_value()) {
    per_scroll_ = JankData();
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

  // Sometimes the vsync interval is not accurate and is slightly more
  // than the actual signal arrival time, adding (vsync_interval / 2)
  // here insures the result is always ceiled.
  int curr_frame_total_vsyncs =
      (presentation_ts - prev_presentation_ts_ + (vsync_interval / 2)) /
      vsync_interval;
  int curr_frame_missed_vsyncs = curr_frame_total_vsyncs - 1;

  if (missed_frame && input_available) {
    ++fixed_window_.missed_frames;
    ++per_scroll_->missed_frames;
    UMA_HISTOGRAM_CUSTOM_COUNTS(kMissedVsyncsPerFrameHistogram,
                                curr_frame_missed_vsyncs, kVsyncCountsMin,
                                kVsyncCountsMax, kVsyncCountsBuckets);
    fixed_window_.missed_vsyncs += curr_frame_missed_vsyncs;
    per_scroll_->missed_vsyncs += curr_frame_missed_vsyncs;

    if (scroll_jank_ukm_reporter_) {
      scroll_jank_ukm_reporter_->IncrementDelayedFrameCount();
      scroll_jank_ukm_reporter_->AddMissedVsyncs(curr_frame_missed_vsyncs);
    }

    if (curr_frame_missed_vsyncs > per_scroll_->max_missed_vsyncs) {
      per_scroll_->max_missed_vsyncs = curr_frame_missed_vsyncs;
      if (scroll_jank_ukm_reporter_) {
        scroll_jank_ukm_reporter_->set_max_missed_vsyncs(
            curr_frame_missed_vsyncs);
      }
    }
    if (curr_frame_missed_vsyncs > fixed_window_.max_missed_vsyncs) {
      fixed_window_.max_missed_vsyncs = curr_frame_missed_vsyncs;
    }

    TRACE_EVENT_INSTANT(
        "input,input.scrolling", "MissedFrame", "per_scroll_->missed_frames",
        per_scroll_->missed_frames, "per_scroll_->missed_vsyncs",
        per_scroll_->missed_vsyncs, "vsync_interval", vsync_interval);
    latest_event.set_is_janky_scrolled_frame(true);
  } else {
    latest_event.set_is_janky_scrolled_frame(false);
    UMA_HISTOGRAM_CUSTOM_COUNTS(kMissedVsyncsPerFrameHistogram, 0,
                                kVsyncCountsMin, kVsyncCountsMax,
                                kVsyncCountsBuckets);
  }

  if (scroll_jank_ukm_reporter_) {
    scroll_jank_ukm_reporter_->AddVsyncs(
        input_available ? curr_frame_total_vsyncs : 1);
  }

  ++fixed_window_.num_presented_frames;
  ++per_scroll_->num_presented_frames;
  if (scroll_jank_ukm_reporter_) {
    scroll_jank_ukm_reporter_->IncrementFrameCount();
  }

  if (fixed_window_.num_presented_frames == kHistogramEmitFrequency) {
    EmitPerWindowHistogramsAndResetCounters();
  }
  DCHECK_LT(fixed_window_.num_presented_frames, kHistogramEmitFrequency);

  prev_presentation_ts_ = presentation_ts;
  prev_last_input_generation_ts_ = last_input_generation_ts;
}

void ScrollJankDroppedFrameTracker::OnScrollStarted() {
  EmitPerScrollHistogramsAndResetCounters();
  per_scroll_ = JankData();
}

}  // namespace cc
