// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/system/power/battery_saver_controller.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/power/battery_notification.h"
#include "ash/system/power/power_status.h"
#include "ash/system/system_notification_controller.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "ash/system/toast/toast_overlay.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"

namespace ash {

class BatterySaverControllerTest : public AshTestBase {
 public:
  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>(
        features::kBatterySaver);
    chromeos::PowerManagerClient::InitializeFake();
    AshTestBase::SetUp();

    // Let initial PowerStatus call to
    // PowerManagerClient::GetBatterySaverModeState to complete.
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

  void TearDown() override {
    AshTestBase::TearDown();
    chromeos::PowerManagerClient::Shutdown();
    scoped_feature_list_.reset();
  }

 protected:
  BatterySaverController* battery_saver_controller() {
    return Shell::Get()->battery_saver_controller();
  }

  void UpdatePowerStatus(double battery_percent,
                         base::TimeDelta time_to_empty,
                         bool charging,
                         bool is_usb_charger = false) {
    power_manager::PowerSupplyProperties props;

    // Set battery percentage
    props.set_battery_percent(battery_percent);

    // Determine battery state
    auto external_power =
        power_manager::PowerSupplyProperties_ExternalPower_DISCONNECTED;
    auto battery_state =
        power_manager::PowerSupplyProperties_BatteryState_DISCHARGING;

    if (is_usb_charger) {
      external_power = power_manager::PowerSupplyProperties_ExternalPower_USB;
    } else if (charging) {  // Otherwise, treat like AC charger.
      external_power = power_manager::PowerSupplyProperties_ExternalPower_AC;
    }

    if (battery_percent >= 100) {
      battery_state = power_manager::PowerSupplyProperties_BatteryState_FULL;
    } else if (charging) {
      battery_state =
          power_manager::PowerSupplyProperties_BatteryState_CHARGING;
    }

    // Set battery state
    props.set_external_power(external_power);
    props.set_battery_state(battery_state);

    // Set time
    props.set_is_calculating_battery_time(false);
    props.set_battery_time_to_empty_sec(time_to_empty.InSecondsF());

    // Flush
    base::RunLoop run_loop;
    chromeos::FakePowerManagerClient::Get()->UpdatePowerProperties(props);
    run_loop.RunUntilIdle();
  }

  ToastOverlay* GetCurrentToast() {
    return Shell::Get()->toast_manager()->GetCurrentOverlayForTesting();
  }

  void DismissToast() {
    Shell::Get()->toast_manager()->CloseAllToastsWithAnimation();
  }

  bool IsBatterySaverActive() {
    return PowerStatus::Get()->IsBatterySaverActive();
  }

  double GetActivationPercent() {
    return features::kBatterySaverActivationChargePercent.Get();
  }

  constexpr static base::TimeDelta eight_hours_ = base::Hours(8);

  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_ = nullptr;
};

class BatterySaverControllerNotificationTest
    : public BatterySaverControllerTest,
      public testing::WithParamInterface<
          features::BatterySaverNotificationBehavior> {
 protected:
  message_center::Notification* GetCurrentNotification() {
    return message_center::MessageCenter::Get()->FindNotificationById(
        BatteryNotification::kNotificationId);
  }

  void DismissNotification() {
    message_center::MessageCenter::Get()->RemoveNotification(
        BatteryNotification::kNotificationId, false);
    EXPECT_EQ(GetCurrentNotification(), nullptr);
  }

  void NotificationNotPresent() {
    EXPECT_EQ(GetCurrentNotification(), nullptr);
  }

  void NotificationPresent() { EXPECT_NE(GetCurrentNotification(), nullptr); }

  void SetExperimentArm(features::BatterySaverNotificationBehavior arm) {
    scoped_feature_list_.reset();
    base::FieldTrialParams parameters;
    parameters[features::kBatterySaverNotificationBehavior.name] =
        features::kBatterySaverNotificationBehavior.options[arm].name;
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitAndEnableFeatureWithParameters(
        features::kBatterySaver, parameters);
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }
};

// Test that the automatic logic to turn battery saver on and off without
// direct user action works.
TEST_F(BatterySaverControllerTest, AutoEnableDisable) {
  // Battery near full and charging, no battery saver.
  UpdatePowerStatus(80.0, eight_hours_, true);
  EXPECT_FALSE(IsBatterySaverActive());

  // Battery near full and discharging, still no battery saver.
  UpdatePowerStatus(80.0, eight_hours_, false);
  EXPECT_FALSE(IsBatterySaverActive());

  // Battery discharging but just above the activation %, still no battery
  // saver.
  UpdatePowerStatus(GetActivationPercent() + 1, eight_hours_, false);
  EXPECT_FALSE(IsBatterySaverActive());

  // Battery discharging and at activation %, battery saver turns on.
  UpdatePowerStatus(GetActivationPercent(), eight_hours_, false);
  EXPECT_TRUE(IsBatterySaverActive());

  // Discharge more, battery saver remains on.
  UpdatePowerStatus(5.0, eight_hours_, false);
  EXPECT_TRUE(IsBatterySaverActive());

  // Start charging, even with a low battery %, battery saver disables.
  UpdatePowerStatus(5.0, eight_hours_, true);
  EXPECT_FALSE(IsBatterySaverActive());
}

// Test that we detect charging while asleep, as seen by a sudden jump in charge
// while discharging.
TEST_F(BatterySaverControllerTest, DetectSleepCharging) {
  // Battery discharging and below activation %, battery saver turns on.
  UpdatePowerStatus(5.0, eight_hours_, false);
  EXPECT_TRUE(IsBatterySaverActive());

  // Increase charge while still discharging, but stay below activiation %,
  // battery saver remains on.
  UpdatePowerStatus(GetActivationPercent(), eight_hours_, false);
  EXPECT_TRUE(IsBatterySaverActive());

  // Increase charge again, but this time above the activation %, battery saver
  // turns off.
  UpdatePowerStatus(
      GetActivationPercent() +
          BatterySaverController::kBatterySaverSleepChargeThreshold,
      eight_hours_, false);
  EXPECT_FALSE(IsBatterySaverActive());
}

TEST_F(BatterySaverControllerTest, EnsureThresholdsCrossed) {
  // Start the test with full battery, discharging.
  UpdatePowerStatus(100.0, eight_hours_, false);
  EXPECT_FALSE(IsBatterySaverActive());

  // Enable battery saver mode manually.
  battery_saver_controller()->SetState(
      true, BatterySaverController::UpdateReason::kSettings);

  // Discharge to the percent threshold
  UpdatePowerStatus(GetActivationPercent(), eight_hours_, false);
  EXPECT_TRUE(IsBatterySaverActive());

  // Disable battery saver mode manually.
  battery_saver_controller()->SetState(
      false, BatterySaverController::UpdateReason::kSettings);

  // When we get to percent_threshold-1, it should still be disabled.
  UpdatePowerStatus(GetActivationPercent() - 1, eight_hours_, false);
  EXPECT_FALSE(IsBatterySaverActive());
}

// Test that Battery Saver remains enabled when charging with a low power USB
// charger.
TEST_F(BatterySaverControllerTest, USBCharging) {
  UpdatePowerStatus(GetActivationPercent() - 5, eight_hours_, false);
  EXPECT_TRUE(IsBatterySaverActive());

  // Attache a low-power charger and slowly charge, Battery Saver remains on.
  for (int i = -5; i <= 5; i++) {
    UpdatePowerStatus(GetActivationPercent() + i, eight_hours_, true, true);
    EXPECT_TRUE(IsBatterySaverActive());
  }
}

// Metrics always logged on enable.
void ExpectEnabledMetrics(base::HistogramTester& histogram_tester,
                          base::HistogramBase::Count enabled_count) {
  histogram_tester.ExpectTotalCount("Ash.BatterySaver.BatteryPercent.Enabled",
                                    enabled_count);
  histogram_tester.ExpectTotalCount("Ash.BatterySaver.TimeToEmpty.Enabled",
                                    enabled_count);
}

// Metrics logged on enable when enabled via settings.
void ExpectSettingsEnabledMetrics(
    base::HistogramTester& histogram_tester,
    base::HistogramBase::Count settings_enabled_count) {
  histogram_tester.ExpectTotalCount(
      "Ash.BatterySaver.BatteryPercent.EnabledSettings",
      settings_enabled_count);
  histogram_tester.ExpectTotalCount(
      "Ash.BatterySaver.TimeToEmpty.EnabledSettings", settings_enabled_count);
}

// Metrics always logged on disable.
void ExpectDisabledMetrics(base::HistogramTester& histogram_tester,
                           base::HistogramBase::Count disabled_count) {
  histogram_tester.ExpectTotalCount("Ash.BatterySaver.BatteryPercent.Disabled",
                                    disabled_count);
  histogram_tester.ExpectTotalCount("Ash.BatterySaver.TimeToEmpty.Disabled",
                                    disabled_count);
  histogram_tester.ExpectTotalCount("Ash.BatterySaver.Duration",
                                    disabled_count);
}

// Metrics logged on disable when enabled via notification.
void ExpectNotificationEnabledMetricsOnDisable(
    base::HistogramTester& histogram_tester,
    base::HistogramBase::Count notification_enabled_count) {
  histogram_tester.ExpectTotalCount(
      "Ash.BatterySaver.Duration.EnabledNotification",
      notification_enabled_count);
}

// Metrics logged on disable when enabled via settings.
void ExpectSettingsEnabledMetricsOnDisable(
    base::HistogramTester& histogram_tester,
    base::HistogramBase::Count settings_enabled_count) {
  histogram_tester.ExpectTotalCount("Ash.BatterySaver.Duration.EnabledSettings",
                                    settings_enabled_count);
}

// Metrics logged on disable when disabled via charging.
void ExpectChargingDisabledMetrics(
    base::HistogramTester& histogram_tester,
    base::HistogramBase::Count charging_disabled_count) {
  histogram_tester.ExpectTotalCount(
      "Ash.BatterySaver.Duration.DisabledCharging", charging_disabled_count);
}

// Metrics logged on disable when disabled via notification.
void ExpectNotificationDisabledMetrics(
    base::HistogramTester& histogram_tester,
    base::HistogramBase::Count notification_disabled_count) {
  histogram_tester.ExpectTotalCount(
      "Ash.BatterySaver.Duration.DisabledNotification",
      notification_disabled_count);
}

// Metrics logged on disable when disabled via settings.
void ExpectSettingsDisabledMetrics(
    base::HistogramTester& histogram_tester,
    base::HistogramBase::Count settings_disabled_count) {
  histogram_tester.ExpectTotalCount(
      "Ash.BatterySaver.BatteryPercent.DisabledSettings",
      settings_disabled_count);
  histogram_tester.ExpectTotalCount(
      "Ash.BatterySaver.TimeToEmpty.DisabledSettings", settings_disabled_count);
  histogram_tester.ExpectTotalCount(
      "Ash.BatterySaver.Duration.DisabledSettings", settings_disabled_count);
}

TEST_F(BatterySaverControllerTest, Metrics) {
  base::HistogramTester ht;

  // TimeToEmpty should have a value.
  UpdatePowerStatus(80.0, eight_hours_, false);

  // Enable with settings.
  ExpectEnabledMetrics(ht, 0);
  ExpectSettingsEnabledMetrics(ht, 0);
  battery_saver_controller()->SetState(
      true, BatterySaverController::UpdateReason::kSettings);
  ExpectEnabledMetrics(ht, 1);
  ExpectSettingsEnabledMetrics(ht, 1);

  // Disable with settings.
  ExpectDisabledMetrics(ht, 0);
  ExpectSettingsEnabledMetricsOnDisable(ht, 0);
  ExpectSettingsDisabledMetrics(ht, 0);
  battery_saver_controller()->SetState(
      false, BatterySaverController::UpdateReason::kSettings);
  ExpectDisabledMetrics(ht, 1);
  ExpectSettingsEnabledMetricsOnDisable(ht, 1);
  ExpectSettingsDisabledMetrics(ht, 1);

  // Enable with notification.
  ExpectEnabledMetrics(ht, 1);
  battery_saver_controller()->SetState(
      true, BatterySaverController::UpdateReason::kThreshold);
  ExpectEnabledMetrics(ht, 2);

  // Disable with notification.
  ExpectDisabledMetrics(ht, 1);
  ExpectNotificationEnabledMetricsOnDisable(ht, 0);
  ExpectNotificationDisabledMetrics(ht, 0);
  battery_saver_controller()->SetState(
      false, BatterySaverController::UpdateReason::kThreshold);
  ExpectDisabledMetrics(ht, 2);
  ExpectNotificationEnabledMetricsOnDisable(ht, 1);
  ExpectNotificationDisabledMetrics(ht, 1);

  // Enable again with notifications, just because we need it enabled to test
  // disabling with charging.
  ExpectEnabledMetrics(ht, 2);
  battery_saver_controller()->SetState(
      true, BatterySaverController::UpdateReason::kThreshold);
  ExpectEnabledMetrics(ht, 3);

  // Disable with charging.
  ExpectDisabledMetrics(ht, 2);
  ExpectNotificationEnabledMetricsOnDisable(ht, 1);
  ExpectChargingDisabledMetrics(ht, 0);
  battery_saver_controller()->SetState(
      false, BatterySaverController::UpdateReason::kCharging);
  ExpectDisabledMetrics(ht, 3);
  ExpectNotificationEnabledMetricsOnDisable(ht, 2);
  ExpectChargingDisabledMetrics(ht, 1);

  // Check that we didn't have any spurious metrics logged.
  ExpectEnabledMetrics(ht, 3);
  ExpectSettingsEnabledMetrics(ht, 1);
  ExpectSettingsEnabledMetricsOnDisable(ht, 1);
  ExpectNotificationDisabledMetrics(ht, 1);
  ExpectSettingsDisabledMetrics(ht, 1);
}

TEST_F(BatterySaverControllerTest, ShowDisableToast) {
  // Ensure there is no `ToastOverlay` being displayed at the start of the test.
  ToastOverlay* current_toast = GetCurrentToast();
  EXPECT_EQ(current_toast, nullptr);

  // Test Disabled Toast via Button.
  // Enable battery saver mode.
  battery_saver_controller()->SetState(
      true, BatterySaverController::UpdateReason::kThreshold);

  // There should be no `ToastOverlay` displayed when battery saver is enabled.
  current_toast = GetCurrentToast();
  EXPECT_EQ(current_toast, nullptr);

  // Disable battery saver mode via button.
  battery_saver_controller()->SetState(
      false, BatterySaverController::UpdateReason::kThreshold);

  // Check to see if a `ToastOverlay` was displayed, and that it's accurate.
  current_toast = GetCurrentToast();
  EXPECT_NE(current_toast, nullptr);
  EXPECT_EQ(
      current_toast->GetText(),
      l10n_util::GetStringUTF16(IDS_ASH_BATTERY_SAVER_DISABLED_TOAST_TEXT));
  DismissToast();

  // Test Disabled Toast via Charging.
  // Enable battery saver mode.
  battery_saver_controller()->SetState(
      true, BatterySaverController::UpdateReason::kThreshold);

  // There should be no `ToastOverlay` displayed when battery saver is enabled.
  current_toast = GetCurrentToast();
  EXPECT_EQ(current_toast, nullptr);

  // Disable battery saver mode via Charging.
  battery_saver_controller()->SetState(
      false, BatterySaverController::UpdateReason::kCharging);

  // Check to see if a `ToastOverlay` was displayed, and that it's accurate.
  current_toast = GetCurrentToast();
  EXPECT_NE(current_toast, nullptr);
  EXPECT_EQ(
      current_toast->GetText(),
      l10n_util::GetStringUTF16(IDS_ASH_BATTERY_SAVER_DISABLED_TOAST_TEXT));
  DismissToast();

  // Test Disabled Toast via Settings.
  // Reenable to test that toast doesn't appear when toggled via Settings.
  battery_saver_controller()->SetState(
      true, BatterySaverController::UpdateReason::kSettings);

  // Disable battery saver mode via Settings toggle.
  battery_saver_controller()->SetState(
      false, BatterySaverController::UpdateReason::kSettings);

  // Check there is still no toast since we disabled via Settings.
  current_toast = GetCurrentToast();
  EXPECT_EQ(current_toast, nullptr);
}

TEST_F(BatterySaverControllerTest, Allowed) {
  // Battery Saver is allowed by default.
  EXPECT_TRUE(IsBatterySaverAllowed());

  // If pref is managed and false, then Battery Saver is not allowed.
  local_state()->SetManagedPref(prefs::kPowerBatterySaver, base::Value(false));
  EXPECT_FALSE(IsBatterySaverAllowed());

  local_state()->RemoveManagedPref(prefs::kPowerBatterySaver);

  // If the experiment is off, Battery Saver is not allowed.
  scoped_feature_list_->Reset();
  scoped_feature_list_->InitAndDisableFeature(features::kBatterySaver);
  EXPECT_FALSE(IsBatterySaverAllowed());
}

TEST_F(BatterySaverControllerNotificationTest,
       ShowEnableToastAtCriticalPercentageForAutoEnable) {
  SetExperimentArm(features::kBSMAutoEnable);
  const int critical_percentage = Shell::Get()
                                      ->system_notification_controller()
                                      ->power_notification_controller()
                                      ->GetCriticalPowerPercentage();

  // Ensure there is no `ToastOverlay` being displayed at the start of the test.
  ToastOverlay* current_toast = GetCurrentToast();
  EXPECT_EQ(current_toast, nullptr);

  // Set the battery to a critical percentage.
  UpdatePowerStatus(critical_percentage, eight_hours_, /*charging=*/false);

  // The enabled toast should be displayed.
  current_toast = GetCurrentToast();
  EXPECT_NE(current_toast, nullptr);
  EXPECT_EQ(
      current_toast->GetText(),
      l10n_util::GetStringUTF16(IDS_ASH_BATTERY_SAVER_ENABLED_TOAST_TEXT));
  DismissToast();

  // Plug In AC Charger.
  UpdatePowerStatus(critical_percentage, eight_hours_, /*charging=*/true);

  // Toast should be 'Disabled' text.
  current_toast = GetCurrentToast();
  EXPECT_NE(current_toast, nullptr);
  EXPECT_EQ(
      current_toast->GetText(),
      l10n_util::GetStringUTF16(IDS_ASH_BATTERY_SAVER_DISABLED_TOAST_TEXT));
  DismissToast();
}

TEST_P(BatterySaverControllerNotificationTest, NotificationStateUpdatesOnUSB) {
  SetExperimentArm(features::kBatterySaverNotificationBehavior.Get());
  DismissNotification();

  // Start test with 100% battery, eight hours time remaining, not charging.
  UpdatePowerStatus(100, eight_hours_, /*charging=*/false);
  NotificationNotPresent();
  DismissNotification();

  // Battery read jumps (not smoothly) from above threshold to below threshold.
  UpdatePowerStatus(GetActivationPercent() - 1, eight_hours_,
                    /*charging=*/false);
  NotificationPresent();
  DismissNotification();
  EXPECT_TRUE(IsBatterySaverActive());

  // Simulate USB Plug-In, which detects as AC first, then USB.
  UpdatePowerStatus(GetActivationPercent() - 1, eight_hours_,
                    /*charging=*/true);
  EXPECT_FALSE(IsBatterySaverActive());
  UpdatePowerStatus(GetActivationPercent() - 1, eight_hours_,
                    /*charging=*/false, /*is_usb_charger=*/true);
  EXPECT_EQ(IsBatterySaverActive(),
            features::kBatterySaverNotificationBehavior.Get() ==
                features::kBSMAutoEnable);
  NotificationPresent();

  // Disable Battery Saver via Settings toggle.
  // This should cause an update in notification state, but not in notification.
  battery_saver_controller()->SetState(
      false, BatterySaverController::UpdateReason::kSettings);

  // Unplug USB Charger.
  UpdatePowerStatus(GetActivationPercent() - 1, eight_hours_,
                    /*charging=*/false, /*is_usb_charger=*/false);

  // If notification state was updated, then we should show a generic low power
  // notification.
  EXPECT_EQ(GetCurrentNotification()->title(),
            l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_LOW_BATTERY_TITLE));
  EXPECT_EQ(GetCurrentNotification()->buttons().size(), static_cast<ulong>(0));
}

TEST_P(BatterySaverControllerNotificationTest,
       ReenableOnUnplugBelowThresholds) {
  SetExperimentArm(GetParam());

  // Start the test at percent threshold with no charging.
  UpdatePowerStatus(GetActivationPercent(), eight_hours_, false);
  switch (features::kBatterySaverNotificationBehavior.Get()) {
    case features::kBSMAutoEnable:
      EXPECT_TRUE(IsBatterySaverActive());
      break;
    case features::kBSMOptIn:
      EXPECT_FALSE(IsBatterySaverActive());
      break;
    default:
      FAIL();
  }

  // Plug in the charger at percent threshold.
  UpdatePowerStatus(GetActivationPercent(), eight_hours_, true);
  EXPECT_FALSE(IsBatterySaverActive());

  // Unplug the charger, and expect BSM to stay on if we are in an auto-enable
  // state.
  UpdatePowerStatus(GetActivationPercent(), eight_hours_, false);
  switch (features::kBatterySaverNotificationBehavior.Get()) {
    case features::kBSMAutoEnable:
      EXPECT_TRUE(IsBatterySaverActive());
      break;
    case features::kBSMOptIn:
      EXPECT_FALSE(IsBatterySaverActive());
      break;
    default:
      FAIL();
  }
}

TEST_P(BatterySaverControllerNotificationTest,
       TestNotificationThresholdsForExperimentArms) {
  SetExperimentArm(GetParam());

  auto NotificationNotPresent = [&]() {
    EXPECT_EQ(GetCurrentNotification(), nullptr);
  };
  auto NotificationPresent = [&]() {
    EXPECT_NE(GetCurrentNotification(), nullptr);
  };

  // Start test with 100% battery, eight hours time remaining, not charging.
  UpdatePowerStatus(100, eight_hours_, true);
  DismissNotification();

  // Battery discharging but just above the activation %, battery saver should
  // be disabled.
  UpdatePowerStatus(GetActivationPercent() + 1, eight_hours_, false);
  EXPECT_FALSE(IsBatterySaverActive());
  NotificationNotPresent();

  // Battery read jumps (not smoothly) from above threshold to below threshold.
  UpdatePowerStatus(GetActivationPercent() - 1, eight_hours_, false);
  switch (features::kBatterySaverNotificationBehavior.Get()) {
    case features::kBSMAutoEnable:
      EXPECT_TRUE(IsBatterySaverActive());
      break;
    case features::kBSMOptIn:
      EXPECT_FALSE(IsBatterySaverActive());
      break;
    default:
      FAIL();
  }

  // Notification should appear regardless of battery saver state (so user can
  // opt-in/out).
  NotificationPresent();
  DismissNotification();

  // Check to make sure that the notification doesn't reappear after it's been
  // dismissed.
  UpdatePowerStatus(GetActivationPercent() - 2, eight_hours_, false);
  NotificationNotPresent();
}

TEST_P(BatterySaverControllerNotificationTest,
       NotificationReappearsAfterChargeThenDischarge) {
  SetExperimentArm(GetParam());

  // Start test with 100% battery, eight hours time remaining, not charging.
  UpdatePowerStatus(100, eight_hours_, true);
  DismissNotification();

  // Battery read jumps (not smoothly) from above threshold to below threshold.
  UpdatePowerStatus(GetActivationPercent() - 1, eight_hours_, false);

  // Notification for threshold should appear.
  NotificationPresent();
  DismissNotification();

  // Charging, battery goes above threshold.
  UpdatePowerStatus(GetActivationPercent() + 1, eight_hours_, true);
  NotificationNotPresent();

  // Discharging, battery goes below threshold.
  UpdatePowerStatus(GetActivationPercent() - 1, eight_hours_, false);

  // Notification should reappear.
  NotificationPresent();
  DismissNotification();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    BatterySaverControllerNotificationTest,
    testing::Values(features::BatterySaverNotificationBehavior::kBSMAutoEnable,
                    features::BatterySaverNotificationBehavior::kBSMOptIn));

class BatterySaverControllerInitTest : public testing::Test {
 public:
  void SetUp() override {
    BatterySaverController::RegisterLocalStatePrefs(local_state_.registry());

    chromeos::PowerManagerClient::InitializeFake();
    power_manager_client_ = chromeos::FakePowerManagerClient::Get();

    PowerStatus::Initialize();
  }

  void TearDown() override {
    PowerStatus::Shutdown();
    power_manager_client_ = nullptr;
    chromeos::PowerManagerClient::Shutdown();
  }

 protected:
  void UpdatePowerPropertiesToDischarging(int battery_percent) {
    power_manager::PowerSupplyProperties proto;
    proto.set_battery_state(
        power_manager::PowerSupplyProperties_BatteryState_DISCHARGING);
    proto.set_external_power(
        power_manager::PowerSupplyProperties_ExternalPower_DISCONNECTED);
    proto.set_battery_percent(battery_percent);
    proto.set_is_calculating_battery_time(false);
    proto.set_battery_time_to_empty_sec(3600.0);
    power_manager_client_->UpdatePowerProperties(proto);
    base::RunLoop().RunUntilIdle();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  TestingPrefServiceSimple local_state_;
  raw_ptr<chromeos::FakePowerManagerClient> power_manager_client_;
};

// Test that Battery Saver active state is preserved across restarts.
TEST_F(BatterySaverControllerInitTest, RestoreState) {
  EXPECT_FALSE(power_manager_client_->battery_saver_mode_enabled());

  // Disabled by default.
  {
    local_state_.ClearPref(prefs::kPowerBatterySaver);
    std::unique_ptr<BatterySaverController> controller =
        std::make_unique<BatterySaverController>(&local_state_);
    EXPECT_FALSE(power_manager_client_->battery_saver_mode_enabled());
  }

  // Restore disabled.
  {
    local_state_.SetBoolean(prefs::kPowerBatterySaver, false);
    std::unique_ptr<BatterySaverController> controller =
        std::make_unique<BatterySaverController>(&local_state_);
    EXPECT_FALSE(power_manager_client_->battery_saver_mode_enabled());
  }

  // Restore enabled.
  {
    local_state_.SetBoolean(prefs::kPowerBatterySaver, true);
    std::unique_ptr<BatterySaverController> controller =
        std::make_unique<BatterySaverController>(&local_state_);
    EXPECT_TRUE(power_manager_client_->battery_saver_mode_enabled());
  }
}

// Test that Battery Saver is disabled when charging when shut down.
TEST_F(BatterySaverControllerInitTest, DisableAfterChargingWhenOff) {
  base::HistogramTester ht;
  const int battery_percent = 80;
  local_state_.SetBoolean(prefs::kPowerBatterySaver, true);
  local_state_.SetInteger(prefs::kPowerBatterySaverPercent, battery_percent);

  // Set battery_percent above saved pref value to trigger charge detection.
  UpdatePowerPropertiesToDischarging(
      battery_percent +
      BatterySaverController::kBatterySaverSleepChargeThreshold);

  // Initialize BatterySaverController and check that battery saver is OFF.
  std::unique_ptr<BatterySaverController> controller =
      std::make_unique<BatterySaverController>(&local_state_);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(power_manager_client_->battery_saver_mode_enabled());
  EXPECT_FALSE(PowerStatus::Get()->IsBatterySaverActive());

  ExpectDisabledMetrics(ht, 1);
  ExpectChargingDisabledMetrics(ht, 1);
}

// Test that Battery Saver remains on after charging when shut down if the
// battery is low enough that it would be re-enabled.
TEST_F(BatterySaverControllerInitTest, EnableAfterChargingWhenOffAtLowBattery) {
  const int battery_percent = 5;
  local_state_.SetBoolean(prefs::kPowerBatterySaver, true);
  local_state_.SetInteger(prefs::kPowerBatterySaverPercent, battery_percent);

  // Set battery_percent above saved pref value to trigger charge detection.
  UpdatePowerPropertiesToDischarging(
      battery_percent +
      BatterySaverController::kBatterySaverSleepChargeThreshold);

  // Initialize BatterySaverController and check that battery saver is ON.
  std::unique_ptr<BatterySaverController> controller =
      std::make_unique<BatterySaverController>(&local_state_);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(power_manager_client_->battery_saver_mode_enabled());
  EXPECT_TRUE(PowerStatus::Get()->IsBatterySaverActive());
}

}  // namespace ash
