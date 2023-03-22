// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SLIM_SIMPLE_SCHEDULER_H_
#define CC_SLIM_SIMPLE_SCHEDULER_H_

#include "base/component_export.h"
#include "cc/slim/scheduler.h"

namespace cc::slim {

// Simplest scheduler implementation where it tries to produce immediately in
// OnBeginFrameFromViz and send DidNotProduceFrame immediately.
class COMPONENT_EXPORT(CC_SLIM) SimpleScheduler : public Scheduler {
 public:
  SimpleScheduler();
  ~SimpleScheduler() override;

  void Initialize(SchedulerClient* client) override;
  void OnBeginFrameFromViz(
      const viz::BeginFrameArgs& begin_frame_args) override;

 private:
  SchedulerClient* client_ = nullptr;
};

}  // namespace cc::slim

#endif  // CC_SLIM_SIMPLE_SCHEDULER_H_
