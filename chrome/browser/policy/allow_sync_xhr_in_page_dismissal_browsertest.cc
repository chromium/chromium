// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "url/gurl.h"

namespace policy {

class PolicyTestSyncXHR : public PolicyTest {
 protected:
  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    PolicyMap policies;
    policies.Set(policy::key::kAllowSyncXHRInPageDismissal,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(true), nullptr);
    provider_.UpdateChromePolicy(policies);
  }
};

IN_PROC_BROWSER_TEST_F(PolicyTestSyncXHR, CheckAllowSyncXHRInPageDismissal) {
  ASSERT_TRUE(embedded_test_server()->Start());

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAllowSyncXHRInPageDismissal);
  PrefService* prefs = browser()->profile()->GetPrefs();
  EXPECT_TRUE(prefs->GetBoolean(prefs::kAllowSyncXHRInPageDismissal));

  GURL url(embedded_test_server()->GetURL("/empty.html"));
  ui_test_utils::NavigateToURL(browser(), url);
  constexpr char kScript[] =
      R"({
           window.addEventListener('unload', function() {
             var xhr = new XMLHttpRequest();
             xhr.open('GET', '', false);
             try { xhr.send(); } catch(err) {
               window.domAutomationController.send(false);
             }
             window.domAutomationController.send(xhr.status === 200);
           });
           window.location.href='about:blank';
         })";
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::DOMMessageQueue message_queue;
  content::ExecuteScriptAsync(web_contents, kScript);
  std::string message;
  EXPECT_TRUE(message_queue.WaitForMessage(&message));
  EXPECT_EQ("true", message);
}

}  // namespace policy
