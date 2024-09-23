// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/public/cpp/login_screen_test_api.h"
#include "ash/shell.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/policy/login/login_policy_test_base.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/test/display_manager_test_api.h"

namespace policy {

// TODO(crbug.com/40142202): Enable and modify for lacros.
class ForceMaximizeOnFirstRunTest : public LoginPolicyTestBase {
 public:
  ForceMaximizeOnFirstRunTest(const ForceMaximizeOnFirstRunTest&) = delete;
  ForceMaximizeOnFirstRunTest& operator=(const ForceMaximizeOnFirstRunTest&) =
      delete;

 protected:
  ForceMaximizeOnFirstRunTest() {}

  void GetPolicySettings(
      enterprise_management::CloudPolicySettings* policy) const override {
    policy->mutable_forcemaximizeonfirstrun()->set_value(true);
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
    Profile* const profile = ash::ProfileHelper::Get()->GetProfileByUser(user);
    return CreateBrowser(profile);
  }
};

IN_PROC_BROWSER_TEST_F(ForceMaximizeOnFirstRunTest, PRE_TwoRuns) {
  SetUpResolution();
  SkipToLoginScreen();
  LogIn();

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
  ash::LoginScreenTestApi::SubmitPassword(account_id(), "123456",
                                          true /* check_if_submittable */);
  ash::test::WaitForPrimaryUserSessionStart();

  const Browser* const browser = OpenNewBrowserWindow();
  ASSERT_TRUE(browser);
  EXPECT_FALSE(browser->window()->IsMaximized());
}

class ForceMaximizePolicyFalseTest : public ForceMaximizeOnFirstRunTest {
 public:
  ForceMaximizePolicyFalseTest(const ForceMaximizePolicyFalseTest&) = delete;
  ForceMaximizePolicyFalseTest& operator=(const ForceMaximizePolicyFalseTest&) =
      delete;

 protected:
  ForceMaximizePolicyFalseTest() : ForceMaximizeOnFirstRunTest() {}

  void GetPolicySettings(
      enterprise_management::CloudPolicySettings* policy) const override {
    policy->mutable_forcemaximizeonfirstrun()->set_value(false);
  }
};

IN_PROC_BROWSER_TEST_F(ForceMaximizePolicyFalseTest, GeneralFirstRun) {
  SetUpResolution();
  SkipToLoginScreen();
  LogIn();

  const BrowserList* const browser_list = BrowserList::GetInstance();
  EXPECT_EQ(1U, browser_list->size());
  const Browser* const browser = browser_list->get(0);
  ASSERT_TRUE(browser);
  EXPECT_FALSE(browser->window()->IsMaximized());
}

}  // namespace policy
