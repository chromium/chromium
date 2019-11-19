// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/first_run/goodies_displayer.h"

#include "base/command_line.h"
#include "base/run_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

class GoodiesDisplayerBrowserTest : public InProcessBrowserTest,
                                    public testing::WithParamInterface<bool> {
 public:
  GoodiesDisplayerBrowserTest() {}

 protected:
  ~GoodiesDisplayerBrowserTest() override {
    first_run::GoodiesDisplayer::Delete();
  }

  // Set up windowless browser and GoodiesDisplayer.  |delta_days| is +/- delta
  // in days from kMaxDaysAfterOobeForGoodies; <= 0: "show", > 0: "don't show".
  Browser* CreateBrowserAndDisplayer(int delta_days) {
    Browser* browser = new Browser(
        Browser::CreateParams(ProfileManager::GetActiveUserProfile(), true));

    // Set up Goodies Displayer and set fake age of device.
    setup_info_.days_since_oobe =
        first_run::GoodiesDisplayer::kMaxDaysAfterOobeForGoodies + delta_days;
    first_run::GoodiesDisplayer::InitForTesting(&setup_info_);
    WaitForGoodiesSetup();

    return browser;
  }

  // The point of all tests here is to check whether the Goodies tab has been
  // correctly opened; this function does the checking.  |expected_tabs| is the
  // expected number of total tabs; |expected_goodies_tabs| should be 0 or 1.
  void ExpectTabCounts(Browser* browser,
                       int expected_tabs,
                       int expected_goodies_tabs) {
    const int tab_count = browser->tab_strip_model()->count();
    int goodies_tab_count = 0;
    for (int index = 0; index < tab_count; index++) {
      const std::string tab_url = browser->tab_strip_model()
          ->GetWebContentsAt(index)->GetVisibleURL().spec();
      if (tab_url == first_run::GoodiesDisplayer::kGoodiesURL)
        ++goodies_tab_count;
    }
    EXPECT_EQ(expected_tabs, tab_count);
    EXPECT_EQ(expected_goodies_tabs, goodies_tab_count);
  }

  // Is --no-first-run specified?
  bool NoFirstRunSpecified() const { return GetParam(); }

 private:
  void WaitForGoodiesSetup() {
    if (setup_info_.setup_complete)
      return;

    // Wait for GoodiesDisplayer setup completion.  The completion callback will
    // shut down the message loop.
    base::RunLoop run_loop;
    setup_info_.on_setup_complete_callback = run_loop.QuitClosure();
    run_loop.Run();
    setup_info_.on_setup_complete_callback.Reset();
    EXPECT_TRUE(setup_info_.setup_complete);
  }

  // InProcessBrowserTest overrides.
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    base::CommandLine default_command_line(base::CommandLine::NO_PROGRAM);
    InProcessBrowserTest::SetUpDefaultCommandLine(&default_command_line);
    if (NoFirstRunSpecified()) {  // --no-first-run is present by default.
      *command_line = default_command_line;
      ASSERT_TRUE(command_line->HasSwitch(switches::kNoFirstRun));
    } else {  // Remove --no-first-run.
      test_launcher_utils::RemoveCommandLineSwitch(
          default_command_line, switches::kNoFirstRun, command_line);
    }
    command_line->AppendSwitch(switches::kNoStartupWindow);
  }

  first_run::GoodiesDisplayerTestInfo setup_info_;
};

// Tests that the Goodies page is not shown on older device.
IN_PROC_BROWSER_TEST_P(GoodiesDisplayerBrowserTest, OldDeviceNoDisplay) {
  if (NoFirstRunSpecified())  // --no-first-run disables Goodies page.
    return;

  EXPECT_TRUE(g_browser_process->local_state()->GetBoolean(
      prefs::kCanShowOobeGoodiesPage));

  ASSERT_EQ(0u, chrome::GetTotalBrowserCount());
  Browser* browser = CreateBrowserAndDisplayer(1);  // 1 day too old.
  ASSERT_EQ(1u, chrome::GetTotalBrowserCount());

  AddBlankTabAndShow(browser);
  ExpectTabCounts(browser, 1, 0);  // Shouldn't show Goodies tab.
  EXPECT_FALSE(g_browser_process->local_state()->GetBoolean(
      prefs::kCanShowOobeGoodiesPage));
}

// Tests that the Goodies page is shown, only once, on non-incognito browser
// when device isn't too old, and when --no-first-run is not specified.
IN_PROC_BROWSER_TEST_P(GoodiesDisplayerBrowserTest, DisplayGoodies) {
  ASSERT_EQ(0u, chrome::GetTotalBrowserCount());
  Browser* browser = CreateBrowserAndDisplayer(-1);
  ASSERT_EQ(1u, chrome::GetTotalBrowserCount());

  // Shouldn't show Goodies tab in incognito mode.
  Browser* incognito_browser = new Browser(Browser::CreateParams(
      browser->profile()->GetOffTheRecordProfile(), true));
  ASSERT_EQ(2u, chrome::GetTotalBrowserCount());
  AddBlankTabAndShow(incognito_browser);
  ExpectTabCounts(incognito_browser, 1, 0);
  CloseBrowserSynchronously(incognito_browser);
  ASSERT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_TRUE(g_browser_process->local_state()->GetBoolean(
      prefs::kCanShowOobeGoodiesPage));

  // First logged-in browser shows Goodies if --no-first-run is not specified.
  AddBlankTabAndShow(browser);
  if (NoFirstRunSpecified())
    ExpectTabCounts(browser, 1, 0);
  else
    ExpectTabCounts(browser, 2, 1);

  EXPECT_FALSE(g_browser_process->local_state()->GetBoolean(
      prefs::kCanShowOobeGoodiesPage));

  // Next time a browser is opened, no Goodies.
  Browser* browser2 = CreateBrowser(browser->profile());
  ASSERT_EQ(2u, chrome::GetTotalBrowserCount());
  ExpectTabCounts(browser2, 1, 0);
}

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         GoodiesDisplayerBrowserTest,
                         testing::Values(true, false));

}  // namespace chromeos

