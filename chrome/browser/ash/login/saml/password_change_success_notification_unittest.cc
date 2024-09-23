// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/saml/password_change_success_notification.h"

#include <optional>
#include <string>

#include "base/strings/utf_string_conversions.h"
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

class PasswordChangeSuccessNotificationTest : public testing::Test {
 protected:
  std::optional<Notification> Notification() {
    return NotificationDisplayServiceTester::Get()->GetNotification(
        "saml.password-change-success-notification");
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  NotificationDisplayServiceTester display_service_tester_{&profile_};
};

}  // namespace

TEST_F(PasswordChangeSuccessNotificationTest, ShowPasswordChangeSuccess) {
  PasswordChangeSuccessNotification::Show(&profile_);
  ASSERT_TRUE(Notification().has_value());

  EXPECT_EQ(utf16("ChromeOS password updated"), Notification()->title());
  EXPECT_EQ(utf16("Your password change was successful. Please use the new "
                  "password from now on."),
            Notification()->message());
}

}  // namespace ash
