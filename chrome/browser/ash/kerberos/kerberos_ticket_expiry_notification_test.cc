// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/kerberos/kerberos_ticket_expiry_notification.h"

#include "ash/public/cpp/notification_utils.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

using message_center::Notification;

namespace ash {

namespace {

constexpr char kUser[] = "user@EXAMPLE.COM";
constexpr char16_t kUser16[] = u"user@EXAMPLE.COM";
constexpr char kUserEmail[] = "profile@example.com";

constexpr char kNotificationId[] = "kerberos.ticket-expiry-notification";

class KerberosTicketExpiryNotificationTest : public testing::Test {
 public:
  void SetUp() override {
    message_center::MessageCenter::Initialize();
    user_ = fake_user_manager_->AddUser(AccountId::FromUserEmail(kUserEmail));
  }

  void TearDown() override {
    user_ = nullptr;
    message_center::MessageCenter::Shutdown();
  }

  void OnNotificationClick(const std::string& principal_name) {
    notification_click_count_[principal_name]++;
  }

 protected:
  const Notification* GetNotification() {
    return message_center::MessageCenter::Get()->FindVisibleNotificationById(
        CreateUserScopedNotificationId(kNotificationId,
                                       user_->username_hash()));
  }

  void Show() {
    kerberos_ticket_expiry_notification::Show(
        *user_, kUser,
        base::BindRepeating(
            &KerberosTicketExpiryNotificationTest::OnNotificationClick,
            base::Unretained(this)));
  }

  content::BrowserTaskEnvironment test_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  user_manager::TypedScopedUserManager<FakeChromeUserManager>
      fake_user_manager_{std::make_unique<FakeChromeUserManager>()};
  raw_ptr<const user_manager::User> user_ = nullptr;

  // Counts how many times a notification for a given user was clicked.
  std::map<std::string, int> notification_click_count_;
};

}  // namespace

TEST_F(KerberosTicketExpiryNotificationTest, ShowClose) {
  Show();
  ASSERT_TRUE(GetNotification());

  // Don't check the exact text here, just check if the username is there.
  EXPECT_NE(std::string::npos, GetNotification()->message().find(kUser16));

  kerberos_ticket_expiry_notification::Close(*user_);
  EXPECT_FALSE(GetNotification());
}

TEST_F(KerberosTicketExpiryNotificationTest, Click) {
  Show();
  EXPECT_EQ(0, notification_click_count_[kUser]);
  ASSERT_TRUE(GetNotification());
  message_center::MessageCenter::Get()->ClickOnNotification(
      CreateUserScopedNotificationId(kNotificationId, user_->username_hash()));
  EXPECT_EQ(1, notification_click_count_[kUser]);
}

}  // namespace ash
