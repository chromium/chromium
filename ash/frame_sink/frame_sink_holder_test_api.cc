// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame_sink/frame_sink_holder_test_api.h"

#include "ash/frame_sink/frame_sink_holder.h"
#include "ui/gfx/geometry/size.h"

namespace ash {

FrameSinkHolderTestApi::FrameSinkHolderTestApi(
    FrameSinkHolder* frame_sink_holder)
    : frame_sink_holder_(frame_sink_holder) {}

FrameSinkHolderTestApi::~FrameSinkHolderTestApi() = default;

const gfx::Size& FrameSinkHolderTestApi::LastSubmittedFrameSize() const {
  return frame_sink_holder_->last_frame_size_in_pixels_;
}

bool FrameSinkHolderTestApi::IsPendingFrameAck() const {
  return frame_sink_holder_->pending_compositor_frame_ack_;
}

bool FrameSinkHolderTestApi::IsPendingFrame() const {
  return frame_sink_holder_->pending_compositor_frame_;
}

bool FrameSinkHolderTestApi::IsFirstFrameRequested() const {
  return frame_sink_holder_->first_frame_requested_;
}

bool FrameSinkHolderTestApi::IsObservingBeginFrameSource() const {
  return frame_sink_holder_->begin_frame_observation_.IsObserving();
}

}  // namespace ash
