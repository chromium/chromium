// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/supervised/supervised_notification_controller.h"

#include "ash/session/session_controller_impl.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification_list.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"

using base::UTF8ToUTF16;
using message_center::NotificationList;

namespace ash {

// Tests handle creating their own sessions.
class SupervisedNotificationControllerTest : public NoSessionAshTestBase {
 public:
  SupervisedNotificationControllerTest() = default;
  ~SupervisedNotificationControllerTest() override = default;

 protected:
  message_center::Notification* GetPopup();

 private:
  DISALLOW_COPY_AND_ASSIGN(SupervisedNotificationControllerTest);
};

message_center::Notification* SupervisedNotificationControllerTest::GetPopup() {
  NotificationList::PopupNotifications popups =
      message_center::MessageCenter::Get()->GetPopupNotifications();
  for (NotificationList::PopupNotifications::const_iterator iter =
           popups.begin();
       iter != popups.end(); ++iter) {
    if ((*iter)->id() == SupervisedNotificationController::kNotificationId)
      return *iter;
  }
  return NULL;
}

// Verifies that when a supervised user logs in that a warning notification is
// shown and ash does not crash.
TEST_F(SupervisedNotificationControllerTest, SupervisedUserHasNotification) {
  SessionControllerImpl* session = Shell::Get()->session_controller();
  ASSERT_EQ(LoginStatus::NOT_LOGGED_IN, session->login_status());
  ASSERT_FALSE(session->IsActiveUserSessionStarted());

  // Simulate a supervised user logging in.
  TestSessionControllerClient* client = GetSessionControllerClient();
  client->Reset();
  client->AddUserSession("child@test.com", user_manager::USER_TYPE_SUPERVISED);
  client->SetSessionState(session_manager::SessionState::ACTIVE);

  // No notification because custodian email not available yet.
  message_center::Notification* notification = GetPopup();
  EXPECT_FALSE(notification);

  const std::string custodian_email = "parent1@test.com";
  const std::string custodian_email2 = "parent2@test.com";

  // Update the user session with the custodian data (which happens after the
  // profile loads).
  UserSession user_session = *session->GetUserSession(0);
  user_session.custodian_email = custodian_email;
  session->UpdateUserSession(std::move(user_session));

  // Notification is shown.
  notification = GetPopup();
  ASSERT_TRUE(notification);
  EXPECT_EQ(static_cast<int>(message_center::SYSTEM_PRIORITY),
            notification->rich_notification_data().priority);
  EXPECT_NE(base::string16::npos,
            notification->message().find(UTF8ToUTF16(custodian_email)));

  // Update the user session with new custodian data.
  user_session = *session->GetUserSession(0);
  user_session.custodian_email = custodian_email2;
  session->UpdateUserSession(std::move(user_session));

  // Notification is shown with updated message.
  notification = GetPopup();
  ASSERT_TRUE(notification);
  EXPECT_EQ(base::string16::npos,
            notification->message().find(UTF8ToUTF16(custodian_email)));
  EXPECT_NE(base::string16::npos,
            notification->message().find(UTF8ToUTF16(custodian_email2)));
}

}  // namespace ash
