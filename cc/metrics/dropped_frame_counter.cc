// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/dropped_frame_counter.h"

#include <algorithm>
#include <cmath>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "cc/metrics/frame_sorter.h"
#include "cc/metrics/total_frame_counter.h"
#include "cc/metrics/ukm_smoothness_data.h"

namespace cc {

using SlidingWindowHistogram = DroppedFrameCounter::SlidingWindowHistogram;

void SlidingWindowHistogram::AddPercentDroppedFrame(
    double percent_dropped_frame,
    size_t count) {
  DCHECK_GE(percent_dropped_frame, 0.0);
  DCHECK_GE(100.0, percent_dropped_frame);
  histogram_bins_[static_cast<int>(std::round(percent_dropped_frame))] += count;
  total_count_ += count;
}

uint32_t SlidingWindowHistogram::GetPercentDroppedFramePercentile(
    double percentile) const {
  if (total_count_ == 0)
    return 0;
  DCHECK_GE(percentile, 0.0);
  DCHECK_GE(1.0, percentile);
  int current_index = 100;  // Last bin in historgam
  uint32_t skipped_counter = histogram_bins_[current_index];  // Last bin values
  double samples_to_skip = ((1 - percentile) * total_count_);
  // We expect this method to calculate higher end percentiles such 95 and as a
  // result we count from the last bin to find the correct bin.
  while (skipped_counter < samples_to_skip && current_index > 0) {
    current_index--;
    skipped_counter += histogram_bins_[current_index];
  }
  return current_index;
}

void SlidingWindowHistogram::Clear() {
  std::fill(std::begin(histogram_bins_), std::end(histogram_bins_), 0);
  total_count_ = 0;
}

std::ostream& SlidingWindowHistogram::Dump(std::ostream& stream) const {
  for (size_t i = 0; i < base::size(histogram_bins_); ++i) {
    stream << i << ": " << histogram_bins_[i] << std::endl;
  }
  return stream << "Total: " << total_count_;
}

std::ostream& operator<<(
    std::ostream& stream,
    const DroppedFrameCounter::SlidingWindowHistogram& histogram) {
  return histogram.Dump(stream);
}

DroppedFrameCounter::DroppedFrameCounter()
    : frame_sorter_(base::BindRepeating(&DroppedFrameCounter::NotifyFrameResult,
                                        base::Unretained(this))) {}
DroppedFrameCounter::~DroppedFrameCounter() = default;

uint32_t DroppedFrameCounter::GetAverageThroughput() const {
  size_t good_frames = 0;
  for (auto it = --end(); it; --it) {
    if (**it == kFrameStateComplete)
      ++good_frames;
  }
  double throughput = 100. * good_frames / ring_buffer_.BufferSize();
  return static_cast<uint32_t>(throughput);
}

void DroppedFrameCounter::AddGoodFrame() {
  ring_buffer_.SaveToBuffer(kFrameStateComplete);
  ++total_frames_;
}

void DroppedFrameCounter::AddPartialFrame() {
  ring_buffer_.SaveToBuffer(kFrameStatePartial);
  ++total_frames_;
  ++total_partial_;
}

void DroppedFrameCounter::AddDroppedFrame() {
  ring_buffer_.SaveToBuffer(kFrameStateDropped);
  ++total_frames_;
  ++total_dropped_;
}

void DroppedFrameCounter::ResetPendingFrames(base::TimeTicks timestamp) {
  // Before resetting the pending frames, update the measurements for the
  // sliding windows.
  if (!latest_sliding_window_start_.is_null()) {
    const auto report_until = timestamp - kSlidingWindowInterval;
    // Report the sliding window metrics for frames that have already been
    // completed (and some of which may have been dropped).
    while (!sliding_window_.empty()) {
      const auto& args = sliding_window_.front().first;
      latest_sliding_window_start_ = args.frame_time;
      latest_sliding_window_interval_ = args.interval;
      bool was_dropped = sliding_window_.front().second;
      if (was_dropped) {
        DCHECK_GT(dropped_frame_count_in_window_, 0u);
        --dropped_frame_count_in_window_;
      }
      sliding_window_.pop();
      if (latest_sliding_window_start_ > report_until)
        break;
      double percent_dropped_frame = std::min(
          (dropped_frame_count_in_window_ * 100.0) / total_frames_in_window_,
          100.0);
      sliding_window_histogram_.AddPercentDroppedFrame(percent_dropped_frame,
                                                       /*count=*/1);
    }
    if (sliding_window_.empty()) {
      DCHECK_EQ(dropped_frame_count_in_window_, 0u);
    }

    // Report no dropped frames for the sliding windows spanning the rest of the
    // time.
    if (latest_sliding_window_start_ < report_until) {
      const auto difference = report_until - latest_sliding_window_start_;
      const size_t count =
          std::ceil(difference / latest_sliding_window_interval_);
      if (count > 0)
        sliding_window_histogram_.AddPercentDroppedFrame(0., count);
    }
  }

  dropped_frame_count_in_window_ = 0;
  sliding_window_ = {};
  latest_sliding_window_start_ = {};
  latest_sliding_window_interval_ = {};
  frame_sorter_.Reset();
}

void DroppedFrameCounter::OnBeginFrame(const viz::BeginFrameArgs& args,
                                       bool is_scroll_active) {
  // Remember when scrolling starts/ends. Do this even if fcp has not happened
  // yet.
  if (!is_scroll_active) {
    scroll_start_.reset();
  } else if (!scroll_start_.has_value()) {
    ScrollStartInfo info = {args.frame_time, args.frame_id};
    scroll_start_ = info;
  }

  if (fcp_received_) {
    frame_sorter_.AddNewFrame(args);
    if (is_scroll_active) {
      DCHECK(scroll_start_.has_value());
      scroll_start_per_frame_[args.frame_id] = *scroll_start_;
    }
  }
}

void DroppedFrameCounter::OnEndFrame(const viz::BeginFrameArgs& args,
                                     bool is_dropped) {
  if (!args.interval.is_zero())
    total_frames_in_window_ = kSlidingWindowInterval / args.interval;

  if (is_dropped) {
    if (fcp_received_)
      ++total_smoothness_dropped_;
    ReportFrames();
  }
  auto iter = scroll_start_per_frame_.find(args.frame_id);
  if (iter != scroll_start_per_frame_.end()) {
    ScrollStartInfo& scroll_start = iter->second;
    if (args.frame_id.source_id == scroll_start.frame_id.source_id) {
      UMA_HISTOGRAM_CUSTOM_TIMES(
          "Graphics.Smoothness.Diagnostic.DroppedFrameAfterScrollStart.Time",
          (args.frame_time - scroll_start.timestamp),
          base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromSeconds(4),
          50);
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Graphics.Smoothness.Diagnostic.DroppedFrameAfterScrollStart.Frames",
          (args.frame_id.sequence_number -
           scroll_start.frame_id.sequence_number),
          1, 250, 50);
    }
    scroll_start_per_frame_.erase(iter);
  }

  if (fcp_received_)
    frame_sorter_.AddFrameResult(args, is_dropped);
}

void DroppedFrameCounter::ReportFrames() {
  const auto total_frames =
      total_counter_->ComputeTotalVisibleFrames(base::TimeTicks::Now());
  TRACE_EVENT2("cc,benchmark", "SmoothnessDroppedFrame", "total", total_frames,
               "smoothness", total_smoothness_dropped_);
  UMA_HISTOGRAM_PERCENTAGE(
      "Graphics.Smoothness.MaxPercentDroppedFrames_1sWindow",
      sliding_window_max_percent_dropped_);

  uint32_t sliding_window_95pct_percent_dropped =
      SlidingWindow95PercentilePercentDropped();
  UMA_HISTOGRAM_PERCENTAGE(
      "Graphics.Smoothness.95pctPercentDroppedFrames_1sWindow",
      sliding_window_95pct_percent_dropped);

  DCHECK_LE(
      sliding_window_95pct_percent_dropped,
      static_cast<uint32_t>(std::round(sliding_window_max_percent_dropped_)));

  // Emit trace event with most recent smoothness calculation. This matches
  // the smoothness metrics displayed on HeadsUpDisplay.
  TRACE_EVENT2("cc,benchmark", "SmoothnessDroppedFrame::MostRecentCalculation",
               "worst_smoothness", sliding_window_max_percent_dropped_,
               "95_percentile_smoothness",
               sliding_window_95pct_percent_dropped);

  if (ukm_smoothness_data_ && total_frames > 0) {
    UkmSmoothnessData smoothness_data;
    smoothness_data.avg_smoothness =
        static_cast<double>(total_smoothness_dropped_) * 100 / total_frames;
    smoothness_data.worst_smoothness = sliding_window_max_percent_dropped_;
    smoothness_data.percentile_95 = sliding_window_95pct_percent_dropped;
    smoothness_data.time_max_delta = time_max_delta_;
    ukm_smoothness_data_->Write(smoothness_data);
  }
}

double DroppedFrameCounter::GetMostRecentAverageSmoothness() const {
  if (ukm_smoothness_data_)
    return ukm_smoothness_data_->data.avg_smoothness;

  return -1.f;
}

double DroppedFrameCounter::GetMostRecent95PercentileSmoothness() const {
  if (ukm_smoothness_data_)
    return ukm_smoothness_data_->data.percentile_95;

  return -1.f;
}

void DroppedFrameCounter::SetUkmSmoothnessDestination(
    UkmSmoothnessDataShared* smoothness_data) {
  ukm_smoothness_data_ = smoothness_data;
}

void DroppedFrameCounter::Reset() {
  total_frames_ = 0;
  total_partial_ = 0;
  total_dropped_ = 0;
  total_smoothness_dropped_ = 0;
  sliding_window_max_percent_dropped_ = 0;
  dropped_frame_count_in_window_ = 0;
  fcp_received_ = false;
  sliding_window_ = {};
  latest_sliding_window_start_ = {};
  sliding_window_histogram_.Clear();
  ring_buffer_.Clear();
  frame_sorter_.Reset();
  time_max_delta_ = {};
}

base::TimeDelta DroppedFrameCounter::ComputeCurrentWindowSize() const {
  if (sliding_window_.empty())
    return {};
  return sliding_window_.back().first.frame_time +
         sliding_window_.back().first.interval -
         sliding_window_.front().first.frame_time;
}

void DroppedFrameCounter::NotifyFrameResult(const viz::BeginFrameArgs& args,
                                            bool is_dropped) {
  // Entirely disregard the frames with interval larger than the window --
  // these are violating the assumptions in the below code and should
  // only occur with external frame control, where dropped frame stats
  // are not relevant.
  if (args.interval >= kSlidingWindowInterval)
    return;

  sliding_window_.push({args, is_dropped});

  if (ComputeCurrentWindowSize() < kSlidingWindowInterval) {
    if (is_dropped)
      ++dropped_frame_count_in_window_;
    return;
  }

  DCHECK_GE(dropped_frame_count_in_window_, 0u);
  DCHECK_GE(sliding_window_.size(), dropped_frame_count_in_window_);

  const auto max_sliding_window_start =
      args.frame_time - kSlidingWindowInterval;
  const auto max_difference = args.interval * 1.5;
  while (ComputeCurrentWindowSize() > kSlidingWindowInterval) {
    const auto removed_args = sliding_window_.front().first;
    const auto removed_was_dropped = sliding_window_.front().second;
    if (removed_was_dropped) {
      DCHECK_GT(dropped_frame_count_in_window_, 0u);
      --dropped_frame_count_in_window_;
    }
    sliding_window_.pop();
    DCHECK(!sliding_window_.empty());

    auto dropped = dropped_frame_count_in_window_;
    if (ComputeCurrentWindowSize() <= kSlidingWindowInterval && is_dropped)
      ++dropped;

    // If two consecutive 'completed' frames are far apart from each other (in
    // time), then report the 'dropped frame count' for the sliding window(s) in
    // between. Note that the window-size still needs to be at least
    // kSlidingWindowInterval.
    const auto& remaining_oldest_args = sliding_window_.front().first;
    const auto last_timestamp =
        std::min(remaining_oldest_args.frame_time, max_sliding_window_start);
    const auto difference = last_timestamp - removed_args.frame_time;
    const size_t count =
        difference > max_difference ? std::ceil(difference / args.interval) : 1;
    double percent_dropped_frame =
        std::min((dropped * 100.0) / total_frames_in_window_, 100.0);
    sliding_window_histogram_.AddPercentDroppedFrame(percent_dropped_frame,
                                                     count);

    if (percent_dropped_frame > sliding_window_max_percent_dropped_) {
      time_max_delta_ = args.frame_time - time_fcp_received_;
      sliding_window_max_percent_dropped_ = percent_dropped_frame;
    }

    latest_sliding_window_start_ = last_timestamp;
    latest_sliding_window_interval_ = remaining_oldest_args.interval;
  }

  if (is_dropped)
    ++dropped_frame_count_in_window_;
}

void DroppedFrameCounter::OnFcpReceived() {
  fcp_received_ = true;
  time_fcp_received_ = base::TimeTicks::Now();
}

}  // namespace cc
