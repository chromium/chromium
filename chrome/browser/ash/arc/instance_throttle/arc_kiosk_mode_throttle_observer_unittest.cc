// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/instance_throttle/arc_kiosk_mode_throttle_observer.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/login/users/scoped_test_user_manager.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/common/content_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

constexpr char kTestProfileName[] = "user@gmail.com";

void TestCallback(int* counter,
                  int* active_counter,
                  const ash::ThrottleObserver* self) {
  (*counter)++;
  if (self->active())
    (*active_counter)++;
}

class ScopedKioskModeLogIn {
 public:
  explicit ScopedKioskModeLogIn(bool kiosk_user) {
    // Prevent access to DBus. This switch is reset in case set from test SetUp
    // due massive usage of InitFromArgv.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kTestType);

    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        std::make_unique<ash::FakeChromeUserManager>());
    const AccountId account_id(AccountId::FromUserEmail(kTestProfileName));
    if (kiosk_user)
      GetFakeUserManager()->AddArcKioskAppUser(account_id);
    else
      GetFakeUserManager()->AddUser(account_id);
    GetFakeUserManager()->LoginUser(account_id);
  }

  ScopedKioskModeLogIn(const ScopedKioskModeLogIn&) = delete;
  ScopedKioskModeLogIn& operator=(const ScopedKioskModeLogIn&) = delete;

  ~ScopedKioskModeLogIn() {
    GetFakeUserManager()->RemoveUserFromList(
        AccountId::FromUserEmail(kTestProfileName));
    user_manager_enabler_.reset();
  }

 private:
  ash::FakeChromeUserManager* GetFakeUserManager() const {
    return static_cast<ash::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
};
}  // namespace

using ArcKioskModeThrottleObserverTest = testing::Test;

TEST_F(ArcKioskModeThrottleObserverTest, Default) {
  ScopedKioskModeLogIn login(false /* kiosk_user */);
  ArcKioskModeThrottleObserver observer;
  int call_count = 0;
  int active_count = 0;
  observer.StartObserving(
      nullptr /* context */,
      base::BindRepeating(&TestCallback, &call_count, &active_count));
  EXPECT_EQ(1, call_count);
  EXPECT_EQ(0, active_count);
  EXPECT_FALSE(observer.active());
}

TEST_F(ArcKioskModeThrottleObserverTest, Active) {
  ScopedKioskModeLogIn login(true /* kiosk_user */);
  ArcKioskModeThrottleObserver observer;
  int call_count = 0;
  int active_count = 0;
  observer.StartObserving(
      nullptr /* context */,
      base::BindRepeating(&TestCallback, &call_count, &active_count));
  EXPECT_EQ(1, call_count);
  EXPECT_EQ(1, active_count);
  EXPECT_TRUE(observer.active());
}

}  // namespace arc
