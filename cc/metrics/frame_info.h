// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_FRAME_INFO_H_
#define CC_METRICS_FRAME_INFO_H_

#include "base/time/time.h"
#include "cc/cc_export.h"

namespace cc {

struct CC_EXPORT FrameInfo {
  enum class FrameFinalState {
    kNoUpdateDesired,
    kDropped,

    // A `presented all` frame contains all the desired update for this vsync.
    // Note that this doesn't necessarily mean the frame included updates from
    // both the main and the compositor thread. For example, if there's only a
    // main-thread animation running, and the animation update was included in
    // the frame produced, then it's `presented all`, although the compositor
    // thread did not have any updates for this frame.
    kPresentedAll,

    // A `partial update` frame contains updates from a compositor frame, but
    // misses the update from the main-thread for the same vsync. However, it is
    // still possible for such a `partial update` frame to contain new update
    // from an earlier main-thread.
    //
    // `kPresentedPartialOldMain` represents a partial update frame without any
    //     new update from the main-thread.
    // `kPresentedPartialNewMain` represents a partial update frame with some
    //     new update from the main-thread.
    kPresentedPartialOldMain,
    kPresentedPartialNewMain,
  };
  FrameFinalState final_state = FrameFinalState::kNoUpdateDesired;

  enum class SmoothThread {
    kSmoothNone,
    kSmoothCompositor,
    kSmoothMain,
    kSmoothBoth
  };
  SmoothThread smooth_thread = SmoothThread::kSmoothNone;

  enum class MainThreadResponse {
    kIncluded,
    kMissing,
  };
  MainThreadResponse main_thread_response = MainThreadResponse::kIncluded;

  enum class SmoothEffectDrivingThread { kMain, kCompositor, kUnknown };
  SmoothEffectDrivingThread scroll_thread = SmoothEffectDrivingThread::kUnknown;

  bool has_missing_content = false;

  // The total latency for the frame. If the frame had to be 'split' (i.e.
  // compositor-thread update and main-thread updates were presented in separate
  // frames), then this contains the maximum latency of the two updates.
  base::TimeDelta total_latency;

  bool IsDroppedAffectingSmoothness() const;
  void MergeWith(const FrameInfo& info);

  bool Validate() const;

  // Returns whether any update from the compositor/main thread was dropped, and
  // whether the update was part of a smooth sequence.
  bool WasSmoothCompositorUpdateDropped() const;
  bool WasSmoothMainUpdateDropped() const;
  bool WasSmoothMainUpdateExpected() const;

  bool IsScrollPrioritizeFrameDropped() const;

 private:
  bool was_merged = false;
  bool compositor_update_was_dropped = false;
  bool main_update_was_dropped = false;
};

}  // namespace cc

#endif  // CC_METRICS_FRAME_INFO_H_
