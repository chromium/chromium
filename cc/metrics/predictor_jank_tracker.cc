// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/predictor_jank_tracker.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/typed_macros.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"

namespace cc {
namespace {

// We define an irregular sequence of screen displacement as an abrupt
// change in acceleration in a sequence of 3 frames, meaning that in
// a sequence of 3 frames acceleration should be ether positive or
// negative, and the sequence should either be increasing or decreasing
// but not both.
// for Example [1, 5, 10] and [10, 5, 1] are good screen displacement
// sequences but [1, 10, 5] is bad, because the acceleration between
// between the the first and second frame is 9, while between the second
// and third is -5, indicating an acceleration direction change.
// We conducted an experiment to find the ratio of the bigger to smaller
// displacement at which the human eye notices scrolling performance
// degradation, and the results were |d_large}/{d_small| > 1.4
// for less than 7 pixels of max displacement, and > 1.2 for more than 7.
// for more details please check the following document:
// http://doc/1Y0u0Tq5eUZff75nYUzQVw6JxmbZAW9m64pJidmnGWsY
constexpr float kScrollDeltaThreshold = 7.0;
constexpr float kSlowJankyThreshold = 1.4;
constexpr float kFastJankyThreshold = 1.2;

float GetMaxDelta(float d1, float d2, float d3) {
  return std::max(std::abs(d1), std::max(std::abs(d2), std::abs(d3)));
}

std::pair<float, bool> GetJankyThresholdAndScrollSpeed(float d1,
                                                       float d2,
                                                       float d3) {
  // Maximum displacement in a sequence of 3 frames is used to decide
  // the janky threshold at which the user will start noticing jank.
  float max_delta = GetMaxDelta(d1, d2, d3);
  float janky_threshold = kSlowJankyThreshold;
  bool slow_scroll = true;
  if (max_delta > kScrollDeltaThreshold) {
    janky_threshold = kFastJankyThreshold;
    slow_scroll = false;
  }
  return std::make_pair(janky_threshold, slow_scroll);
}

// To compare predictor performance for 3 consecutive frames, the
// frames have to been displaced in the same direction, otherwise
// comparasion can not occur.
bool VerifyFramesSameDirection(float& d1, float& d2, float& d3) {
  return (d1 > 0 && d2 > 0 && d3 > 0) || (d1 < 0 && d2 < 0 && d3 < 0);
}

}  // namespace

PredictorJankTracker::PredictorJankTracker() = default;
PredictorJankTracker::~PredictorJankTracker() = default;

float PredictorJankTracker::GetSlowScrollDeltaThreshold() {
  return kScrollDeltaThreshold;
}

float PredictorJankTracker::GetSlowScrollJankyThreshold() {
  return kSlowJankyThreshold;
}

float PredictorJankTracker::GetFastScrollJankyThreshold() {
  return kFastJankyThreshold;
}

void PredictorJankTracker::ReportLatestScrollDelta(
    float next_delta,
    base::TimeTicks next_presentation_ts,
    base::TimeDelta vsync_interval,
    std::optional<EventMetrics::TraceId> trace_id) {
  total_frames_++;
  float d1 = frame_data_.prev_delta_;
  float d2 = frame_data_.cur_delta_;
  float d3 = next_delta;

  // Verify no scrolling direction change as we can't compare
  // frames if the user changed their scrolling direction.
  if (!VerifyFramesSameDirection(d1, d2, d3)) {
    StoreLatestFrameData(next_delta, next_presentation_ts, trace_id);
    return;
  }

  // Compare absolute values of screen displacement to ensure
  // max/min functions returning the maximum displacement in pixels
  // meaning a displacement of -10 pixels is more than a displacement
  // of -5 pixels.
  d1 = std::abs(d1);
  d2 = std::abs(d2);
  d3 = std::abs(d3);

  // Maximum displacement in a sequence of 3 frames is used to decide
  // the janky threshold at which the user will start noticing jank.
  const auto [janky_threshold, slow_scroll] =
      GetJankyThresholdAndScrollSpeed(d1, d2, d3);

  // Get the ratio of the middle frame to it's neighbours, |upper_jank|
  // is for when a frame displacement is more than it's neighbouring
  // frames and |janky_lower| is for when a frame displacement is less than
  // both it's neighbouring frames.
  float frame_janky_upper = d2 / std::max(d1, d3);
  float frame_janky_lower = std::min(d1, d3) / d2;
  bool contains_missed_vsyncs =
      ContainsMissedVSync(next_presentation_ts, vsync_interval);

  bool report_ukm = false;
  if (frame_janky_lower >= janky_threshold) {
    ReportJankyFrame(next_delta, frame_janky_lower - janky_threshold,
                     contains_missed_vsyncs, slow_scroll, trace_id);
    report_ukm = true;
  }
  if (frame_janky_upper >= janky_threshold) {
    ReportJankyFrame(next_delta, frame_janky_upper - janky_threshold,
                     contains_missed_vsyncs, slow_scroll, trace_id);
    report_ukm = true;
  }

  if (scroll_jank_ukm_reporter_ && report_ukm) {
    // The max delta can be used to determine if this is a fast or slow scroll.
    // If this value is > kScrollDeltaThreshold, then the scroll is fast. This
    // value can also let us know the jank threshold (kSlowJankyThreshold or
    // kFastJankyThreshold).
    scroll_jank_ukm_reporter_->set_max_delta(GetMaxDelta(d1, d2, d3));
  }

  if (total_frames_ >= 64) {
    ReportJankyFramePercentage();
  }

  StoreLatestFrameData(next_delta, next_presentation_ts, trace_id);
}

void PredictorJankTracker::ReportJankyFrame(
    float next_delta,
    float janky_value,
    bool contains_missed_vsyncs,
    bool slow_scroll,
    std::optional<EventMetrics::TraceId> trace_id) {
  janky_frames_++;
  if (scroll_jank_ukm_reporter_) {
    scroll_jank_ukm_reporter_->IncrementPredictorJankyFrames();
  }
  TRACE_EVENT_INSTANT(
      "input.scrolling", "PredictorJankTracker::ReportJankyFrame",
      [&](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* scroll_data = event->set_scroll_predictor_metrics();
        {
          // prev data.
          auto* values = scroll_data->set_prev_event_frame_value();
          if (frame_data_.prev_trace_id_) {
            values->set_event_trace_id(frame_data_.prev_trace_id_->value());
          }
          values->set_delta_value_pixels(frame_data_.prev_delta_);
        }
        {
          // cur data.
          auto* values = scroll_data->set_cur_event_frame_value();
          if (frame_data_.cur_trace_id_) {
            values->set_event_trace_id(frame_data_.cur_trace_id_->value());
          }
          values->set_delta_value_pixels(frame_data_.cur_delta_);
        }
        {
          // next data.
          auto* values = scroll_data->set_next_event_frame_value();
          if (trace_id) {
            values->set_event_trace_id(trace_id->value());
          }
          values->set_delta_value_pixels(next_delta);
        }
        scroll_data->set_janky_value_pixels(janky_value);
        scroll_data->set_has_missed_vsyncs(contains_missed_vsyncs);
        scroll_data->set_is_slow_scroll(slow_scroll);
      });

  const int janky_value_percentage = static_cast<int>(janky_value * 100);
  if (contains_missed_vsyncs && slow_scroll) {
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Event.Jank.ScrollUpdate.SlowScroll.MissedVsync."
        "FrameAboveJankyThreshold2",
        janky_value_percentage, 1, 1500, 50);
  } else if (contains_missed_vsyncs && !slow_scroll) {
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Event.Jank.ScrollUpdate.FastScroll.MissedVsync."
        "FrameAboveJankyThreshold2",
        janky_value_percentage, 1, 1500, 50);
  } else if (slow_scroll) {
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Event.Jank.ScrollUpdate.SlowScroll.NoMissedVsync."
        "FrameAboveJankyThreshold2",
        janky_value_percentage, 1, 1500, 50);
  } else {
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Event.Jank.ScrollUpdate.FastScroll.NoMissedVsync."
        "FrameAboveJankyThreshold2",
        janky_value_percentage, 1, 2000, 50);
  }

  if (scroll_jank_ukm_reporter_) {
    if (contains_missed_vsyncs) {
      scroll_jank_ukm_reporter_->set_frame_with_missed_vsync(
          janky_value_percentage);
    } else {
      scroll_jank_ukm_reporter_->set_frame_with_no_missed_vsync(
          janky_value_percentage);
    }
  }
}

bool PredictorJankTracker::ContainsMissedVSync(
    base::TimeTicks& next_presentation_ts,
    base::TimeDelta& vsync_interval) {
  // The presentation delta is usually 16.6ms for 60 Hz devices,
  // but sometimes random errors result in a delta of up to 20ms
  // as observed in traces.
  // This adds a an error margin of 1/2 a vsync before considering
  // the Vsync missed, the need for this error margin is rare
  // and will not introduce bias in the metric.
  base::TimeDelta vsync_error_margin = vsync_interval + vsync_interval / 2;
  return (next_presentation_ts - frame_data_.cur_presentation_ts_ >
          vsync_error_margin) ||
         (frame_data_.cur_presentation_ts_ - frame_data_.prev_presentation_ts_ >
          vsync_error_margin);
}

void PredictorJankTracker::StoreLatestFrameData(
    float delta,
    base::TimeTicks presentation_ts,
    std::optional<EventMetrics::TraceId> trace_id) {
  frame_data_.prev_delta_ = frame_data_.cur_delta_;
  frame_data_.prev_trace_id_ = frame_data_.cur_trace_id_;
  frame_data_.cur_delta_ = delta;
  frame_data_.cur_trace_id_ = trace_id;
  frame_data_.prev_presentation_ts_ = frame_data_.cur_presentation_ts_;
  frame_data_.cur_presentation_ts_ = presentation_ts;
}

void PredictorJankTracker::ResetCurrentScrollReporting() {
  frame_data_.prev_delta_ = 0;
  frame_data_.cur_delta_ = 0;
  if (scroll_jank_ukm_reporter_) {
    scroll_jank_ukm_reporter_->ResetPredictorMetrics();
  }
}

void PredictorJankTracker::ReportJankyFramePercentage() {
  UMA_HISTOGRAM_PERCENTAGE(
      "Event.Jank.PredictorJankyFramePercentage2",
      static_cast<int>(100 * (janky_frames_ / total_frames_)));
  total_frames_ = 0;
  janky_frames_ = 0;
}

}  // namespace cc
