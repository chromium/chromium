// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_multitask_cue_controller.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/tablet_mode/tablet_mode_multitask_menu_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_window_manager.h"
#include "base/functional/bind.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/layer_animation_stopped_waiter.h"
#include "ui/wm/core/window_util.h"

namespace ash {

class TabletModeMultitaskCueControllerTest : public AshTestBase {
 public:
  TabletModeMultitaskCueControllerTest() = default;
  TabletModeMultitaskCueControllerTest(
      const TabletModeMultitaskCueControllerTest&) = delete;
  TabletModeMultitaskCueControllerTest& operator=(
      const TabletModeMultitaskCueControllerTest&) = delete;
  ~TabletModeMultitaskCueControllerTest() override = default;

  TabletModeMultitaskCueController* GetMultitaskCue() {
    return TabletModeControllerTestApi()
        .tablet_mode_window_manager()
        ->tablet_mode_multitask_menu_controller()
        ->multitask_cue_controller();
  }

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    TabletModeControllerTestApi().EnterTabletMode();
  }
};

// Tests that the cue layer is created properly.
TEST_F(TabletModeMultitaskCueControllerTest, BasicShowCue) {
  auto window = CreateAppWindow();
  gfx::Rect window_bounds = window->bounds();

  auto* multitask_cue_controller = GetMultitaskCue();
  ASSERT_TRUE(multitask_cue_controller);

  ui::Layer* cue_layer = multitask_cue_controller->cue_layer();
  ASSERT_TRUE(cue_layer);

  EXPECT_EQ(gfx::Rect((window_bounds.width() -
                       TabletModeMultitaskCueController::kCueWidth) /
                          2,
                      TabletModeMultitaskCueController::kCueYOffset,
                      TabletModeMultitaskCueController::kCueWidth,
                      TabletModeMultitaskCueController::kCueHeight),
            cue_layer->bounds());
}

// Tests that the cue bounds are updated properly after a window is split.
TEST_F(TabletModeMultitaskCueControllerTest, SplitCueBounds) {
  auto* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());

  auto window1 = CreateAppWindow();

  split_view_controller->SnapWindow(window1.get(), SnapPosition::kPrimary);

  gfx::Rect split_bounds((window1->bounds().width() -
                          TabletModeMultitaskCueController::kCueWidth) /
                             2,
                         TabletModeMultitaskCueController::kCueYOffset,
                         TabletModeMultitaskCueController::kCueWidth,
                         TabletModeMultitaskCueController::kCueHeight);

  ui::Layer* cue_layer = GetMultitaskCue()->cue_layer();
  ASSERT_TRUE(cue_layer);
  EXPECT_EQ(cue_layer->bounds(), split_bounds);

  auto window2 = CreateAppWindow();
  split_view_controller->SnapWindow(window2.get(), SnapPosition::kSecondary);

  cue_layer = GetMultitaskCue()->cue_layer();
  ASSERT_TRUE(cue_layer);
  EXPECT_EQ(cue_layer->bounds(), split_bounds);
}

// Tests that the `OneShotTimer` properly dismisses the cue after firing.
TEST_F(TabletModeMultitaskCueControllerTest, DismissTimerFiring) {
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  auto window = CreateAppWindow();

  auto* multitask_cue_controller = GetMultitaskCue();
  ui::Layer* cue_layer = multitask_cue_controller->cue_layer();
  ASSERT_TRUE(cue_layer);

  // Wait for fade in to finish.
  ui::LayerAnimationStoppedWaiter animation_waiter;
  animation_waiter.Wait(cue_layer);

  multitask_cue_controller->FireCueDismissTimerForTesting();

  // Wait for fade out to finish.
  animation_waiter.Wait(cue_layer);
  EXPECT_FALSE(multitask_cue_controller->cue_layer());
}

// Tests that the cue dismisses properly during the fade out animation.
TEST_F(TabletModeMultitaskCueControllerTest, DismissEarly) {
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  auto window = CreateAppWindow();

  auto* multitask_cue_controller = GetMultitaskCue();
  ui::Layer* cue_layer = multitask_cue_controller->cue_layer();
  ASSERT_TRUE(cue_layer);

  // Wait for fade in to finish.
  ui::LayerAnimationStoppedWaiter().Wait(cue_layer);

  multitask_cue_controller->FireCueDismissTimerForTesting();
  multitask_cue_controller->DismissCue();
  EXPECT_FALSE(multitask_cue_controller->cue_layer());
}

// Tests that the cue dismisses properly when the float keyboard accelerator is
// pressed.
TEST_F(TabletModeMultitaskCueControllerTest, FloatWindow) {
  auto window = CreateAppWindow();

  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);

  auto* multitask_cue_controller = GetMultitaskCue();
  ASSERT_TRUE(multitask_cue_controller);
  EXPECT_FALSE(multitask_cue_controller->cue_layer());
}

TEST_F(TabletModeMultitaskCueControllerTest, TransientChildFocus) {
  auto window1 = CreateAppWindow();

  // Create a second window with a transient child.
  auto window2 = CreateAppWindow();
  auto transient_child2 =
      CreateTestWindow(gfx::Rect(100, 10), aura::client::WINDOW_TYPE_POPUP);
  wm::AddTransientChild(window2.get(), transient_child2.get());
  wm::ActivateWindow(transient_child2.get());

  // Creating an app window shows the cue. Hide it before testing.
  auto* multitask_cue_controller = GetMultitaskCue();
  ASSERT_TRUE(multitask_cue_controller->cue_layer());
  multitask_cue_controller->DismissCue();

  // Activate `window2`. The cue should not show up, since the window with
  // previous activation was a transient child.
  wm::ActivateWindow(window2.get());
  EXPECT_FALSE(multitask_cue_controller->cue_layer());

  // Reactivate the transient. The cue should not show up, since the transient
  // window is a popup, and cannot change window states.
  wm::ActivateWindow(transient_child2.get());
  EXPECT_FALSE(multitask_cue_controller->cue_layer());

  // Activate `window1`. The cue should show up, since the previous activated
  // window was not associated with it.
  wm::ActivateWindow(window1.get());
  EXPECT_TRUE(multitask_cue_controller->cue_layer());
}

TEST_F(TabletModeMultitaskCueControllerTest,
       CueDoesNotShowOnClamshellTransition) {
  auto* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());

  auto window1 = CreateAppWindow();

  // Dismiss the cue so it can (attempt to) be shown again later.
  auto* multitask_cue_controller = GetMultitaskCue();

  multitask_cue_controller->DismissCue();

  // Window must be split so overview mode is active on the opposite side.
  split_view_controller->SnapWindow(window1.get(), SnapPosition::kPrimary);

  multitask_cue_controller->set_pre_cue_shown_callback_for_test(
      base::BindOnce([]() { ASSERT_TRUE(false); }));

  // When we go back to clamshell mode, overview mode will shutdown and try to
  // restore activation to the window, and therefore call `MaybeShowCue()`. If
  // it passes all checks, then it will run the callback and immediately fail
  // this test.
  TabletModeControllerTestApi().LeaveTabletMode();
}

}  // namespace ash
