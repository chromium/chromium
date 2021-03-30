// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/session/chrome_session_manager.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/fake_gaia_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/ui/user_adding_screen.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/google/google_brand_chromeos.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "chromeos/system/statistics_provider.h"
#include "chromeos/tpm/stub_install_attributes.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/fake_gaia.h"
#include "rlz/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

// Helper class to wait for user adding screen to finish.
class UserAddingScreenWaiter : public UserAddingScreen::Observer {
 public:
  UserAddingScreenWaiter() { UserAddingScreen::Get()->AddObserver(this); }
  ~UserAddingScreenWaiter() override {
    UserAddingScreen::Get()->RemoveObserver(this);
  }

  void Wait() {
    if (!UserAddingScreen::Get()->IsRunning())
      return;
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  // UserAddingScreen::Observer:
  void OnUserAddingFinished() override {
    if (run_loop_)
      run_loop_->Quit();
  }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(UserAddingScreenWaiter);
};

}  // anonymous namespace

class ChromeSessionManagerTest : public LoginManagerTest {
 public:
  ChromeSessionManagerTest() = default;
  ~ChromeSessionManagerTest() override {}

  // LoginManagerTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    LoginManagerTest::SetUpCommandLine(command_line);

    command_line->AppendSwitch(switches::kOobeSkipPostLogin);
  }

 protected:
  FakeGaiaMixin fake_gaia_{&mixin_host_, embedded_test_server()};
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_UNOWNED};

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeSessionManagerTest);
};

IN_PROC_BROWSER_TEST_F(ChromeSessionManagerTest, OobeNewUser) {
  // Verify that session state is OOBE and no user sessions in fresh start.
  session_manager::SessionManager* manager =
      session_manager::SessionManager::Get();
  EXPECT_EQ(session_manager::SessionState::OOBE, manager->session_state());
  EXPECT_EQ(0u, manager->sessions().size());

  // Login via fake gaia to add a new user.
  fake_gaia_.SetupFakeGaiaForLoginManager();
  fake_gaia_.fake_gaia()->SetFakeMergeSessionParams(
      FakeGaiaMixin::kFakeUserEmail, "fake_sid", "fake_lsid");
  OobeScreenWaiter(OobeBaseTest::GetFirstSigninScreen()).Wait();

  LoginDisplayHost::default_host()
      ->GetOobeUI()
      ->GetView<GaiaScreenHandler>()
      ->ShowSigninScreenForTest(FakeGaiaMixin::kFakeUserEmail, "fake_password",
                                "[]");

  test::WaitForPrimaryUserSessionStart();

  // Verify that session state is ACTIVE with one user session.
  EXPECT_EQ(session_manager::SessionState::ACTIVE, manager->session_state());
  EXPECT_EQ(1u, manager->sessions().size());
}

class ChromeSessionManagerExistingUsersTest : public ChromeSessionManagerTest {
 public:
  ChromeSessionManagerExistingUsersTest() {
    login_manager_.AppendRegularUsers(3);
  }

  LoginManagerMixin login_manager_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(ChromeSessionManagerExistingUsersTest,
                       LoginExistingUsers) {
  // Verify that session state is LOGIN_PRIMARY with existing user data dir.
  session_manager::SessionManager* manager =
      session_manager::SessionManager::Get();
  EXPECT_EQ(session_manager::SessionState::LOGIN_PRIMARY,
            manager->session_state());
  EXPECT_EQ(0u, manager->sessions().size());

  const auto& users = login_manager_.users();
  // Verify that session state is ACTIVE with one user session after signing
  // in a user.
  LoginUser(users[0].account_id);
  EXPECT_EQ(session_manager::SessionState::ACTIVE, manager->session_state());
  EXPECT_EQ(1u, manager->sessions().size());

  for (size_t i = 1; i < users.size(); ++i) {
    // Verify that session state is LOGIN_SECONDARY during user adding.
    UserAddingScreen::Get()->Start();
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(session_manager::SessionState::LOGIN_SECONDARY,
              manager->session_state());

    // Verify that session state is ACTIVE with 1+i user sessions after user
    // is added and new user session is started..
    UserAddingScreenWaiter waiter;
    AddUser(users[i].account_id);
    waiter.Wait();
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(session_manager::SessionState::ACTIVE, manager->session_state());
    EXPECT_EQ(1u + i, manager->sessions().size());
  }

  // Verify that session manager has the correct user session info.
  ASSERT_EQ(users.size(), manager->sessions().size());
  for (size_t i = 0; i < users.size(); ++i) {
    EXPECT_EQ(users[i].account_id, manager->sessions()[i].user_account_id);
  }
}

#if BUILDFLAG(ENABLE_RLZ)

class ChromeSessionManagerRlzTest : public ChromeSessionManagerTest {
 protected:
  StubInstallAttributes* stub_install_attributes() {
    return scoped_stub_install_attributes_->Get();
  }

  void StartUserSession() {
    // Verify that session state is OOBE and no user sessions in fresh start.
    session_manager::SessionManager* manager =
        session_manager::SessionManager::Get();
    EXPECT_EQ(session_manager::SessionState::OOBE, manager->session_state());
    EXPECT_EQ(0u, manager->sessions().size());

    // Login via fake gaia to add a new user.
    fake_gaia_.SetupFakeGaiaForLoginManager();
    fake_gaia_.fake_gaia()->SetFakeMergeSessionParams(
        FakeGaiaMixin::kFakeUserEmail, "fake_sid", "fake_lsid");
    OobeScreenWaiter(OobeBaseTest::GetFirstSigninScreen()).Wait();

    LoginDisplayHost::default_host()
        ->GetOobeUI()
        ->GetView<GaiaScreenHandler>()
        ->ShowSigninScreenForTest(FakeGaiaMixin::kFakeUserEmail,
                                  "fake_password", "[]");

    test::WaitForPrimaryUserSessionStart();

    // Verify that session state is ACTIVE with one user session.
    EXPECT_EQ(session_manager::SessionState::ACTIVE, manager->session_state());
    EXPECT_EQ(1u, manager->sessions().size());
  }

 private:
  void SetUpInProcessBrowserTestFixture() override {
    // Set the default brand code to a known value.
    scoped_fake_statistics_provider_.reset(
        new system::ScopedFakeStatisticsProvider());
    scoped_fake_statistics_provider_->SetMachineStatistic(
        system::kRlzBrandCodeKey, "TEST");

    scoped_stub_install_attributes_ =
        std::make_unique<ScopedStubInstallAttributes>(
            StubInstallAttributes::CreateUnset());
    ChromeSessionManagerTest::SetUpInProcessBrowserTestFixture();
  }

  std::unique_ptr<system::ScopedFakeStatisticsProvider>
        scoped_fake_statistics_provider_;
  std::unique_ptr<ScopedStubInstallAttributes> scoped_stub_install_attributes_;
};

IN_PROC_BROWSER_TEST_F(ChromeSessionManagerRlzTest, DeviceIsLocked) {
  // When the device is locked, the brand should stick after session start.
  stub_install_attributes()->set_device_locked(true);
  StartUserSession();
  EXPECT_EQ("TEST", google_brand::chromeos::GetBrand());
}

IN_PROC_BROWSER_TEST_F(ChromeSessionManagerRlzTest, DeviceIsUnlocked) {
  // When the device is unlocked, the brand should still stick after a
  // regular session start.
  stub_install_attributes()->set_device_locked(false);
  StartUserSession();
  EXPECT_EQ("TEST", google_brand::chromeos::GetBrand());
}

class GuestSessionRlzTest : public InProcessBrowserTest,
                            public ::testing::WithParamInterface<bool> {
 public:
  GuestSessionRlzTest() : is_locked_(GetParam()) {}

 protected:
  StubInstallAttributes* stub_install_attributes() {
    return scoped_stub_install_attributes_->Get();
  }

 private:
  void SetUpInProcessBrowserTestFixture() override {
    // Set the default brand code to a known value.
    scoped_fake_statistics_provider_.reset(
        new system::ScopedFakeStatisticsProvider());
    scoped_fake_statistics_provider_->SetMachineStatistic(
        system::kRlzBrandCodeKey, "TEST");

    // Lock the device as needed for this test.
    scoped_stub_install_attributes_ =
        std::make_unique<ScopedStubInstallAttributes>(
            StubInstallAttributes::CreateUnset());
    scoped_stub_install_attributes_->Get()->set_device_locked(is_locked_);

    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(chromeos::switches::kGuestSession);
    command_line->AppendSwitch(::switches::kIncognito);
    command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile, "hash");
    command_line->AppendSwitchASCII(
        chromeos::switches::kLoginUser,
        user_manager::GuestAccountId().GetUserEmail());
  }

  // Test instance parameters.
  const bool is_locked_;

  std::unique_ptr<system::ScopedFakeStatisticsProvider>
      scoped_fake_statistics_provider_;
  std::unique_ptr<ScopedStubInstallAttributes> scoped_stub_install_attributes_;

  DISALLOW_COPY_AND_ASSIGN(GuestSessionRlzTest);
};

IN_PROC_BROWSER_TEST_P(GuestSessionRlzTest, DeviceIsLocked) {
  if (!UserSessionInitializer::Get()->get_inited_for_testing()) {
    // Wait for initialization.
    base::RunLoop loop;
    UserSessionInitializer::Get()->set_init_rlz_impl_closure_for_testing(
        loop.QuitClosure());
    loop.Run();
  }
  const char* const expected_brand =
      stub_install_attributes()->IsDeviceLocked() ? "TEST" : "";
  EXPECT_EQ(expected_brand, google_brand::chromeos::GetBrand());
}

INSTANTIATE_TEST_SUITE_P(GuestSessionRlzTest,
                         GuestSessionRlzTest,
                         ::testing::Values(false, true));

#endif  // BUILDFLAG(ENABLE_RLZ)

}  // namespace chromeos
