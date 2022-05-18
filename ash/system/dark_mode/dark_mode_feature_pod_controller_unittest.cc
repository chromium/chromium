// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/dark_mode/dark_mode_feature_pod_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/dark_mode_controller.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"

namespace ash {

using DarkModeFeaturePodControllerTest = AshTestBase;

// Tests that toggling dark mode from the system tray disables auto scheduling
// and switches the color mode properly.
TEST_F(DarkModeFeaturePodControllerTest, ToggleDarkMode) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(chromeos::features::kDarkLightMode);

  AshColorProvider* provider = AshColorProvider::Get();
  provider->OnActiveUserPrefServiceChanged(
      Shell::Get()->session_controller()->GetActivePrefService());

  UnifiedSystemTray* system_tray = GetPrimaryUnifiedSystemTray();
  system_tray->ShowBubble();
  std::unique_ptr<DarkModeFeaturePodController>
      dark_mode_feature_pod_controller =
          std::make_unique<DarkModeFeaturePodController>(
              system_tray->bubble()->unified_system_tray_controller());

  std::unique_ptr<FeaturePodButton> button(
      dark_mode_feature_pod_controller->CreateButton());

  // Enable dark mode auto scheduling.
  DarkModeController* controller = Shell::Get()->dark_mode_controller();
  controller->SetAutoScheduleEnabled(true);
  EXPECT_TRUE(controller->GetAutoScheduleEnabled());

  // Check that the statuses of toggle and dark mode are consistent.
  bool dark_mode_enabled = provider->IsDarkModeEnabled();
  EXPECT_EQ(dark_mode_enabled, button->IsToggled());

  // Pressing the dark mode button should disable the scheduling and switch the
  // dark mode status.
  dark_mode_feature_pod_controller->OnIconPressed();
  EXPECT_FALSE(controller->GetAutoScheduleEnabled());
  EXPECT_EQ(!dark_mode_enabled, provider->IsDarkModeEnabled());
  EXPECT_EQ(!dark_mode_enabled, button->IsToggled());

  // Pressing the dark mode button again should only switch the dark mode status
  // while maintaining the disabled status of scheduling.
  dark_mode_feature_pod_controller->OnIconPressed();
  EXPECT_FALSE(controller->GetAutoScheduleEnabled());
  EXPECT_EQ(dark_mode_enabled, provider->IsDarkModeEnabled());
  EXPECT_EQ(dark_mode_enabled, button->IsToggled());
  system_tray->CloseBubble();
}

}  // namespace ash
