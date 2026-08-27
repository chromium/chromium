// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/saml/password_change_success_notification.h"

#include <memory>
#include <string>

#include "ash/public/cpp/notification_utils.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
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

inline std::u16string utf16(const char* ascii) {
  return base::ASCIIToUTF16(ascii);
}

class PasswordChangeSuccessNotificationTest : public testing::Test {
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
  const message_center::Notification* Notification() {
    return message_center::MessageCenter::Get()->FindNotificationById(
        CreateUserScopedNotificationId(
            "saml.password-change-success-notification",
            user_->username_hash()));
  }

  content::BrowserTaskEnvironment task_environment_;
  user_manager::TypedScopedUserManager<FakeChromeUserManager>
      fake_user_manager_{std::make_unique<FakeChromeUserManager>()};
  raw_ptr<const user_manager::User> user_ = nullptr;
};

}  // namespace

TEST_F(PasswordChangeSuccessNotificationTest, ShowPasswordChangeSuccess) {
  PasswordChangeSuccessNotification::Show(*user_);
  ASSERT_TRUE(Notification());

  EXPECT_EQ(utf16("ChromeOS password updated"), Notification()->title());
  EXPECT_EQ(utf16("Your password change was successful. Please use the new "
                  "password from now on."),
            Notification()->message());
}

}  // namespace ash
