// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/arc_migration_guide_notification.h"

#include <memory>

#include "ash/public/cpp/notification_utils.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

namespace arc {

using ::testing::HasSubstr;
using ::testing::Not;

class ArcMigrationGuideNotificationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    message_center::MessageCenter::Initialize();
    user_session_test_environment_ =
        std::make_unique<ash::test::UserSessionTestEnvironment>(
            TestingBrowserProcess::GetGlobal()->local_state());
    user_ = user_session_test_environment_->AddRegularUser(
        user_manager::StubAccountId());
    ASSERT_TRUE(user_);
    user_session_test_environment_->LogIn(user_->GetAccountId());
  }

  void TearDown() override {
    user_ = nullptr;
    user_session_test_environment_.reset();
    message_center::MessageCenter::Shutdown();
  }

  const message_center::Notification* FindNotification() {
    return message_center::MessageCenter::Get()->FindVisibleNotificationById(
        ash::CreateUserScopedNotificationId(kSuggestNotificationId,
                                            user_->username_hash()));
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<ash::test::UserSessionTestEnvironment>
      user_session_test_environment_;
  raw_ptr<const user_manager::User> user_ = nullptr;
};

TEST_F(ArcMigrationGuideNotificationTest, BatteryPercent) {
  // Set a high battery state.
  chromeos::PowerManagerClient::InitializeFake();
  auto* power_manager = chromeos::FakePowerManagerClient::Get();
  power_manager::PowerSupplyProperties props = *power_manager->GetLastStatus();
  props.set_battery_percent(99);
  power_manager->UpdatePowerProperties(props);

  // Show notification with sufficient battery.
  ShowArcMigrationGuideNotification(*user_);
  EXPECT_EQ(
      1U,
      message_center::MessageCenter::Get()->GetVisibleNotifications().size());
  const message_center::Notification* notification = FindNotification();
  ASSERT_TRUE(notification);
  std::u16string message = notification->message();
  EXPECT_THAT(base::UTF16ToUTF8(notification->message()),
              Not(HasSubstr("charge")));

  // Set a low battery state.
  props.set_battery_percent(5);
  power_manager->UpdatePowerProperties(props);

  // Show notification with low battery.
  ShowArcMigrationGuideNotification(*user_);
  EXPECT_EQ(
      1U,
      message_center::MessageCenter::Get()->GetVisibleNotifications().size());
  notification = FindNotification();
  ASSERT_TRUE(notification);
  EXPECT_NE(message, notification->message());
  EXPECT_THAT(base::UTF16ToUTF8(notification->message()), HasSubstr("charge"));

  chromeos::PowerManagerClient::Shutdown();
}

}  // namespace arc
