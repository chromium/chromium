// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace policy {

class PolicyTestWindowPlacement : public PolicyTest {
  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    PolicyMap policies;
    SetPolicy(&policies, policy::key::kWindowPlacementAlwaysAllowed,
              base::Value(true));
    provider_.UpdateChromePolicy(policies);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kEnableBlinkFeatures, "WindowPlacement");
  }
};

IN_PROC_BROWSER_TEST_F(PolicyTestWindowPlacement, CheckWindowPlacementAllowed) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL url(embedded_test_server()->GetURL("/empty.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();

  constexpr char kScript[] = R"(
    (async () => {
      try {
        const screenDetails = await self.getScreenDetails();
        return true;
      } catch (e) {
        console.error(e);
        return false;
      }
    })();
  )";
  EXPECT_EQ(true,
            EvalJs(tab, kScript, content::EXECUTE_SCRIPT_NO_USER_GESTURE));
}

}  // namespace policy
