// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/inactive_user_notification_blocker.h"

#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/system/message_center/message_center_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "components/account_id/account_id.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {

namespace {

using base::UTF8ToUTF16;

const char kNotifierSystemPriority[] = "ash.some-high-priority-component";

class InactiveUserNotificationBlockerTest
    : public NoSessionAshTestBase,
      public message_center::NotificationBlocker::Observer {
 public:
  InactiveUserNotificationBlockerTest() = default;
  ~InactiveUserNotificationBlockerTest() override = default;

  // AshTestBase overrides:
  void SetUp() override {
    AshTestBase::SetUp();

    blocker_ = ShellTestApi()
                   .message_center_controller()
                   ->inactive_user_notification_blocker_for_testing();
    blocker_->AddObserver(this);
  }

  void TearDown() override {
    blocker_->RemoveObserver(this);
    AshTestBase::TearDown();
  }

  // message_center::NotificationBlocker::Observer ovverrides:
  void OnBlockingStateChanged(
      message_center::NotificationBlocker* blocker) override {
    state_changed_count_++;
  }

 protected:
  const std::string GetDefaultUserId() { return "user0@tray"; }

  void AddUserSession(const std::string& email) {
    GetSessionControllerClient()->AddUserSession(email);
  }

  void SwitchActiveUser(const std::string& email) {
    const AccountId account_id(AccountId::FromUserEmail(email));
    GetSessionControllerClient()->SwitchActiveUser(account_id);
  }

  int GetStateChangedCountAndReset() {
    int result = state_changed_count_;
    state_changed_count_ = 0;
    return result;
  }

  bool ShouldShowAsPopup(const message_center::NotifierId& notifier_id,
                         const std::string& profile_id) {
    message_center::NotifierId id_with_profile = notifier_id;
    id_with_profile.profile_id = profile_id;

    message_center::Notification notification(
        message_center::NOTIFICATION_TYPE_SIMPLE, "popup-id",
        UTF8ToUTF16("popup-title"), UTF8ToUTF16("popup-message"), gfx::Image(),
        UTF8ToUTF16("popup-source"), GURL(), id_with_profile,
        message_center::RichNotificationData(), nullptr);

    if (notifier_id.id == kNotifierSystemPriority)
      notification.set_priority(message_center::SYSTEM_PRIORITY);

    return blocker_->ShouldShowNotificationAsPopup(notification);
  }

  bool ShouldShow(const message_center::NotifierId& notifier_id,
                  const std::string& profile_id) {
    message_center::NotifierId id_with_profile = notifier_id;
    id_with_profile.profile_id = profile_id;

    message_center::Notification notification(
        message_center::NOTIFICATION_TYPE_SIMPLE, "notification-id",
        UTF8ToUTF16("notification-title"), UTF8ToUTF16("notification-message"),
        gfx::Image(), UTF8ToUTF16("notification-source"), GURL(),
        id_with_profile, message_center::RichNotificationData(), nullptr);

    if (notifier_id.id == kNotifierSystemPriority)
      notification.set_priority(message_center::SYSTEM_PRIORITY);

    return blocker_->ShouldShowNotification(notification);
  }

 private:
  int state_changed_count_ = 0;
  InactiveUserNotificationBlocker* blocker_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(InactiveUserNotificationBlockerTest);
};

TEST_F(InactiveUserNotificationBlockerTest, Basic) {
  message_center::NotifierId notifier_id(
      message_center::NotifierType::APPLICATION, "test-app");
  // System priority notifiers should always show regardless of fullscreen
  // or lock state.
  message_center::NotifierId ash_system_notifier(
      message_center::NotifierType::SYSTEM_COMPONENT, kNotifierSystemPriority);
  // Other system notifiers should be treated as same as a normal notifier.
  message_center::NotifierId random_system_notifier(
      message_center::NotifierType::SYSTEM_COMPONENT,
      "ash.some-other-component");

  // Notifications are not blocked before login.
  const std::string kEmptyUserId;
  EXPECT_TRUE(ShouldShow(notifier_id, kEmptyUserId));
  EXPECT_TRUE(ShouldShow(ash_system_notifier, kEmptyUserId));
  EXPECT_TRUE(ShouldShow(random_system_notifier, kEmptyUserId));

  // Login triggers blocking state change.
  SimulateUserLogin(GetDefaultUserId());
  EXPECT_EQ(1, GetStateChangedCountAndReset());

  // Notifications for a single user session are not blocked.
  EXPECT_TRUE(ShouldShow(ash_system_notifier, GetDefaultUserId()));
  EXPECT_TRUE(ShouldShow(notifier_id, GetDefaultUserId()));
  EXPECT_TRUE(ShouldShow(random_system_notifier, GetDefaultUserId()));

  // Notifications from a user other than the active one (in this case, default)
  // are generally blocked unless they're ash system notifications.
  AddUserSession("user1@tray");
  EXPECT_EQ(0, GetStateChangedCountAndReset());
  const std::string kInvalidUserId("invalid");
  EXPECT_FALSE(ShouldShowAsPopup(notifier_id, kInvalidUserId));
  EXPECT_TRUE(ShouldShowAsPopup(ash_system_notifier, kEmptyUserId));
  EXPECT_FALSE(ShouldShowAsPopup(random_system_notifier, kInvalidUserId));
  EXPECT_TRUE(ShouldShowAsPopup(notifier_id, GetDefaultUserId()));
  EXPECT_FALSE(ShouldShowAsPopup(notifier_id, "user1@tray"));
  EXPECT_TRUE(ShouldShowAsPopup(random_system_notifier, GetDefaultUserId()));
  EXPECT_FALSE(ShouldShowAsPopup(random_system_notifier, "user1@tray"));
  EXPECT_FALSE(ShouldShow(notifier_id, kInvalidUserId));
  EXPECT_TRUE(ShouldShow(ash_system_notifier, kEmptyUserId));
  EXPECT_FALSE(ShouldShow(random_system_notifier, kInvalidUserId));
  EXPECT_TRUE(ShouldShow(notifier_id, GetDefaultUserId()));
  EXPECT_FALSE(ShouldShow(notifier_id, "user1@tray"));
  EXPECT_TRUE(ShouldShow(random_system_notifier, GetDefaultUserId()));
  EXPECT_FALSE(ShouldShow(random_system_notifier, "user1@tray"));

  // Activate the second user and make sure the original/default user's
  // notifications are now hidden.
  SwitchActiveUser("user1@tray");
  EXPECT_FALSE(ShouldShowAsPopup(notifier_id, kInvalidUserId));
  EXPECT_TRUE(ShouldShowAsPopup(ash_system_notifier, kEmptyUserId));
  EXPECT_FALSE(ShouldShowAsPopup(random_system_notifier, kInvalidUserId));
  EXPECT_FALSE(ShouldShowAsPopup(notifier_id, GetDefaultUserId()));
  EXPECT_TRUE(ShouldShowAsPopup(notifier_id, "user1@tray"));
  EXPECT_FALSE(ShouldShowAsPopup(random_system_notifier, GetDefaultUserId()));
  EXPECT_TRUE(ShouldShowAsPopup(random_system_notifier, "user1@tray"));
  EXPECT_FALSE(ShouldShow(notifier_id, kInvalidUserId));
  EXPECT_TRUE(ShouldShow(ash_system_notifier, kEmptyUserId));
  EXPECT_FALSE(ShouldShow(random_system_notifier, kInvalidUserId));
  EXPECT_FALSE(ShouldShow(notifier_id, GetDefaultUserId()));
  EXPECT_TRUE(ShouldShow(notifier_id, "user1@tray"));
  EXPECT_FALSE(ShouldShow(random_system_notifier, GetDefaultUserId()));
  EXPECT_TRUE(ShouldShow(random_system_notifier, "user1@tray"));

  // Switch back and verify the active user's notifications are once again
  // shown.
  SwitchActiveUser(GetDefaultUserId());
  EXPECT_FALSE(ShouldShowAsPopup(notifier_id, kInvalidUserId));
  EXPECT_TRUE(ShouldShowAsPopup(ash_system_notifier, kEmptyUserId));
  EXPECT_FALSE(ShouldShowAsPopup(random_system_notifier, kInvalidUserId));
  EXPECT_TRUE(ShouldShowAsPopup(notifier_id, GetDefaultUserId()));
  EXPECT_FALSE(ShouldShowAsPopup(notifier_id, "user1@tray"));
  EXPECT_TRUE(ShouldShowAsPopup(random_system_notifier, GetDefaultUserId()));
  EXPECT_FALSE(ShouldShowAsPopup(random_system_notifier, "user1@tray"));
  EXPECT_FALSE(ShouldShow(notifier_id, kInvalidUserId));
  EXPECT_TRUE(ShouldShow(ash_system_notifier, kEmptyUserId));
  EXPECT_FALSE(ShouldShow(random_system_notifier, kInvalidUserId));
  EXPECT_TRUE(ShouldShow(notifier_id, GetDefaultUserId()));
  EXPECT_FALSE(ShouldShow(notifier_id, "user1@tray"));
  EXPECT_TRUE(ShouldShow(random_system_notifier, GetDefaultUserId()));
  EXPECT_FALSE(ShouldShow(random_system_notifier, "user1@tray"));
}

}  // namespace

}  // namespace ash
