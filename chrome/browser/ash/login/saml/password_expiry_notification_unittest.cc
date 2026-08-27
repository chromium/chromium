// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/saml/password_expiry_notification.h"

#include <memory>
#include <string>

#include "ash/public/cpp/notification_utils.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

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
 public:
  void SetUp() override {
    message_center::MessageCenter::Initialize();
    user_ = fake_user_manager_->AddUser(user_manager::StubAccountId());
  }

  void TearDown() override {
    user_ = nullptr;
    message_center::MessageCenter::Shutdown();
  }

 protected:
  const Notification* Notification() {
    return message_center::MessageCenter::Get()->FindNotificationById(
        CreateUserScopedNotificationId("saml.password-expiry-notification",
                                       user_->username_hash()));
  }

  content::BrowserTaskEnvironment task_environment_;
  user_manager::TypedScopedUserManager<FakeChromeUserManager>
      fake_user_manager_{std::make_unique<FakeChromeUserManager>()};
  raw_ptr<const user_manager::User> user_ = nullptr;
};

}  // namespace

TEST_F(PasswordExpiryNotificationTest, ShowWillSoonExpire) {
  PasswordExpiryNotification::Show(*user_, base::Days(14));
  ASSERT_TRUE(Notification());

  EXPECT_EQ(utf16("Password expires in 14 days"), Notification()->title());
  EXPECT_EQ(utf16("Choose a new one now"), Notification()->message());

  PasswordExpiryNotification::Dismiss(*user_);
  EXPECT_FALSE(Notification());
}

TEST_F(PasswordExpiryNotificationTest, ShowAlreadyExpired) {
  PasswordExpiryNotification::Show(*user_, base::Days(0));
  ASSERT_TRUE(Notification());

  EXPECT_EQ(utf16("Password change overdue"), Notification()->title());
  EXPECT_EQ(utf16("Choose a new one now"), Notification()->message());

  PasswordExpiryNotification::Dismiss(*user_);
  EXPECT_FALSE(Notification());
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

  PasswordExpiryNotification::Dismiss(*user_);
  EXPECT_FALSE(Notification());
}

}  // namespace ash
