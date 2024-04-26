// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/average_lag_tracker.h"

#include <algorithm>

#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"

namespace cc {

AverageLagTracker::AverageLagTracker() = default;
AverageLagTracker::~AverageLagTracker() = default;

void AverageLagTracker::AddScrollEventInFrame(const EventInfo& event_info) {
  if (event_info.event_type == EventType::kScrollbegin) {
    AddScrollBeginInFrame(event_info);
  } else if (!last_event_timestamp_.is_null()) {
    AddScrollUpdateInFrame(event_info);
  }

  last_event_timestamp_ = event_info.event_timestamp;
  last_event_accumulated_delta_ += event_info.event_scroll_delta;
  last_rendered_accumulated_delta_ += event_info.predicted_scroll_delta;
}

std::string AverageLagTracker::GetAverageLagMetricName(EventType event) const {
  std::string metric_name = "AverageLagPresentation";

  std::string event_name =
      event == EventType::kScrollbegin ? "ScrollBegin" : "ScrollUpdate";

  return base::JoinString(
      {"Event", "Latency", event_name, "Touch", metric_name}, ".");
}

void AverageLagTracker::AddScrollBeginInFrame(const EventInfo& event_info) {
  DCHECK_EQ(event_info.event_type, EventType::kScrollbegin);

  // Flush all unfinished frames.
  while (!frame_lag_infos_.empty()) {
    frame_lag_infos_.front().lag_area += LagForUnfinishedFrame(
        frame_lag_infos_.front().rendered_accumulated_delta);
    frame_lag_infos_.front().lag_area_no_prediction += LagForUnfinishedFrame(
        frame_lag_infos_.front().rendered_accumulated_delta_no_prediction);

    // Record UMA when it's the last item in queue.
    CalculateAndReportAverageLagUma(frame_lag_infos_.size() == 1);
  }
  // |accumulated_lag_| should be cleared/reset.
  DCHECK_EQ(accumulated_lag_, 0);

  // Create kScrollbegin report, with report time equals to the frame
  // timestamp.
  LagAreaInFrame first_frame(event_info.finish_timestamp);
  frame_lag_infos_.push_back(first_frame);

  // Reset fields.
  last_reported_time_ = event_info.event_timestamp;
  last_finished_frame_time_ = event_info.event_timestamp;
  last_event_accumulated_delta_ = 0;
  last_rendered_accumulated_delta_ = 0;
  is_begin_ = true;
}

void AverageLagTracker::AddScrollUpdateInFrame(const EventInfo& event_info) {
  DCHECK_EQ(event_info.event_type, EventType::kScrollupdate);

  // Only accept events in nondecreasing order.
  if ((event_info.event_timestamp - last_event_timestamp_).InMilliseconds() < 0)
    return;

  // Pop all frames where frame_time <= event_timestamp.
  while (!frame_lag_infos_.empty() &&
         frame_lag_infos_.front().frame_time <= event_info.event_timestamp) {
    base::TimeTicks front_time =
        std::max(last_event_timestamp_, last_finished_frame_time_);
    base::TimeTicks back_time = frame_lag_infos_.front().frame_time;
    frame_lag_infos_.front().lag_area +=
        LagBetween(front_time, back_time, event_info.event_scroll_delta,
                   event_info.event_timestamp,
                   frame_lag_infos_.front().rendered_accumulated_delta);
    frame_lag_infos_.front().lag_area_no_prediction += LagBetween(
        front_time, back_time, event_info.event_scroll_delta,
        event_info.event_timestamp,
        frame_lag_infos_.front().rendered_accumulated_delta_no_prediction);

    CalculateAndReportAverageLagUma();
  }

  // Initialize a new LagAreaInFrame when current_frame_time > frame_time.
  if (frame_lag_infos_.empty() ||
      event_info.finish_timestamp > frame_lag_infos_.back().frame_time) {
    LagAreaInFrame new_frame(event_info.finish_timestamp,
                             last_rendered_accumulated_delta_,
                             last_event_accumulated_delta_);
    frame_lag_infos_.push_back(new_frame);
  }

  // last_frame_time <= event_timestamp < frame_time
  if (!frame_lag_infos_.empty()) {
    // The front element in queue (if any) must satisfy frame_time >
    // event_timestamp, otherwise it would be popped in the while loop.
    DCHECK_LE(last_finished_frame_time_, event_info.event_timestamp);
    DCHECK_LE(event_info.event_timestamp, frame_lag_infos_.front().frame_time);
    base::TimeTicks front_time =
        std::max(last_finished_frame_time_, last_event_timestamp_);
    base::TimeTicks back_time = event_info.event_timestamp;

    frame_lag_infos_.front().lag_area +=
        LagBetween(front_time, back_time, event_info.event_scroll_delta,
                   event_info.event_timestamp,
                   frame_lag_infos_.front().rendered_accumulated_delta);

    frame_lag_infos_.front().lag_area_no_prediction += LagBetween(
        front_time, back_time, event_info.event_scroll_delta,
        event_info.event_timestamp,
        frame_lag_infos_.front().rendered_accumulated_delta_no_prediction);
  }
}

float AverageLagTracker::LagBetween(base::TimeTicks front_time,
                                    base::TimeTicks back_time,
                                    const float scroll_delta,
                                    base::TimeTicks event_timestamp,
                                    float rendered_accumulated_delta) {
  // In some tests, we use const event time. return 0 to avoid divided by 0.
  if (event_timestamp == last_event_timestamp_)
    return 0;

  float front_delta =
      (last_event_accumulated_delta_ +
       (scroll_delta * ((front_time - last_event_timestamp_) /
                        (event_timestamp - last_event_timestamp_)))) -
      rendered_accumulated_delta;

  float back_delta =
      (last_event_accumulated_delta_ +
       scroll_delta * ((back_time - last_event_timestamp_) /
                       (event_timestamp - last_event_timestamp_))) -
      rendered_accumulated_delta;

  // Calculate the trapezoid area.
  if (front_delta * back_delta >= 0) {
    return 0.5f * std::abs(front_delta + back_delta) *
           (back_time - front_time).InMillisecondsF();
  }

  // Corner case that rendered_accumulated_delta is in between of front_pos
  // and back_pos.
  return 0.5f *
         std::abs((front_delta * front_delta + back_delta * back_delta) /
                  (back_delta - front_delta)) *
         (back_time - front_time).InMillisecondsF();
}

float AverageLagTracker::LagForUnfinishedFrame(
    float rendered_accumulated_delta) {
  base::TimeTicks last_time =
      std::max(last_event_timestamp_, last_finished_frame_time_);
  return std::abs(last_event_accumulated_delta_ - rendered_accumulated_delta) *
         (frame_lag_infos_.front().frame_time - last_time).InMillisecondsF();
}

void AverageLagTracker::CalculateAndReportAverageLagUma(bool send_anyway) {
  // TODO(crbug.com/40236436): re-enable DCHECK and remove early-out
  // once bugs are fixed.
  // DCHECK(!frame_lag_infos_.empty());
  if (frame_lag_infos_.empty()) {
    return;
  }
  const LagAreaInFrame& frame_lag = frame_lag_infos_.front();

  // TODO(crbug.com/40236436): re-enable DCHECKs once bugs are fixed.
  // DCHECK_GE(frame_lag.lag_area, 0.f);
  // DCHECK_GE(frame_lag.lag_area_no_prediction, 0.f);
  accumulated_lag_ += frame_lag.lag_area;
  accumulated_lag_no_prediction_ += frame_lag.lag_area_no_prediction;

  if (is_begin_) {
    // TODO(crbug.com/40236436): re-enable DCHECK once bugs are fixed.
    // DCHECK_EQ(accumulated_lag_, accumulated_lag_no_prediction_);
  }

  // |send_anyway| is true when we are flush all remaining frames on next
  // |kScrollbegin|. Otherwise record UMA when it's kScrollbegin, or when
  // reaching the 1 second gap.
  if (send_anyway || is_begin_ ||
      (frame_lag.frame_time - last_reported_time_) >= base::Seconds(1)) {
    const EventType event_type =
        is_begin_ ? EventType::kScrollbegin : EventType::kScrollupdate;

    const float time_delta =
        (frame_lag.frame_time - last_reported_time_).InMillisecondsF();
    const float scaled_lag_with_prediction = accumulated_lag_ / time_delta;
    const float scaled_lag_no_prediction =
        accumulated_lag_no_prediction_ / time_delta;

    base::UmaHistogramCounts1000(GetAverageLagMetricName(event_type),
                                 scaled_lag_with_prediction);
    base::UmaHistogramCounts1000(
        base::JoinString({GetAverageLagMetricName(event_type), "NoPrediction"},
                         "."),
        scaled_lag_no_prediction);

    const float lag_improvement =
        scaled_lag_no_prediction - scaled_lag_with_prediction;

    // Log positive and negative prediction effects. kScrollbegin currently
    // doesn't take prediction into account so don't log for it.
    // Positive effect means that the prediction reduced the perceived lag,
    // where negative means prediction made lag worse (most likely due to
    // misprediction).
    if (event_type == EventType::kScrollupdate) {
      if (lag_improvement >= 0.f) {
        base::UmaHistogramCounts1000(
            base::JoinString(
                {GetAverageLagMetricName(event_type), "PredictionPositive"},
                "."),
            lag_improvement);
      } else {
        base::UmaHistogramCounts1000(
            base::JoinString(
                {GetAverageLagMetricName(event_type), "PredictionNegative"},
                "."),
            -lag_improvement);
      }

      if (scaled_lag_no_prediction > 0) {
        // How much of the original lag wasn't removed by prediction.
        float remaining_lag_ratio =
            scaled_lag_with_prediction / scaled_lag_no_prediction;

        // Using custom bucket count for high precision on values in (0, 100).
        // With 100 buckets, (0, 100) is mapped into 60 buckets.
        base::UmaHistogramCustomCounts(
            base::JoinString(
                {GetAverageLagMetricName(event_type), "RemainingLagPercentage"},
                "."),
            100 * remaining_lag_ratio, 1, 500, 100);
      }
    }
    accumulated_lag_ = 0;
    accumulated_lag_no_prediction_ = 0;
    last_reported_time_ = frame_lag.frame_time;
    is_begin_ = false;
  }

  last_finished_frame_time_ = frame_lag.frame_time;
  frame_lag_infos_.pop_front();
}

}  // namespace cc
