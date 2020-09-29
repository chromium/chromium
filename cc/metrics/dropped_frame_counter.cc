// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/dropped_frame_counter.h"

#include <stddef.h>

#include <limits>

#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "cc/metrics/total_frame_counter.h"

namespace cc {

DroppedFrameCounter::DroppedFrameCounter() = default;

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

void DroppedFrameCounter::AddDroppedFrameAffectingSmoothness() {
  if (fcp_received_)
    ++total_smoothness_dropped_;
  ReportFrames();
}

void DroppedFrameCounter::ReportFrames() {
  TRACE_EVENT2(
      "cc,benchmark", "SmoothnessDroppedFrame", "total",
      total_counter_->ComputeTotalVisibleFrames(base::TimeTicks::Now()),
      "smoothness", total_smoothness_dropped_);
}

void DroppedFrameCounter::Reset() {
  total_frames_ = 0;
  total_partial_ = 0;
  total_dropped_ = 0;
  total_smoothness_dropped_ = 0;
  fcp_received_ = false;
  ring_buffer_.Clear();
}

void DroppedFrameCounter::OnFcpReceived() {
  fcp_received_ = true;
}

}  // namespace cc
