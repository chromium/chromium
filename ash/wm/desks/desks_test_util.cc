// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desks_test_util.h"

#include "ash/shell.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/overview/overview_controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

DeskSwitchAnimationWaiter::DeskSwitchAnimationWaiter() {
  DesksController::Get()->AddObserver(this);
}

DeskSwitchAnimationWaiter::~DeskSwitchAnimationWaiter() {
  DesksController::Get()->RemoveObserver(this);
}

void DeskSwitchAnimationWaiter::Wait() {
  auto* controller = DesksController::Get();
  EXPECT_TRUE(controller->AreDesksBeingModified());
  run_loop_.Run();
  EXPECT_FALSE(controller->AreDesksBeingModified());
}

void DeskSwitchAnimationWaiter::OnDeskAdded(const Desk* desk) {}

void DeskSwitchAnimationWaiter::OnDeskRemoved(const Desk* desk) {}

void DeskSwitchAnimationWaiter::OnDeskActivationChanged(
    const Desk* activated,
    const Desk* deactivated) {}

void DeskSwitchAnimationWaiter::OnDeskSwitchAnimationLaunching() {}

void DeskSwitchAnimationWaiter::OnDeskSwitchAnimationFinished() {
  run_loop_.Quit();
}

void ActivateDesk(const Desk* desk) {
  ASSERT_FALSE(desk->is_active());
  DeskSwitchAnimationWaiter waiter;
  DesksController::Get()->ActivateDesk(desk,
                                       DesksSwitchSource::kMiniViewButton);
  waiter.Wait();
  ASSERT_TRUE(desk->is_active());
}

void RemoveDesk(const Desk* desk) {
  auto* controller = DesksController::Get();
  const bool in_overview =
      Shell::Get()->overview_controller()->InOverviewSession();
  const bool should_wait = controller->active_desk() == desk && !in_overview;
  DeskSwitchAnimationWaiter waiter;
  controller->RemoveDesk(desk, DesksCreationRemovalSource::kButton);
  if (should_wait)
    waiter.Wait();
}

}  // namespace ash
