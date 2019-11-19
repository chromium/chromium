// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESKS_TEST_UTIL_H_
#define ASH_WM_DESKS_DESKS_TEST_UTIL_H_

#include "ash/wm/desks/desks_controller.h"
#include "base/macros.h"
#include "base/run_loop.h"

namespace ash {

// Used for waiting for the desk switch animations on all root windows to
// complete.
class DeskSwitchAnimationWaiter : public DesksController::Observer {
 public:
  DeskSwitchAnimationWaiter();
  ~DeskSwitchAnimationWaiter() override;

  void Wait();

  // DesksController::Observer:
  void OnDeskAdded(const Desk* desk) override;
  void OnDeskRemoved(const Desk* desk) override;
  void OnDeskActivationChanged(const Desk* activated,
                               const Desk* deactivated) override;
  void OnDeskSwitchAnimationLaunching() override;
  void OnDeskSwitchAnimationFinished() override;

 private:
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(DeskSwitchAnimationWaiter);
};

// Activates the given |desk| and waits for the desk switch animation to
// complete before returning.
void ActivateDesk(const Desk* desk);

// Removes the given |desk| and waits for the desk-removal animation to finish
// if one would launch.
void RemoveDesk(const Desk* desk);

}  // namespace ash

#endif  // ASH_WM_DESKS_DESKS_TEST_UTIL_H_
