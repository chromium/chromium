// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/dropped_frame_counter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>

#include "base/functional/bind.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "build/chromeos_buildflags.h"
#include "cc/base/features.h"
#include "cc/metrics/custom_metrics_recorder.h"
#include "cc/metrics/frame_sorter.h"
#include "cc/metrics/total_frame_counter.h"
#include "cc/metrics/ukm_smoothness_data.h"

namespace cc {
namespace {

const base::TimeDelta kDefaultSlidingWindowInterval = base::Seconds(1);

// The start ranges of each bucket, up to but not including the start of the
// next bucket. The last bucket contains the remaining values.
constexpr std::array<double, 7> kBucketBounds = {0, 3, 6, 12, 25, 50, 75};

}  // namespace

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

double SlidingWindowHistogram::GetPercentDroppedFrameVariance() const {
  double sum = 0;
  size_t bin_count = sizeof(histogram_bins_) / sizeof(uint32_t);
  for (size_t i = 0; i < bin_count; ++i) {
    sum += histogram_bins_[i] * i;
  }

  // Don't calculate if count is 1 or less. Avoid divide by zero.
  if (total_count_ <= 1)
    return 0;

  double average = sum / total_count_;
  sum = 0;  // Sum is reset to be used for variance calculation

  for (size_t i = 0; i < bin_count; ++i) {
    sum += histogram_bins_[i] * (i - average) * (i - average);
    // histogram_bins_[i] is the number of PDFs which were in the range of
    // [i,i+1) so i is used as the actual value which is repeated for
    // histogram_bins_[i] times.
  }

  return sum / (total_count_ - 1);
}

std::vector<double> SlidingWindowHistogram::GetPercentDroppedFrameBuckets()
    const {
  if (total_count_ == 0)
    return std::vector<double>(std::size(kBucketBounds), 0);
  std::vector<double> buckets(std::size(kBucketBounds));
  for (size_t i = 0; i < std::size(kBucketBounds); ++i) {
    buckets[i] =
        static_cast<double>(smoothness_buckets_[i]) * 100 / total_count_;
  }
  return buckets;
}

void SlidingWindowHistogram::Clear() {
  std::fill(std::begin(histogram_bins_), std::end(histogram_bins_), 0);
  std::fill(std::begin(smoothness_buckets_), std::end(smoothness_buckets_), 0);
  total_count_ = 0;
}

std::ostream& SlidingWindowHistogram::Dump(std::ostream& stream) const {
  for (size_t i = 0; i < std::size(histogram_bins_); ++i) {
    stream << i << ": " << histogram_bins_[i] << std::endl;
  }
  return stream << "Total: " << total_count_;
}

std::ostream& operator<<(
    std::ostream& stream,
    const DroppedFrameCounter::SlidingWindowHistogram& histogram) {
  return histogram.Dump(stream);
}

DroppedFrameCounter::DroppedFrameCounter() = default;
DroppedFrameCounter::~DroppedFrameCounter() = default;

uint32_t DroppedFrameCounter::GetAverageThroughput() const {
  size_t good_frames = 0;
  for (auto it = End(); it; --it) {
    if (**it == kFrameStateComplete || **it == kFrameStatePartial)
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

// Start with flushing the frames in frame_sorter ignoring the currently
// pending frames, so all callers should call frame_sorter_.Reset();
// prior to this function.
// TODO(crbug.com/409093076): Remove all uses of this function.
void DroppedFrameCounter::ResetPendingFrames(base::TimeTicks timestamp) {
  // Before resetting the pending frames, update the measurements for the
  // sliding windows.
  if (!latest_sliding_window_start_.is_null()) {
    const auto report_until = timestamp - kDefaultSlidingWindowInterval;
    // Report the sliding window metrics for frames that have already been
    // completed (and some of which may have been dropped).
    while (!sliding_window_.empty()) {
      const auto& args = sliding_window_.front().first;
      if (args.frame_time > report_until)
        break;
      PopSlidingWindow();
    }
    if (sliding_window_.empty()) {
      DCHECK_EQ(
          dropped_frame_count_in_window_[SmoothnessStrategy::kDefaultStrategy],
          0u);
      DCHECK_EQ(dropped_frame_count_in_window_
                    [SmoothnessStrategy::kCompositorFocusedStrategy],
                0u);
    }

    // Report no dropped frames for the sliding windows spanning the rest of the
    // time.
    if (latest_sliding_window_start_ < report_until) {
      const auto difference = report_until - latest_sliding_window_start_;
      const size_t count =
          std::ceil(difference / latest_sliding_window_interval_);
      if (count > 0) {
        sliding_window_histogram_[SmoothnessStrategy::kDefaultStrategy]
            .AddPercentDroppedFrame(0., count);
        sliding_window_histogram_
            [SmoothnessStrategy::kCompositorFocusedStrategy]
                .AddPercentDroppedFrame(0., count);
      }
    }
  }

  dropped_frame_count_in_window_.fill(0);
  sliding_window_ = {};
  latest_sliding_window_start_ = {};
  latest_sliding_window_interval_ = {};
}

void DroppedFrameCounter::EnableReportForUI() {
  report_for_ui_ = true;
}

void DroppedFrameCounter::OnEndFrame(const viz::BeginFrameArgs& args,
                                     const FrameInfo& frame_info) {
  const bool is_dropped = frame_info.IsDroppedAffectingSmoothness();
  if (!args.interval.is_zero())
    total_frames_in_window_ = kDefaultSlidingWindowInterval / args.interval;

  // Don't measure smoothness for frames that start before FCP is received, or
  // that have already been reported as dropped.
  if (is_dropped && first_contentful_paint_received_ &&
      args.frame_time >= time_first_contentful_paint_received_) {
    ++total_smoothness_dropped_;

    if (!report_for_ui_) {
      ReportFrames();
    }
  }

  // Report frames on every frame for UI. And this needs to happen after
  // `frame_sorter_.AddFrameResult` so that the current ending frame is included
  // in the sliding window.
  if (report_for_ui_) {
    ReportFramesOnEveryFrameForUI();
  }
}

void DroppedFrameCounter::ReportFrames() {
  DCHECK(!report_for_ui_);

  const auto total_frames =
      total_counter_->ComputeTotalVisibleFrames(base::TimeTicks::Now());
  TRACE_EVENT2("cc,benchmark", "SmoothnessDroppedFrame", "total", total_frames,
               "smoothness", total_smoothness_dropped_);
  if (sliding_window_max_percent_dropped_ !=
      last_reported_metrics_.max_window) {
    UMA_HISTOGRAM_PERCENTAGE(
        "Graphics.Smoothness.MaxPercentDroppedFrames_1sWindow",
        sliding_window_max_percent_dropped_);
    last_reported_metrics_.max_window = sliding_window_max_percent_dropped_;
  }

  if (ukm_smoothness_data_ && total_frames > 0) {
    UkmSmoothnessData smoothness_data;
    smoothness_data.avg_smoothness =
        static_cast<double>(total_smoothness_dropped_) * 100 / total_frames;
    smoothness_data.median_smoothness =
        SlidingWindowMedianPercentDropped(SmoothnessStrategy::kDefaultStrategy);
    smoothness_data.compositor_focused_median =
        SlidingWindowMedianPercentDropped(
            SmoothnessStrategy::kCompositorFocusedStrategy);
    ukm_smoothness_data_->Write(smoothness_data);
  }
}

void DroppedFrameCounter::ReportFramesOnEveryFrameForUI() {
  DCHECK(report_for_ui_);

  if (!sliding_window_current_percent_dropped_) {
    return;
  }

  auto* recorder = CustomMetricRecorder::Get();
  if (!recorder) {
    return;
  }

  recorder->ReportPercentDroppedFramesInOneSecondWindow2(
      *sliding_window_current_percent_dropped_);
}

void DroppedFrameCounter::SetUkmSmoothnessDestination(
    UkmSmoothnessDataShared* smoothness_data) {
  ukm_smoothness_data_ = smoothness_data;
}

// Start with flushing the frames in frame_sorter ignoring the currently
// pending frames, so all callers should call frame_sorter_.Reset();
// prior to invoking this function.
// TODO(crbug.com/409093076): Remove all uses of this function.
void DroppedFrameCounter::Reset() {
  total_frames_ = 0;
  total_partial_ = 0;
  total_dropped_ = 0;
  total_smoothness_dropped_ = 0;
  sliding_window_max_percent_dropped_ = 0;
  dropped_frame_count_in_window_.fill(0);
  first_contentful_paint_received_ = false;
  sliding_window_ = {};
  latest_sliding_window_start_ = {};
  sliding_window_histogram_[SmoothnessStrategy::kDefaultStrategy].Clear();
  sliding_window_histogram_[SmoothnessStrategy::kCompositorFocusedStrategy]
      .Clear();
  ring_buffer_.Clear();
  last_reported_metrics_ = {};
  sliding_window_current_percent_dropped_.reset();
}

base::TimeDelta DroppedFrameCounter::ComputeCurrentWindowSize() const {
  if (sliding_window_.empty())
    return {};
  return sliding_window_.back().first.frame_time +
         sliding_window_.back().first.interval -
         sliding_window_.front().first.frame_time;
}

void DroppedFrameCounter::AddSortedFrame(const viz::BeginFrameArgs& args,
                                         const FrameInfo& frame_info) {
  // Entirely disregard the frames with interval larger than the window --
  // these are violating the assumptions in the below code and should
  // only occur with external frame control, where dropped frame stats
  // are not relevant.
  if (args.interval >= kDefaultSlidingWindowInterval) {
    return;
  }

  sliding_window_.emplace(args, frame_info);
  UpdateDroppedFrameCountInWindow(frame_info, 1);

  const bool is_dropped = frame_info.IsDroppedAffectingSmoothness();
  if (!in_dropping_ && is_dropped) {
    TRACE_EVENT_BEGIN("cc,benchmark,latency", "DroppedFrameDuration",
                      perfetto::Track(reinterpret_cast<uint64_t>(this),
                                      perfetto::ThreadTrack::Current()),
                      args.frame_time);
    in_dropping_ = true;
  } else if (in_dropping_ && !is_dropped) {
    TRACE_EVENT_END("cc,benchmark,latency" /* "DroppedFrameDuration" */,
                    perfetto::Track(reinterpret_cast<uint64_t>(this),
                                    perfetto::ThreadTrack::Current()),
                    args.frame_time);
    in_dropping_ = false;
  }

  OnEndFrame(args, frame_info);

  if (ComputeCurrentWindowSize() < kDefaultSlidingWindowInterval) {
    return;
  }

  DCHECK_GE(
      dropped_frame_count_in_window_[SmoothnessStrategy::kDefaultStrategy], 0u);
  DCHECK_GE(
      sliding_window_.size(),
      dropped_frame_count_in_window_[SmoothnessStrategy::kDefaultStrategy]);

  while (ComputeCurrentWindowSize() > kDefaultSlidingWindowInterval) {
    PopSlidingWindow();
  }
  DCHECK(!sliding_window_.empty());
}

void DroppedFrameCounter::PopSlidingWindow() {
  const auto removed_args = sliding_window_.front().first;
  const auto removed_frame_info = sliding_window_.front().second;
  UpdateDroppedFrameCountInWindow(removed_frame_info, -1);
  sliding_window_.pop();
  if (sliding_window_.empty())
    return;

  // Don't count the newest element if it is outside the current window.
  const auto& newest_args = sliding_window_.back().first;
  const auto newest_was_dropped =
      sliding_window_.back().second.IsDroppedAffectingSmoothness();

  uint32_t invalidated_frames = 0;
  if (ComputeCurrentWindowSize() > kDefaultSlidingWindowInterval &&
      newest_was_dropped) {
    invalidated_frames++;
  }

  // If two consecutive 'completed' frames are far apart from each other (in
  // time), then report the 'dropped frame count' for the sliding window(s) in
  // between. Note that the window-size still needs to be at least
  // kDefaultSlidingWindowInterval.
  const auto max_sliding_window_start =
      newest_args.frame_time - kDefaultSlidingWindowInterval;
  const auto max_difference = newest_args.interval * 1.5;
  const auto& remaining_oldest_args = sliding_window_.front().first;
  const auto last_timestamp =
      std::min(remaining_oldest_args.frame_time, max_sliding_window_start);
  const auto difference = last_timestamp - removed_args.frame_time;
  const size_t count = difference > max_difference
                           ? std::ceil(difference / newest_args.interval)
                           : 1;

  uint32_t dropped =
      dropped_frame_count_in_window_[SmoothnessStrategy::kDefaultStrategy] -
      invalidated_frames;
  const double percent_dropped_frame =
      std::min((dropped * 100.0) / total_frames_in_window_, 100.0);
  sliding_window_histogram_[SmoothnessStrategy::kDefaultStrategy]
      .AddPercentDroppedFrame(percent_dropped_frame, count);

  uint32_t dropped_compositor =
      dropped_frame_count_in_window_
          [SmoothnessStrategy::kCompositorFocusedStrategy] -
      invalidated_frames;
  double percent_dropped_frame_compositor =
      std::min((dropped_compositor * 100.0) / total_frames_in_window_, 100.0);
  sliding_window_histogram_[SmoothnessStrategy::kCompositorFocusedStrategy]
      .AddPercentDroppedFrame(percent_dropped_frame_compositor, count);

  sliding_window_current_percent_dropped_ = percent_dropped_frame;

  latest_sliding_window_start_ = last_timestamp;
  latest_sliding_window_interval_ = remaining_oldest_args.interval;
}

void DroppedFrameCounter::UpdateDroppedFrameCountInWindow(
    const FrameInfo& frame_info,
    int count) {
  if (frame_info.IsDroppedAffectingSmoothness()) {
    DCHECK_GE(
        dropped_frame_count_in_window_[SmoothnessStrategy::kDefaultStrategy] +
            count,
        0u);
    dropped_frame_count_in_window_[SmoothnessStrategy::kDefaultStrategy] +=
        count;
  }
  if (frame_info.WasSmoothCompositorUpdateDropped()) {
    DCHECK_GE(dropped_frame_count_in_window_
                      [SmoothnessStrategy::kCompositorFocusedStrategy] +
                  count,
              0u);
    dropped_frame_count_in_window_
        [SmoothnessStrategy::kCompositorFocusedStrategy] += count;
  }
}

void DroppedFrameCounter::OnFirstContentfulPaintReceived() {
  DCHECK(!first_contentful_paint_received_);
  first_contentful_paint_received_ = true;
  time_first_contentful_paint_received_ = base::TimeTicks::Now();
}

}  // namespace cc
