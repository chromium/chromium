// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/saml/password_expiry_notification.h"

#include <optional>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using ::message_center::Notification;

inline std::u16string utf16(const char* ascii) {
  return base::ASCIIToUTF16(ascii);
}

inline std::u16string GetTitleText(base::TimeDelta time_until_expiry) {
  return PasswordExpiryNotification::GetTitleText(time_until_expiry);
}

class PasswordExpiryNotificationTest : public testing::Test {
 protected:
  std::optional<Notification> Notification() {
    return NotificationDisplayServiceTester::Get()->GetNotification(
        "saml.password-expiry-notification");
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  NotificationDisplayServiceTester display_service_tester_{&profile_};
};

}  // namespace

TEST_F(PasswordExpiryNotificationTest, ShowWillSoonExpire) {
  PasswordExpiryNotification::Show(&profile_, base::Days(14));
  ASSERT_TRUE(Notification().has_value());

  EXPECT_EQ(utf16("Password expires in 14 days"), Notification()->title());
  EXPECT_EQ(utf16("Choose a new one now"), Notification()->message());

  PasswordExpiryNotification::Dismiss(&profile_);
  EXPECT_FALSE(Notification().has_value());
}

TEST_F(PasswordExpiryNotificationTest, ShowAlreadyExpired) {
  PasswordExpiryNotification::Show(&profile_, base::Days(0));
  ASSERT_TRUE(Notification().has_value());

  EXPECT_EQ(utf16("Password change overdue"), Notification()->title());
  EXPECT_EQ(utf16("Choose a new one now"), Notification()->message());

  PasswordExpiryNotification::Dismiss(&profile_);
  EXPECT_FALSE(Notification().has_value());
}

TEST_F(PasswordExpiryNotificationTest, GetTitleText) {
  EXPECT_EQ(utf16("Password expires in 2 days"), GetTitleText(base::Days(2)));
  EXPECT_EQ(utf16("Password expires in 1 day"), GetTitleText(base::Days(1)));
  EXPECT_EQ(utf16("Password expires in 12 hours"),
            GetTitleText(base::Hours(12)));
  EXPECT_EQ(utf16("Password expires in 1 hour"), GetTitleText(base::Hours(1)));
  EXPECT_EQ(utf16("Password expires in 30 minutes"),
            GetTitleText(base::Minutes(30)));
  EXPECT_EQ(utf16("Password expires in 1 minute"),
            GetTitleText(base::Minutes(1)));

  EXPECT_EQ(utf16("Password change overdue"), GetTitleText(base::Seconds(30)));
  EXPECT_EQ(utf16("Password change overdue"), GetTitleText(base::Seconds(0)));
  EXPECT_EQ(utf16("Password change overdue"), GetTitleText(base::Seconds(-10)));

  PasswordExpiryNotification::Dismiss(&profile_);
  EXPECT_FALSE(Notification().has_value());
}

}  // namespace ash
