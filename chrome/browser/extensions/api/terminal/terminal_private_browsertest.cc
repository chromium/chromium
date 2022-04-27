// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/browser/ash/crostini/crostini_browser_test_util.h"
#include "chrome/browser/ash/crostini/fake_crostini_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/system_features_disable_list_policy_handler.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions {

class TerminalPrivateBrowserTest : public CrostiniBrowserTestBase {
 public:
  TerminalPrivateBrowserTest(const TerminalPrivateBrowserTest&) = delete;
  TerminalPrivateBrowserTest& operator=(const TerminalPrivateBrowserTest&) =
      delete;

 protected:
  TerminalPrivateBrowserTest()
      : CrostiniBrowserTestBase(/*register_termina=*/false) {}

  void ExpectJsResult(const std::string& script, const std::string& expected) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::EvalJsResult eval_result =
        EvalJs(web_contents, script, content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
               /*world_id=*/1);
    EXPECT_EQ(eval_result.value.GetString(), expected);
  }
};

IN_PROC_BROWSER_TEST_F(TerminalPrivateBrowserTest, OpenTerminalProcessChecks) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("chrome-untrusted://terminal/html/terminal.html")));

  const std::string script = R"(new Promise((resolve) => {
    chrome.terminalPrivate.openVmshellProcess([], () => {
      const lastError = chrome.runtime.lastError;
      resolve(lastError ? lastError.message : "success");
    })}))";

  // 'vmshell not allowed' when crostini is not allowed.
  fake_crostini_features_.set_could_be_allowed(true);
  fake_crostini_features_.set_is_allowed_now(false);
  ExpectJsResult(script, "vmshell not allowed");

  fake_crostini_features_.set_is_allowed_now(true);
  ExpectJsResult(script, "success");

  // openTerminalProcess not defined.
  ExpectJsResult("typeof chrome.terminalPrivate.openTerminalProcess",
                 "undefined");
}

IN_PROC_BROWSER_TEST_F(TerminalPrivateBrowserTest, OpenCroshProcessChecks) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("chrome-untrusted://crosh/html/crosh.html")));

  const std::string script = R"(new Promise((resolve) => {
    chrome.terminalPrivate.openTerminalProcess("crosh", [], () => {
      const lastError = chrome.runtime.lastError;
      resolve(lastError ? lastError.message : "success");
    })
    }))";

  base::Value system_features(base::Value::Type::LIST);
  system_features.Append(static_cast<int>(policy::SystemFeature::kCrosh));
  g_browser_process->local_state()->Set(
      policy::policy_prefs::kSystemFeaturesDisableList,
      std::move(system_features));
  // 'crosh not allowed' when crosh is not allowed.
  ExpectJsResult(script, "crosh not allowed");

  ListPrefUpdate update(g_browser_process->local_state(),
                        policy::policy_prefs::kSystemFeaturesDisableList);
  update->ClearList();
  ExpectJsResult(script, "success");
}

}  // namespace extensions
