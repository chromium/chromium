// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_animation_shield_controller.h"

#include "ash/ambient/test/ambient_ash_test_base.h"
#include "ash/ambient/ui/ambient_view_ids.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "base/logging.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/view.h"

namespace ash {
namespace {

using ::testing::IsNull;
using ::testing::NotNull;

}  // namespace

class AmbientAnimationShieldControllerTest : public AmbientAshTestBase {
 protected:
  void SetDarkModeEnabled(bool dark_mode_enabled) {
    auto* dark_light_mode_controller = DarkLightModeControllerImpl::Get();
    CHECK(dark_light_mode_controller);
    if (dark_light_mode_controller->IsDarkModeEnabled() != dark_mode_enabled)
      dark_light_mode_controller->ToggleColorMode();
  }

  std::unique_ptr<views::View> CreateShieldView() {
    auto shield_view = std::make_unique<views::View>();
    shield_view->SetID(kAmbientShieldView);
    return shield_view;
  }

  views::View parent_view_;
};

TEST_F(AmbientAnimationShieldControllerTest, InitialDarkMode) {
  SetDarkModeEnabled(true);
  AmbientAnimationShieldController controller(CreateShieldView(),
                                              &parent_view_);
  EXPECT_THAT(parent_view_.GetViewByID(kAmbientShieldView), NotNull());
}

TEST_F(AmbientAnimationShieldControllerTest, InitialLightMode) {
  SetDarkModeEnabled(false);
  AmbientAnimationShieldController controller(CreateShieldView(),
                                              &parent_view_);
  EXPECT_THAT(parent_view_.GetViewByID(kAmbientShieldView), IsNull());
}

TEST_F(AmbientAnimationShieldControllerTest, TogglesShield) {
  SetDarkModeEnabled(true);
  AmbientAnimationShieldController controller(CreateShieldView(),
                                              &parent_view_);
  SetDarkModeEnabled(false);
  EXPECT_THAT(parent_view_.GetViewByID(kAmbientShieldView), IsNull());
  SetDarkModeEnabled(true);
  EXPECT_THAT(parent_view_.GetViewByID(kAmbientShieldView), NotNull());
}

}  // namespace ash
