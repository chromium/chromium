// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/unified/unified_system_tray.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/login_wizard.h"
#include "chrome/browser/chromeos/login/test/device_state_mixin.h"
#include "chrome/browser/chromeos/login/test/embedded_test_server_mixin.h"
#include "chrome/browser/chromeos/login/test/fake_gaia_mixin.h"
#include "chrome/browser/chromeos/login/test/guest_session_mixin.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "chrome/browser/chromeos/login/test/offline_gaia_test_mixin.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/test/session_manager_state_waiter.h"
#include "chrome/browser/chromeos/login/test/test_predicate_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_webui.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/chromeos/login/welcome_screen_handler.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/geometry/test/rect_test_util.h"

using ::gfx::test::RectContains;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;

namespace chromeos {
namespace {

class LoginUserTest : public InProcessBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kLoginUser, "TestUser@gmail.com");
    command_line->AppendSwitchASCII(switches::kLoginProfile, "hash");
  }
};

class LoginGuestTest : public MixinBasedInProcessBrowserTest {
 protected:
  GuestSessionMixin guest_session_{&mixin_host_};
};

class LoginCursorTest : public OobeBaseTest {
 public:
  LoginCursorTest() = default;
  ~LoginCursorTest() override = default;
};

using LoginSigninTest = LoginManagerTest;

class LoginOfflineTest : public LoginManagerTest {
 public:
  LoginOfflineTest() {
    login_manager_.AppendRegularUsers(1);
    test_account_id_ = login_manager_.users()[0].account_id;
  }
  ~LoginOfflineTest() override {}

 protected:
  AccountId test_account_id_;
  LoginManagerMixin login_manager_{&mixin_host_};
  OfflineGaiaTestMixin offline_gaia_test_mixin_{&mixin_host_};
  // We need Fake gaia to avoid network errors that can be caused by
  // attempts to load real GAIA.
  FakeGaiaMixin fake_gaia_{&mixin_host_, embedded_test_server()};
};

class LoginOfflineManagedTest : public LoginManagerTest {
 public:
  LoginOfflineManagedTest() {
    login_manager_.AppendManagedUsers(1);
    managed_user_id_ = login_manager_.users()[0].account_id;
  }

  ~LoginOfflineManagedTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    LoginManagerTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        chromeos::switches::kAllowFailedPolicyFetchForTest);
  }

  void ConfigurePolicy(const std::string& autocomplete_domain) {
    std::unique_ptr<ScopedDevicePolicyUpdate> device_policy_update =
        device_state_.RequestDevicePolicyUpdate();
    device_policy_update->policy_payload()
        ->mutable_login_screen_domain_auto_complete()
        ->set_login_screen_domain_auto_complete(autocomplete_domain);
    device_policy_update->policy_payload()
        ->mutable_show_user_names()
        ->set_show_user_names(false);
  }

 protected:
  AccountId managed_user_id_;
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
  LoginManagerMixin login_manager_{&mixin_host_};
  OfflineGaiaTestMixin offline_gaia_test_mixin_{&mixin_host_};
  // We need Fake gaia to avoid network errors that can be caused by
  // attempts to load real GAIA.
  FakeGaiaMixin fake_gaia_{&mixin_host_, embedded_test_server()};
};

// Used to make sure that the system tray is visible and within the screen
// bounds after login.
void TestSystemTrayIsVisible(bool otr) {
  aura::Window* primary_win = ash::Shell::GetPrimaryRootWindow();
  ash::Shelf* shelf = ash::Shelf::ForWindow(primary_win);
  ash::TrayBackgroundView* tray =
      shelf->GetStatusAreaWidget()->unified_system_tray();
  SCOPED_TRACE(testing::Message()
               << "ShelfVisibilityState=" << shelf->GetVisibilityState()
               << " ShelfAutoHideBehavior="
               << static_cast<int>(shelf->auto_hide_behavior()));
  ash::StatusAreaWidgetTestHelper::WaitForAnimationEnd(
      shelf->GetStatusAreaWidget());
  EXPECT_TRUE(tray->GetVisible());

  if (otr)
    return;
  // Wait for the system tray be inside primary bounds.
  chromeos::test::TestPredicateWaiter(
      base::BindRepeating(
          [](const aura::Window* primary_win,
             const ash::TrayBackgroundView* tray) {
            if (RectContains(primary_win->bounds(), tray->GetBoundsInScreen()))
              return true;
            LOG(WARNING) << primary_win->bounds().ToString()
                         << " does not contain "
                         << tray->GetBoundsInScreen().ToString();
            return false;
          },
          primary_win, tray))
      .Wait();
}

}  // namespace

// After a chrome crash, the session manager will restart chrome with
// the -login-user flag indicating that the user is already logged in.
// This profile should NOT be an OTR profile.
IN_PROC_BROWSER_TEST_F(LoginUserTest, UserPassed) {
  Profile* profile = browser()->profile();
  std::string profile_base_path("hash");
  profile_base_path.insert(0, chrome::kProfileDirPrefix);
  EXPECT_EQ(profile_base_path, profile->GetPath().BaseName().value());
  EXPECT_FALSE(profile->IsOffTheRecord());

  TestSystemTrayIsVisible(false);
}

// After a guest login, we should get the OTR default profile.
IN_PROC_BROWSER_TEST_F(LoginGuestTest, GuestIsOTR) {
  Profile* profile = browser()->profile();
  EXPECT_TRUE(profile->IsOffTheRecord());
  // Ensure there's extension service for this profile.
  EXPECT_TRUE(extensions::ExtensionSystem::Get(profile)->extension_service());

  TestSystemTrayIsVisible(true);
}

// Verifies the cursor is hidden at startup on login screen.
IN_PROC_BROWSER_TEST_F(LoginCursorTest, CursorHidden) {
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
  // Cursor should be hidden at startup
  EXPECT_FALSE(ash::Shell::Get()->cursor_manager()->IsCursorVisible());

  // Cursor should be shown after cursor is moved.
  EXPECT_TRUE(ui_test_utils::SendMouseMoveSync(gfx::Point()));
  EXPECT_TRUE(ash::Shell::Get()->cursor_manager()->IsCursorVisible());

  TestSystemTrayIsVisible(false);
}

// Verifies that the webui for login comes up successfully.
IN_PROC_BROWSER_TEST_F(LoginSigninTest, WebUIVisible) {
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
      content::NotificationService::AllSources())
      .Wait();
}

IN_PROC_BROWSER_TEST_F(LoginOfflineTest, PRE_GaiaAuthOffline) {
  offline_gaia_test_mixin_.PrepareOfflineGaiaLogin();
}

IN_PROC_BROWSER_TEST_F(LoginOfflineTest, GaiaAuthOffline) {
  offline_gaia_test_mixin_.GoOffline();
  offline_gaia_test_mixin_.InitOfflineLogin(test_account_id_,
                                            LoginManagerTest::kPassword);
  offline_gaia_test_mixin_.CheckManagedStatus(false);
  offline_gaia_test_mixin_.SubmitGaiaAuthOfflineForm(
      test_account_id_.GetUserEmail(), LoginManagerTest::kPassword,
      true /* wait for sign-in */);
  TestSystemTrayIsVisible(false);
}

IN_PROC_BROWSER_TEST_F(LoginOfflineManagedTest, CorrectDomainCompletion) {
  std::string domain = gaia::ExtractDomainName(managed_user_id_.GetUserEmail());

  ConfigurePolicy(domain);

  std::string email = managed_user_id_.GetUserEmail();
  size_t separator_pos = email.find('@');
  ASSERT_TRUE(separator_pos != email.npos &&
              separator_pos < email.length() - 1);
  std::string prefix = email.substr(0, separator_pos);

  offline_gaia_test_mixin_.GoOffline();
  offline_gaia_test_mixin_.InitOfflineLogin(managed_user_id_,
                                            LoginManagerTest::kPassword);

  offline_gaia_test_mixin_.CheckManagedStatus(true);

  offline_gaia_test_mixin_.SubmitGaiaAuthOfflineForm(
      prefix, LoginManagerTest::kPassword, true /* wait for sign-in */);
  TestSystemTrayIsVisible(false);
}

IN_PROC_BROWSER_TEST_F(LoginOfflineManagedTest, FullEmailDontMatchProvided) {
  ConfigurePolicy("another.domain");

  offline_gaia_test_mixin_.GoOffline();
  offline_gaia_test_mixin_.InitOfflineLogin(managed_user_id_,
                                            LoginManagerTest::kPassword);

  offline_gaia_test_mixin_.SubmitGaiaAuthOfflineForm(
      managed_user_id_.GetUserEmail(), LoginManagerTest::kPassword,
      true /* wait for sign-in */);
  TestSystemTrayIsVisible(false);
}

}  // namespace chromeos
