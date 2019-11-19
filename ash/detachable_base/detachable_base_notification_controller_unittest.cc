// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/detachable_base/detachable_base_notification_controller.h"

#include <string>

#include "ash/detachable_base/detachable_base_handler.h"
#include "ash/public/cpp/session/user_info.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_type.h"
#include "ui/message_center/message_center.h"

namespace ash {

UserInfo CreateTestUserInfo(const std::string& user_email) {
  UserInfo user_info;
  user_info.type = user_manager::USER_TYPE_REGULAR;
  user_info.account_id = AccountId::FromUserEmail(user_email);
  user_info.display_name = "Test user";
  user_info.display_email = user_email;
  user_info.is_ephemeral = false;
  user_info.is_new_profile = false;
  return user_info;
}

class DetachableBaseNotificationControllerTest : public NoSessionAshTestBase {
 public:
  DetachableBaseNotificationControllerTest() = default;
  ~DetachableBaseNotificationControllerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    chromeos::FakePowerManagerClient::Get()->SetTabletMode(
        chromeos::PowerManagerClient::TabletMode::OFF, base::TimeTicks());
  }

  bool IsBaseChangedNotificationVisible() {
    return message_center::MessageCenter::Get()->FindVisibleNotificationById(
        DetachableBaseNotificationController::kBaseChangedNotificationId);
  }

  bool IsBaseRequiresUpdateNotificationVisible() {
    return message_center::MessageCenter::Get()->FindVisibleNotificationById(
        DetachableBaseNotificationController::
            kBaseRequiresUpdateNotificationId);
  }

  void CloseBaseChangedNotification() {
    message_center::MessageCenter::Get()->RemoveNotification(
        DetachableBaseNotificationController::kBaseChangedNotificationId,
        true /*by_user*/);
  }

  DetachableBaseHandler* detachable_base_handler() {
    return Shell::Get()->detachable_base_handler();
  }

  SessionControllerImpl* session_controller() {
    return Shell::Get()->session_controller();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DetachableBaseNotificationControllerTest);
};

TEST_F(DetachableBaseNotificationControllerTest,
       ShowPairingNotificationIfSessionNotBlocked) {
  CreateUserSessions(1);

  // The first detachable base used by the user - no notification expected.
  detachable_base_handler()->PairChallengeSucceeded({0x01, 0x01});
  EXPECT_FALSE(IsBaseChangedNotificationVisible());

  // If the user changes the paired base in session, the detachable base change
  // notification should be shown.
  detachable_base_handler()->PairChallengeSucceeded({0x02, 0x02});
  EXPECT_TRUE(IsBaseChangedNotificationVisible());

  CloseBaseChangedNotification();
  EXPECT_FALSE(IsBaseChangedNotificationVisible());

  // Verify that the notification is reshown if the base changes again.
  detachable_base_handler()->PairChallengeSucceeded({0x03, 0x03});
  EXPECT_TRUE(IsBaseChangedNotificationVisible());
}

TEST_F(DetachableBaseNotificationControllerTest,
       ShowNotificationOnNonAuthenticatedBases) {
  CreateUserSessions(1);

  detachable_base_handler()->PairChallengeFailed();
  EXPECT_TRUE(IsBaseChangedNotificationVisible());
}

TEST_F(DetachableBaseNotificationControllerTest,
       UpdateNotificationOnUserSwitch) {
  CreateUserSessions(1);

  // The first detachable base used by the user - no notification expected.
  detachable_base_handler()->PairChallengeSucceeded({0x01, 0x01});
  EXPECT_FALSE(IsBaseChangedNotificationVisible());

  SimulateUserLogin("secondary_user@test.com");
  EXPECT_FALSE(IsBaseChangedNotificationVisible());

  detachable_base_handler()->PairChallengeSucceeded({0x02, 0x02});
  EXPECT_TRUE(IsBaseChangedNotificationVisible());
  CloseBaseChangedNotification();

  GetSessionControllerClient()->SwitchActiveUser(
      session_controller()->GetUserSession(1)->user_info.account_id);

  EXPECT_TRUE(IsBaseChangedNotificationVisible());
}

TEST_F(DetachableBaseNotificationControllerTest,
       NonAuthenticatedBaseNotificationOnUserSwitch) {
  CreateUserSessions(1);

  detachable_base_handler()->PairChallengeFailed();
  EXPECT_TRUE(IsBaseChangedNotificationVisible());
  CloseBaseChangedNotification();

  SimulateUserLogin("secondary_user@test.com");
  EXPECT_TRUE(IsBaseChangedNotificationVisible());
}

TEST_F(DetachableBaseNotificationControllerTest,
       NoNotificationIfSessionNotStarted) {
  const char kTestUser[] = "user_1@test.com";
  UserInfo test_user_info = CreateTestUserInfo(kTestUser);
  // Set a detachable base as previously used by the user before log in.
  detachable_base_handler()->PairChallengeSucceeded({0x01, 0x01});
  EXPECT_TRUE(
      detachable_base_handler()->SetPairedBaseAsLastUsedByUser(test_user_info));

  // Set up another detachable base as attached when the user logs in.
  detachable_base_handler()->PairChallengeSucceeded({0x02, 0x02});
  // No active user, so the notification should not be shown, yet.
  EXPECT_FALSE(IsBaseChangedNotificationVisible());

  SimulateUserLogin(kTestUser);
  EXPECT_TRUE(IsBaseChangedNotificationVisible());
}

TEST_F(DetachableBaseNotificationControllerTest,
       NoNotificationOnSessionStartIfBaseMarkedAsLastUsed) {
  const char kTestUser[] = "user_1@test.com";
  UserInfo test_user_info = CreateTestUserInfo(kTestUser);
  // Set a detachable base as previously used by the user before log in.
  detachable_base_handler()->PairChallengeSucceeded({0x01, 0x01});
  EXPECT_TRUE(
      detachable_base_handler()->SetPairedBaseAsLastUsedByUser(test_user_info));

  // Set up another detachable base as attached when the user logs in.
  detachable_base_handler()->PairChallengeSucceeded({0x02, 0x02});
  // No active user, so the notification should not be shown, yet.
  EXPECT_FALSE(IsBaseChangedNotificationVisible());

  // Mark the current device as last used by the user, and verify there is no
  // notification when the user logs in.
  EXPECT_TRUE(
      detachable_base_handler()->SetPairedBaseAsLastUsedByUser(test_user_info));
  SimulateUserLogin(kTestUser);
  EXPECT_FALSE(IsBaseChangedNotificationVisible());
}

// Tests that a notification for non authenticated base is not shown before the
// session is started - the login UI will show a custom UI to inform the user
// about the base.
TEST_F(DetachableBaseNotificationControllerTest,
       NonAuthenticatedBaseNotificationNotShownBeforeLogin) {
  detachable_base_handler()->PairChallengeFailed();
  EXPECT_FALSE(IsBaseChangedNotificationVisible());

  CreateUserSessions(1);
  EXPECT_TRUE(IsBaseChangedNotificationVisible());
}

TEST_F(DetachableBaseNotificationControllerTest, NoNotificationOnLockScreen) {
  CreateUserSessions(1);
  // The first detachable base used by the user - no notification expected.
  detachable_base_handler()->PairChallengeSucceeded({0x01, 0x01});

  BlockUserSession(UserSessionBlockReason::BLOCKED_BY_LOCK_SCREEN);

  detachable_base_handler()->PairChallengeSucceeded({0x02, 0x02});
  EXPECT_FALSE(IsBaseChangedNotificationVisible());

  UnblockUserSession();
  EXPECT_TRUE(IsBaseChangedNotificationVisible());
}

TEST_F(DetachableBaseNotificationControllerTest,
       NoNotificationAfterLockScreenIfSetAsUsed) {
  CreateUserSessions(1);
  // The first detachable base used by the user - no notification expected.
  detachable_base_handler()->PairChallengeSucceeded({0x01, 0x01});
  BlockUserSession(UserSessionBlockReason::BLOCKED_BY_LOCK_SCREEN);

  detachable_base_handler()->PairChallengeSucceeded({0x02, 0x02});
  EXPECT_FALSE(IsBaseChangedNotificationVisible());

  EXPECT_TRUE(detachable_base_handler()->SetPairedBaseAsLastUsedByUser(
      session_controller()->GetUserSession(0)->user_info));

  UnblockUserSession();
  EXPECT_FALSE(IsBaseChangedNotificationVisible());
}

// Tests that a notification for non authenticated base is not shown before the
// session is started - the lock UI will show a custom UI to inform the user
// about the base.
TEST_F(DetachableBaseNotificationControllerTest,
       NonAuthenticatedBaseNotificationNotShownOnLock) {
  BlockUserSession(UserSessionBlockReason::BLOCKED_BY_LOCK_SCREEN);
  detachable_base_handler()->PairChallengeFailed();
  EXPECT_FALSE(IsBaseChangedNotificationVisible());

  UnblockUserSession();
  EXPECT_TRUE(IsBaseChangedNotificationVisible());
}

TEST_F(DetachableBaseNotificationControllerTest, NotificationOnUpdateRequired) {
  CreateUserSessions(1);

  detachable_base_handler()->BaseFirmwareUpdateNeeded();
  EXPECT_TRUE(IsBaseRequiresUpdateNotificationVisible());

  // The notification should be removed when the base gets detached.
  chromeos::FakePowerManagerClient::Get()->SetTabletMode(
      chromeos::PowerManagerClient::TabletMode::ON, base::TimeTicks());
  EXPECT_FALSE(IsBaseRequiresUpdateNotificationVisible());
}

TEST_F(DetachableBaseNotificationControllerTest,
       NotificationOnUpdateRequiredBeforeLogin) {
  // Update requirement detected before login - expect the update required
  // notification to be shown.
  detachable_base_handler()->BaseFirmwareUpdateNeeded();
  EXPECT_TRUE(IsBaseRequiresUpdateNotificationVisible());

  // Login, expect the notification to still be there.
  CreateUserSessions(1);
  EXPECT_TRUE(IsBaseRequiresUpdateNotificationVisible());

  // The notification should be removed when the base gets detached.
  chromeos::FakePowerManagerClient::Get()->SetTabletMode(
      chromeos::PowerManagerClient::TabletMode::ON, base::TimeTicks());
  EXPECT_FALSE(IsBaseRequiresUpdateNotificationVisible());
}

TEST_F(DetachableBaseNotificationControllerTest,
       NotificationOnUpdateRequiredOnLockScreen) {
  // Update requirement detected while the session is blocked by the lock
  // screen - expect the update required notification to be shown.
  BlockUserSession(UserSessionBlockReason::BLOCKED_BY_LOCK_SCREEN);

  detachable_base_handler()->BaseFirmwareUpdateNeeded();
  EXPECT_TRUE(IsBaseRequiresUpdateNotificationVisible());

  // The notification should be removed when the base gets detached.
  chromeos::FakePowerManagerClient::Get()->SetTabletMode(
      chromeos::PowerManagerClient::TabletMode::ON, base::TimeTicks());
  EXPECT_FALSE(IsBaseRequiresUpdateNotificationVisible());
}

}  // namespace ash
