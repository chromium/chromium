// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/latency_jank_tracker.h"

#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_id_helper.h"
#include "base/trace_event/typed_macros.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "services/tracing/public/cpp/perfetto/flow_event_utils.h"

namespace cc {

namespace {
// Checking whether one update event length (measured in frames) is janky
// compared to another (either previous or next). Update is deemed janky when
// it's half of a frame longer than a neighbouring update.
//
// A small number is added to 0.5 in order to make sure that the comparison does
// not filter out ratios that are precisely 0.5, which can fall a little above
// or below exact value due to inherent inaccuracy of operations with
// floating-point numbers. Value 1e-9 have been chosen as follows: the ratio has
// less than nanosecond precision in numerator and VSync interval in
// denominator. Assuming refresh rate more than 1 FPS (and therefore VSync
// interval less than a second), this ratio should increase with increments more
// than minimal value in numerator (1ns) divided by maximum value in
// denominator, giving 1e-9.
static bool IsJankyComparison(double frames, double other_frames) {
  return frames > other_frames + 0.5 + 1e-9;
}

}  // namespace

LatencyJankTracker::LatencyJankTracker() = default;
LatencyJankTracker::~LatencyJankTracker() = default;

void LatencyJankTracker::ReportScrollTimings(
    base::TimeTicks original_timestamp,
    base::TimeTicks gpu_swap_end_timestamp,
    bool first_frame) {
  DCHECK(!original_timestamp.is_null());
  DCHECK(!gpu_swap_end_timestamp.is_null());
  base::TimeDelta dur = gpu_swap_end_timestamp - original_timestamp;

  if (first_frame) {
    if (jank_state_.total_update_events_ > 0) {
      // If we have some data from previous scroll, report it to UMA.
      UMA_HISTOGRAM_MEDIUM_TIMES("Event.Latency.ScrollUpdate.TotalDuration2",
                                 jank_state_.total_update_duration_);
      UMA_HISTOGRAM_MEDIUM_TIMES("Event.Latency.ScrollUpdate.JankyDuration2",
                                 jank_state_.janky_update_duration_);

      UMA_HISTOGRAM_COUNTS_10000("Event.Latency.ScrollUpdate.TotalEvents2",
                                 jank_state_.total_update_events_);
      UMA_HISTOGRAM_COUNTS_10000("Event.Latency.ScrollUpdate.JankyEvents2",
                                 jank_state_.janky_update_events_);

      if (!jank_state_.total_update_duration_.is_zero()) {
        UMA_HISTOGRAM_PERCENTAGE(
            "Event.Latency.ScrollUpdate.JankyDurationPercentage2",
            static_cast<int>(100 * (jank_state_.janky_update_duration_ /
                                    jank_state_.total_update_duration_)));
      }
    }

    jank_state_ = JankTrackerState{};
  }

  jank_state_.total_update_events_++;
  jank_state_.total_update_duration_ += dur;

  // When processing first frame in a scroll, we do not have any other frames to
  // compare it to, and thus no way to detect the jank.
  if (!first_frame) {
    // TODO(b/185884172): Investigate using proper vsync interval.

    // Assuming 60fps, each frame is rendered in (1/60) of a second.
    // To see how many of those intervals fit into the real frame timing,
    // we divide it on 1/60 which is the same thing as multiplying by 60.
    double frames_taken = dur.InSecondsF() * 60;
    double prev_frames_taken = jank_state_.prev_duration_.InSecondsF() * 60;

    // For each GestureScroll update, we would like to report whether it was
    // janky. However, in order to do that, we need to compare it both to the
    // previous as well as to the next event. This condition means that no jank
    // was reported for the previous frame (as compared to the one before that),
    // so we need to compare it to the current one and report whether it's
    // janky:
    if (!jank_state_.prev_scroll_update_reported_) {
      // The information about previous GestureScrollUpdate was not reported:
      // check whether it's janky by comparing to the current frame and report.

      if (IsJankyComparison(prev_frames_taken, frames_taken)) {
        UMA_HISTOGRAM_BOOLEAN("Event.Latency.ScrollJank2", true);
        jank_state_.janky_update_events_++;
        jank_state_.janky_update_duration_ += jank_state_.prev_duration_;
      } else {
        UMA_HISTOGRAM_BOOLEAN("Event.Latency.ScrollJank2", false);
      }
    }

    // The current GestureScrollUpdate is janky compared to the previous one.
    if (IsJankyComparison(frames_taken, prev_frames_taken)) {
      UMA_HISTOGRAM_BOOLEAN("Event.Latency.ScrollJank2", true);
      jank_state_.janky_update_events_++;
      jank_state_.janky_update_duration_ += dur;

      // Since we have reported the current event as janky, there is no need to
      // report anything about it on the next iteration, as we would like to
      // report every GestureScrollUpdate only once.
      jank_state_.prev_scroll_update_reported_ = true;
    } else {
      // We do not have enough information to report whether the current event
      // is janky, and need to compare it to the next one before reporting
      // anything about it.
      jank_state_.prev_scroll_update_reported_ = false;
    }
  }

  jank_state_.prev_duration_ = dur;
}

}  // namespace cc
