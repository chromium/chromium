// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/lock_screen_notification_controller.h"

#include "ash/system/notification_center/notification_center_test_api.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "ui/message_center/message_center.h"

namespace ash {

class LockScreenNotificationControllerTest
    : public AshTestBase,
      public testing::WithParamInterface<bool> {
 public:
  LockScreenNotificationControllerTest() = default;
  LockScreenNotificationControllerTest(
      const LockScreenNotificationControllerTest&) = delete;
  LockScreenNotificationControllerTest& operator=(
      const LockScreenNotificationControllerTest&) = delete;
  ~LockScreenNotificationControllerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    test_api_ = std::make_unique<NotificationCenterTestApi>();
  }

  bool IsLockScreenNotificationPresent() {
    return message_center::MessageCenter::Get()->FindNotificationById(
        LockScreenNotificationController::kLockScreenNotificationId);
  }

  NotificationCenterTestApi* test_api() { return test_api_.get(); }

 private:
  std::unique_ptr<NotificationCenterTestApi> test_api_;
};

// Tests that locking/unlocking the screen adds/removes the lockscreen
// notification if notifications are hidden.
TEST_F(LockScreenNotificationControllerTest,
       NoLockScreenNotificationWithoutHiddenNotifications) {
  // The notification should not exist on an unlocked screen.
  ASSERT_FALSE(IsLockScreenNotificationPresent());

  // The notification should not exist on the lockscreen either because there is
  // no hidden notification.
  BlockUserSession(BLOCKED_BY_LOCK_SCREEN);
  EXPECT_FALSE(IsLockScreenNotificationPresent());
}

TEST_F(LockScreenNotificationControllerTest,
       LockScreenNotificationWithHiddenNotification) {
  // Locking the screen with a hidden notification should cause the lockscreen
  // notification to show up.
  test_api()->AddNotification();
  BlockUserSession(BLOCKED_BY_LOCK_SCREEN);
  EXPECT_TRUE(IsLockScreenNotificationPresent());

  // Unlocking the screen should result in the lockscreen notification being
  // removed.
  UnblockUserSession();
  EXPECT_FALSE(IsLockScreenNotificationPresent());
}

TEST_F(LockScreenNotificationControllerTest,
       NotificationNotShownWithSystemNotifications) {
  // A system notification should not trigger the lockscreen notification since
  // it's not hidden.
  test_api()->AddSystemNotification();
  BlockUserSession(BLOCKED_BY_LOCK_SCREEN);
  EXPECT_FALSE(IsLockScreenNotificationPresent());
}

TEST_F(LockScreenNotificationControllerTest, NotificationsAddedOnLockScreen) {
  BlockUserSession(BLOCKED_BY_LOCK_SCREEN);
  EXPECT_FALSE(IsLockScreenNotificationPresent());

  // Adding a system notification on the lockscreen should have no effect.
  test_api()->AddSystemNotification();
  EXPECT_FALSE(IsLockScreenNotificationPresent());

  // Adding a normal notification should trigger the lock screen notification.
  test_api()->AddNotification();
  EXPECT_TRUE(IsLockScreenNotificationPresent());
}

TEST_F(LockScreenNotificationControllerTest, NotificationsRemovedOnLockScreen) {
  std::string id, system_id;
  system_id = test_api()->AddSystemNotification();
  id = test_api()->AddNotification();

  BlockUserSession(BLOCKED_BY_LOCK_SCREEN);
  EXPECT_TRUE(IsLockScreenNotificationPresent());

  // Removing `id` should remove the lock screen notification since there are no
  // hidden notifications left.
  test_api()->RemoveNotification(id);
  EXPECT_FALSE(IsLockScreenNotificationPresent());
}

}  // namespace ash
