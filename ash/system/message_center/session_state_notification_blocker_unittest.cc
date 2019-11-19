// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/session_state_notification_blocker.h"

#include <memory>

#include "ash/session/test_session_controller_client.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

using base::UTF8ToUTF16;
using session_manager::SessionState;

namespace ash {

namespace {

const char kNotifierSystemPriority[] = "ash.some-high-priority-component";

class SessionStateNotificationBlockerTest
    : public NoSessionAshTestBase,
      public message_center::NotificationBlocker::Observer {
 public:
  SessionStateNotificationBlockerTest() = default;
  ~SessionStateNotificationBlockerTest() override = default;

  // tests::AshTestBase overrides:
  void SetUp() override {
    NoSessionAshTestBase::SetUp();
    blocker_.reset(new SessionStateNotificationBlocker(
        message_center::MessageCenter::Get()));
    blocker_->AddObserver(this);
  }

  void TearDown() override {
    blocker_->RemoveObserver(this);
    blocker_.reset();
    NoSessionAshTestBase::TearDown();
  }

  // message_center::NotificationBlocker::Observer overrides:
  void OnBlockingStateChanged(
      message_center::NotificationBlocker* blocker) override {
    state_changed_count_++;
  }

  int GetStateChangedCountAndReset() {
    int result = state_changed_count_;
    state_changed_count_ = 0;
    return result;
  }

  bool ShouldShowNotificationAsPopup(
      const message_center::NotifierId& notifier_id) {
    message_center::Notification notification(
        message_center::NOTIFICATION_TYPE_SIMPLE, "chromeos-id",
        UTF8ToUTF16("chromeos-title"), UTF8ToUTF16("chromeos-message"),
        gfx::Image(), UTF8ToUTF16("chromeos-source"), GURL(), notifier_id,
        message_center::RichNotificationData(), nullptr);
    if (notifier_id.id == kNotifierSystemPriority)
      notification.set_priority(message_center::SYSTEM_PRIORITY);
    return blocker_->ShouldShowNotificationAsPopup(notification);
  }

  void SetLockedState(bool locked) {
    GetSessionControllerClient()->SetSessionState(
        locked ? SessionState::LOCKED : SessionState::ACTIVE);
  }

 private:
  int state_changed_count_ = 0;
  std::unique_ptr<message_center::NotificationBlocker> blocker_;

  DISALLOW_COPY_AND_ASSIGN(SessionStateNotificationBlockerTest);
};

TEST_F(SessionStateNotificationBlockerTest, BaseTest) {
  // Default status: OOBE.
  message_center::NotifierId notifier_id(
      message_center::NotifierType::APPLICATION, "test-notifier");
  EXPECT_FALSE(ShouldShowNotificationAsPopup(notifier_id));

  // Login screen.
  GetSessionControllerClient()->SetSessionState(SessionState::LOGIN_PRIMARY);
  EXPECT_EQ(0, GetStateChangedCountAndReset());
  EXPECT_FALSE(ShouldShowNotificationAsPopup(notifier_id));

  // Logged in as a normal user.
  SimulateUserLogin("user@test.com");
  EXPECT_EQ(1, GetStateChangedCountAndReset());
  EXPECT_TRUE(ShouldShowNotificationAsPopup(notifier_id));

  // Lock.
  SetLockedState(true);
  EXPECT_EQ(1, GetStateChangedCountAndReset());
  EXPECT_FALSE(ShouldShowNotificationAsPopup(notifier_id));

  // Unlock.
  SetLockedState(false);
  EXPECT_EQ(1, GetStateChangedCountAndReset());
  EXPECT_TRUE(ShouldShowNotificationAsPopup(notifier_id));
}

TEST_F(SessionStateNotificationBlockerTest, AlwaysAllowedNotifier) {
  // NOTIFIER_DISPLAY is allowed to shown in the login screen.
  message_center::NotifierId notifier_id(
      message_center::NotifierType::SYSTEM_COMPONENT, kNotifierSystemPriority);

  // Default status: OOBE.
  EXPECT_TRUE(ShouldShowNotificationAsPopup(notifier_id));

  // Login screen.
  GetSessionControllerClient()->SetSessionState(SessionState::LOGIN_PRIMARY);
  EXPECT_EQ(0, GetStateChangedCountAndReset());
  EXPECT_TRUE(ShouldShowNotificationAsPopup(notifier_id));

  // Logged in as a normal user.
  SimulateUserLogin("user@test.com");
  EXPECT_EQ(1, GetStateChangedCountAndReset());
  EXPECT_TRUE(ShouldShowNotificationAsPopup(notifier_id));

  // Lock.
  SetLockedState(true);
  EXPECT_EQ(1, GetStateChangedCountAndReset());
  EXPECT_TRUE(ShouldShowNotificationAsPopup(notifier_id));

  // Unlock.
  SetLockedState(false);
  EXPECT_EQ(1, GetStateChangedCountAndReset());
  EXPECT_TRUE(ShouldShowNotificationAsPopup(notifier_id));
}

TEST_F(SessionStateNotificationBlockerTest, BlockOnPrefService) {
  // Default status: OOBE.
  message_center::NotifierId notifier_id(
      message_center::NotifierType::APPLICATION, "test-notifier");
  EXPECT_FALSE(ShouldShowNotificationAsPopup(notifier_id));

  // Login screen.
  GetSessionControllerClient()->SetSessionState(SessionState::LOGIN_PRIMARY);
  EXPECT_EQ(0, GetStateChangedCountAndReset());
  EXPECT_FALSE(ShouldShowNotificationAsPopup(notifier_id));

  // Simulates login event sequence in production code:
  // - Add a user session;
  // - User session is set as active session;
  // - Session state changes to active;
  // - User PrefService is initialized sometime later.
  const AccountId kUserAccountId = AccountId::FromUserEmail("user@test.com");
  TestSessionControllerClient* const session_controller_client =
      GetSessionControllerClient();
  session_controller_client->AddUserSession(kUserAccountId.GetUserEmail(),
                                            user_manager::USER_TYPE_REGULAR,
                                            true, /* enable_settings */
                                            false /* provide_pref_service */);
  EXPECT_EQ(0, GetStateChangedCountAndReset());
  EXPECT_FALSE(ShouldShowNotificationAsPopup(notifier_id));

  session_controller_client->SwitchActiveUser(kUserAccountId);
  EXPECT_EQ(0, GetStateChangedCountAndReset());
  EXPECT_FALSE(ShouldShowNotificationAsPopup(notifier_id));

  session_controller_client->SetSessionState(SessionState::ACTIVE);
  EXPECT_EQ(0, GetStateChangedCountAndReset());
  EXPECT_FALSE(ShouldShowNotificationAsPopup(notifier_id));

  session_controller_client->ProvidePrefServiceForUser(kUserAccountId);
  EXPECT_EQ(1, GetStateChangedCountAndReset());
  EXPECT_TRUE(ShouldShowNotificationAsPopup(notifier_id));
}

TEST_F(SessionStateNotificationBlockerTest, BlockInKioskMode) {
  message_center::NotifierId notifier_id(
      message_center::NotifierType::SYSTEM_COMPONENT, kNotifierSystemPriority);
  EXPECT_TRUE(ShouldShowNotificationAsPopup(notifier_id));

  SimulateKioskMode(user_manager::USER_TYPE_KIOSK_APP);
  EXPECT_FALSE(ShouldShowNotificationAsPopup(notifier_id));
}

}  // namespace
}  // namespace ash
