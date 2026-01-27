// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCHEDULER_HEADLESS_SCHEDULER_STATE_MACHINE_H_
#define CC_SCHEDULER_HEADLESS_SCHEDULER_STATE_MACHINE_H_

#include "cc/cc_export.h"
#include "cc/scheduler/scheduler_state_machine.h"

namespace cc {

class CC_EXPORT HeadlessSchedulerStateMachine : public SchedulerStateMachine {
 public:
  explicit HeadlessSchedulerStateMachine(const SchedulerSettings& settings);
  ~HeadlessSchedulerStateMachine() override;

  bool CheckShouldDraw() const override;
  bool ShouldPrepareTiles() const override;
  bool ShouldSubscribeToBeginFrames() const override;
  // Avoids early exit when determining if we shouldn't schedule a deadline.
  // Used to defer drawing until the entire pipeline is flushed and active tree
  // is ready to draw for headless mode.
  bool CheckShouldBlockDeadlineIndefinitely() const override;
  bool ShouldTriggerBeginImplFrameDeadlineImmediately() const override;
  bool CheckWillCommit() const override;
};

}  // namespace cc

#endif  // CC_SCHEDULER_HEADLESS_SCHEDULER_STATE_MACHINE_H_
