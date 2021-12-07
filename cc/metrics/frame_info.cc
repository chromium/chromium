// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/frame_info.h"

#include <algorithm>

namespace cc {

namespace {

bool IsCompositorSmooth(FrameInfo::SmoothThread thread) {
  return thread == FrameInfo::SmoothThread::kSmoothCompositor ||
         thread == FrameInfo::SmoothThread::kSmoothBoth;
}

bool IsMainSmooth(FrameInfo::SmoothThread thread) {
  return thread == FrameInfo::SmoothThread::kSmoothMain ||
         thread == FrameInfo::SmoothThread::kSmoothBoth;
}

}  // namespace

bool FrameInfo::IsDroppedAffectingSmoothness() const {
  // If neither of the threads are expected to be smooth, then this frame cannot
  // affect smoothness.
  if (smooth_thread == SmoothThread::kSmoothNone)
    return false;

  switch (final_state) {
    case FrameFinalState::kDropped:
      return true;

    case FrameFinalState::kPresentedAll:
    case FrameFinalState::kPresentedPartialNewMain:
      // If the frame includes new main-thread update, even if it's for an
      // earlier begin-frame, then do not count it as a dropped frame affecting
      // smoothness.
      return false;

    case FrameFinalState::kPresentedPartialOldMain:
      // Partial-update frames without new updates from the main-thread affect
      // smoothness if the main-thread is expected to be smooth.
      return smooth_thread == SmoothThread::kSmoothBoth ||
             smooth_thread == SmoothThread::kSmoothMain;

    case FrameFinalState::kNoUpdateDesired:
      return false;
  }
}

void FrameInfo::MergeWith(const FrameInfo& info) {
  // The |scroll_thread| information cannot change once the frame starts. So
  // it should not need to be updated during merge.
  DCHECK_EQ(scroll_thread, info.scroll_thread);

  if (info.has_missing_content)
    has_missing_content = true;
  if (info.final_state == FrameFinalState::kDropped)
    final_state = FrameFinalState::kDropped;

  const bool is_compositor_smooth = IsCompositorSmooth(smooth_thread) ||
                                    IsCompositorSmooth(info.smooth_thread);
  const bool is_main_smooth =
      IsMainSmooth(smooth_thread) || IsMainSmooth(info.smooth_thread);
  if (is_compositor_smooth && is_main_smooth) {
    smooth_thread = SmoothThread::kSmoothBoth;
  } else if (is_compositor_smooth) {
    smooth_thread = SmoothThread::kSmoothCompositor;
  } else if (is_main_smooth) {
    smooth_thread = SmoothThread::kSmoothMain;
  } else {
    smooth_thread = SmoothThread::kSmoothNone;
  }

  total_latency = std::max(total_latency, info.total_latency);
}

}  // namespace cc
