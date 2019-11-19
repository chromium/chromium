// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/unified/unified_system_tray.h"
#include "base/command_line.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/login_wizard.h"
#include "chrome/browser/chromeos/login/test/guest_session_mixin.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "chrome/browser/chromeos/login/test/offline_gaia_test_mixin.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_webui.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
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

class LoginGuestTest : public MixinBasedInProcessBrowserTest {
 protected:
  GuestSessionMixin guest_session_{&mixin_host_};
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

class LoginTest : public MixinBasedInProcessBrowserTest {
 public:
  LoginTest() = default;
  ~LoginTest() override {}

 protected:
  const LoginManagerMixin::TestUserInfo test_user_{
      AccountId::FromUserEmailGaiaId(kTestUser, kGaiaId),
      user_manager::USER_TYPE_REGULAR};

  LoginManagerMixin login_manager_{&mixin_host_, {test_user_}};
  OfflineGaiaTestMixin offline_gaia_test_mixin_{&mixin_host_};
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
  EXPECT_TRUE(tray->GetVisible());

  // This check flakes for LoginGuestTest: https://crbug.com/693106.
  if (!otr)
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
  offline_gaia_test_mixin_.PrepareOfflineGaiaLogin();
}

// Flaking: https://crbug.com/1023591
IN_PROC_BROWSER_TEST_F(LoginTest, DISABLED_GaiaAuthOffline) {
  offline_gaia_test_mixin_.GoOffline();
  offline_gaia_test_mixin_.SignIn(test_user_.account_id, kPassword);
  TestSystemTrayIsVisible(false);
}

}  // namespace chromeos
