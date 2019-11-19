// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_migration_guide_notification.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

using ArcMigrationGuideNotificationTest = ::testing::Test;
using ::testing::HasSubstr;
using ::testing::Not;

TEST_F(ArcMigrationGuideNotificationTest, BatteryPercent) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;

  // Set a high battery state.
  chromeos::PowerManagerClient::InitializeFake();
  auto* power_manager = chromeos::FakePowerManagerClient::Get();
  power_manager::PowerSupplyProperties props = *power_manager->GetLastStatus();
  props.set_battery_percent(99);
  power_manager->UpdatePowerProperties(props);

  // Show notification with sufficient battery.
  NotificationDisplayServiceTester notification_service(&profile);
  ShowArcMigrationGuideNotification(&profile);
  auto notifications = notification_service.GetDisplayedNotificationsForType(
      NotificationHandler::Type::TRANSIENT);
  ASSERT_EQ(1U, notifications.size());
  EXPECT_EQ("arc_fs_migration/suggest", notifications[0].id());
  base::string16 message = notifications[0].message();
  EXPECT_THAT(base::UTF16ToUTF8(notifications[0].message()),
              Not(HasSubstr("charge")));

  // Set a low battery state.
  props.set_battery_percent(5);
  power_manager->UpdatePowerProperties(props);

  // Show notification with low battery.
  ShowArcMigrationGuideNotification(&profile);
  notifications = notification_service.GetDisplayedNotificationsForType(
      NotificationHandler::Type::TRANSIENT);
  ASSERT_EQ(1U, notifications.size());
  EXPECT_EQ("arc_fs_migration/suggest", notifications[0].id());
  EXPECT_NE(message, notifications[0].message());
  EXPECT_THAT(base::UTF16ToUTF8(notifications[0].message()),
              HasSubstr("charge"));

  chromeos::PowerManagerClient::Shutdown();
}

}  // namespace arc
