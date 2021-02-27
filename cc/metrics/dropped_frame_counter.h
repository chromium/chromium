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
#include "cc/metrics/ukm_smoothness_data.h"

namespace cc {
class TotalFrameCounter;

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
    void AddPercentDroppedFrame(double percent_dropped_frame, size_t count = 1);
    uint32_t GetPercentDroppedFramePercentile(double percentile) const;
    void Clear();
    std::ostream& Dump(std::ostream& stream) const;

    uint32_t total_count() const { return total_count_; }

   private:
    uint32_t histogram_bins_[101] = {0};
    uint32_t total_count_ = 0;
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

  void OnBeginFrame(const viz::BeginFrameArgs& args, bool is_scroll_active);
  void OnEndFrame(const viz::BeginFrameArgs& args, bool is_dropped);
  void SetUkmSmoothnessDestination(UkmSmoothnessDataShared* smoothness_data);
  void OnFcpReceived();

  // Reset is used on navigation, which resets frame statistics as well as
  // frame sorter.
  void Reset();

  // ResetPendingFrames is used when we need to keep track of frame statistics,
  // but should no longer wait for the pending frames (e.g. connection to
  // gpu-process was reset, or the page became invisible, etc.). The pending
  // frames are not considered to be dropped.
  void ResetPendingFrames(base::TimeTicks timestamp);

  void set_total_counter(TotalFrameCounter* total_counter) {
    total_counter_ = total_counter;
  }

  double sliding_window_max_percent_dropped() const {
    return sliding_window_max_percent_dropped_;
  }

  uint32_t SlidingWindow95PercentilePercentDropped() const {
    return sliding_window_histogram_.GetPercentDroppedFramePercentile(0.95);
  }

  const SlidingWindowHistogram* GetSlidingWindowHistogram() const {
    return &sliding_window_histogram_;
  }

 private:
  void NotifyFrameResult(const viz::BeginFrameArgs& args, bool is_dropped);
  base::TimeDelta ComputeCurrentWindowSize() const;

  const base::TimeDelta kSlidingWindowInterval =
      base::TimeDelta::FromSeconds(1);
  std::queue<std::pair<const viz::BeginFrameArgs, bool>> sliding_window_;
  uint32_t dropped_frame_count_in_window_ = 0;
  double total_frames_in_window_ = 60.0;
  SlidingWindowHistogram sliding_window_histogram_;

  base::TimeTicks latest_sliding_window_start_;
  base::TimeDelta latest_sliding_window_interval_;

  RingBufferType ring_buffer_;
  size_t total_frames_ = 0;
  size_t total_partial_ = 0;
  size_t total_dropped_ = 0;
  size_t total_smoothness_dropped_ = 0;
  bool fcp_received_ = false;
  double sliding_window_max_percent_dropped_ = 0;
  base::TimeTicks time_fcp_received_;
  base::TimeDelta time_max_delta_;
  UkmSmoothnessDataShared* ukm_smoothness_data_ = nullptr;
  FrameSorter frame_sorter_;
  TotalFrameCounter* total_counter_ = nullptr;

  struct ScrollStartInfo {
    // The timestamp of when the scroll started.
    base::TimeTicks timestamp;

    // The vsync corresponding to the scroll-start.
    viz::BeginFrameId frame_id;
  };
  base::Optional<ScrollStartInfo> scroll_start_;
  std::map<viz::BeginFrameId, ScrollStartInfo> scroll_start_per_frame_;
};

CC_EXPORT std::ostream& operator<<(
    std::ostream&,
    const DroppedFrameCounter::SlidingWindowHistogram&);

}  // namespace cc

#endif  // CC_METRICS_DROPPED_FRAME_COUNTER_H_
