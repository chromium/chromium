// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/battery_notification.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/power/battery_saver_controller.h"
#include "ash/system/power/power_notification_controller.h"
#include "ash/system/power/power_status.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"

namespace ash {

namespace {
constexpr int kCriticalMinutes = 5;
constexpr int kLowPowerMinutes = 15;
}  // namespace

using l10n_util::GetStringFUTF16;
using l10n_util::GetStringUTF16;
using message_center::FullscreenVisibility;
using message_center::SystemNotificationWarningLevel;

class BatteryNotificationTest : public AshTestBase {
 public:
  BatteryNotificationTest() = default;
  BatteryNotificationTest(const BatteryNotificationTest&) = delete;
  BatteryNotificationTest& operator=(const BatteryNotificationTest&) = delete;
  ~BatteryNotificationTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>(
        features::kBatterySaver);
    chromeos::FakePowerManagerClient::InitializeFake();
    AshTestBase::SetUp();

    battery_notification_ = std::make_unique<BatteryNotification>(
        message_center::MessageCenter::Get(),
        PowerNotificationController::NotificationState::NOTIFICATION_LOW_POWER,
        false);
  }

  void TearDown() override {
    battery_notification_.reset();
    AshTestBase::TearDown();
    chromeos::PowerManagerClient::Shutdown();
    scoped_feature_list_->Reset();
  }

 protected:
  struct ExpectedNotificationValues {
    size_t expected_button_size;
    message_center::SystemNotificationWarningLevel expected_warning_level;
    message_center::FullscreenVisibility expected_fullscreen_visibility;
    std::u16string expected_title;
    std::u16string expected_message;
    std::u16string expected_button_title;
  };

  BatterySaverController* battery_saver_controller() {
    return Shell::Get()->battery_saver_controller();
  }

  message_center::Notification* GetBatteryNotification() {
    return message_center::MessageCenter::Get()->FindNotificationById(
        BatteryNotification::kNotificationId);
  }

  void TestBatterySaverNotification(
      const PowerStatus& status,
      const ExpectedNotificationValues& expected_values,
      PowerNotificationController::NotificationState notification_state,
      bool expected_bsm_state_after_click) {
    auto VerifyBatterySaverModeState =
        [](base::RunLoop* run_loop, bool active,
           absl::optional<power_manager::BatterySaverModeState> state) {
          ASSERT_TRUE(state);
          EXPECT_EQ(state->enabled(), active);
          run_loop->Quit();
        };

    PowerStatus::Get()->SetBatterySaverStateForTesting(
        !expected_bsm_state_after_click);

    // Display notification.
    battery_notification_->Update(notification_state, false);

    auto* notification = GetBatteryNotification();
    ASSERT_TRUE(notification);

    // Test expectations against actual values.
    TestExpectedNotificationValues(expected_values, notification);

    // Click the button to turn off/on battery saver mode depending on
    // NotificationState.
    notification->delegate()->Click(0, absl::nullopt);

    // Verify battery saver mode state changed respective to the
    // NotificationState.
    base::RunLoop run_loop;
    chromeos::PowerManagerClient::Get()->GetBatterySaverModeState(
        base::BindOnce(VerifyBatterySaverModeState, &run_loop,
                       expected_bsm_state_after_click));
    run_loop.Run();
  }

  void TestExpectedNotificationValues(
      const ExpectedNotificationValues& values,
      const message_center::Notification* notification) {
    EXPECT_EQ(values.expected_warning_level,
              notification->system_notification_warning_level());
    EXPECT_EQ(values.expected_title, notification->title());
    EXPECT_EQ(values.expected_message, notification->message());
    EXPECT_EQ(values.expected_fullscreen_visibility,
              notification->fullscreen_visibility());
    EXPECT_FALSE(notification->pinned());
    EXPECT_EQ(values.expected_button_size, notification->buttons().size());
    EXPECT_EQ(values.expected_button_title, notification->buttons()[0].title);
  }

  std::u16string GetLowPowerTitle() {
    return GetStringUTF16(IDS_ASH_STATUS_TRAY_LOW_BATTERY_TITLE);
  }

  std::u16string GetLowPowerMessage() {
    return GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_LOW_BATTERY_MESSAGE, GetRemainingTimeString(),
        base::NumberToString16(PowerStatus::Get()->GetRoundedBatteryPercent()));
  }

  std::u16string GetBatterySaverTitle() {
    return GetStringUTF16(IDS_ASH_STATUS_TRAY_LOW_BATTERY_BSM_TITLE);
  }

  std::u16string GetBatterySaverMessage() {
    return GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_LOW_BATTERY_BSM_MESSAGE, GetRemainingTimeString(),
        base::NumberToString16(PowerStatus::Get()->GetRoundedBatteryPercent()));
  }

  std::u16string GetBatterySaverOptOutButtonString() {
    return GetStringUTF16(IDS_ASH_STATUS_TRAY_LOW_BATTERY_BSM_BUTTON_OPT_OUT);
  }

  std::u16string GetBatterySaverOptInButtonString() {
    return GetStringUTF16(IDS_ASH_STATUS_TRAY_LOW_BATTERY_BSM_BUTTON_OPT_IN);
  }

  void SetPowerStatus(double battery_percent = 100,
                      long time_to_empty_sec = 28800) {
    power_manager::PowerSupplyProperties proto;
    proto.set_battery_percent(battery_percent);
    proto.set_battery_time_to_empty_sec(time_to_empty_sec);
    PowerStatus::Get()->SetProtoForTesting(proto);
    chromeos::FakePowerManagerClient::Get()->UpdatePowerProperties(proto);
  }

  std::unique_ptr<BatteryNotification> battery_notification_;

 private:
  std::u16string GetRemainingTimeString() {
    return ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_DURATION,
                                  ui::TimeFormat::LENGTH_LONG,
                                  *PowerStatus::Get()->GetBatteryTimeToEmpty());
  }

  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
};

TEST_F(BatteryNotificationTest, LowPowerNotification) {
  power_manager::PowerSupplyProperties proto;
  // Set the rounded value matches the low power threshold.
  proto.set_battery_time_to_empty_sec(kLowPowerMinutes * 60 + 29);
  PowerStatus::Get()->SetProtoForTesting(proto);

  battery_notification_->Update(
      PowerNotificationController::NotificationState::NOTIFICATION_LOW_POWER,
      false);

  auto* notification = GetBatteryNotification();
  ASSERT_TRUE(notification);

  EXPECT_EQ(message_center::SystemNotificationWarningLevel::WARNING,
            notification->system_notification_warning_level());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_LOW_BATTERY_TITLE),
            notification->title());
  EXPECT_EQ(message_center::FullscreenVisibility::OVER_USER,
            notification->fullscreen_visibility());
  EXPECT_FALSE(notification->pinned());
}

TEST_F(BatteryNotificationTest, LowPowerBatterySaverOptOutNotification) {
  // Set the rounded value matches the low power threshold.
  SetPowerStatus(/*battery_percent=*/100, kLowPowerMinutes * 60 + 29);

  // Expect a notification with 'turning on battery saver', and a
  // 'turn off' button to appear.
  ExpectedNotificationValues expected_values{
      1,
      SystemNotificationWarningLevel::WARNING,
      FullscreenVisibility::OVER_USER,
      GetBatterySaverTitle(),
      GetBatterySaverMessage(),
      GetBatterySaverOptOutButtonString()};

  // Battery Saver should turn off when the button is clicked.
  TestBatterySaverNotification(
      *PowerStatus::Get(), expected_values,
      PowerNotificationController::NOTIFICATION_LOW_POWER,
      /*expected_bsm_state_after_click=*/false);
}

TEST_F(BatteryNotificationTest, LowPowerBatterySaverOptInNotification) {
  // Set the rounded value matches the low power threshold.
  SetPowerStatus(/*battery_percent=*/100, kLowPowerMinutes * 60 + 29);

  // Expect a regular Low Power notification, and a 'turn on battery saver'
  // button to appear.
  ExpectedNotificationValues expected_values{
      1,
      SystemNotificationWarningLevel::WARNING,
      FullscreenVisibility::OVER_USER,
      GetLowPowerTitle(),
      GetLowPowerMessage(),
      GetBatterySaverOptInButtonString()};

  // Battery Saver should turn on when the button is clicked.
  TestBatterySaverNotification(
      *PowerStatus::Get(), expected_values,
      PowerNotificationController::NOTIFICATION_BSM_LOW_POWER_OPT_IN,
      /*expected_bsm_state_after_click=*/true);
}

TEST_F(BatteryNotificationTest, ThresholdBatterySaverOptOutNotification) {
  // Set the battery percentage to the threshold amount.
  SetPowerStatus(BatterySaverController::kActivationChargePercent);

  // Expect a notification with 'turning on battery saver', and a
  // 'turn off' button to appear.
  ExpectedNotificationValues expected_values{
      1,
      SystemNotificationWarningLevel::WARNING,
      FullscreenVisibility::OVER_USER,
      GetBatterySaverTitle(),
      GetBatterySaverMessage(),
      GetBatterySaverOptOutButtonString()};

  // Battery Saver should turn off when the button is clicked.
  TestBatterySaverNotification(
      *PowerStatus::Get(), expected_values,
      PowerNotificationController::NOTIFICATION_BSM_THRESHOLD_OPT_OUT,
      /*expected_bsm_state_after_click=*/false);
}

TEST_F(BatteryNotificationTest, ThresholdBatterySaverOptInNotification) {
  // Set the battery percentage to the threshold amount.
  SetPowerStatus(BatterySaverController::kActivationChargePercent);

  // Expect a regular Low Power notification, and a 'turn on battery saver'
  // button to appear.
  ExpectedNotificationValues expected_values{
      1,
      SystemNotificationWarningLevel::WARNING,
      FullscreenVisibility::OVER_USER,
      GetLowPowerTitle(),
      GetLowPowerMessage(),
      GetBatterySaverOptInButtonString()};

  // Battery Saver should turn on when the button is clicked.
  TestBatterySaverNotification(
      *PowerStatus::Get(), expected_values,
      PowerNotificationController::NOTIFICATION_BSM_THRESHOLD_OPT_IN,
      /*expected_bsm_state_after_click=*/true);
}

TEST_F(BatteryNotificationTest, CriticalPowerNotification) {
  power_manager::PowerSupplyProperties proto;
  // Set the rounded value matches the critical power threshold.
  proto.set_battery_time_to_empty_sec(kCriticalMinutes * 60 + 29);
  PowerStatus::Get()->SetProtoForTesting(proto);

  battery_notification_->Update(
      PowerNotificationController::NotificationState::NOTIFICATION_CRITICAL,
      false);

  auto* notification = GetBatteryNotification();
  ASSERT_TRUE(notification);

  EXPECT_EQ(message_center::SystemNotificationWarningLevel::CRITICAL_WARNING,
            notification->system_notification_warning_level());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_CRITICAL_BATTERY_TITLE),
      notification->title());
  EXPECT_EQ(message_center::FullscreenVisibility::OVER_USER,
            notification->fullscreen_visibility());
  EXPECT_EQ(message_center::NotificationPriority::SYSTEM_PRIORITY,
            notification->priority());
  EXPECT_TRUE(notification->pinned());
}

}  // namespace ash
