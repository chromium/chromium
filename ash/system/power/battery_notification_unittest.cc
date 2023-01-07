// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/battery_notification.h"

#include <string>

#include "ash/strings/grit/ash_strings.h"
#include "ash/system/power/power_notification_controller.h"
#include "ash/system/power/power_status.h"
#include "ash/test/ash_test_base.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "ui/base/l10n/l10n_util.h"
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
