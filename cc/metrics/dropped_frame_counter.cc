// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/dropped_frame_counter.h"

#include <algorithm>
#include <cmath>

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "cc/metrics/frame_sorter.h"
#include "cc/metrics/total_frame_counter.h"
#include "cc/metrics/ukm_smoothness_data.h"

namespace cc {

using SlidingWindowHistogram = DroppedFrameCounter::SlidingWindowHistogram;

void SlidingWindowHistogram::AddPercentDroppedFrame(
    double percent_dropped_frame) {
  DCHECK_GE(percent_dropped_frame, 0.0);
  DCHECK_GE(100.0, percent_dropped_frame);
  histogram_bins_[static_cast<int>(round(percent_dropped_frame))]++;
  total_count++;
}

uint32_t SlidingWindowHistogram::GetPercentDroppedFramePercentile(
    double percentile) const {
  if (total_count == 0)
    return 0;
  DCHECK_GE(percentile, 0.0);
  DCHECK_GE(1.0, percentile);
  int current_index = 100;  // Last bin in historgam
  uint32_t skipped_counter = histogram_bins_[current_index];  // Last bin values
  double samples_to_skip = (1 - percentile) * total_count;
  // We expect this method to calculate higher end percentiles such 95 and as a
  // result we count from the last bin to find the correct bin.
  while (skipped_counter < samples_to_skip && current_index > 0) {
    current_index--;
    skipped_counter += histogram_bins_[current_index];
  }
  return current_index;
}

void SlidingWindowHistogram::clear() {
  std::fill(std::begin(histogram_bins_), std::end(histogram_bins_), 0);
  total_count = 0;
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

void DroppedFrameCounter::ResetFrameSorter() {
  frame_sorter_.Reset();
}

void DroppedFrameCounter::OnBeginFrame(const viz::BeginFrameArgs& args) {
  if (fcp_received_)
    frame_sorter_.AddNewFrame(args);
}

void DroppedFrameCounter::OnEndFrame(const viz::BeginFrameArgs& args,
                                     bool is_dropped) {
  if (is_dropped) {
    if (fcp_received_)
      ++total_smoothness_dropped_;
    ReportFrames();
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

  DCHECK_LE(sliding_window_95pct_percent_dropped,
            static_cast<uint32_t>(round(sliding_window_max_percent_dropped_)));

  if (ukm_smoothness_data_ && total_frames > 0) {
    UkmSmoothnessData smoothness_data;
    smoothness_data.avg_smoothness =
        static_cast<double>(total_smoothness_dropped_) * 100 / total_frames;
    smoothness_data.worst_smoothness = sliding_window_max_percent_dropped_;
    smoothness_data.percentile_95 = sliding_window_95pct_percent_dropped;

    ukm_smoothness_data_->seq_lock.WriteBegin();
    device::OneWriterSeqLock::AtomicWriterMemcpy(&ukm_smoothness_data_->data,
                                                 &smoothness_data,
                                                 sizeof(UkmSmoothnessData));
    ukm_smoothness_data_->seq_lock.WriteEnd();
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
  sliding_window_histogram_.clear();
  ring_buffer_.Clear();
  frame_sorter_.Reset();
}

base::TimeDelta DroppedFrameCounter::ComputeCurrentWindowSize() const {
  DCHECK_GT(sliding_window_.size(), 0u);
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
  if (is_dropped)
    dropped_frame_count_in_window_++;

  if (ComputeCurrentWindowSize() < kSlidingWindowInterval)
    return;

  DCHECK_GE(dropped_frame_count_in_window_, 0u);
  DCHECK_GE(sliding_window_.size(), dropped_frame_count_in_window_);

  double percent_dropped_frame =
      (dropped_frame_count_in_window_ * 100.0) / sliding_window_.size();
  sliding_window_max_percent_dropped_ =
      fmax(sliding_window_max_percent_dropped_, percent_dropped_frame);
  sliding_window_histogram_.AddPercentDroppedFrame(percent_dropped_frame);

  while (ComputeCurrentWindowSize() >= kSlidingWindowInterval) {
    if (sliding_window_.front().second)  // If frame is dropped.
      dropped_frame_count_in_window_--;
    sliding_window_.pop();
  }
}

void DroppedFrameCounter::OnFcpReceived() {
  fcp_received_ = true;
}

}  // namespace cc
