// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TODO(crbug.com/898942): Remove this in Chrome 95.
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace policy {

class PolicyTestWindowOpener : public PolicyTest {
  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    PolicyMap policies;
    // Configure the policy to disable the new behavior.
    SetPolicy(&policies, policy::key::kTargetBlankImpliesNoOpener,
              base::Value(false));
    provider_.UpdateChromePolicy(policies);
  }
};

// Check that when the TargetBlankImpliesNoOpener policy is configured and set
// to false, windows targeting _blank do not have their opener cleared.
IN_PROC_BROWSER_TEST_F(PolicyTestWindowOpener, CheckWindowOpenerNonNull) {
  ASSERT_TRUE(embedded_test_server()->Start());

  PrefService* local_state = g_browser_process->local_state();
  EXPECT_FALSE(local_state->GetBoolean(
      policy::policy_prefs::kTargetBlankImpliesNoOpener));

  GURL url(
      "data:text/html,<a href='about:blank' target='_blank' "
      "id='link'>popup</a>");
  ui_test_utils::NavigateToURL(browser(), url);

  ASSERT_EQ(browser()->tab_strip_model()->count(), 1);
  content::WebContents* tab_1 =
      browser()->tab_strip_model()->GetActiveWebContents();

  ui_test_utils::TabAddedWaiter tab_Added_waiter(browser());
  SimulateMouseClickOrTapElementWithId(tab_1, "link");
  tab_Added_waiter.Wait();
  content::WebContents* tab_2 =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(tab_1, tab_2);

  constexpr char kScript[] =
      R"({ window.domAutomationController.send(window.opener === null); })";
  content::ExecuteScriptAsync(tab_2, kScript);

  content::DOMMessageQueue message_queue;
  std::string message;
  EXPECT_TRUE(message_queue.WaitForMessage(&message));
  EXPECT_EQ("false", message);
}

}  // namespace policy
