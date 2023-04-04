// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SLIM_SCHEDULER_H_
#define CC_SLIM_SCHEDULER_H_

#include "components/viz/common/frame_sinks/begin_frame_args.h"

namespace cc::slim {

// Implemented by slim compositor for Scheduler implementations to call.
class SchedulerClient {
 public:
  virtual ~SchedulerClient() = default;

  // Produce frame. If returns false, it's safe to call DoBeginFrame again, or
  // call SendDidNotProduceFrame with the same args.
  virtual bool DoBeginFrame(const viz::BeginFrameArgs& begin_frame_args) = 0;

  // After calling this, the same BeginFrameArgs cannot be used to call
  // DoBeginFrame again.
  virtual void SendDidNotProduceFrame(
      const viz::BeginFrameArgs& begin_frame_args) = 0;
};

// Scheduler class controls timing of slim compositor frame production. In
// particular, it controls when compositor should respond a OnBeginFrame from
// viz with SubmitCompositorFrame or DidNotProduceFrame.
class Scheduler {
 public:
  virtual ~Scheduler() = default;

  // First method to be called to set the client.
  virtual void Initialize(SchedulerClient* client) = 0;

  // Viz called OnBeginFrame with new BeginFrameArgs.
  virtual void OnBeginFrameFromViz(
      const viz::BeginFrameArgs& begin_frame_args) = 0;
};

}  // namespace cc::slim

#endif  // CC_SLIM_SCHEDULER_H_
