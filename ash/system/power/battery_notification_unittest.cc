// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/battery_notification.h"

#include <string>

#include "ash/strings/grit/ash_strings.h"
#include "ash/system/power/power_notification_controller.h"
#include "ash/system/power/power_status.h"
#include "ash/test/ash_test_base.h"
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

class BatteryNotificationTest : public AshTestBase {
 public:
  BatteryNotificationTest() = default;
  BatteryNotificationTest(const BatteryNotificationTest&) = delete;
  BatteryNotificationTest& operator=(const BatteryNotificationTest&) = delete;
  ~BatteryNotificationTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    battery_notification_ = std::make_unique<BatteryNotification>(
        message_center::MessageCenter::Get(),
        PowerNotificationController::NotificationState::NOTIFICATION_LOW_POWER);
  }

  void TearDown() override {
    battery_notification_.reset();
    AshTestBase::TearDown();
  }

  message_center::Notification* GetBatteryNotification() {
    return message_center::MessageCenter::Get()->FindNotificationById(
        BatteryNotification::kNotificationId);
  }

 protected:
  std::unique_ptr<BatteryNotification> battery_notification_;
};

TEST_F(BatteryNotificationTest, LowPowerNotification) {
  power_manager::PowerSupplyProperties proto;
  // Set the rounded value matches the low power threshold.
  proto.set_battery_time_to_empty_sec(kLowPowerMinutes * 60 + 29);
  PowerStatus::Get()->SetProtoForTesting(proto);

  battery_notification_->Update(
      PowerNotificationController::NotificationState::NOTIFICATION_LOW_POWER);

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

void EnableBatterySaverMode() {
  power_manager::SetBatterySaverModeStateRequest request;
  request.set_enabled(true);
  chromeos::FakePowerManagerClient::Get()->SetBatterySaverModeState(request);
  base::RunLoop().RunUntilIdle();
}

void VerifyBatterySaverModeNotActive(
    base::RunLoop* run_loop,
    absl::optional<power_manager::BatterySaverModeState> state) {
  ASSERT_TRUE(state);
  EXPECT_FALSE(state->enabled());
  run_loop->Quit();
}

TEST_F(BatteryNotificationTest, LowPowerBatterySaverNotification) {
  // Set the rounded value matches the low power threshold.
  power_manager::PowerSupplyProperties proto;
  proto.set_battery_time_to_empty_sec(kLowPowerMinutes * 60 + 29);
  PowerStatus::Get()->SetProtoForTesting(proto);
  chromeos::FakePowerManagerClient::Get()->UpdatePowerProperties(proto);
  const PowerStatus& status = *PowerStatus::Get();

  // Compute remaining battery time string
  const std::u16string time_remaining = ui::TimeFormat::Simple(
      ui::TimeFormat::FORMAT_DURATION, ui::TimeFormat::LENGTH_LONG,
      *status.GetBatteryTimeToEmpty());

  // Define expected values.
  const size_t kExpectedButtonSize = 1;
  const message_center::SystemNotificationWarningLevel kExpectedWarningLevel =
      message_center::SystemNotificationWarningLevel::WARNING;
  const message_center::FullscreenVisibility kExpectedFullscreenVisibility =
      message_center::FullscreenVisibility::OVER_USER;
  const std::u16string kExpectedTitle =
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_LOW_BATTERY_BSM_TITLE);
  const std::u16string kExpectedMessage = l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_LOW_BATTERY_BSM_MESSAGE, time_remaining,
      base::NumberToString16(status.GetRoundedBatteryPercent()));
  const std::u16string kExpectedButtonTitle =
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_LOW_BATTERY_BSM_BUTTON);

  EnableBatterySaverMode();

  // Display notification.
  battery_notification_->Update(
      PowerNotificationController::NotificationState::NOTIFICATION_LOW_POWER);

  auto* notification = GetBatteryNotification();
  ASSERT_TRUE(notification);

  // Test expectations against actual values.
  EXPECT_EQ(kExpectedWarningLevel,
            notification->system_notification_warning_level());
  EXPECT_EQ(kExpectedTitle, notification->title());
  EXPECT_EQ(kExpectedMessage, notification->message());
  EXPECT_EQ(kExpectedFullscreenVisibility,
            notification->fullscreen_visibility());
  EXPECT_FALSE(notification->pinned());
  EXPECT_EQ(kExpectedButtonSize, notification->buttons().size());
  EXPECT_EQ(kExpectedButtonTitle, notification->buttons()[0].title);

  // Click the button to turn off battery saver mode.
  notification->delegate()->Click(0, absl::nullopt);

  // Verify battery saver mode is off.
  base::RunLoop run_loop;
  chromeos::PowerManagerClient::Get()->GetBatterySaverModeState(
      base::BindOnce(&VerifyBatterySaverModeNotActive, &run_loop));
  run_loop.Run();
}

TEST_F(BatteryNotificationTest, CriticalPowerNotification) {
  power_manager::PowerSupplyProperties proto;
  // Set the rounded value matches the critical power threshold.
  proto.set_battery_time_to_empty_sec(kCriticalMinutes * 60 + 29);
  PowerStatus::Get()->SetProtoForTesting(proto);

  battery_notification_->Update(
      PowerNotificationController::NotificationState::NOTIFICATION_CRITICAL);

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
