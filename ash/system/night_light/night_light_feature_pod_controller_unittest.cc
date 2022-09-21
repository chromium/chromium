// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/night_light/night_light_feature_pod_controller.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/clock_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/night_light/night_light_controller_impl.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

class NightLightFeaturePodControllerTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();

    UnifiedSystemTray* system_tray = GetPrimaryUnifiedSystemTray();
    system_tray->ShowBubble();

    feature_pod_controller_ = std::make_unique<NightLightFeaturePodController>(
        system_tray->bubble()->unified_system_tray_controller());
    feature_pod_button_.reset(feature_pod_controller_->CreateButton());
  }

  void TearDown() override {
    feature_pod_controller_.reset();
    feature_pod_button_.reset();

    AshTestBase::TearDown();
  }

 protected:
  NightLightFeaturePodController* feature_pod_controller() {
    return feature_pod_controller_.get();
  }

  FeaturePodButton* feature_pod_button() { return feature_pod_button_.get(); }

  const ash::FeaturePodLabelButton* feature_pod_label_button() {
    return feature_pod_button_->label_button_;
  }

  void PressIcon() { feature_pod_controller_->OnIconPressed(); }

  void PressLabel() { feature_pod_controller_->OnLabelPressed(); }

 private:
  std::unique_ptr<FeaturePodButton> feature_pod_button_;
  std::unique_ptr<NightLightFeaturePodController> feature_pod_controller_;
};

// Tests that toggling night light from the system tray switches the color
// mode and its button label properly.
TEST_F(NightLightFeaturePodControllerTest, Toggle) {
  NightLightControllerImpl* controller = Shell::Get()->night_light_controller();
  // Check that the feature pod button and its label reflects the default
  // Night light off without any auto scheduling.
  EXPECT_FALSE(controller->GetEnabled());
  EXPECT_FALSE(feature_pod_button()->IsToggled());
  EXPECT_EQ(NightLightController::ScheduleType::kNone,
            controller->GetScheduleType());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_OFF_STATE),
      feature_pod_label_button()->GetSubLabelText());

  // Toggling the button should enable night light and update the button label
  // correctly and maintaining no scheduling.
  feature_pod_controller()->OnIconPressed();
  EXPECT_TRUE(controller->GetEnabled());
  EXPECT_TRUE(feature_pod_button()->IsToggled());
  EXPECT_EQ(NightLightController::ScheduleType::kNone,
            controller->GetScheduleType());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_ON_STATE),
            feature_pod_label_button()->GetSubLabelText());
}

// Tests that toggling sunset-to-sunrise-scheduled night light from the system
// tray while switches the color mode temporarily and maintains the auto
// scheduling.
TEST_F(NightLightFeaturePodControllerTest, SunsetToSunrise) {
  // Enable sunset-to-sunrise scheduling.
  NightLightControllerImpl* controller = Shell::Get()->night_light_controller();
  controller->SetScheduleType(
      NightLightController::ScheduleType::kSunsetToSunrise);
  EXPECT_EQ(NightLightController::ScheduleType::kSunsetToSunrise,
            controller->GetScheduleType());

  const std::u16string sublabel_on = l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_ON_STATE_SUNSET_TO_SUNRISE_SCHEDULED);
  const std::u16string sublabel_off = l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_OFF_STATE_SUNSET_TO_SUNRISE_SCHEDULED);

  // Pressing the night light button should switch the status but keep
  // sunset-to-sunrise scheduling.
  bool enabled = controller->GetEnabled();
  feature_pod_controller()->OnIconPressed();
  EXPECT_EQ(NightLightController::ScheduleType::kSunsetToSunrise,
            controller->GetScheduleType());
  EXPECT_EQ(!enabled, controller->GetEnabled());
  EXPECT_EQ(!enabled, feature_pod_button()->IsToggled());
  EXPECT_EQ(!enabled ? sublabel_on : sublabel_off,
            feature_pod_label_button()->GetSubLabelText());

  // Pressing the night light button should switch the status but keep
  // sunset-to-sunrise scheduling.
  feature_pod_controller()->OnIconPressed();
  EXPECT_EQ(NightLightController::ScheduleType::kSunsetToSunrise,
            controller->GetScheduleType());
  EXPECT_EQ(enabled, controller->GetEnabled());
  EXPECT_EQ(enabled, feature_pod_button()->IsToggled());
  EXPECT_EQ(enabled ? sublabel_on : sublabel_off,
            feature_pod_label_button()->GetSubLabelText());
}

// Tests that custom-scheduled night light displays the right custom start or
// end time for custom schedule type on the button label of the system tray.
TEST_F(NightLightFeaturePodControllerTest, Custom) {
  // Enable custom scheduling.
  NightLightControllerImpl* controller = Shell::Get()->night_light_controller();
  controller->SetScheduleType(NightLightController::ScheduleType::kCustom);
  EXPECT_EQ(NightLightController::ScheduleType::kCustom,
            controller->GetScheduleType());

  auto* clock_model = Shell::Get()->system_tray_model()->clock();
  const std::u16string start_time_str =
      base::TimeFormatTimeOfDayWithHourClockType(
          controller->GetCustomStartTime().ToTimeToday(),
          clock_model->hour_clock_type(), base::kKeepAmPm);
  const std::u16string end_time_str =
      base::TimeFormatTimeOfDayWithHourClockType(
          controller->GetCustomEndTime().ToTimeToday(),
          clock_model->hour_clock_type(), base::kKeepAmPm);
  const std::u16string sublabel_on = l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_ON_STATE_CUSTOM_SCHEDULED, end_time_str);
  const std::u16string sublabel_off = l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_OFF_STATE_CUSTOM_SCHEDULED,
      start_time_str);

  // Pressing the night light button should switch the status and update the
  // label but keep the custom scheduling.
  bool enabled = controller->GetEnabled();
  feature_pod_controller()->OnIconPressed();
  EXPECT_EQ(NightLightController::ScheduleType::kCustom,
            controller->GetScheduleType());
  EXPECT_EQ(!enabled, controller->GetEnabled());
  EXPECT_EQ(!enabled, feature_pod_button()->IsToggled());
  EXPECT_EQ(!enabled ? sublabel_on : sublabel_off,
            feature_pod_label_button()->GetSubLabelText());

  // Pressing the night light button should switch the status and update the
  // label but keep the custom scheduling.
  feature_pod_controller()->OnIconPressed();
  EXPECT_EQ(NightLightController::ScheduleType::kCustom,
            controller->GetScheduleType());
  EXPECT_EQ(enabled, controller->GetEnabled());
  EXPECT_EQ(enabled, feature_pod_button()->IsToggled());
  EXPECT_EQ(enabled ? sublabel_on : sublabel_off,
            feature_pod_label_button()->GetSubLabelText());
}

TEST_F(NightLightFeaturePodControllerTest, IconUMATracking) {
  // Disable sunset-to-sunrise scheduling.
  NightLightControllerImpl* controller = Shell::Get()->night_light_controller();
  controller->SetScheduleType(NightLightController::ScheduleType::kNone);

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

  // Toggle on the nightlight feature when pressing on the icon.
  PressIcon();
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOn",
      /*count=*/1);
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOff",
      /*count=*/0);
  histogram_tester->ExpectTotalCount("Ash.UnifiedSystemView.FeaturePod.DiveIn",
                                     /*count=*/0);
  histogram_tester->ExpectBucketCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOn",
      QsFeatureCatalogName::kNightLight,
      /*expected_count=*/1);

  // Toggle off the nightlight feature when pressing on the icon again.
  PressIcon();
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOn",
      /*count=*/1);
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOff",
      /*count=*/1);
  histogram_tester->ExpectTotalCount("Ash.UnifiedSystemView.FeaturePod.DiveIn",
                                     /*count=*/0);
  histogram_tester->ExpectBucketCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOff",
      QsFeatureCatalogName::kNightLight,
      /*expected_count=*/1);
}

TEST_F(NightLightFeaturePodControllerTest, LabelUMATracking) {
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

  // Show nightlight detailed view (settings window) when pressing on the
  // label.
  PressLabel();
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOn",
      /*count=*/0);
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOff",
      /*count=*/0);
  histogram_tester->ExpectTotalCount("Ash.UnifiedSystemView.FeaturePod.DiveIn",
                                     /*count=*/1);
  histogram_tester->ExpectBucketCount("Ash.UnifiedSystemView.FeaturePod.DiveIn",
                                      QsFeatureCatalogName::kNightLight,
                                      /*expected_count=*/1);
}

}  // namespace ash