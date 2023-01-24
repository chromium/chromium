// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_multitask_cue.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/tablet_mode/tablet_mode_multitask_menu_event_handler.h"
#include "ash/wm/tablet_mode/tablet_mode_window_manager.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ui/wm/features.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/layer_animation_stopped_waiter.h"

namespace ash {

namespace {

constexpr int kCueYOffset = 6;
constexpr int kCueWidth = 48;
constexpr int kCueHeight = 4;

}  // namespace

class TabletModeMultitaskCueTest : public AshTestBase {
 public:
  TabletModeMultitaskCueTest()
      : scoped_feature_list_(chromeos::wm::features::kFloatWindow) {}
  TabletModeMultitaskCueTest(const TabletModeMultitaskCueTest&) = delete;
  TabletModeMultitaskCueTest& operator=(const TabletModeMultitaskCueTest&) =
      delete;
  ~TabletModeMultitaskCueTest() override = default;

  TabletModeMultitaskCue* GetMultitaskCue() {
    return TabletModeControllerTestApi()
        .tablet_mode_window_manager()
        ->tablet_mode_multitask_menu_event_handler_for_testing()
        ->multitask_cue_for_testing();
  }

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    TabletModeControllerTestApi().EnterTabletMode();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the cue layer is created properly.
TEST_F(TabletModeMultitaskCueTest, BasicShowCue) {
  auto window = CreateAppWindow();
  gfx::Rect window_bounds = window->bounds();

  auto* multitask_cue = GetMultitaskCue();
  ASSERT_TRUE(multitask_cue);

  ui::Layer* cue_layer = multitask_cue->cue_layer_for_testing();
  ASSERT_TRUE(cue_layer);

  EXPECT_EQ(gfx::Rect((window_bounds.width() - kCueWidth) / 2, kCueYOffset,
                      kCueWidth, kCueHeight),
            cue_layer->bounds());
}

// Tests that the cue bounds are updated properly after a window is split.
TEST_F(TabletModeMultitaskCueTest, SplitCueBounds) {
  auto* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());

  auto window1 = CreateAppWindow();

  split_view_controller->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);

  gfx::Rect split_bounds((window1->bounds().width() - kCueWidth) / 2,
                         kCueYOffset, kCueWidth, kCueHeight);

  ui::Layer* cue_layer = GetMultitaskCue()->cue_layer_for_testing();
  ASSERT_TRUE(cue_layer);
  EXPECT_EQ(cue_layer->bounds(), split_bounds);

  auto window2 = CreateAppWindow();
  split_view_controller->SnapWindow(
      window2.get(), SplitViewController::SnapPosition::kSecondary);

  cue_layer = GetMultitaskCue()->cue_layer_for_testing();
  ASSERT_TRUE(cue_layer);
  EXPECT_EQ(cue_layer->bounds(), split_bounds);
}

// Tests that the `OneShotTimer` properly dismisses the cue after firing.
TEST_F(TabletModeMultitaskCueTest, DismissTimerFiring) {
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  auto window = CreateAppWindow();

  auto* multitask_cue = GetMultitaskCue();
  ui::Layer* cue_layer = multitask_cue->cue_layer_for_testing();
  ASSERT_TRUE(cue_layer);

  // Wait for fade in to finish.
  ui::LayerAnimationStoppedWaiter animation_waiter;
  animation_waiter.Wait(cue_layer);

  multitask_cue->FireCueDismissTimerForTesting();

  // Wait for fade out to finish.
  animation_waiter.Wait(cue_layer);
  EXPECT_FALSE(multitask_cue->cue_layer_for_testing());
}

// Tests that the cue dismisses properly during the fade out animation.
TEST_F(TabletModeMultitaskCueTest, DismissEarly) {
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  auto window = CreateAppWindow();

  auto* multitask_cue = GetMultitaskCue();
  ui::Layer* cue_layer = multitask_cue->cue_layer_for_testing();
  ASSERT_TRUE(cue_layer);

  // Wait for fade in to finish.
  ui::LayerAnimationStoppedWaiter().Wait(cue_layer);

  multitask_cue->FireCueDismissTimerForTesting();
  multitask_cue->DismissCue();
  EXPECT_FALSE(multitask_cue->cue_layer_for_testing());
}

// Tests that the cue dismisses properly when the float keyboard accelerator is
// pressed.
TEST_F(TabletModeMultitaskCueTest, FloatWindow) {
  auto window = CreateAppWindow();

  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);

  auto* multitask_cue = GetMultitaskCue();
  ASSERT_TRUE(multitask_cue);
  EXPECT_FALSE(multitask_cue->cue_layer_for_testing());
}

}  // namespace ash