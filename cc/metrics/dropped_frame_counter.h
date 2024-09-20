// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef CC_METRICS_DROPPED_FRAME_COUNTER_H_
#define CC_METRICS_DROPPED_FRAME_COUNTER_H_

#include <stddef.h>

#include <map>
#include <optional>
#include <queue>
#include <utility>
#include <vector>

#include "base/containers/ring_buffer.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "cc/cc_export.h"
#include "cc/metrics/frame_info.h"
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

  enum SmoothnessStrategy {
    kDefaultStrategy,  // All threads and interactions are considered equal.
    kScrollFocusedStrategy,  // Scroll interactions has the highest priority.
    kMainFocusedStrategy,    // Reports dropped frames with main thread updates.
    kCompositorFocusedStrategy,  // Reports dropped frames with compositor
    // thread updates.
    kStrategyCount
  };

  class CC_EXPORT SlidingWindowHistogram {
   public:
    void AddPercentDroppedFrame(double percent_dropped_frame, size_t count = 1);
    uint32_t GetPercentDroppedFramePercentile(double percentile) const;
    double GetPercentDroppedFrameVariance() const;
    std::vector<double> GetPercentDroppedFrameBuckets() const;
    void Clear();
    std::ostream& Dump(std::ostream& stream) const;

    uint32_t total_count() const { return total_count_; }

   private:
    uint32_t histogram_bins_[101] = {0};
    uint32_t smoothness_buckets_[7] = {0};
    uint32_t total_count_ = 0;
  };

  DroppedFrameCounter();
  ~DroppedFrameCounter();

  DroppedFrameCounter(const DroppedFrameCounter&) = delete;
  DroppedFrameCounter& operator=(const DroppedFrameCounter&) = delete;

  size_t frame_history_size() const { return ring_buffer_.BufferSize(); }
  size_t total_frames() const { return total_frames_; }
  size_t total_dropped() const { return total_dropped_; }
  size_t total_partial() const { return total_partial_; }
  size_t total_smoothness_dropped() const { return total_smoothness_dropped_; }

  uint32_t GetAverageThroughput() const;

  using SortedFrameCallback =
      base::RepeatingCallback<void(const viz::BeginFrameArgs& args,
                                   const FrameInfo&)>;
  void SetSortedFrameCallback(SortedFrameCallback callback);

  typedef base::RingBuffer<FrameState, 180> RingBufferType;
  RingBufferType::Iterator Begin() const { return ring_buffer_.Begin(); }
  // `End()` points to the last `FrameState`, not past it.
  RingBufferType::Iterator End() const { return ring_buffer_.End(); }

  void AddGoodFrame();
  void AddPartialFrame();
  void AddDroppedFrame();
  void ReportFrames();
  void ReportFramesOnEveryFrameForUI();

  void OnBeginFrame(const viz::BeginFrameArgs& args);
  void OnEndFrame(const viz::BeginFrameArgs& args, const FrameInfo& frame_info);
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

  // Enable dropped frame report for ui::Compositor..
  void EnableReporForUI();

  void set_total_counter(TotalFrameCounter* total_counter) {
    total_counter_ = total_counter;
  }

  void SetTimeFcpReceivedForTesting(base::TimeTicks time_fcp_received) {
    DCHECK(fcp_received_);
    time_fcp_received_ = time_fcp_received;
  }

  double sliding_window_max_percent_dropped() const {
    return sliding_window_max_percent_dropped_;
  }

  std::optional<double> max_percent_dropped_After_1_sec() const {
    return sliding_window_max_percent_dropped_After_1_sec_;
  }

  std::optional<double> max_percent_dropped_After_2_sec() const {
    return sliding_window_max_percent_dropped_After_2_sec_;
  }

  std::optional<double> max_percent_dropped_After_5_sec() const {
    return sliding_window_max_percent_dropped_After_5_sec_;
  }

  uint32_t SlidingWindow95PercentilePercentDropped(
      SmoothnessStrategy strategy) const {
    DCHECK_GT(SmoothnessStrategy::kStrategyCount, strategy);
    return sliding_window_histogram_[strategy].GetPercentDroppedFramePercentile(
        0.95);
  }

  uint32_t SlidingWindowMedianPercentDropped(
      SmoothnessStrategy strategy) const {
    DCHECK_GT(SmoothnessStrategy::kStrategyCount, strategy);
    return sliding_window_histogram_[strategy].GetPercentDroppedFramePercentile(
        0.5);
  }

  double SlidingWindowPercentDroppedVariance(
      SmoothnessStrategy strategy) const {
    DCHECK_GT(SmoothnessStrategy::kStrategyCount, strategy);
    return sliding_window_histogram_[strategy].GetPercentDroppedFrameVariance();
  }

  const SlidingWindowHistogram* GetSlidingWindowHistogram(
      SmoothnessStrategy strategy) const {
    DCHECK_GT(SmoothnessStrategy::kStrategyCount, strategy);
    return &sliding_window_histogram_[strategy];
  }

  double sliding_window_current_percent_dropped() const {
    return sliding_window_current_percent_dropped_.value_or(0);
  }

 private:
  void NotifyFrameResult(const viz::BeginFrameArgs& args,
                         const FrameInfo& frame_info);
  base::TimeDelta ComputeCurrentWindowSize() const;

  void PopSlidingWindow();
  void UpdateMaxPercentDroppedFrame(double percent_dropped_frame);

  // Adds count to dropped_frame_count_in_window_ of each strategy.
  void UpdateDroppedFrameCountInWindow(const FrameInfo& frame_info, int count);

  std::queue<std::pair<const viz::BeginFrameArgs, FrameInfo>> sliding_window_;
  uint32_t dropped_frame_count_in_window_[SmoothnessStrategy::kStrategyCount] =
      {0};
  double total_frames_in_window_ = 60.0;
  SlidingWindowHistogram
      sliding_window_histogram_[SmoothnessStrategy::kStrategyCount];

  base::TimeTicks latest_sliding_window_start_;
  base::TimeDelta latest_sliding_window_interval_;

  RingBufferType ring_buffer_;
  size_t total_frames_ = 0;
  size_t total_partial_ = 0;
  size_t total_dropped_ = 0;
  size_t total_smoothness_dropped_ = 0;
  bool fcp_received_ = false;
  double sliding_window_max_percent_dropped_ = 0;
  std::optional<double> sliding_window_max_percent_dropped_After_1_sec_;
  std::optional<double> sliding_window_max_percent_dropped_After_2_sec_;
  std::optional<double> sliding_window_max_percent_dropped_After_5_sec_;
  base::TimeTicks time_fcp_received_;
  raw_ptr<UkmSmoothnessDataShared> ukm_smoothness_data_ = nullptr;
  FrameSorter frame_sorter_;
  raw_ptr<TotalFrameCounter> total_counter_ = nullptr;

  struct {
    double max_window = 0;
    double p95_window = 0;
  } last_reported_metrics_;

  std::optional<SortedFrameCallback> sorted_frame_callback_;

  bool report_for_ui_ = false;
  std::optional<double> sliding_window_current_percent_dropped_;

  // Sets to true on a newly dropped frame and stays true as long as the frames
  // that follow are dropped. Reset when a frame is presented. It is used to
  // generate asynchronous trace events that cover the duration of consecutive
  // dropped frames
  bool in_dropping_ = false;
};

CC_EXPORT std::ostream& operator<<(
    std::ostream&,
    const DroppedFrameCounter::SlidingWindowHistogram&);

}  // namespace cc

#endif  // CC_METRICS_DROPPED_FRAME_COUNTER_H_
