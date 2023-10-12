// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "ash/public/cpp/login_screen_test_api.h"
#include "base/values.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/policy/login/login_policy_test_base.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace policy {

namespace {
// We should not use "chrome://help/" here, because it will be rewritten into
// "chrome://settings/help".
const char kStartUpURL1[] = "chrome://settings/help";
const char kStartUpURL2[] = "chrome://version/";
}  // namespace

// Verifies that the |kRestoreOnStartup| and |kRestoreOnStartupURLs| policies
// are honored on Chrome OS.
class RestoreOnStartupTest : public LoginPolicyTestBase {
 public:
  RestoreOnStartupTest() = default;

  RestoreOnStartupTest(const RestoreOnStartupTest&) = delete;
  RestoreOnStartupTest& operator=(const RestoreOnStartupTest&) = delete;

  // LoginPolicyTestBase:
  void GetPolicySettings(
      enterprise_management::CloudPolicySettings* policy) const override;

  void VerifyStartUpURLs();
};

void RestoreOnStartupTest::GetPolicySettings(
    enterprise_management::CloudPolicySettings* policy) const {
  policy->mutable_restoreonstartup()->set_value(
      SessionStartupPref::kPrefValueURLs);
  auto* urls = policy->mutable_restoreonstartupurls()->mutable_value();
  urls->add_entries(kStartUpURL1);
  urls->add_entries(kStartUpURL2);
}

void RestoreOnStartupTest::VerifyStartUpURLs() {
  const BrowserList* const browser_list = BrowserList::GetInstance();
  ASSERT_EQ(1U, browser_list->size());
  const Browser* const browser = browser_list->get(0);
  ASSERT_TRUE(browser);
  const TabStripModel* tabs = browser->tab_strip_model();
  ASSERT_TRUE(tabs);
  ASSERT_EQ(2, tabs->count());
  EXPECT_EQ(GURL(kStartUpURL1), tabs->GetWebContentsAt(0)->GetURL());
  EXPECT_EQ(GURL(kStartUpURL2), tabs->GetWebContentsAt(1)->GetURL());
}

// Verify that the policies are honored on a new user's login.
IN_PROC_BROWSER_TEST_F(RestoreOnStartupTest, PRE_LogInAndVerify) {
  SkipToLoginScreen();
  LogIn();
  VerifyStartUpURLs();
}

// Verify that the policies are honored on an existing user's login.
IN_PROC_BROWSER_TEST_F(RestoreOnStartupTest, LogInAndVerify) {
  ash::LoginScreenTestApi::SubmitPassword(account_id(), "7654321",
                                          true /* check_if_submittable */);
  ash::test::WaitForPrimaryUserSessionStart();
  VerifyStartUpURLs();
}

}  // namespace policy
