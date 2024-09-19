// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/session/chrome_session_manager.h"

#include <memory>

#include "ash/constants/ash_switches.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/login/lock/screen_locker_tester.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/ash/login/user_adding_screen.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/fake_gaia.h"
#include "rlz/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"

#if BUILDFLAG(ENABLE_RLZ)
#include "chrome/browser/ash/login/session/user_session_initializer.h"
#include "chrome/browser/google/google_brand_chromeos.h"  // nogncheck
#include "chrome/common/chrome_switches.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"  // nogncheck
#include "components/user_manager/user_names.h"
#endif  // BUILDFLAG(ENABLE_RLZ)

namespace ash {

namespace {

// Helper class to wait for user adding screen to finish.
class UserAddingScreenWaiter : public UserAddingScreen::Observer {
 public:
  UserAddingScreenWaiter() { UserAddingScreen::Get()->AddObserver(this); }

  UserAddingScreenWaiter(const UserAddingScreenWaiter&) = delete;
  UserAddingScreenWaiter& operator=(const UserAddingScreenWaiter&) = delete;

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
};

}  // namespace

class ChromeSessionManagerTest : public LoginManagerTest {
 public:
  ChromeSessionManagerTest() = default;

  ChromeSessionManagerTest(const ChromeSessionManagerTest&) = delete;
  ChromeSessionManagerTest& operator=(const ChromeSessionManagerTest&) = delete;

  ~ChromeSessionManagerTest() override {}

  // LoginManagerTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    LoginManagerTest::SetUpCommandLine(command_line);

    command_line->AppendSwitch(switches::kOobeSkipPostLogin);
  }

 protected:
  FakeGaiaMixin fake_gaia_{&mixin_host_};
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_UNOWNED};
};

IN_PROC_BROWSER_TEST_F(ChromeSessionManagerTest, OobeNewUser) {
  // Verify that session state is OOBE and no user sessions in fresh start.
  session_manager::SessionManager* manager =
      session_manager::SessionManager::Get();
  EXPECT_EQ(session_manager::SessionState::OOBE, manager->session_state());
  EXPECT_EQ(0u, manager->sessions().size());

  // Login via fake gaia to add a new user.
  fake_gaia_.SetupFakeGaiaForLoginManager();
  fake_gaia_.fake_gaia()->SetConfigurationHelper(FakeGaiaMixin::kFakeUserEmail,
                                                 "fake_sid", "fake_lsid");
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

// http://crbug.com/1338401
IN_PROC_BROWSER_TEST_F(ChromeSessionManagerExistingUsersTest,
                       DISABLED_LoginExistingUsers) {
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

IN_PROC_BROWSER_TEST_F(ChromeSessionManagerExistingUsersTest,
                       LoginExistingUsersWithLocalPassword) {
  // Verify that session state is LOGIN_PRIMARY with existing user data dir.
  session_manager::SessionManager* manager =
      session_manager::SessionManager::Get();
  EXPECT_EQ(session_manager::SessionState::LOGIN_PRIMARY,
            manager->session_state());
  EXPECT_EQ(0u, manager->sessions().size());

  const auto& users = login_manager_.users();
  // Verify that session state is ACTIVE with one user session after signing
  // in a user with a local password.
  LoginUserWithLocalPassword(users[0].account_id);
  EXPECT_EQ(session_manager::SessionState::ACTIVE, manager->session_state());
  EXPECT_EQ(1u, manager->sessions().size());
}

IN_PROC_BROWSER_TEST_F(ChromeSessionManagerExistingUsersTest,
                       CheckPastingBehavior) {
  const auto& users = login_manager_.users();
  LoginUser(users[0].account_id);
  auto* session_controller = Shell::Get()->session_controller();

  // Write a text in the clipboard during active session.
  EXPECT_EQ(session_manager::SessionState::ACTIVE,
            session_controller->GetSessionState());
  const std::u16string session_clipboard_text = u"active session text";
  ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste)
      .WriteText(session_clipboard_text);

  // Reach the secondary login screen.
  UserAddingScreen::Get()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(session_manager::SessionState::LOGIN_SECONDARY,
            session_controller->GetSessionState());

  // Check that the text can still be pasted: secondary login screen clipboard
  // should be the same than the active session one since we can return to
  // active session by selecting Cancel.
  std::u16string clipboard_text;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, /*data_dst=*/nullptr, &clipboard_text);
  EXPECT_EQ(clipboard_text, session_clipboard_text);

  // Go back to active session, with another user.
  UserAddingScreenWaiter waiter;
  AddUser(users[1].account_id);
  waiter.Wait();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(session_manager::SessionState::ACTIVE,
            session_controller->GetSessionState());

  // Check that the new active session clipboard is empty.
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, /*data_dst=*/nullptr, &clipboard_text);
  EXPECT_TRUE(clipboard_text.empty());

  // Write a text in the new active session clipboard.
  const std::u16string other_session_clipboard_text =
      u"other active session text";
  ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste)
      .WriteText(other_session_clipboard_text);

  // Lock the screen.
  ScreenLockerTester locker_tester;
  locker_tester.Lock();
  EXPECT_EQ(session_manager::SessionState::LOCKED,
            session_controller->GetSessionState());

  // Check that the clipboard is empty, for security reasons.
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, /*data_dst=*/nullptr, &clipboard_text);
  EXPECT_TRUE(clipboard_text.empty());

  // Go back to the active session.
  locker_tester.UnlockWithPassword(users[1].account_id, "password");
  locker_tester.WaitForUnlock();
  EXPECT_EQ(session_manager::SessionState::ACTIVE,
            session_controller->GetSessionState());

  // Check that the clipboard has been restored.
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, /*data_dst=*/nullptr, &clipboard_text);
  EXPECT_EQ(clipboard_text, other_session_clipboard_text);
}

class ChromeSessionManagerRmaTest : public ChromeSessionManagerTest {
 public:
  // LoginManagerTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ChromeSessionManagerTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kLaunchRma);
  }
};

IN_PROC_BROWSER_TEST_F(ChromeSessionManagerRmaTest, DeviceInRma) {
  // Verify that session state is RMA.
  session_manager::SessionManager* manager =
      session_manager::SessionManager::Get();
  EXPECT_EQ(session_manager::SessionState::RMA, manager->session_state());
  EXPECT_EQ(0u, manager->sessions().size());
}

class ChromeSessionManagerRmaNotAllowedTest
    : public ChromeSessionManagerRmaTest {
 public:
  ChromeSessionManagerRmaNotAllowedTest() = default;
  // LoginManagerTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ChromeSessionManagerRmaTest::SetUpCommandLine(command_line);
    // Block RMA with kRmaNotAllowed switch.
    command_line->AppendSwitch(switches::kRmaNotAllowed);
  }
};

IN_PROC_BROWSER_TEST_F(ChromeSessionManagerRmaNotAllowedTest,
                       RmaNotAllowedBlocksRma) {
  // Verify that session state is not RMA, even though kLaunchRma switch was
  // passed.
  session_manager::SessionManager* manager =
      session_manager::SessionManager::Get();
  EXPECT_EQ(session_manager::SessionState::OOBE, manager->session_state());
  EXPECT_EQ(0u, manager->sessions().size());
}

class ChromeSessionManagerRmaSafeModeTest : public ChromeSessionManagerRmaTest {
 public:
  ChromeSessionManagerRmaSafeModeTest() = default;
  // LoginManagerTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ChromeSessionManagerRmaTest::SetUpCommandLine(command_line);
    // Block RMA with when kSafeMode switch is present.
    command_line->AppendSwitch(switches::kSafeMode);
  }
};

IN_PROC_BROWSER_TEST_F(ChromeSessionManagerRmaSafeModeTest, SafeModeBlocksRma) {
  // Verify that session state is not RMA, even though kLaunchRma switch was
  // passed.
  session_manager::SessionManager* manager =
      session_manager::SessionManager::Get();
  EXPECT_EQ(session_manager::SessionState::OOBE, manager->session_state());
  EXPECT_EQ(0u, manager->sessions().size());
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
    fake_gaia_.fake_gaia()->SetConfigurationHelper(
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

  GuestSessionRlzTest(const GuestSessionRlzTest&) = delete;
  GuestSessionRlzTest& operator=(const GuestSessionRlzTest&) = delete;

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
    command_line->AppendSwitch(switches::kGuestSession);
    command_line->AppendSwitch(::switches::kIncognito);
    command_line->AppendSwitchASCII(switches::kLoginProfile, "hash");
    command_line->AppendSwitchASCII(
        switches::kLoginUser, user_manager::GuestAccountId().GetUserEmail());
  }

  // Test instance parameters.
  const bool is_locked_;

  std::unique_ptr<system::ScopedFakeStatisticsProvider>
      scoped_fake_statistics_provider_;
  std::unique_ptr<ScopedStubInstallAttributes> scoped_stub_install_attributes_;
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

}  // namespace ash
