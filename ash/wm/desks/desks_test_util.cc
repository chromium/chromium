// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desks_test_util.h"

#include "ash/shell.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desk_animation_base.h"
#include "ash/wm/desks/desk_animation_impl.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/desks/desks_histogram_enums.h"
#include "ash/wm/desks/root_window_desk_switch_animator_test_api.h"
#include "ash/wm/gestures/wm_gesture_handler.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/test/event_generator.h"

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

void DeskSwitchAnimationWaiter::OnDeskReordered(int old_index, int new_index) {}

void DeskSwitchAnimationWaiter::OnDeskActivationChanged(
    const Desk* activated,
    const Desk* deactivated) {}

void DeskSwitchAnimationWaiter::OnDeskSwitchAnimationLaunching() {}

void DeskSwitchAnimationWaiter::OnDeskSwitchAnimationFinished() {
  run_loop_.Quit();
}

void DeskSwitchAnimationWaiter::OnDeskNameChanged(
    const Desk* desk,
    const std::u16string& new_name) {}

void ActivateDesk(const Desk* desk) {
  ASSERT_FALSE(desk->is_active());
  DeskSwitchAnimationWaiter waiter;
  auto* desks_controller = DesksController::Get();
  desks_controller->ActivateDesk(desk, DesksSwitchSource::kMiniViewButton);
  EXPECT_EQ(desk, desks_controller->GetTargetActiveDesk());
  waiter.Wait();
  ASSERT_TRUE(desk->is_active());
}

void NewDesk() {
  // Do not use `kButton` to avoid empty name.
  DesksController::Get()->NewDesk(DesksCreationRemovalSource::kKeyboard);
}

void RemoveDesk(const Desk* desk, DeskCloseType close_type) {
  auto* controller = DesksController::Get();
  const bool in_overview =
      Shell::Get()->overview_controller()->InOverviewSession();
  const bool should_wait = controller->active_desk() == desk && !in_overview;
  DeskSwitchAnimationWaiter waiter;
  controller->RemoveDesk(desk, DesksCreationRemovalSource::kButton, close_type);
  if (should_wait)
    waiter.Wait();
}

const Desk* GetActiveDesk() {
  return DesksController::Get()->active_desk();
}

const Desk* GetNextDesk() {
  return DesksController::Get()->GetNextDesk();
}

void ScrollToSwitchDesks(bool scroll_left,
                         ui::test::EventGenerator* event_generator) {
  // Scrolling to switch desks with enhanced desk animations is a bit tricky
  // because it involves multiple async operations.
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Start off with a fling cancel (touchpad start) to start the touchpad
  // swipe sequence.
  base::TimeTicks timestamp = ui::EventTimeForNow();
  ui::ScrollEvent fling_cancel(ui::ET_SCROLL_FLING_CANCEL, gfx::Point(),
                               timestamp, 0, 0, 0, 0, 0,
                               kNumFingersForDesksSwitch);
  event_generator->Dispatch(&fling_cancel);

  // Continue with a large enough scroll to start the desk switch animation.
  // The animation does not start on fling cancel since there is no finger
  // data in production code.
  const base::TimeDelta step_delay = base::Milliseconds(5);
  timestamp += step_delay;
  const int direction = scroll_left ? -1 : 1;
  const int initial_move_x =
      (WmGestureHandler::kContinuousGestureMoveThresholdDp + 5) * direction;
  ui::ScrollEvent initial_move(ui::ET_SCROLL, gfx::Point(), timestamp, 0,
                               initial_move_x, 0, initial_move_x, 0,
                               kNumFingersForDesksSwitch);
  event_generator->Dispatch(&initial_move);

  // Wait for animation if applicable.
  auto* animation = DesksController::Get()->animation();
  if (animation) {
    // Wait until the animations ending screenshot has been taken. Otherwise,
    // we will just stay at the initial desk if no screenshot has been taken.
    auto* desk_switch_animator =
        animation->GetDeskSwitchAnimatorAtIndexForTesting(0);
    base::RunLoop run_loop;
    RootWindowDeskSwitchAnimatorTestApi(desk_switch_animator)
        .SetOnEndingScreenshotTakenCallback(run_loop.QuitClosure());
    run_loop.Run();

    // Send some more move events, enough to shift to the next desk.
    const int steps = 100;
    const float x_offset = direction * WmGestureHandler::kHorizontalThresholdDp;
    float dx = x_offset / steps;
    for (int i = 0; i < steps; ++i) {
      timestamp += step_delay;
      ui::ScrollEvent move(ui::ET_SCROLL, gfx::Point(), timestamp, 0, dx, 0, dx,
                           0, kNumFingersForDesksSwitch);
      event_generator->Dispatch(&move);
    }

    // End the swipe and wait for the animation to finish.
    ui::ScrollEvent fling_start(ui::ET_SCROLL_FLING_START, gfx::Point(),
                                timestamp, 0, x_offset, 0, x_offset, 0,
                                kNumFingersForDesksSwitch);
    DeskSwitchAnimationWaiter animation_finished_waiter;
    event_generator->Dispatch(&fling_start);
    animation_finished_waiter.Wait();
  }
}

void WaitUntilEndingScreenshotTaken(DeskActivationAnimation* animation) {
  base::RunLoop run_loop;
  auto* desk_switch_animator =
      animation->GetDeskSwitchAnimatorAtIndexForTesting(0);
  RootWindowDeskSwitchAnimatorTestApi(desk_switch_animator)
      .SetOnEndingScreenshotTakenCallback(run_loop.QuitClosure());
  run_loop.Run();
}

const DesksBarView* GetPrimaryRootDesksBarView() {
  auto* root_window = Shell::GetPrimaryRootWindow();
  auto* overview_controller = Shell::Get()->overview_controller();
  DCHECK(overview_controller->InOverviewSession());
  return overview_controller->overview_session()
      ->GetGridWithRootWindow(root_window)
      ->desks_bar_view();
}

}  // namespace ash
