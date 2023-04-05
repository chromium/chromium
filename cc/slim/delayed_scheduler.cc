// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/slim/delayed_scheduler.h"

namespace cc::slim {

DelayedScheduler::DelayedScheduler() = default;
DelayedScheduler::~DelayedScheduler() = default;

void DelayedScheduler::Initialize(SchedulerClient* client) {
  DCHECK(!client_);
  DCHECK(client);
  client_ = client;
}

void DelayedScheduler::OnBeginFrameFromViz(
    const viz::BeginFrameArgs& begin_frame_args) {
  if (!needs_begin_frame_) {
    // If begin frames are not needed anymore, then just rely immediately with
    // DidNotProduceFrame for all args.
    if (unused_args_.IsValid()) {
      SendDidNotProduceFrameAndResetArgs(unused_args_);
    }
    client_->SendDidNotProduceFrame(begin_frame_args);
    return;
  }

  viz::BeginFrameArgs args;
  if (unused_args_.IsValid()) {
    // If there is cached args, use it and cache new args.
    args = unused_args_;
    unused_args_ = begin_frame_args;
  } else {
    // There is no existing cached args. Decide whether to begin frame with
    // new args or skip and cache new args.
    if (missed_composite_now_ && !is_swap_throttled_) {
      // If already missed a `MaybeCompositeNow` and can produce frame now, try
      // to catch up and begin frame immediately.
      args = begin_frame_args;
    } else {
      // Cache args and skip begin frame.
      unused_args_ = begin_frame_args;
    }
  }

  if (!args.IsValid()) {
    return;
  }
  missed_composite_now_ = false;
  BeginFrameAndResetArgs(args);
}

void DelayedScheduler::OnBeginFramePausedChanged(bool paused) {
  // If there are no more begin frames even when requested, then try to produce
  // frame with any cached args since client probably still wants begin frames.
  if (paused && unused_args_.IsValid()) {
    BeginFrameAndResetArgs(unused_args_);
  }
}

void DelayedScheduler::SetNeedsBeginFrame(bool needs_begin_frame) {
  needs_begin_frame_ = needs_begin_frame;
  if (!needs_begin_frame && unused_args_.IsValid()) {
    // Make sure there are no un-acked frame when idle.
    SendDidNotProduceFrameAndResetArgs(unused_args_);
  }
}

void DelayedScheduler::SetIsSwapThrottled(bool is_swap_throttled) {
  is_swap_throttled_ = is_swap_throttled;
  // A begin frame was missed due to being throttled, begin frame immediately
  // when no longer throttled.
  if (!is_swap_throttled && missed_composite_now_ && unused_args_.IsValid()) {
    BeginFrameAndResetArgs(unused_args_);
  }
}

void DelayedScheduler::MaybeCompositeNow() {
  // Begin frame immediately if possible, or mark a MaybeCompositeNow was
  // missed.
  if (!unused_args_.IsValid() || is_swap_throttled_) {
    missed_composite_now_ = true;
    return;
  }
  BeginFrameAndResetArgs(unused_args_);
}

void DelayedScheduler::BeginFrameAndResetArgs(viz::BeginFrameArgs& args) {
  // Make a copy and clear immediately to avoid re-entrancy issues.
  viz::BeginFrameArgs copy = args;
  args = viz::BeginFrameArgs();
  if (!client_->DoBeginFrame(copy)) {
    SendDidNotProduceFrameAndResetArgs(copy);
  }
}

void DelayedScheduler::SendDidNotProduceFrameAndResetArgs(
    viz::BeginFrameArgs& args) {
  // Make a copy and clear immediately to avoid re-entrancy issues.
  viz::BeginFrameArgs copy = args;
  args = viz::BeginFrameArgs();
  client_->SendDidNotProduceFrame(copy);
}

}  // namespace cc::slim
