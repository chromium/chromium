// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_FRAME_RATE_COUNTER_H_
#define CC_TREES_FRAME_RATE_COUNTER_H_

#include <stddef.h>

#include <memory>

#include "base/containers/ring_buffer.h"
#include "base/time/time.h"

namespace cc {

// This class maintains a history of timestamps, and provides functionality to
// intelligently compute average frames per second.
class FrameRateCounter {
 public:
  static std::unique_ptr<FrameRateCounter> Create(bool has_impl_thread);

  FrameRateCounter(const FrameRateCounter&) = delete;
  FrameRateCounter& operator=(const FrameRateCounter&) = delete;

  size_t current_frame_number() const { return ring_buffer_.CurrentIndex(); }
  int dropped_frame_count() const { return dropped_frame_count_; }
  size_t time_stamp_history_size() const { return ring_buffer_.BufferSize(); }

  void SaveTimeStamp(base::TimeTicks timestamp, bool software);

  // n = 0 returns the oldest frame interval retained in the history, while n =
  // time_stamp_history_size() - 1 returns the most recent frame interval.
  base::TimeDelta RecentFrameInterval(size_t n) const;

  // This is a heuristic that can be used to ignore frames in a reasonable way.
  // Returns true if the given frame interval is too fast or too slow, based on
  // constant thresholds.
  bool IsBadFrameInterval(
      base::TimeDelta interval_between_consecutive_frames) const;

  void GetMinAndMaxFPS(double* min_fps, double* max_fps) const;
  double GetAverageFPS() const;

  typedef base::RingBuffer<base::TimeTicks, 136> RingBufferType;
  RingBufferType::Iterator begin() const { return ring_buffer_.Begin(); }
  RingBufferType::Iterator end() const { return ring_buffer_.End(); }

 private:
  explicit FrameRateCounter(bool has_impl_thread);

  RingBufferType ring_buffer_;

  bool has_impl_thread_;
  int dropped_frame_count_;
};

}  // namespace cc

#endif  // CC_TREES_FRAME_RATE_COUNTER_H_
