// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/public/cpp/ash_switches.h"
#include "ash/shell.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/policy/login_policy_test_base.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/policy/policy_constants.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/test/display_manager_test_api.h"

namespace policy {

class ForceMaximizeOnFirstRunTest : public LoginPolicyTestBase {
 protected:
  ForceMaximizeOnFirstRunTest() {}

  void GetMandatoryPoliciesValue(base::DictionaryValue* policy) const override {
    policy->SetBoolean(key::kForceMaximizeOnFirstRun, true);
  }

  void SetUpResolution() {
    // Set a screen resolution for which the first browser window will not be
    // maximized by default. 1466 is greater than kForceMaximizeWidthLimit.
    const std::string resolution("1466x300");
    display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
        .UpdateDisplay(resolution);
  }

  const Browser* OpenNewBrowserWindow() {
    const user_manager::User* const user =
        user_manager::UserManager::Get()->GetActiveUser();
    Profile* const profile =
        chromeos::ProfileHelper::Get()->GetProfileByUser(user);
    return CreateBrowser(profile);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    LoginPolicyTestBase::SetUpCommandLine(command_line);
    command_line->AppendSwitch(ash::switches::kShowWebUiLogin);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ForceMaximizeOnFirstRunTest);
};

IN_PROC_BROWSER_TEST_F(ForceMaximizeOnFirstRunTest, PRE_TwoRuns) {
  SetUpResolution();
  SkipToLoginScreen();
  LogIn(kAccountId, kAccountPassword, kEmptyServices);

  // Check that the first browser window is maximized.
  const BrowserList* const browser_list = BrowserList::GetInstance();
  EXPECT_EQ(1U, browser_list->size());
  const Browser* const browser = browser_list->get(0);
  ASSERT_TRUE(browser);
  EXPECT_TRUE(browser->window()->IsMaximized());

  // Un-maximize the window as its state will be carried forward to the next
  // opened window.
  browser->window()->Restore();
  EXPECT_FALSE(browser->window()->IsMaximized());

  // Create a second window and check that it is not affected by the policy.
  const Browser* const browser1 = OpenNewBrowserWindow();
  ASSERT_TRUE(browser1);
  EXPECT_FALSE(browser1->window()->IsMaximized());
}

IN_PROC_BROWSER_TEST_F(ForceMaximizeOnFirstRunTest, TwoRuns) {
  SetUpResolution();
  LogIn(kAccountId, kAccountPassword, kEmptyServices);

  const Browser* const browser = OpenNewBrowserWindow();
  ASSERT_TRUE(browser);
  EXPECT_FALSE(browser->window()->IsMaximized());
}

class ForceMaximizePolicyFalseTest : public ForceMaximizeOnFirstRunTest {
 protected:
  ForceMaximizePolicyFalseTest() : ForceMaximizeOnFirstRunTest() {}

  void GetMandatoryPoliciesValue(base::DictionaryValue* policy) const override {
    policy->SetBoolean(key::kForceMaximizeOnFirstRun, false);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ForceMaximizePolicyFalseTest);
};

IN_PROC_BROWSER_TEST_F(ForceMaximizePolicyFalseTest, GeneralFirstRun) {
  SetUpResolution();
  SkipToLoginScreen();
  LogIn(kAccountId, kAccountPassword, kEmptyServices);

  const BrowserList* const browser_list = BrowserList::GetInstance();
  EXPECT_EQ(1U, browser_list->size());
  const Browser* const browser = browser_list->get(0);
  ASSERT_TRUE(browser);
  EXPECT_FALSE(browser->window()->IsMaximized());
}

}  // namespace policy
