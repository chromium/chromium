// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/dropped_frame_counter.h"

#include "base/bind.h"
#include "base/trace_event/trace_event.h"
#include "cc/metrics/frame_sorter.h"
#include "cc/metrics/total_frame_counter.h"
#include "cc/metrics/ukm_smoothness_data.h"

namespace cc {

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

  if (ukm_smoothness_data_ && total_frames > 0) {
    UkmSmoothnessData smoothness_data;
    smoothness_data.avg_smoothness =
        static_cast<double>(total_smoothness_dropped_) * 100 / total_frames;

    ukm_smoothness_data_->seq_lock.WriteBegin();
    device::OneWriterSeqLock::AtomicWriterMemcpy(&ukm_smoothness_data_->data,
                                                 &smoothness_data,
                                                 sizeof(UkmSmoothnessData));
    ukm_smoothness_data_->seq_lock.WriteEnd();
  }
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
  fcp_received_ = false;
  ring_buffer_.Clear();
  frame_sorter_.Reset();
}

void DroppedFrameCounter::NotifyFrameResult(const viz::BeginFrameArgs& args,
                                            bool is_dropped) {
  // TODO(crbug.com/1115141) The implementation of smoothness metrics.
}

void DroppedFrameCounter::OnFcpReceived() {
  fcp_received_ = true;
}

}  // namespace cc
