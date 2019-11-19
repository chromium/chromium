// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/login_screen_test_api.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "components/user_manager/user_manager.h"

namespace chromeos {

// Tests guest user log in.
class GuestLoginTest : public MixinBasedInProcessBrowserTest {
 public:
  GuestLoginTest() { login_manager_.set_session_restore_enabled(); }
  ~GuestLoginTest() override = default;

  // Test overrides can implement this to add login policy switches to login
  // screen command line.
  virtual void SetDefaultLoginSwitches() {}

  // MixinBaseInProcessBrowserTest:
  void SetUp() override {
    SetDefaultLoginSwitches();
    MixinBasedInProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    FakeSessionManagerClient::Get()->set_supports_browser_restart(true);
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
  }

 protected:
  LoginManagerMixin login_manager_{&mixin_host_, {}};
};

class GuestLoginWithLoginSwitchesTest : public GuestLoginTest {
 public:
  GuestLoginWithLoginSwitchesTest() = default;
  ~GuestLoginWithLoginSwitchesTest() override = default;

  // GuestLoginTest:
  void SetDefaultLoginSwitches() override {
    login_manager_.SetDefaultLoginSwitches(
        {std::make_pair("test_switch_1", ""),
         std::make_pair("test_switch_2", "test_switch_2_value")});
  }
};

IN_PROC_BROWSER_TEST_F(GuestLoginTest, PRE_Login) {
  base::RunLoop restart_job_waiter;
  FakeSessionManagerClient::Get()->set_restart_job_callback(
      restart_job_waiter.QuitClosure());

  ASSERT_TRUE(ash::LoginScreenTestApi::ClickGuestButton());

  restart_job_waiter.Run();
  EXPECT_TRUE(FakeSessionManagerClient::Get()->restart_job_argv().has_value());
}

IN_PROC_BROWSER_TEST_F(GuestLoginTest, Login) {
  login_manager_.WaitForActiveSession();

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  EXPECT_TRUE(user_manager->IsLoggedInAsGuest());
}

IN_PROC_BROWSER_TEST_F(GuestLoginWithLoginSwitchesTest, PRE_Login) {
  base::RunLoop restart_job_waiter;
  FakeSessionManagerClient::Get()->set_restart_job_callback(
      restart_job_waiter.QuitClosure());

  ASSERT_TRUE(ash::LoginScreenTestApi::ClickGuestButton());

  restart_job_waiter.Run();
  EXPECT_TRUE(FakeSessionManagerClient::Get()->restart_job_argv().has_value());
}

// Verifies that login policy flags do not spill over to the guest session.
IN_PROC_BROWSER_TEST_F(GuestLoginWithLoginSwitchesTest, Login) {
  login_manager_.WaitForActiveSession();

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  EXPECT_TRUE(user_manager->IsLoggedInAsGuest());

  EXPECT_FALSE(
      base::CommandLine::ForCurrentProcess()->HasSwitch("test_switch_1"));
  EXPECT_FALSE(
      base::CommandLine::ForCurrentProcess()->HasSwitch("test_switch_2"));
}

}  // namespace chromeos
