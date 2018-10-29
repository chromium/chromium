// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/unified/unified_system_tray.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/login_wizard.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_webui.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/chromeos/settings/stub_install_attributes.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_names.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_system.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/geometry/test/rect_test_util.h"

using ::gfx::test::RectContains;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;

namespace chromeos {
namespace {

const char kGaiaId[] = "12345";
const char kTestUser[] = "test-user@gmail.com";
const char kPassword[] = "password";

class LoginUserTest : public InProcessBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kLoginUser, "TestUser@gmail.com");
    command_line->AppendSwitchASCII(switches::kLoginProfile, "hash");
  }
};

class LoginGuestTest : public InProcessBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kGuestSession);
    command_line->AppendSwitch(::switches::kIncognito);
    command_line->AppendSwitchASCII(switches::kLoginProfile, "hash");
    command_line->AppendSwitchASCII(
        switches::kLoginUser, user_manager::GuestAccountId().GetUserEmail());
  }
};

class LoginCursorTest : public InProcessBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kLoginManager);
  }
};

class LoginSigninTest : public InProcessBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kLoginManager);
    command_line->AppendSwitch(switches::kForceLoginManagerInTests);
  }

  void SetUpOnMainThread() override {
    LoginDisplayHostWebUI::DisableRestrictiveProxyCheckForTest();
  }
};

class LoginTest : public LoginManagerTest {
 public:
  LoginTest() : LoginManagerTest(true, true) {}
  ~LoginTest() override {}

  void StartGaiaAuthOffline() {
    content::DOMMessageQueue message_queue;
    // clang-format off
    const std::string js = "(function() {"
      "var authenticator = $('gaia-signin').gaiaAuthHost_;"
      "authenticator.addEventListener('ready',"
        "function f() {"
          "authenticator.removeEventListener('ready', f);"
          "window.domAutomationController.send('offlineLoaded');"
        "});"
      "$('error-offline-login-link').onclick();"
    "})();";
    // clang-format on
    ASSERT_TRUE(content::ExecuteScript(web_contents(), js));

    std::string message;
    do {
      ASSERT_TRUE(message_queue.WaitForMessage(&message));
    } while (message != "\"offlineLoaded\"");
  }

  void SubmitGaiaAuthOfflineForm(const std::string& user_email,
                                 const std::string& password) {
    const std::string animated_pages =
        "document.querySelector('#offline-gaia /deep/ "
        "#animatedPages')";
    const std::string email_input =
        "document.querySelector('#offline-gaia /deep/ #emailInput')";
    const std::string email_next_button =
        "document.querySelector('#offline-gaia /deep/ #emailSection "
        "/deep/ #button')";
    const std::string password_input =
        "document.querySelector('#offline-gaia /deep/ "
        "#passwordInput')";
    const std::string password_next_button =
        "document.querySelector('#offline-gaia /deep/ #passwordSection"
        " /deep/ #button')";

    content::DOMMessageQueue message_queue;
    JSExpect("!document.querySelector('#offline-gaia').hidden");
    JSExpect("document.querySelector('#signin-frame').hidden");
    const std::string js =
        animated_pages +
        ".addEventListener('neon-animation-finish',"
        "function() {"
        "window.domAutomationController.send('switchToPassword');"
        "})";
    ASSERT_TRUE(content::ExecuteScript(web_contents(), js));
    std::string set_email = email_input + ".value = '$Email'";
    base::ReplaceSubstringsAfterOffset(&set_email, 0, "$Email", user_email);
    ASSERT_TRUE(content::ExecuteScript(web_contents(), set_email));
    ASSERT_TRUE(content::ExecuteScript(web_contents(),
                                       email_next_button + ".fire('tap')"));
    std::string message;
    do {
      ASSERT_TRUE(message_queue.WaitForMessage(&message));
    } while (message != "\"switchToPassword\"");

    std::string set_password = password_input + ".value = '$Password'";
    base::ReplaceSubstringsAfterOffset(&set_password, 0, "$Password", password);
    ASSERT_TRUE(content::ExecuteScript(web_contents(), set_password));
    ASSERT_TRUE(content::ExecuteScript(web_contents(),
                                       password_next_button + ".fire('tap')"));
  }

  void PrepareOfflineLogin() {
    bool show_user;
    ASSERT_TRUE(CrosSettings::Get()->GetBoolean(
        kAccountsPrefShowUserNamesOnSignIn, &show_user));
    ASSERT_FALSE(show_user);

    StartGaiaAuthOffline();

    UserContext user_context(
        user_manager::UserType::USER_TYPE_REGULAR,
        AccountId::FromUserEmailGaiaId(kTestUser, kGaiaId));
    user_context.SetKey(Key(kPassword));
    SetExpectedCredentials(user_context);
  }

 protected:
  ScopedCrosSettingsTestHelper settings_helper_{
      /* create_settings_service= */ false};
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
               << " ShelfAutoHideBehavior=" << shelf->auto_hide_behavior());
  EXPECT_TRUE(tray->visible());

  // This check flakes for LoginGuestTest: https://crbug.com/693106.
  // This check is suppressed for Mash since the warning button of Mash changes
  // the tray bounds which triggers the failure. See: https://crbug.com/892730
  // TODO(jamescook): remove this when Mash is on by default or the button is
  // removed.
  if (!otr && !features::IsUsingWindowService())
    EXPECT_TRUE(RectContains(primary_win->bounds(), tray->GetBoundsInScreen()));
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
  // Login screen needs to be shown explicitly when running test.
  ShowLoginWizard(OobeScreen::SCREEN_SPECIAL_LOGIN);

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

IN_PROC_BROWSER_TEST_F(LoginTest, PRE_GaiaAuthOffline) {
  RegisterUser(AccountId::FromUserEmailGaiaId(kTestUser, kGaiaId));
  StartupUtils::MarkOobeCompleted();
  settings_helper_.SetBoolean(kAccountsPrefShowUserNamesOnSignIn, false);
}

// Flaky, see http://crbug/692364.
IN_PROC_BROWSER_TEST_F(LoginTest, DISABLED_GaiaAuthOffline) {
  PrepareOfflineLogin();
  content::WindowedNotificationObserver session_start_waiter(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::NotificationService::AllSources());
  SubmitGaiaAuthOfflineForm(kTestUser, kPassword);
  session_start_waiter.Wait();

  TestSystemTrayIsVisible(false);
}

}  // namespace chromeos
