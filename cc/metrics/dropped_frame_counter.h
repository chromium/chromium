// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_DROPPED_FRAME_COUNTER_H_
#define CC_METRICS_DROPPED_FRAME_COUNTER_H_

#include <stddef.h>
#include <queue>
#include <utility>

#include "base/containers/ring_buffer.h"
#include "cc/cc_export.h"
#include "cc/metrics/frame_sorter.h"

namespace cc {
class TotalFrameCounter;
struct UkmSmoothnessDataShared;

// This class maintains a counter for produced/dropped frames, and can be used
// to estimate the recent throughput.
class CC_EXPORT DroppedFrameCounter {
 public:
  enum FrameState {
    kFrameStateDropped,
    kFrameStatePartial,
    kFrameStateComplete
  };

  class CC_EXPORT SlidingWindowHistogram {
   public:
    void AddPercentDroppedFrame(double percent_dropped_frame);
    uint32_t GetPercentDroppedFramePercentile(double percentile) const;
    void clear();

   private:
    uint32_t histogram_bins_[101] = {0};
    uint32_t total_count = 0;
  };

  DroppedFrameCounter();
  ~DroppedFrameCounter();

  DroppedFrameCounter(const DroppedFrameCounter&) = delete;
  DroppedFrameCounter& operator=(const DroppedFrameCounter&) = delete;

  size_t frame_history_size() const { return ring_buffer_.BufferSize(); }
  size_t total_frames() const { return total_frames_; }
  size_t total_compositor_dropped() const { return total_dropped_; }
  size_t total_main_dropped() const { return total_partial_; }
  size_t total_smoothness_dropped() const { return total_smoothness_dropped_; }

  uint32_t GetAverageThroughput() const;

  double GetMostRecentAverageSmoothness() const;
  double GetMostRecent95PercentileSmoothness() const;

  typedef base::RingBuffer<FrameState, 180> RingBufferType;
  RingBufferType::Iterator begin() const { return ring_buffer_.Begin(); }
  RingBufferType::Iterator end() const { return ring_buffer_.End(); }

  void AddGoodFrame();
  void AddPartialFrame();
  void AddDroppedFrame();
  void ReportFrames();

  void OnBeginFrame(const viz::BeginFrameArgs& args);
  void OnEndFrame(const viz::BeginFrameArgs& args, bool is_dropped);
  void SetUkmSmoothnessDestination(UkmSmoothnessDataShared* smoothness_data);
  void OnFcpReceived();

  // Reset is used on navigation, which resets frame statistics as well as
  // frame sorter.
  void Reset();
  // ResetFrameSorter is used when we need to keep track of frame statistics
  // but not to track the frames prior to reset in frame sorter.
  void ResetFrameSorter();

  void set_total_counter(TotalFrameCounter* total_counter) {
    total_counter_ = total_counter;
  }

  double sliding_window_max_percent_dropped() const {
    return sliding_window_max_percent_dropped_;
  }

  uint32_t SlidingWindow95PercentilePercentDropped() const {
    return sliding_window_histogram_.GetPercentDroppedFramePercentile(0.95);
  }

 private:
  void NotifyFrameResult(const viz::BeginFrameArgs& args, bool is_dropped);
  base::TimeDelta ComputeCurrentWindowSize() const;

  const base::TimeDelta kSlidingWindowInterval =
      base::TimeDelta::FromSeconds(1);
  std::queue<std::pair<const viz::BeginFrameArgs, bool>> sliding_window_;
  uint32_t dropped_frame_count_in_window_ = 0;
  SlidingWindowHistogram sliding_window_histogram_;

  RingBufferType ring_buffer_;
  size_t total_frames_ = 0;
  size_t total_partial_ = 0;
  size_t total_dropped_ = 0;
  size_t total_smoothness_dropped_ = 0;
  bool fcp_received_ = false;
  double sliding_window_max_percent_dropped_ = 0;

  UkmSmoothnessDataShared* ukm_smoothness_data_ = nullptr;
  FrameSorter frame_sorter_;
  TotalFrameCounter* total_counter_ = nullptr;
};

}  // namespace cc

#endif  // CC_METRICS_DROPPED_FRAME_COUNTER_H_
