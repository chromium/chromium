// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/dark_mode/dark_mode_feature_pod_controller.h"

#include "ash/constants/quick_settings_catalogs.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"

namespace ash {

class DarkModeFeaturePodControllerTest : public AshTestBase {
 public:
  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    system_tray_ = GetPrimaryUnifiedSystemTray();
    system_tray_->ShowBubble();
    feature_pod_controller_ = std::make_unique<DarkModeFeaturePodController>(
        system_tray_->bubble()->unified_system_tray_controller());
  }

  void TearDown() override {
    tile_.reset();
    feature_pod_controller_.reset();
    system_tray_->CloseBubble();
    AshTestBase::TearDown();
  }

  void CreateButton() { tile_ = feature_pod_controller_->CreateTile(); }

  bool IsButtonVisible() { return tile_->GetVisible(); }

  bool IsButtonToggled() { return tile_->IsToggled(); }

  void PressIcon() { feature_pod_controller_->OnIconPressed(); }

  void PressLabel() { feature_pod_controller_->OnLabelPressed(); }

 private:
  std::unique_ptr<DarkModeFeaturePodController> feature_pod_controller_;
  std::unique_ptr<FeatureTile> tile_;
  raw_ptr<UnifiedSystemTray, DanglingUntriaged> system_tray_ = nullptr;
};

// Tests that toggling dark mode from the system tray disables auto scheduling
// and switches the color mode properly.
TEST_F(DarkModeFeaturePodControllerTest, ToggleDarkMode) {
  CreateButton();
  EXPECT_TRUE(IsButtonVisible());

  auto* dark_light_mode_controller = DarkLightModeControllerImpl::Get();
  dark_light_mode_controller->OnActiveUserPrefServiceChanged(
      Shell::Get()->session_controller()->GetActivePrefService());

  // No metrics logged before clicking on any views.
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  histogram_tester->ExpectTotalCount("Ash.QuickSettings.FeaturePod.ToggledOn",
                                     /*expected_count=*/0);
  histogram_tester->ExpectTotalCount("Ash.QuickSettings.FeaturePod.ToggledOff",
                                     /*expected_count=*/0);
  histogram_tester->ExpectTotalCount("Ash.QuickSettings.FeaturePod.DiveIn",
                                     /*expected_count=*/0);

  // Enable dark mode auto scheduling.
  auto* controller = Shell::Get()->dark_light_mode_controller();
  controller->SetAutoScheduleEnabled(true);
  EXPECT_TRUE(controller->GetAutoScheduleEnabled());

  // Check that the statuses of toggle and dark mode are consistent.
  bool dark_mode_enabled = dark_light_mode_controller->IsDarkModeEnabled();
  EXPECT_EQ(dark_mode_enabled, IsButtonToggled());

  // Set the init state to enabled.
  if (!dark_mode_enabled) {
    dark_light_mode_controller->ToggleColorMode();
  }

  // Pressing the dark mode button should disable the scheduling and switch the
  // dark mode status.
  PressIcon();
  EXPECT_FALSE(controller->GetAutoScheduleEnabled());
  EXPECT_EQ(false, dark_light_mode_controller->IsDarkModeEnabled());
  EXPECT_EQ(false, IsButtonToggled());

  histogram_tester->ExpectTotalCount("Ash.QuickSettings.FeaturePod.ToggledOn",
                                     /*expected_count=*/0);
  histogram_tester->ExpectTotalCount("Ash.QuickSettings.FeaturePod.ToggledOff",
                                     /*expected_count=*/1);
  histogram_tester->ExpectTotalCount("Ash.QuickSettings.FeaturePod.DiveIn",
                                     /*expected_count=*/0);
  histogram_tester->ExpectBucketCount("Ash.QuickSettings.FeaturePod.ToggledOff",
                                      QsFeatureCatalogName::kDarkMode,
                                      /*expected_count=*/1);

  // Pressing the dark mode button again should only switch the dark mode
  // status while maintaining the disabled status of scheduling.
  PressIcon();
  EXPECT_FALSE(controller->GetAutoScheduleEnabled());
  EXPECT_EQ(true, dark_light_mode_controller->IsDarkModeEnabled());
  EXPECT_EQ(true, IsButtonToggled());

  histogram_tester->ExpectTotalCount("Ash.QuickSettings.FeaturePod.ToggledOn",
                                     /*expected_count=*/1);
  histogram_tester->ExpectTotalCount("Ash.QuickSettings.FeaturePod.ToggledOff",
                                     /*expected_count=*/1);
  histogram_tester->ExpectTotalCount("Ash.QuickSettings.FeaturePod.DiveIn",
                                     /*expected_count=*/0);
  histogram_tester->ExpectBucketCount("Ash.QuickSettings.FeaturePod.ToggledOn",
                                      QsFeatureCatalogName::kDarkMode,
                                      /*expected_count=*/1);

  PressLabel();
  histogram_tester->ExpectTotalCount("Ash.QuickSettings.FeaturePod.ToggledOn",
                                     /*expected_count=*/1);
  histogram_tester->ExpectTotalCount("Ash.QuickSettings.FeaturePod.ToggledOff",
                                     /*expected_count=*/1);
  histogram_tester->ExpectTotalCount("Ash.QuickSettings.FeaturePod.DiveIn",
                                     /*expected_count=*/0);
  histogram_tester->ExpectBucketCount("Ash.QuickSettings.FeaturePod.DiveIn",
                                      QsFeatureCatalogName::kDarkMode,
                                      /*expected_count=*/0);
}

}  // namespace ash
