// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/dark_mode/dark_mode_feature_pod_controller.h"

#include "ash/constants/quick_settings_catalogs.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"

namespace ash {

using DarkModeFeaturePodControllerTest = AshTestBase;

// Tests that toggling dark mode from the system tray disables auto scheduling
// and switches the color mode properly.
TEST_F(DarkModeFeaturePodControllerTest, ToggleDarkMode) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(chromeos::features::kDarkLightMode);

  auto* dark_light_mode_controller = DarkLightModeControllerImpl::Get();
  dark_light_mode_controller->OnActiveUserPrefServiceChanged(
      Shell::Get()->session_controller()->GetActivePrefService());

  UnifiedSystemTray* system_tray = GetPrimaryUnifiedSystemTray();
  system_tray->ShowBubble();
  std::unique_ptr<DarkModeFeaturePodController>
      dark_mode_feature_pod_controller =
          std::make_unique<DarkModeFeaturePodController>(
              system_tray->bubble()->unified_system_tray_controller());

  std::unique_ptr<FeaturePodButton> button(
      dark_mode_feature_pod_controller->CreateButton());

  // No metrics logged before clicking on any views.
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOn",
      /*count=*/0);
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOff",
      /*count=*/0);
  histogram_tester->ExpectTotalCount("Ash.UnifiedSystemView.FeaturePod.DiveIn",
                                     /*count=*/0);

  // Enable dark mode auto scheduling.
  auto* controller = Shell::Get()->dark_light_mode_controller();
  controller->SetAutoScheduleEnabled(true);
  EXPECT_TRUE(controller->GetAutoScheduleEnabled());

  // Check that the statuses of toggle and dark mode are consistent.
  bool dark_mode_enabled = dark_light_mode_controller->IsDarkModeEnabled();
  EXPECT_EQ(dark_mode_enabled, button->IsToggled());

  // Set the init state to enabled.
  if (!dark_mode_enabled)
    dark_light_mode_controller->ToggleColorMode();

  // Pressing the dark mode button should disable the scheduling and switch the
  // dark mode status.
  dark_mode_feature_pod_controller->OnIconPressed();
  EXPECT_FALSE(controller->GetAutoScheduleEnabled());
  EXPECT_EQ(false, dark_light_mode_controller->IsDarkModeEnabled());
  EXPECT_EQ(false, button->IsToggled());
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOn",
      /*count=*/0);
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOff",
      /*count=*/1);
  histogram_tester->ExpectTotalCount("Ash.UnifiedSystemView.FeaturePod.DiveIn",
                                     /*count=*/0);
  histogram_tester->ExpectBucketCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOff",
      QsFeatureCatalogName::kDarkMode,
      /*expected_count=*/1);

  // Pressing the dark mode button again should only switch the dark mode status
  // while maintaining the disabled status of scheduling.
  dark_mode_feature_pod_controller->OnIconPressed();
  EXPECT_FALSE(controller->GetAutoScheduleEnabled());
  EXPECT_EQ(true, dark_light_mode_controller->IsDarkModeEnabled());
  EXPECT_EQ(true, button->IsToggled());
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOn",
      /*count=*/1);
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOff",
      /*count=*/1);
  histogram_tester->ExpectTotalCount("Ash.UnifiedSystemView.FeaturePod.DiveIn",
                                     /*count=*/0);
  histogram_tester->ExpectBucketCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOn",
      QsFeatureCatalogName::kDarkMode,
      /*expected_count=*/1);

  dark_mode_feature_pod_controller->OnLabelPressed();
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOn",
      /*count=*/1);
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOff",
      /*count=*/1);
  histogram_tester->ExpectTotalCount("Ash.UnifiedSystemView.FeaturePod.DiveIn",
                                     /*count=*/1);
  histogram_tester->ExpectBucketCount("Ash.UnifiedSystemView.FeaturePod.DiveIn",
                                      QsFeatureCatalogName::kDarkMode,
                                      /*expected_count=*/1);

  system_tray->CloseBubble();
}

}  // namespace ash
