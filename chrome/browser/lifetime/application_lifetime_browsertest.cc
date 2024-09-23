// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"

#include "base/command_line.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/first_run/scoped_relaunch_chrome_browser_override.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// A gMock matcher that is satisfied when its argument is a command line
// containing a given switch.
MATCHER_P(HasSwitch, switch_name, "") {
  return arg.HasSwitch(switch_name);
}

}  // namespace

// Test Fixture for testing chrome::AttemptRestart method.
//
// The SetupCommandLine method takes care of initiating the browser with the
// incognito or guest switch. This switch is accessed via GetParam() method and
// the switches are passed during the testcase declaration which invokes the
// same test once per switch.
//
// The SetUpInProcessBrowserTestFixture method, creates a mock callback which
// gets hooked when chrome tries to Relaunch the browser. The hooking is thanks
// to the upgrade_util::ScopedRelaunchChromeBrowserOverride. Our mock callback
// runs the MATCHER_P.
//
// It is template parameterized on the type of switches::kIncognito and
// switches::kGuest which is const char*
class AttemptRestartTest : public InProcessBrowserTest,
                           public testing::WithParamInterface<const char*> {
 protected:
  AttemptRestartTest() = default;

  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(GetParam());
  }

  void SetUpInProcessBrowserTestFixture() override {
    // Expect a browser relaunch late in browser shutdown.
    EXPECT_CALL(mock_relaunch_callback_,
                Run(testing::Not(HasSwitch(GetParam()))));
  }

 private:
  base::MockCallback<upgrade_util::RelaunchChromeBrowserCallback>
      mock_relaunch_callback_;
  upgrade_util::ScopedRelaunchChromeBrowserOverride relaunch_chrome_override_{
      mock_relaunch_callback_.Get()};
};

INSTANTIATE_TEST_SUITE_P(,
                         AttemptRestartTest,
                         testing::Values(switches::kIncognito,
                                         switches::kGuest));

IN_PROC_BROWSER_TEST_P(AttemptRestartTest, AttemptRestartWithOTRProfiles) {
  // We will now attempt restart, prior to (crbug.com/999085)
  // the new session after restart defaulted to the browser type
  // of the last session. Now, we will restart always to regular mode.
  chrome::AttemptRestart();
}

// Ensures that a relaunch actually happens without interaction when a
// beforeunload handler is in place when RelaunchIgnoreUnloadHandlers is called.
class RelaunchIgnoreUnloadHandlersTest : public InProcessBrowserTest {
 protected:
  RelaunchIgnoreUnloadHandlersTest() = default;

  // InProcessBrowserTest:
  void SetUpInProcessBrowserTestFixture() override {
    // Expect a browser relaunch late in browser shutdown.
    EXPECT_CALL(mock_relaunch_callback_, Run(::testing::_));
  }

 private:
  base::MockCallback<upgrade_util::RelaunchChromeBrowserCallback>
      mock_relaunch_callback_;
  upgrade_util::ScopedRelaunchChromeBrowserOverride relaunch_chrome_override_{
      mock_relaunch_callback_.Get()};
};

IN_PROC_BROWSER_TEST_F(RelaunchIgnoreUnloadHandlersTest, Do) {
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(
      content::ExecJs(tab,
                      "window.addEventListener('beforeunload',"
                      "function(event) { event.returnValue = 'Foo'; });"));
  content::PrepContentsForBeforeUnloadTest(tab);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&chrome::RelaunchIgnoreUnloadHandlers));
  ui_test_utils::WaitForBrowserToClose(browser());
}
