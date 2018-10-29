// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/screenshot_testing/login_screen_areas.h"
#include "chrome/browser/chromeos/login/screenshot_testing/screenshot_testing_mixin.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "ui/compositor/compositor_switches.h"

namespace chromeos {

namespace {

struct {
  const char* email;
  const char* gaia_id;
} const kTestUsers[] = {{"test-user1@gmail.com", "1111111111"},
                        {"test-user2@gmail.com", "2222222222"}};

}  // namespace

class LoginUITest : public chromeos::LoginManagerTest {
 public:
  LoginUITest() : LoginManagerTest(false, true /* should_initialize_webui */) {
    for (size_t i = 0; i < arraysize(kTestUsers); ++i) {
      test_users_.emplace_back(AccountId::FromUserEmailGaiaId(
          kTestUsers[i].email, kTestUsers[i].gaia_id));
    }

    screenshot_testing_ = new ScreenshotTestingMixin;
    screenshot_testing_->IgnoreArea(areas::kClockArea);
    screenshot_testing_->IgnoreArea(areas::kFirstUserpod);
    screenshot_testing_->IgnoreArea(areas::kSecondUserpod);
    AddMixin(base::WrapUnique(screenshot_testing_));
  }
  ~LoginUITest() override {}

 protected:
  std::vector<AccountId> test_users_;

  ScreenshotTestingMixin* screenshot_testing_;
};

IN_PROC_BROWSER_TEST_F(LoginUITest, PRE_LoginUIVisible) {
  RegisterUser(test_users_[0]);
  RegisterUser(test_users_[1]);
  StartupUtils::MarkOobeCompleted();
}

// Verifies basic login UI properties.
IN_PROC_BROWSER_TEST_F(LoginUITest, LoginUIVisible) {
  JSExpect("!!document.querySelector('#account-picker')");
  JSExpect("!!document.querySelector('#pod-row')");
  JSExpect(
      "document.querySelectorAll('.pod:not(#user-pod-template)').length == 2");

  JSExpect(
      "document.querySelectorAll('.pod:not(#user-pod-template)')[0]"
      ".user.emailAddress == '" +
      test_users_[0].GetUserEmail() + "'");
  JSExpect(
      "document.querySelectorAll('.pod:not(#user-pod-template)')[1]"
      ".user.emailAddress == '" +
      test_users_[1].GetUserEmail() + "'");
  screenshot_testing_->RunScreenshotTesting("LoginUITest-LoginUIVisible");
}

IN_PROC_BROWSER_TEST_F(LoginUITest, PRE_InterruptedAutoStartEnrollment) {
  StartupUtils::MarkOobeCompleted();
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kDeviceEnrollmentAutoStart, true);
  prefs->SetBoolean(prefs::kDeviceEnrollmentCanExit, false);
}

// Tests that the default first screen is the welcome screen after OOBE
// when auto enrollment is enabled and device is not yet enrolled.
IN_PROC_BROWSER_TEST_F(LoginUITest, InterruptedAutoStartEnrollment) {
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_WELCOME).Wait();
}

IN_PROC_BROWSER_TEST_F(LoginUITest, OobeNoExceptions) {
  JSExpect("cr.ErrorStore.getInstance().length == 0");
}

IN_PROC_BROWSER_TEST_F(LoginUITest, PRE_LoginNoExceptions) {
  RegisterUser(test_users_[0]);
  RegisterUser(test_users_[1]);
  StartupUtils::MarkOobeCompleted();
}

IN_PROC_BROWSER_TEST_F(LoginUITest, LoginNoExceptions) {
  OobeScreenWaiter(OobeScreen::SCREEN_ACCOUNT_PICKER).Wait();
  JSExpect("cr.ErrorStore.getInstance().length == 0");
}

IN_PROC_BROWSER_TEST_F(LoginUITest, OobeCatchException) {
  JSExpect("cr.ErrorStore.getInstance().length == 0");
  js_checker().ExecuteAsync("aelrt('misprint')");
  JSExpect("cr.ErrorStore.getInstance().length == 1");
  js_checker().ExecuteAsync("consle.error('Some error')");
  JSExpect("cr.ErrorStore.getInstance().length == 2");
}

}  // namespace chromeos
