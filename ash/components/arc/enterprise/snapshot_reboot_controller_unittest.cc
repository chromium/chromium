// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/enterprise/snapshot_reboot_controller.h"
#include "ash/components/arc/test/fake_snapshot_reboot_notification.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace data_snapshotd {

namespace {

constexpr char kPublicAccountEmail[] = "public@localhost";

chromeos::FakePowerManagerClient* client() {
  return chromeos::FakePowerManagerClient::Get();
}

}  // namespace

class SnapshotRebootControllerTest : public testing::Test {
 public:
  void SetUp() override {
    chromeos::PowerManagerClient::InitializeFake();

    fake_user_manager_ = new user_manager::FakeUserManager();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        base::WrapUnique(fake_user_manager_));
  }

  void TearDown() override {
    chromeos::PowerManagerClient::Shutdown();

    scoped_user_manager_.reset();
    fake_user_manager_ = nullptr;
  }

  void LoginUserSession() {
    auto account_id = AccountId::FromUserEmail(kPublicAccountEmail);
    auto* user = user_manager()->AddUser(account_id);

    user_manager()->UserLoggedIn(account_id, user->username_hash(), false,
                                 false);
  }

  void FastForwardAttempt() {
    task_environment_.FastForwardBy(kRebootAttemptDelay);
    task_environment_.RunUntilIdle();
  }

  user_manager::FakeUserManager* user_manager() { return fake_user_manager_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  session_manager::SessionManager session_manager_;
  user_manager::FakeUserManager* fake_user_manager_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
};

TEST_F(SnapshotRebootControllerTest, UserLoggedIn) {
  LoginUserSession();
  EXPECT_TRUE(user_manager()->IsUserLoggedIn());

  auto notification_ptr = std::make_unique<FakeSnapshotRebootNotification>();
  auto* notification = notification_ptr.get();
  SnapshotRebootController controller(std::move(notification_ptr));

  EXPECT_FALSE(controller.get_timer_for_testing()->IsRunning());
  EXPECT_EQ(0, client()->num_request_restart_calls());
  EXPECT_TRUE(notification->shown());
}

TEST_F(SnapshotRebootControllerTest, BasicReboot) {
  auto notification_ptr = std::make_unique<FakeSnapshotRebootNotification>();
  auto* notification = notification_ptr.get();
  SnapshotRebootController controller(std::move(notification_ptr));

  EXPECT_FALSE(notification->shown());
  EXPECT_EQ(0, client()->num_request_restart_calls());
  for (int i = 0; i < kMaxRebootAttempts; i++) {
    EXPECT_TRUE(controller.get_timer_for_testing()->IsRunning());
    FastForwardAttempt();
    EXPECT_EQ(i + 1, client()->num_request_restart_calls());
    EXPECT_FALSE(notification->shown());
  }
  EXPECT_FALSE(controller.get_timer_for_testing()->IsRunning());
}

TEST_F(SnapshotRebootControllerTest, OnSessionStateChangedLogin) {
  auto notification_ptr = std::make_unique<FakeSnapshotRebootNotification>();
  auto* notification = notification_ptr.get();
  SnapshotRebootController controller(std::move(notification_ptr));

  EXPECT_TRUE(controller.get_timer_for_testing()->IsRunning());

  LoginUserSession();
  controller.OnSessionStateChanged();
  EXPECT_FALSE(controller.get_timer_for_testing()->IsRunning());
  EXPECT_EQ(0, client()->num_request_restart_calls());
  EXPECT_TRUE(notification->shown());
}

TEST_F(SnapshotRebootControllerTest, UserConsentReboot) {
  LoginUserSession();
  EXPECT_TRUE(user_manager()->IsUserLoggedIn());

  auto notification_ptr = std::make_unique<FakeSnapshotRebootNotification>();
  auto* notification = notification_ptr.get();
  SnapshotRebootController controller(std::move(notification_ptr));

  EXPECT_FALSE(controller.get_timer_for_testing()->IsRunning());
  EXPECT_EQ(0, client()->num_request_restart_calls());
  EXPECT_TRUE(notification->shown());

  // The reboot is requested immediately once user consents to reboot.
  notification->Click();
  EXPECT_EQ(1, client()->num_request_restart_calls());
  EXPECT_TRUE(controller.get_timer_for_testing()->IsRunning());
}
}  // namespace data_snapshotd
}  // namespace arc
