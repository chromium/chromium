// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/keyboard/keyboard_controller.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screens_utils.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/webui/ash/login/guest_tos_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/user_creation_screen_handler.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/flags_ui/feature_entry_macros.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "third_party/cros_system_api/switches/chrome_switches.h"

namespace ash {

constexpr char kGuestTosId[] = "guest-tos";
const test::UIPath kOverviewDialog = {kGuestTosId, "overview"};
const test::UIPath kGuestTosAcceptButton = {kGuestTosId, "acceptButton"};

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

  void StartGuestSession() {
    OobeScreenWaiter(UserCreationView::kScreenId).Wait();
    ASSERT_TRUE(LoginScreenTestApi::ClickGuestButton());

    OobeScreenWaiter(GuestTosScreenView::kScreenId).Wait();
    test::OobeJS().CreateVisibilityWaiter(true, kOverviewDialog)->Wait();
    test::OobeJS().ClickOnPath(kGuestTosAcceptButton);
  }

  void CheckCryptohomeMountAssertions() {
    ASSERT_EQ(FakeUserDataAuthClient::Get()->get_prepare_guest_request_count(),
              1);
  }

 protected:
  LoginManagerMixin login_manager_{&mixin_host_, {}};
  base::HistogramTester histogram_tester_;
};

class GuestLoginWithLoginSwitchesTest : public GuestLoginTest {
 public:
  GuestLoginWithLoginSwitchesTest()
      : scoped_feature_entries_(
            {{"feature-name", "name-1", "description-1",
              /*supported_platforms=*/static_cast<unsigned short>(-1),  // All.
              SINGLE_VALUE_TYPE("feature-switch")}}) {}
  ~GuestLoginWithLoginSwitchesTest() override = default;

  // GuestLoginTest:
  void SetDefaultLoginSwitches() override {
    login_manager_.SetDefaultLoginSwitches(
        {std::make_pair(chromeos::switches::kFeatureFlags,
                        "[\"feature-name\"]"),
         std::make_pair("test_switch_1", ""),
         std::make_pair("test_switch_2", "test_switch_2_value")});
  }

 private:
  about_flags::testing::ScopedFeatureEntries scoped_feature_entries_;
};

IN_PROC_BROWSER_TEST_F(GuestLoginTest, PRE_Login) {
  base::RunLoop restart_job_waiter;
  FakeSessionManagerClient::Get()->set_restart_job_callback(
      restart_job_waiter.QuitClosure());

  StartGuestSession();

  restart_job_waiter.Run();
  EXPECT_TRUE(FakeSessionManagerClient::Get()->restart_job_argv().has_value());
  CheckCryptohomeMountAssertions();

  histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime.Guest-tos", 1);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Guest-tos.Accept", 1);
  histogram_tester_.ExpectTotalCount("OOBE.StepShownStatus.Guest-tos", 1);
}

IN_PROC_BROWSER_TEST_F(GuestLoginTest, Login) {
  login_manager_.WaitForActiveSession();

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  EXPECT_TRUE(user_manager->IsLoggedInAsGuest());

  // Checks User::GetProfilePrefs() uses the correct instance.
  user_manager::User* user = user_manager->GetActiveUser();
  ASSERT_TRUE(user);
  EXPECT_EQ(user_manager::UserType::kGuest, user->GetType());
  EXPECT_EQ(ProfileHelper::Get()->GetProfileByUser(user)->GetPrefs(),
            user->GetProfilePrefs());
}

// Check that the guest button is visible on user creation screen before
// entering and after exiting the guest session.
IN_PROC_BROWSER_TEST_F(GuestLoginTest,
                       PRE_PRE_UserCreationGuestButtonVisibility) {
  base::RunLoop restart_job_waiter;
  FakeSessionManagerClient::Get()->set_restart_job_callback(
      restart_job_waiter.QuitClosure());

  EXPECT_TRUE(LoginScreenTestApi::IsGuestButtonShown());
  StartGuestSession();

  restart_job_waiter.Run();
}

IN_PROC_BROWSER_TEST_F(GuestLoginTest, PRE_UserCreationGuestButtonVisibility) {
  login_manager_.WaitForActiveSession();

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  EXPECT_TRUE(user_manager->IsLoggedInAsGuest());
}

IN_PROC_BROWSER_TEST_F(GuestLoginTest, UserCreationGuestButtonVisibility) {
  EXPECT_TRUE(LoginScreenTestApi::IsGuestButtonShown());
}

// The test verifies that clicking the Guest button multiple times doesn't
// trigger extra userdataauth requests. A regression test for b/213835042.
IN_PROC_BROWSER_TEST_F(GuestLoginTest, PRE_MultipleClicks) {
  StartupUtils::MarkEulaAccepted();
  base::RunLoop restart_job_waiter;
  FakeSessionManagerClient::Get()->set_restart_job_callback(
      restart_job_waiter.QuitClosure());

  // Start the guest session, with additional clicks right before and after this
  // UI activity, and additionally after the restart job is created.
  EXPECT_TRUE(LoginScreenTestApi::ClickGuestButton());
  EXPECT_TRUE(LoginScreenTestApi::ClickGuestButton());
  EXPECT_TRUE(LoginScreenTestApi::ClickGuestButton());

  // Every guest session must accept EULA before guest session is created.
  // Device owner EULA is independent from guest session EULA.
  ash::test::WaitForGuestTosScreen();
  ash::test::TapGuestTosAccept();

  restart_job_waiter.Run();

  EXPECT_TRUE(LoginScreenTestApi::ClickGuestButton());

  // Not strictly necessary, but useful to potentially catch bugs stemming from
  // asynchronous jobs.
  base::RunLoop().RunUntilIdle();
  CheckCryptohomeMountAssertions();
}

IN_PROC_BROWSER_TEST_F(GuestLoginTest, MultipleClicks) {
  login_manager_.WaitForActiveSession();

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  EXPECT_TRUE(user_manager->IsLoggedInAsGuest());
}

IN_PROC_BROWSER_TEST_F(GuestLoginTest, PRE_ExitFullscreenOnSuspend) {
  base::RunLoop restart_job_waiter;
  FakeSessionManagerClient::Get()->set_restart_job_callback(
      restart_job_waiter.QuitClosure());

  StartGuestSession();

  restart_job_waiter.Run();
  EXPECT_TRUE(FakeSessionManagerClient::Get()->restart_job_argv().has_value());
}

IN_PROC_BROWSER_TEST_F(GuestLoginTest, ExitFullscreenOnSuspend) {
  login_manager_.WaitForActiveSession();
  BrowserWindow* browser_window = browser()->window();
  browser()
      ->exclusive_access_manager()
      ->fullscreen_controller()
      ->ToggleBrowserFullscreenMode();
  EXPECT_TRUE(browser_window->IsFullscreen());
  chromeos::FakePowerManagerClient::Get()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);
  EXPECT_FALSE(browser_window->IsFullscreen());
}

IN_PROC_BROWSER_TEST_F(GuestLoginTest,
                       PRE_VirtualKeyboardFeaturesEnabledByDefault) {
  base::RunLoop restart_job_waiter;
  FakeSessionManagerClient::Get()->set_restart_job_callback(
      restart_job_waiter.QuitClosure());

  StartGuestSession();

  restart_job_waiter.Run();
  EXPECT_TRUE(FakeSessionManagerClient::Get()->restart_job_argv().has_value());
}

IN_PROC_BROWSER_TEST_F(GuestLoginTest,
                       VirtualKeyboardFeaturesEnabledByDefault) {
  login_manager_.WaitForActiveSession();

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  EXPECT_TRUE(user_manager->IsLoggedInAsGuest());

  keyboard::KeyboardConfig config =
      KeyboardController::Get()->GetKeyboardConfig();
  EXPECT_TRUE(config.auto_capitalize);
  EXPECT_TRUE(config.auto_complete);
  EXPECT_TRUE(config.auto_correct);
  EXPECT_TRUE(config.handwriting);
  EXPECT_TRUE(config.spell_check);
  EXPECT_TRUE(config.voice_input);
}

// Every Guest session displays the ToS.
IN_PROC_BROWSER_TEST_F(GuestLoginTest, PRE_ShowGuestToS) {
  // Assume device owner accepts Eula ToS.
  StartupUtils::MarkEulaAccepted();

  base::RunLoop restart_job_waiter;
  FakeSessionManagerClient::Get()->set_restart_job_callback(
      restart_job_waiter.QuitClosure());

  ASSERT_TRUE(LoginScreenTestApi::ClickGuestButton());

  // Every guest session must accept EULA before guest session is created.
  // Device owner EULA is independent from guest session EULA.
  ash::test::WaitForGuestTosScreen();
  ash::test::TapGuestTosAccept();

  restart_job_waiter.Run();
  EXPECT_TRUE(FakeSessionManagerClient::Get()->restart_job_argv().has_value());

  histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime.Guest-tos", 1);
  histogram_tester_.ExpectTotalCount("OOBE.StepShownStatus.Guest-tos", 1);
}

IN_PROC_BROWSER_TEST_F(GuestLoginTest, ShowGuestToS) {
  login_manager_.WaitForActiveSession();

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  EXPECT_TRUE(user_manager->IsLoggedInAsGuest());
}

IN_PROC_BROWSER_TEST_F(GuestLoginWithLoginSwitchesTest, PRE_Login) {
  base::RunLoop restart_job_waiter;
  FakeSessionManagerClient::Get()->set_restart_job_callback(
      restart_job_waiter.QuitClosure());

  EXPECT_TRUE(
      base::CommandLine::ForCurrentProcess()->HasSwitch("feature-switch"));
  StartGuestSession();

  restart_job_waiter.Run();
  EXPECT_TRUE(FakeSessionManagerClient::Get()->restart_job_argv().has_value());
}

// Verifies that login policy flags do not spill over to the guest session.
IN_PROC_BROWSER_TEST_F(GuestLoginWithLoginSwitchesTest, Login) {
  login_manager_.WaitForActiveSession();

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  EXPECT_TRUE(user_manager->IsLoggedInAsGuest());

  EXPECT_FALSE(
      base::CommandLine::ForCurrentProcess()->HasSwitch("feature-switch"));
  EXPECT_FALSE(
      base::CommandLine::ForCurrentProcess()->HasSwitch("test_switch_1"));
  EXPECT_FALSE(
      base::CommandLine::ForCurrentProcess()->HasSwitch("test_switch_2"));
}

}  // namespace ash
