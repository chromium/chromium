// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/optimization_guide_tab_url_provider.h"

#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::ElementsAre;

class OptimizationGuideTabUrlProviderBrowserTest : public InProcessBrowserTest {
 public:
  OptimizationGuideTabUrlProviderBrowserTest() = default;
  ~OptimizationGuideTabUrlProviderBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_F(OptimizationGuideTabUrlProviderBrowserTest,
                       GetUrlsNoOpenTabs) {
  // InProcessBrowserTest starts with one tab for the default profile. To test
  // the scenario where a profile has 0 open tabs, we cannot simply close the
  // last browser window because that triggers the application shutdown sequence
  // (which destroys the Profile we want to test).
  //
  // Additionally, creating a secondary "empty" profile is problematic on
  // ChromeOS, where profiles are strictly tied to logged-in users.
  //
  // Instead, we open an Incognito browser to keep the BrowserProcess and the
  // original Profile alive, and then safely close the main browser window.
  CreateIncognitoBrowser();
  Profile* original_profile = browser()->profile();

  // Close the only window associated with original_profile.
  CloseBrowserSynchronously(browser());

  OptimizationGuideTabUrlProvider provider(original_profile);

  // The provider iterates over all open browser windows. It should find 0 tabs
  // matching `original_profile` because the only open window belongs to the
  // OTR profile.
  std::vector<GURL> urls = provider.GetUrlsOfActiveTabs(base::Days(90));
  EXPECT_TRUE(urls.empty());
}

IN_PROC_BROWSER_TEST_F(OptimizationGuideTabUrlProviderBrowserTest,
                       GetUrlsFiltersOutIncognitoTabs) {
  // Create an Incognito browser.
  Browser* otr_browser = CreateIncognitoBrowser();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(otr_browser,
                                           GURL("https://otrshouldskip.com")));

  // Use the default browser for regular tabs.
  // The first tab already exists, so navigate it.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://example.com")));

  // Add another tab to the regular browser.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://example2.com"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  OptimizationGuideTabUrlProvider provider(browser()->profile());
  std::vector<GURL> urls = provider.GetUrlsOfActiveTabs(base::Days(90));

  // The behavior of OptimizationGuideTabUrlProvider is to sort by last active
  // time (descending). example2.com was added last and is active, so it should
  // be first.
  EXPECT_THAT(urls, ElementsAre(GURL("https://example2.com"),
                                GURL("https://example.com")));
}
