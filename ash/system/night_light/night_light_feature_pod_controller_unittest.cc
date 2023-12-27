// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/night_light/night_light_feature_pod_controller.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/public/cpp/schedule_enums.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/clock_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/night_light/night_light_controller_impl.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/time_of_day_test_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

class NightLightFeaturePodControllerTest : public AshTestBase {
 public:
  NightLightFeaturePodControllerTest() = default;

  void SetUp() override {
    AshTestBase::SetUp();
    system_tray_ = GetPrimaryUnifiedSystemTray();
    system_tray_->ShowBubble();
  }

  void TearDown() override {
    tile_.reset();
    controller_.reset();
    system_tray_->CloseBubble();
    AshTestBase::TearDown();
  }

  void CreateButton() {
    if (!system_tray_->IsBubbleShown()) {
      system_tray_->ShowBubble();
    }
    controller_ = std::make_unique<NightLightFeaturePodController>(
        system_tray_->bubble()->unified_system_tray_controller());
    tile_ = controller_->CreateTile();
  }

  bool IsButtonVisible() { return tile_->GetVisible(); }

  bool IsButtonToggled() { return tile_->IsToggled(); }

 protected:
  void PressIcon() { controller_->OnIconPressed(); }

  void PressLabel() { controller_->OnLabelPressed(); }

  const std::u16string& GetButtonLabelText() {
    return tile_->sub_label()->GetText();
  }

 private:
  raw_ptr<UnifiedSystemTray, DanglingUntriaged> system_tray_;
  std::unique_ptr<NightLightFeaturePodController> controller_;
  std::unique_ptr<FeatureTile> tile_;
};

TEST_F(NightLightFeaturePodControllerTest, ButtonVisibility) {
  // The button is visible in an active session.
  CreateButton();
  EXPECT_TRUE(IsButtonVisible());

  GetSessionControllerClient()->LockScreen();
  CreateButton();
  // The feature tile is visible in the locked screen.
  EXPECT_TRUE(IsButtonVisible());
}

// Tests that toggling night light from the system tray switches the color
// mode and its button label properly.
TEST_F(NightLightFeaturePodControllerTest, Toggle) {
  CreateButton();

  NightLightControllerImpl* controller = Shell::Get()->night_light_controller();
  // Check that the feature pod button and its label reflects the default
  // Night light off without any auto scheduling.
  EXPECT_FALSE(controller->IsNightLightEnabled());
  EXPECT_FALSE(IsButtonToggled());
  EXPECT_EQ(ScheduleType::kNone, controller->GetScheduleType());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_OFF_STATE),
      GetButtonLabelText());

  // Toggling the button should enable night light and update the button label
  // correctly and maintaining no scheduling.
  PressIcon();
  EXPECT_TRUE(controller->IsNightLightEnabled());
  EXPECT_TRUE(IsButtonToggled());
  EXPECT_EQ(ScheduleType::kNone, controller->GetScheduleType());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_ON_STATE),
            GetButtonLabelText());
}

// Tests that toggling sunset-to-sunrise-scheduled night light from the system
// tray while switches the color mode temporarily and maintains the auto
// scheduling.
TEST_F(NightLightFeaturePodControllerTest, SunsetToSunrise) {
  CreateButton();

  // Enable sunset-to-sunrise scheduling.
  NightLightControllerImpl* controller = Shell::Get()->night_light_controller();
  controller->SetScheduleType(ScheduleType::kSunsetToSunrise);
  EXPECT_EQ(ScheduleType::kSunsetToSunrise, controller->GetScheduleType());

  const std::u16string sublabel_on = l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_ON_STATE_SUNSET_TO_SUNRISE_SCHEDULED);
  const std::u16string sublabel_off = l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_OFF_STATE_SUNSET_TO_SUNRISE_SCHEDULED);

  // Pressing the night light button should switch the status but keep
  // sunset-to-sunrise scheduling.
  bool enabled = controller->IsNightLightEnabled();
  PressIcon();
  EXPECT_EQ(ScheduleType::kSunsetToSunrise, controller->GetScheduleType());
  EXPECT_EQ(!enabled, controller->IsNightLightEnabled());
  EXPECT_EQ(!enabled, IsButtonToggled());
  EXPECT_EQ(!enabled ? sublabel_on : sublabel_off, GetButtonLabelText());

  // Pressing the night light button should switch the status but keep
  // sunset-to-sunrise scheduling.
  PressIcon();
  EXPECT_EQ(ScheduleType::kSunsetToSunrise, controller->GetScheduleType());
  EXPECT_EQ(enabled, controller->IsNightLightEnabled());
  EXPECT_EQ(enabled, IsButtonToggled());
  EXPECT_EQ(enabled ? sublabel_on : sublabel_off, GetButtonLabelText());
}

// Tests that custom-scheduled night light displays the right custom start or
// end time for custom schedule type on the button label of the system tray.
TEST_F(NightLightFeaturePodControllerTest, Custom) {
  CreateButton();

  // Enable custom scheduling.
  NightLightControllerImpl* controller = Shell::Get()->night_light_controller();
  controller->SetScheduleType(ScheduleType::kCustom);
  EXPECT_EQ(ScheduleType::kCustom, controller->GetScheduleType());

  auto* clock_model = Shell::Get()->system_tray_model()->clock();
  const std::u16string start_time_str =
      base::TimeFormatTimeOfDayWithHourClockType(
          ToTimeToday(controller->GetCustomStartTime()),
          clock_model->hour_clock_type(), base::kKeepAmPm);
  const std::u16string end_time_str =
      base::TimeFormatTimeOfDayWithHourClockType(
          ToTimeToday(controller->GetCustomEndTime()),
          clock_model->hour_clock_type(), base::kKeepAmPm);
  const std::u16string sublabel_on = l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_ON_STATE_CUSTOM_SCHEDULED, end_time_str);
  const std::u16string sublabel_off = l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_OFF_STATE_CUSTOM_SCHEDULED,
      start_time_str);

  // Pressing the night light button should switch the status and update the
  // label but keep the custom scheduling.
  bool enabled = controller->IsNightLightEnabled();
  PressIcon();
  EXPECT_EQ(ScheduleType::kCustom, controller->GetScheduleType());
  EXPECT_EQ(!enabled, controller->IsNightLightEnabled());
  EXPECT_EQ(!enabled, IsButtonToggled());
  EXPECT_EQ(!enabled ? sublabel_on : sublabel_off, GetButtonLabelText());

  // Pressing the night light button should switch the status and update the
  // label but keep the custom scheduling.
  PressIcon();
  EXPECT_EQ(ScheduleType::kCustom, controller->GetScheduleType());
  EXPECT_EQ(enabled, controller->IsNightLightEnabled());
  EXPECT_EQ(enabled, IsButtonToggled());
  EXPECT_EQ(enabled ? sublabel_on : sublabel_off, GetButtonLabelText());
}

TEST_F(NightLightFeaturePodControllerTest, IconUMATracking) {
  CreateButton();

  // Disable sunset-to-sunrise scheduling.
  NightLightControllerImpl* controller = Shell::Get()->night_light_controller();
  controller->SetScheduleType(ScheduleType::kNone);

  // No metrics logged before clicking on any views.
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  histogram_tester->ExpectTotalCount("Ash.QuickSettings.FeaturePod.ToggledOn",
                                     /*expected_count=*/0);
  histogram_tester->ExpectTotalCount("Ash.QuickSettings.FeaturePod.ToggledOff",
                                     /*expected_count=*/0);
  histogram_tester->ExpectTotalCount("Ash.QuickSettings.FeaturePod.DiveIn",
                                     /*expected_count=*/0);

  // Toggle on the nightlight feature when pressing on the icon.
  PressIcon();
  histogram_tester->ExpectTotalCount("Ash.QuickSettings.FeaturePod.ToggledOn",
                                     /*expected_count=*/1);
  histogram_tester->ExpectTotalCount("Ash.QuickSettings.FeaturePod.ToggledOff",
                                     /*expected_count=*/0);
  histogram_tester->ExpectTotalCount("Ash.QuickSettings.FeaturePod.DiveIn",
                                     /*expected_count=*/0);
  histogram_tester->ExpectBucketCount("Ash.QuickSettings.FeaturePod.ToggledOn",
                                      QsFeatureCatalogName::kNightLight,
                                      /*expected_count=*/1);

  // Toggle off the nightlight feature when pressing on the icon again.
  PressIcon();
  histogram_tester->ExpectTotalCount("Ash.QuickSettings.FeaturePod.ToggledOn",
                                     /*expected_count=*/1);
  histogram_tester->ExpectTotalCount("Ash.QuickSettings.FeaturePod.ToggledOff",
                                     /*expected_count=*/1);
  histogram_tester->ExpectTotalCount("Ash.QuickSettings.FeaturePod.DiveIn",
                                     /*expected_count=*/0);
  histogram_tester->ExpectBucketCount("Ash.QuickSettings.FeaturePod.ToggledOff",
                                      QsFeatureCatalogName::kNightLight,
                                      /*expected_count=*/1);
}

TEST_F(NightLightFeaturePodControllerTest, LabelUMATracking) {
  CreateButton();

  // No metrics logged before clicking on any views.
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  histogram_tester->ExpectTotalCount("Ash.QuickSettings.FeaturePod.ToggledOn",
                                     /*expected_count=*/0);
  histogram_tester->ExpectTotalCount("Ash.QuickSettings.FeaturePod.ToggledOff",
                                     /*expected_count=*/0);
  histogram_tester->ExpectTotalCount("Ash.QuickSettings.FeaturePod.DiveIn",
                                     /*expected_count=*/0);

  // Show nightlight detailed view (settings window) when pressing on the
  // label.
  PressLabel();
  histogram_tester->ExpectTotalCount("Ash.QuickSettings.FeaturePod.ToggledOn",
                                     /*expected_count=*/0);
  histogram_tester->ExpectTotalCount("Ash.QuickSettings.FeaturePod.ToggledOff",
                                     /*expected_count=*/0);
  histogram_tester->ExpectTotalCount("Ash.QuickSettings.FeaturePod.DiveIn",
                                     /*expected_count=*/0);
  histogram_tester->ExpectBucketCount("Ash.QuickSettings.FeaturePod.DiveIn",
                                      QsFeatureCatalogName::kNightLight,
                                      /*expected_count=*/0);
}

}  // namespace ash
