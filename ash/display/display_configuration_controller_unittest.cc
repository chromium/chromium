// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_configuration_controller.h"

#include "ash/display/display_configuration_controller_test_api.h"
#include "ash/rotator/screen_rotation_animator.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "base/command_line.h"
#include "ui/display/manager/display_manager.h"

namespace ash {
namespace {

display::Display::Rotation GetDisplayRotation(int64_t display_id) {
  return Shell::Get()
      ->display_manager()
      ->GetDisplayInfo(display_id)
      .GetActiveRotation();
}

class DisplayConfigurationControllerSmoothRotationTest : public AshTestBase {
 public:
  DisplayConfigurationControllerSmoothRotationTest() = default;

  DisplayConfigurationControllerSmoothRotationTest(
      const DisplayConfigurationControllerSmoothRotationTest&) = delete;
  DisplayConfigurationControllerSmoothRotationTest& operator=(
      const DisplayConfigurationControllerSmoothRotationTest&) = delete;

  ~DisplayConfigurationControllerSmoothRotationTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    // ScreenRotionAnimator skips animation if the wallpaper isn't ready.
    Shell::Get()->wallpaper_controller()->set_bypass_decode_for_testing();
    Shell::Get()->wallpaper_controller()->ShowDefaultWallpaperForTesting();
  }
};

}  // namespace

using DisplayConfigurationControllerTest = AshTestBase;

TEST_F(DisplayConfigurationControllerTest, OnlyHasOneAnimator) {
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  DisplayConfigurationControllerTestApi testapi(
      Shell::Get()->display_configuration_controller());
  ScreenRotationAnimator* old_screen_rotation_animator =
      testapi.GetScreenRotationAnimatorForDisplay(display.id());

  Shell::Get()->display_manager()->SetDisplayRotation(
      display.id(), display::Display::ROTATE_0,
      display::Display::RotationSource::USER);
  old_screen_rotation_animator->Rotate(
      display::Display::ROTATE_90, display::Display::RotationSource::USER,
      DisplayConfigurationController::ANIMATION_SYNC);

  ScreenRotationAnimator* new_screen_rotation_animator =
      testapi.GetScreenRotationAnimatorForDisplay(display.id());
  new_screen_rotation_animator->Rotate(
      display::Display::ROTATE_180, display::Display::RotationSource::USER,
      DisplayConfigurationController::ANIMATION_SYNC);
  EXPECT_EQ(old_screen_rotation_animator, new_screen_rotation_animator);
}

TEST_F(DisplayConfigurationControllerTest, GetTargetRotationWithAnimation) {
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  DisplayConfigurationController* controller =
      Shell::Get()->display_configuration_controller();
  DisplayConfigurationControllerTestApi(controller).SetDisplayAnimator(true);
  controller->SetDisplayRotation(
      display.id(), display::Display::ROTATE_180,
      display::Display::RotationSource::USER,
      DisplayConfigurationController::ANIMATION_SYNC);
  EXPECT_EQ(display::Display::ROTATE_180,
            controller->GetTargetRotation(display.id()));
  EXPECT_EQ(display::Display::ROTATE_180, GetDisplayRotation(display.id()));
}

TEST_F(DisplayConfigurationControllerSmoothRotationTest,
       GetTargetRotationWithAnimation) {
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  DisplayConfigurationController* controller =
      Shell::Get()->display_configuration_controller();
  DisplayConfigurationControllerTestApi(controller).SetDisplayAnimator(true);
  controller->SetDisplayRotation(
      display.id(), display::Display::ROTATE_180,
      display::Display::RotationSource::USER,
      DisplayConfigurationController::ANIMATION_ASYNC);
  EXPECT_EQ(display::Display::ROTATE_180,
            controller->GetTargetRotation(display.id()));
  EXPECT_EQ(display::Display::ROTATE_0, GetDisplayRotation(display.id()));
}

}  // namespace ash
