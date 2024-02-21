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
#include "ash/system/system_notification_controller.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "ash/system/toast/toast_overlay.h"
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

    SetNotificationStateForTesting(
        PowerNotificationController::NotificationState::
            NOTIFICATION_BSM_ENABLING_AT_THRESHOLD);

    battery_notification_ = std::make_unique<BatteryNotification>(
        message_center::MessageCenter::Get(),
        Shell::Get()
            ->system_notification_controller()
            ->power_notification_controller());
  }

  void TearDown() override {
    OverrideIsBatterySaverAllowedForTesting(std::nullopt);
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

  ToastOverlay* GetCurrentToast() {
    return Shell::Get()->toast_manager()->GetCurrentOverlayForTesting();
  }

  void TestBatterySaverNotification(
      const ExpectedNotificationValues& expected_values,
      PowerNotificationController::NotificationState notification_state,
      bool expected_bsm_state_after_click) {
    auto VerifyBatterySaverModeState =
        [](base::RunLoop* run_loop, bool active,
           std::optional<power_manager::BatterySaverModeState> state) {
          ASSERT_TRUE(state);
          EXPECT_EQ(state->enabled(), active);
          run_loop->Quit();
        };

    PowerStatus::Get()->SetBatterySaverStateForTesting(
        !expected_bsm_state_after_click);

    // Display notification.
    SetNotificationStateForTesting(notification_state);
    battery_notification_->Update();

    auto* notification = GetBatteryNotification();
    ASSERT_TRUE(notification);

    // Test expectations against actual values.
    TestExpectedNotificationValues(expected_values, notification);

    // Click the button to turn off/on battery saver mode depending on
    // NotificationState.
    notification->delegate()->Click(0, std::nullopt);

    // Test that notification is dismissed after button is pressed.
    EXPECT_EQ(GetBatteryNotification(), nullptr);

    // Test Enable Toast on Button Click in Opt-In Branch.
    if (notification_state ==
        PowerNotificationController::NOTIFICATION_BSM_THRESHOLD_OPT_IN) {
      EXPECT_NE(GetCurrentToast(), nullptr);
      EXPECT_EQ(
          GetCurrentToast()->GetText(),
          l10n_util::GetStringUTF16(IDS_ASH_BATTERY_SAVER_ENABLED_TOAST_TEXT));
    }

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
    const std::vector<message_center::ButtonInfo> buttons =
        notification->buttons();
    EXPECT_EQ(values.expected_warning_level,
              notification->system_notification_warning_level());
    EXPECT_EQ(values.expected_title, notification->title());
    EXPECT_EQ(values.expected_message, notification->message());
    EXPECT_EQ(values.expected_fullscreen_visibility,
              notification->fullscreen_visibility());
    EXPECT_FALSE(notification->pinned());
    EXPECT_EQ(values.expected_button_size, buttons.size());
    EXPECT_EQ(values.expected_button_title,
              buttons.size() != 0 ? buttons[0].title : u"");
  }

  void SetNotificationStateForTesting(
      PowerNotificationController::NotificationState new_state) {
    Shell::Get()
        ->system_notification_controller()
        ->power_notification_controller()
        ->notification_state_ = new_state;
  }

  std::u16string GetLowPowerTitle() {
    return GetStringUTF16(IDS_ASH_STATUS_TRAY_LOW_BATTERY_TITLE);
  }

  std::u16string GetLowPowerMessageBSMWithoutTime() {
    return GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_LOW_BATTERY_BSM_GENERIC_MESSAGE_WITHOUT_TIME,
        base::NumberToString16(PowerStatus::Get()->GetRoundedBatteryPercent()));
  }

  std::u16string GetLowPowerMessageBSM() {
    return GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_LOW_BATTERY_BSM_GENERIC_MESSAGE,
        base::NumberToString16(PowerStatus::Get()->GetRoundedBatteryPercent()),
        GetRemainingTimeString());
  }

  std::u16string GetLowPowerMessage() {
    return GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_LOW_BATTERY_MESSAGE, GetRemainingTimeString(),
        base::NumberToString16(PowerStatus::Get()->GetRoundedBatteryPercent()));
  }

  std::u16string GetBatterySaverTitle() {
    return GetStringUTF16(
        IDS_ASH_STATUS_TRAY_LOW_BATTERY_BSM_AUTOENABLED_TITLE);
  }

  std::u16string GetBatterySaverMessageWithoutTime() {
    return GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_LOW_BATTERY_BSM_AUTOENABLED_MESSAGE_WITHOUT_TIME,
        base::NumberToString16(PowerStatus::Get()->GetRoundedBatteryPercent()));
  }

  std::u16string GetBatterySaverMessage() {
    return GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_LOW_BATTERY_BSM_AUTOENABLED_MESSAGE,
        base::NumberToString16(PowerStatus::Get()->GetRoundedBatteryPercent()),
        GetRemainingTimeString());
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

// Keep test for backwards compatibility for time-based notifications.
TEST_F(BatteryNotificationTest, LowPowerNotification) {
  // Disable Battery Saver feature to test original notification.
  OverrideIsBatterySaverAllowedForTesting(false);

  // Set the rounded value matches the low power threshold, percentage here is
  // arbitrary.
  SetPowerStatus(25, kLowPowerMinutes * 60 + 29);

  SetNotificationStateForTesting(
      PowerNotificationController::NotificationState::
          NOTIFICATION_BSM_ENABLING_AT_THRESHOLD);
  battery_notification_->Update();

  auto* notification = GetBatteryNotification();
  ASSERT_TRUE(notification);

  // Expect a notification with 'Low Power', and no buttons to appear.
  ExpectedNotificationValues expected_values{
      0,
      SystemNotificationWarningLevel::WARNING,
      FullscreenVisibility::OVER_USER,
      GetLowPowerTitle(),
      GetLowPowerMessage(),
      u""};

  TestExpectedNotificationValues(expected_values, notification);
}

TEST_F(BatteryNotificationTest, ThresholdBatterySaverOptOutNotification) {
  // Set the battery percentage to the threshold amount.
  SetPowerStatus(features::kBatterySaverActivationChargePercent.Get());

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
      expected_values,
      PowerNotificationController::NOTIFICATION_BSM_ENABLING_AT_THRESHOLD,
      /*expected_bsm_state_after_click=*/false);
}

TEST_F(BatteryNotificationTest,
       ThresholdBatterySaverOptOutNotificationTimeCalculating) {
  // Set the battery percentage to the threshold amount, and < 1 minute.
  SetPowerStatus(features::kBatterySaverActivationChargePercent.Get(), 30);

  // Expect a notification with 'turning on battery saver', with a message
  // excluding the time remaining, and a 'turn off' button to appear.
  ExpectedNotificationValues expected_values{
      1,
      SystemNotificationWarningLevel::WARNING,
      FullscreenVisibility::OVER_USER,
      GetBatterySaverTitle(),
      GetBatterySaverMessageWithoutTime(),
      GetBatterySaverOptOutButtonString()};

  // Battery Saver should turn off when the button is clicked.
  TestBatterySaverNotification(
      expected_values,
      PowerNotificationController::NOTIFICATION_BSM_ENABLING_AT_THRESHOLD,
      /*expected_bsm_state_after_click=*/false);
}

TEST_F(BatteryNotificationTest, ThresholdBatterySaverOptInNotification) {
  // Set the battery percentage to the threshold amount.
  SetPowerStatus(features::kBatterySaverActivationChargePercent.Get());

  // Expect a regular Low Power notification, and a 'turn on battery saver'
  // button to appear.
  ExpectedNotificationValues expected_values{
      1,
      SystemNotificationWarningLevel::WARNING,
      FullscreenVisibility::OVER_USER,
      GetLowPowerTitle(),
      GetLowPowerMessageBSM(),
      GetBatterySaverOptInButtonString()};

  // Battery Saver should turn on when the button is clicked.
  TestBatterySaverNotification(
      expected_values,
      PowerNotificationController::NOTIFICATION_BSM_THRESHOLD_OPT_IN,
      /*expected_bsm_state_after_click=*/true);
}

TEST_F(BatteryNotificationTest,
       ThresholdBatterySaverOptInNotificationTimeCalculating) {
  // Set the battery percentage to the threshold amount, and < 1 minute.
  SetPowerStatus(features::kBatterySaverActivationChargePercent.Get(), 30);

  // Expect a regular Low Power notification, with a message
  // excluding the time remaining, and a 'turn on battery saver'
  // button to appear.
  ExpectedNotificationValues expected_values{
      1,
      SystemNotificationWarningLevel::WARNING,
      FullscreenVisibility::OVER_USER,
      GetLowPowerTitle(),
      GetLowPowerMessageBSMWithoutTime(),
      GetBatterySaverOptInButtonString()};

  // Battery Saver should turn on when the button is clicked.
  TestBatterySaverNotification(
      expected_values,
      PowerNotificationController::NOTIFICATION_BSM_THRESHOLD_OPT_IN,
      /*expected_bsm_state_after_click=*/true);
}

TEST_F(BatteryNotificationTest, CriticalPowerNotification) {
  power_manager::PowerSupplyProperties proto;
  // Set the rounded value matches the critical power threshold.
  proto.set_battery_time_to_empty_sec(kCriticalMinutes * 60 + 29);
  PowerStatus::Get()->SetProtoForTesting(proto);

  SetNotificationStateForTesting(
      PowerNotificationController::NotificationState::NOTIFICATION_CRITICAL);
  battery_notification_->Update();

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
