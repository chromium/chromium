// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCHEDULER_SLIM_SCHEDULER_STATE_MACHINE_H_
#define CC_SCHEDULER_SLIM_SCHEDULER_STATE_MACHINE_H_

#include <set>

#include "cc/cc_export.h"
#include "cc/scheduler/scheduler_state_machine.h"

namespace cc {

// This state machine's supports:
//    - Prevents re-entrancy during a VSync Interval. We will no longer repeat
//    an `Action`
//    - Removed BeginImplFrameDeadlineMode::REGULAR and LATE. As these modes
//    lead to the next VSync being IMMEDIATE. This can result in two frame
//    submissions within less than a VSync interval. This leads to dropped
//    frames or exhaustion of frame buffers, which pauses rendering.
//    - Restricts different `Action` to different `BeginImplFrameState`. So that
//    we only attempt to Commit and Activate after submitting the current frame,
//    when we are IDLE.
//    - Allows beginning `BeginMainFrame` while idle to begin preparing the next
//    frame
//    - Allows `PrepareTiles` while idle, so that this doesn't slow down frame
//    submission.
class CC_EXPORT SlimSchedulerStateMachine : public SchedulerStateMachine {
 public:
  explicit SlimSchedulerStateMachine(const SchedulerSettings& settings);
  ~SlimSchedulerStateMachine() override;

  Action NextAction() const override;
  BeginImplFrameDeadlineMode CurrentBeginImplFrameDeadlineMode() const override;
  void OnBeginImplFrame(const viz::BeginFrameArgs& args) override;
  bool ShouldBlockBeginMainFrameWhenIdle() const override;

 private:
  mutable std::set<Action> issued_actions_;
};

}  // namespace cc

#endif  // CC_SCHEDULER_SLIM_SCHEDULER_STATE_MACHINE_H_
