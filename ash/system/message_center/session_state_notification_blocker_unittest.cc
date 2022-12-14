// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/session_state_notification_blocker.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/system/do_not_disturb_notification_controller.h"
#include "ash/system/power/battery_notification.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

using base::UTF8ToUTF16;
using session_manager::SessionState;

namespace ash {

namespace {

const char kNotifierSystemPriority[] = "ash.some-high-priority-component";

class SessionStateNotificationBlockerTest
    : public NoSessionAshTestBase,
      public message_center::NotificationBlocker::Observer,
      public testing::WithParamInterface<bool> {
 public:
  SessionStateNotificationBlockerTest() = default;

  SessionStateNotificationBlockerTest(
      const SessionStateNotificationBlockerTest&) = delete;
  SessionStateNotificationBlockerTest& operator=(
      const SessionStateNotificationBlockerTest&) = delete;

  ~SessionStateNotificationBlockerTest() override = default;

  // tests::AshTestBase overrides:
  void SetUp() override {
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitWithFeatureState(features::kNotificationsRefresh,
                                               IsNotificationsRefreshEnabled());

    NoSessionAshTestBase::SetUp();
    blocker_ = std::make_unique<SessionStateNotificationBlocker>(
        message_center::MessageCenter::Get());
    blocker_->AddObserver(this);
  }

  bool IsNotificationsRefreshEnabled() const { return GetParam(); }

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

  bool ShouldShowNotification(const message_center::NotifierId& notifier_id) {
    message_center::Notification notification(
        message_center::NOTIFICATION_TYPE_SIMPLE,
        GetNotificationId(notifier_id), u"chromeos-title", u"chromeos-message",
        ui::ImageModel(), u"chromeos-source", GURL(), notifier_id,
        message_center::RichNotificationData(), nullptr);
    if (notifier_id.id == kNotifierSystemPriority)
      notification.set_priority(message_center::SYSTEM_PRIORITY);
    return blocker_->ShouldShowNotification(notification);
  }

  bool ShouldShowNotificationAsPopup(
      const message_center::NotifierId& notifier_id) {
    message_center::Notification notification(
        message_center::NOTIFICATION_TYPE_SIMPLE,
        GetNotificationId(notifier_id), u"chromeos-title", u"chromeos-message",
        ui::ImageModel(), u"chromeos-source", GURL(), notifier_id,
        message_center::RichNotificationData(), nullptr);
    if (notifier_id.id == kNotifierSystemPriority)
      notification.set_priority(message_center::SYSTEM_PRIORITY);
    return blocker_->ShouldShowNotificationAsPopup(notification);
  }

  bool ShouldShowDoNotDisturbNotification() {
    message_center::Notification notification(
        message_center::NOTIFICATION_TYPE_SIMPLE,
        DoNotDisturbNotificationController::kDoNotDisturbNotificationId,
        u"chromeos-title", u"chromeos-message", ui::ImageModel(),
        u"chromeos-source", GURL(),
        message_center::NotifierId(
            message_center::NotifierType::SYSTEM_COMPONENT, "test-notifier",
            NotificationCatalogName::kDoNotDisturb),
        message_center::RichNotificationData(), nullptr);
    return blocker_->ShouldShowNotification(notification);
  }

  void SetLockedState(bool locked) {
    GetSessionControllerClient()->SetSessionState(
        locked ? SessionState::LOCKED : SessionState::ACTIVE);
  }

 private:
  std::string GetNotificationId(const message_center::NotifierId& notifier_id) {
    return notifier_id.id == kNotifierSystemPriority
               ? BatteryNotification::kNotificationId
               : "chromeos-id";
  }

  int state_changed_count_ = 0;
  std::unique_ptr<message_center::NotificationBlocker> blocker_;
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         SessionStateNotificationBlockerTest,
                         testing::Bool() /* IsNotificationsRefreshEnabled() */);

TEST_P(SessionStateNotificationBlockerTest, BaseTest) {
  // OOBE.
  GetSessionControllerClient()->SetSessionState(SessionState::OOBE);
  EXPECT_EQ(0, GetStateChangedCountAndReset());
  message_center::NotifierId notifier_id(
      message_center::NotifierType::APPLICATION, "test-notifier");
  EXPECT_FALSE(ShouldShowNotificationAsPopup(notifier_id));
  EXPECT_FALSE(ShouldShowNotification(notifier_id));

  // Login screen.
  GetSessionControllerClient()->SetSessionState(SessionState::LOGIN_PRIMARY);
  EXPECT_EQ(0, GetStateChangedCountAndReset());
  EXPECT_FALSE(ShouldShowNotificationAsPopup(notifier_id));
  EXPECT_FALSE(ShouldShowNotification(notifier_id));

  // Logged in as a normal user.
  SimulateUserLogin("user@test.com");
  EXPECT_EQ(1, GetStateChangedCountAndReset());
  EXPECT_TRUE(ShouldShowNotificationAsPopup(notifier_id));
  EXPECT_TRUE(ShouldShowNotification(notifier_id));

  // Lock.
  SetLockedState(true);
  EXPECT_EQ(1, GetStateChangedCountAndReset());
  EXPECT_FALSE(ShouldShowNotificationAsPopup(notifier_id));
  EXPECT_TRUE(ShouldShowNotification(notifier_id));

  // Unlock.
  SetLockedState(false);
  EXPECT_EQ(1, GetStateChangedCountAndReset());
  EXPECT_TRUE(ShouldShowNotificationAsPopup(notifier_id));
  EXPECT_TRUE(ShouldShowNotification(notifier_id));
}

TEST_P(SessionStateNotificationBlockerTest, AlwaysAllowedNotifier) {
  // NOTIFIER_DISPLAY is allowed to shown in the login screen.
  message_center::NotifierId notifier_id(
      message_center::NotifierType::SYSTEM_COMPONENT, kNotifierSystemPriority,
      NotificationCatalogName::kTestCatalogName);

  // OOBE.
  GetSessionControllerClient()->SetSessionState(SessionState::OOBE);
  EXPECT_EQ(0, GetStateChangedCountAndReset());
  EXPECT_TRUE(ShouldShowNotificationAsPopup(notifier_id));
  EXPECT_TRUE(ShouldShowNotification(notifier_id));

  // Login screen.
  GetSessionControllerClient()->SetSessionState(SessionState::LOGIN_PRIMARY);
  EXPECT_EQ(0, GetStateChangedCountAndReset());
  EXPECT_TRUE(ShouldShowNotificationAsPopup(notifier_id));
  EXPECT_TRUE(ShouldShowNotification(notifier_id));

  // Logged in as a normal user.
  SimulateUserLogin("user@test.com");
  EXPECT_EQ(1, GetStateChangedCountAndReset());
  EXPECT_TRUE(ShouldShowNotificationAsPopup(notifier_id));
  EXPECT_TRUE(ShouldShowNotification(notifier_id));

  // Lock.
  SetLockedState(true);
  EXPECT_EQ(1, GetStateChangedCountAndReset());
  EXPECT_TRUE(ShouldShowNotificationAsPopup(notifier_id));
  EXPECT_TRUE(ShouldShowNotification(notifier_id));

  // Unlock.
  SetLockedState(false);
  EXPECT_EQ(1, GetStateChangedCountAndReset());
  EXPECT_TRUE(ShouldShowNotificationAsPopup(notifier_id));
  EXPECT_TRUE(ShouldShowNotification(notifier_id));
}

TEST_P(SessionStateNotificationBlockerTest, BlockOnPrefService) {
  // OOBE.
  GetSessionControllerClient()->SetSessionState(SessionState::OOBE);
  EXPECT_EQ(0, GetStateChangedCountAndReset());
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
                                            false /* provide_pref_service */);
  EXPECT_EQ(0, GetStateChangedCountAndReset());
  EXPECT_FALSE(ShouldShowNotificationAsPopup(notifier_id));

  session_controller_client->SwitchActiveUser(kUserAccountId);
  EXPECT_EQ(0, GetStateChangedCountAndReset());
  EXPECT_FALSE(ShouldShowNotificationAsPopup(notifier_id));

  session_controller_client->SetSessionState(SessionState::ACTIVE);
  EXPECT_EQ(1, GetStateChangedCountAndReset());
  EXPECT_FALSE(ShouldShowNotificationAsPopup(notifier_id));

  session_controller_client->ProvidePrefServiceForUser(kUserAccountId);
  EXPECT_EQ(1, GetStateChangedCountAndReset());
  EXPECT_TRUE(ShouldShowNotificationAsPopup(notifier_id));
}

TEST_P(SessionStateNotificationBlockerTest, BlockInKioskMode) {
  message_center::NotifierId notifier_id(
      message_center::NotifierType::SYSTEM_COMPONENT, kNotifierSystemPriority,
      NotificationCatalogName::kTestCatalogName);
  EXPECT_TRUE(ShouldShowNotificationAsPopup(notifier_id));
  EXPECT_TRUE(ShouldShowNotification(notifier_id));

  SimulateKioskMode(user_manager::USER_TYPE_KIOSK_APP);
  EXPECT_FALSE(ShouldShowNotificationAsPopup(notifier_id));
  EXPECT_FALSE(ShouldShowNotification(notifier_id));
}

TEST_P(SessionStateNotificationBlockerTest, DelayAfterLogin) {
  SessionStateNotificationBlocker::SetUseLoginNotificationDelayForTest(true);
  GetSessionControllerClient()->SetSessionState(SessionState::LOGIN_PRIMARY);

  // Logged in as a normal user.
  SimulateUserLogin("user@test.com");

  // Non system notification should not be shown immediately after login.
  message_center::NotifierId notifier_id(
      message_center::NotifierType::APPLICATION, "test-notifier");
  EXPECT_FALSE(ShouldShowNotification(notifier_id));

  // System notification should still be shown.
  message_center::NotifierId system_notifier_id(
      message_center::NotifierType::SYSTEM_COMPONENT, "system-notifier",
      NotificationCatalogName::kTestCatalogName);
  EXPECT_TRUE(ShouldShowNotification(system_notifier_id));

  // The notification delay should not be enabled for all other tests.
  SessionStateNotificationBlocker::SetUseLoginNotificationDelayForTest(false);
}

TEST_P(SessionStateNotificationBlockerTest, DoNotDisturbNotification) {
  // OOBE.
  GetSessionControllerClient()->SetSessionState(SessionState::OOBE);
  EXPECT_FALSE(ShouldShowDoNotDisturbNotification());

  // Login screen.
  GetSessionControllerClient()->SetSessionState(SessionState::LOGIN_PRIMARY);
  EXPECT_FALSE(ShouldShowDoNotDisturbNotification());

  // Logged in as a normal user.
  SimulateUserLogin("user@test.com");
  EXPECT_TRUE(ShouldShowDoNotDisturbNotification());

  // Lock.
  SetLockedState(true);
  EXPECT_FALSE(ShouldShowDoNotDisturbNotification());

  // Unlock.
  SetLockedState(false);
  EXPECT_TRUE(ShouldShowDoNotDisturbNotification());
}

}  // namespace
}  // namespace ash
