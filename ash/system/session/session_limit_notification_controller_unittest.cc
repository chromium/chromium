// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/session/session_limit_notification_controller.h"

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"

namespace ash {

class SessionLimitNotificationControllerTest : public AshTestBase {
 public:
  SessionLimitNotificationControllerTest() = default;
  ~SessionLimitNotificationControllerTest() override = default;

 protected:
  static const int kNotificationThresholdInMinutes = 60;

  void UpdateSessionLengthLimitInMin(int mins) {
    Shell::Get()->session_controller()->SetSessionLengthLimit(
        base::TimeDelta::FromMinutes(mins), base::TimeTicks::Now());
  }

  message_center::Notification* GetNotification() {
    const message_center::NotificationList::Notifications& notifications =
        message_center::MessageCenter::Get()->GetVisibleNotifications();
    for (message_center::NotificationList::Notifications::const_iterator iter =
             notifications.begin();
         iter != notifications.end(); ++iter) {
      if ((*iter)->id() == SessionLimitNotificationController::kNotificationId)
        return *iter;
    }
    return nullptr;
  }

  void ClearSessionLengthLimit() {
    Shell::Get()->session_controller()->SetSessionLengthLimit(
        base::TimeDelta(), base::TimeTicks());
  }

  void RemoveNotification() {
    message_center::MessageCenter::Get()->RemoveNotification(
        SessionLimitNotificationController::kNotificationId,
        false /* by_user */);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionLimitNotificationControllerTest);
};

TEST_F(SessionLimitNotificationControllerTest, Notification) {
  // No notifications when no session limit.
  EXPECT_FALSE(GetNotification());

  // Limit is 15 min.
  UpdateSessionLengthLimitInMin(15);
  message_center::Notification* notification = GetNotification();
  EXPECT_TRUE(notification);
  EXPECT_EQ(message_center::SYSTEM_PRIORITY, notification->priority());
  base::string16 first_title = notification->title();
  // Should read the content.
  EXPECT_TRUE(notification->rich_notification_data()
                  .should_make_spoken_feedback_for_popup_updates);

  // Limit is 10 min.
  UpdateSessionLengthLimitInMin(10);
  notification = GetNotification();
  EXPECT_TRUE(notification);
  EXPECT_EQ(message_center::SYSTEM_PRIORITY, notification->priority());
  // The title should be updated.
  EXPECT_NE(first_title, notification->title());
  // Should NOT read, because just update the remaining time.
  EXPECT_FALSE(notification->rich_notification_data()
                   .should_make_spoken_feedback_for_popup_updates);

  // Limit is 3 min.
  UpdateSessionLengthLimitInMin(3);
  notification = GetNotification();
  EXPECT_TRUE(notification);
  EXPECT_EQ(message_center::SYSTEM_PRIORITY, notification->priority());
  // Should read the content again because the state has changed.
  EXPECT_TRUE(notification->rich_notification_data()
                  .should_make_spoken_feedback_for_popup_updates);

  // Session length limit is updated to longer: 15 min.
  UpdateSessionLengthLimitInMin(15);
  notification = GetNotification();
  EXPECT_TRUE(notification);
  EXPECT_EQ(message_center::SYSTEM_PRIORITY, notification->priority());
  // Should read again because an increase of the remaining time is noteworthy.
  EXPECT_TRUE(notification->rich_notification_data()
                  .should_make_spoken_feedback_for_popup_updates);

  // Clears the limit: the notification should be gone.
  ClearSessionLengthLimit();
  EXPECT_FALSE(GetNotification());
}

TEST_F(SessionLimitNotificationControllerTest, FarOffNotificationHidden) {
  // Test that notification is not shown if the session end time is far off into
  // the future, but an item should be present in system tray bubble.

  // Notification should be absent.
  EXPECT_FALSE(GetNotification());
  UpdateSessionLengthLimitInMin(kNotificationThresholdInMinutes + 10);
  EXPECT_FALSE(GetNotification());

  RemoveNotification();
}

TEST_F(SessionLimitNotificationControllerTest,
       NotificationShownAfterThreshold) {
  // Test that a notification is shown when time runs under the notification
  // display threshold.

  // Start with a generous session length. We should not get a notification.
  UpdateSessionLengthLimitInMin(kNotificationThresholdInMinutes + 10);
  EXPECT_FALSE(GetNotification());

  // Update the session length now, without changing limit_state_.
  UpdateSessionLengthLimitInMin(kNotificationThresholdInMinutes - 1);

  // A notification should be displayed now.
  EXPECT_TRUE(GetNotification());

  RemoveNotification();
}

TEST_F(SessionLimitNotificationControllerTest, RemoveNotification) {
  // Limit is 15 min.
  UpdateSessionLengthLimitInMin(15);
  EXPECT_TRUE(GetNotification());

  // Removes the notification.
  RemoveNotification();
  EXPECT_FALSE(GetNotification());

  // Limit is 10 min. The notification should not re-appear.
  UpdateSessionLengthLimitInMin(10);
  EXPECT_FALSE(GetNotification());

  // Limit is 3 min. The notification should re-appear and should be re-read
  // because of state change.
  UpdateSessionLengthLimitInMin(3);
  message_center::Notification* notification = GetNotification();
  EXPECT_TRUE(notification);
  EXPECT_TRUE(notification->rich_notification_data()
                  .should_make_spoken_feedback_for_popup_updates);

  RemoveNotification();

  // Session length limit is updated to longer state. Notification should
  // re-appear and be re-read.
  UpdateSessionLengthLimitInMin(15);
  notification = GetNotification();
  EXPECT_TRUE(notification);
  EXPECT_TRUE(notification->rich_notification_data()
                  .should_make_spoken_feedback_for_popup_updates);

  RemoveNotification();
}

class SessionLimitNotificationControllerLoginTest
    : public SessionLimitNotificationControllerTest {
 public:
  SessionLimitNotificationControllerLoginTest() { set_start_session(false); }

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionLimitNotificationControllerLoginTest);
};

TEST_F(SessionLimitNotificationControllerLoginTest,
       NotificationShownAfterLogin) {
  UpdateSessionLengthLimitInMin(15);

  // No notifications before login.
  EXPECT_FALSE(GetNotification());

  // Notification is shown after login.
  CreateUserSessions(1);
  EXPECT_TRUE(GetNotification());

  RemoveNotification();
}

TEST_F(SessionLimitNotificationControllerLoginTest,
       FarOffNotificationHiddenAfterLogin) {
  // Test that notification is not shown if the session end time is far off into
  // the future, but an item should be present in system tray bubble.

  // Notification should be absent.
  UpdateSessionLengthLimitInMin(kNotificationThresholdInMinutes + 10);
  CreateUserSessions(1);
  EXPECT_FALSE(GetNotification());

  RemoveNotification();
}

}  // namespace ash
