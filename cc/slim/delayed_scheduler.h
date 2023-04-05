// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SLIM_DELAYED_SCHEDULER_H_
#define CC_SLIM_DELAYED_SCHEDULER_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "cc/slim/scheduler.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"

namespace cc::slim {

// DelayedScheduler is optimized specifically for the case of renderer-driven
// scrolling of browser controls in the browser compositor on Android.
// The design aims to immediately submit frame whenever scroll offset arrives
// from the renderer:
// * Expect `MaybeCompositeNow` to be called when scroll offset from renderer
//   arrives.
// * Cache BeginFrameArgs from `OnBeginFrameFromViz` and wait until either
//   `MaybeCompositeNow` or until the next `OnBeginFrameFromViz`. This increases
//   the latency for other cases that does not call `MaybeCompositeNow` by one
//   frame.
// * If `MaybeCompositeNow` is blocked from producing a frame due to some
//   condition, immediately attempt producing from again if the condition
//   changes.
// TODO(boliu): Move this to //content after slim is enabled for browser
// compositor on android.
class COMPONENT_EXPORT(CC_SLIM) DelayedScheduler : public Scheduler {
 public:
  DelayedScheduler();
  ~DelayedScheduler() override;

  void Initialize(SchedulerClient* client) override;
  void OnBeginFrameFromViz(
      const viz::BeginFrameArgs& begin_frame_args) override;
  void OnBeginFramePausedChanged(bool paused) override;
  void SetNeedsBeginFrame(bool needs_begin_frame) override;
  void SetIsSwapThrottled(bool is_swap_throttled) override;
  void MaybeCompositeNow() override;

 private:
  void BeginFrameAndResetArgs(viz::BeginFrameArgs& args);
  void SendDidNotProduceFrameAndResetArgs(viz::BeginFrameArgs& args);

  raw_ptr<SchedulerClient> client_ = nullptr;

  viz::BeginFrameArgs unused_args_;
  bool needs_begin_frame_ = false;
  bool is_swap_throttled_ = false;
  bool missed_composite_now_ = false;
};

}  // namespace cc::slim

#endif  // CC_SLIM_DELAYED_SCHEDULER_H_
