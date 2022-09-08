// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_frame_info.h"

namespace cc {

FrameInfo CreateFakeFrameInfo(FrameInfo::FrameFinalState state) {
  FrameInfo info;
  info.final_state = state;
  info.smooth_thread = FrameInfo::SmoothThread::kSmoothBoth;
  return info;
}

}  // namespace cc
