// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/browser/chromeos/crostini/fake_crostini_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions {

class TerminalPrivateBrowserTest : public InProcessBrowserTest {
 protected:
  TerminalPrivateBrowserTest() = default;

  void ExpectJsResult(const std::string& script, const std::string& expected) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::EvalJsResult eval_result =
        EvalJs(web_contents, script, content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
               /*world_id=*/1);
    EXPECT_EQ(eval_result.value.GetString(), expected);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TerminalPrivateBrowserTest);
};

IN_PROC_BROWSER_TEST_F(TerminalPrivateBrowserTest, OpenTerminalProcessChecks) {
  ui_test_utils::NavigateToURL(
      browser(), GURL("chrome-untrusted://terminal/html/terminal.html"));

  const std::string script = R"(new Promise((resolve) => {
    chrome.terminalPrivate.openVmshellProcess([], () => {
      const lastError = chrome.runtime.lastError;
      resolve(lastError ? lastError.message : "success");
    })}))";

  // 'vmshell not allowed' when crostini is not allowed.
  crostini::FakeCrostiniFeatures crostini_features;
  crostini_features.set_could_be_allowed(true);
  crostini_features.set_is_allowed_now(false);
  ExpectJsResult(script, "vmshell not allowed");

  // 'Error starting crostini for terminal: 20' when crostini is allowed.
  // This is the specific error we expect to see in a test environment
  // (SSHFS_MOUNT_ERROR), but the point of the test is to see that we get
  // past the vmshell allowed checks.
  crostini_features.set_is_allowed_now(true);
  ExpectJsResult(script, "Error starting crostini for terminal: 20");

  // openTerminalProcess not defined.
  ExpectJsResult("typeof chrome.terminalPrivate.openTerminalProcess",
                 "undefined");
}

}  // namespace extensions
