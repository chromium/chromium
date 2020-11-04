// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_DROPPED_FRAME_COUNTER_H_
#define CC_METRICS_DROPPED_FRAME_COUNTER_H_

#include <stddef.h>

#include <memory>

#include "base/containers/ring_buffer.h"
#include "cc/cc_export.h"

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

  DroppedFrameCounter();

  DroppedFrameCounter(const DroppedFrameCounter&) = delete;
  DroppedFrameCounter& operator=(const DroppedFrameCounter&) = delete;

  size_t frame_history_size() const { return ring_buffer_.BufferSize(); }
  size_t total_frames() const { return total_frames_; }
  size_t total_compositor_dropped() const { return total_dropped_; }
  size_t total_main_dropped() const { return total_partial_; }
  size_t total_smoothness_dropped() const { return total_smoothness_dropped_; }

  uint32_t GetAverageThroughput() const;

  typedef base::RingBuffer<FrameState, 180> RingBufferType;
  RingBufferType::Iterator begin() const { return ring_buffer_.Begin(); }
  RingBufferType::Iterator end() const { return ring_buffer_.End(); }

  void AddGoodFrame();
  void AddPartialFrame();
  void AddDroppedFrame();

  void AddDroppedFrameAffectingSmoothness();
  void ReportFrames();

  void SetUkmSmoothnessDestination(UkmSmoothnessDataShared* smoothness_data);

  void Reset();
  void OnFcpReceived();

  void set_total_counter(TotalFrameCounter* total_counter) {
    total_counter_ = total_counter;
  }

 private:
  RingBufferType ring_buffer_;
  size_t total_frames_ = 0;
  size_t total_partial_ = 0;
  size_t total_dropped_ = 0;
  size_t total_smoothness_dropped_ = 0;
  bool fcp_received_ = false;

  UkmSmoothnessDataShared* ukm_smoothness_data_ = nullptr;

  TotalFrameCounter* total_counter_ = nullptr;
};

}  // namespace cc

#endif  // CC_METRICS_DROPPED_FRAME_COUNTER_H_
