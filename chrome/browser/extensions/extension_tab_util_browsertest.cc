// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/manifest_handlers/options_page_info.h"

namespace extensions {

namespace {

const GURL& GetActiveUrl(Browser* browser) {
  return browser->tab_strip_model()
      ->GetActiveWebContents()
      ->GetLastCommittedURL();
}

}  // namespace

using ExtensionTabUtilBrowserTest = ExtensionBrowserTest;

// TODO(crbug.com/41370170): Fix and re-enable.
IN_PROC_BROWSER_TEST_F(ExtensionTabUtilBrowserTest,
                       DISABLED_OpenExtensionsOptionsPage) {
  // Load an extension with an options page that opens in a tab and one that
  // opens in the chrome://extensions page in a view.
  const Extension* options_in_tab =
      LoadExtension(test_data_dir_.AppendASCII("options_page"));
  const Extension* options_in_view =
      LoadExtension(test_data_dir_.AppendASCII("options_page_in_view"));
  ASSERT_TRUE(options_in_tab);
  ASSERT_TRUE(options_in_view);
  ASSERT_TRUE(OptionsPageInfo::HasOptionsPage(options_in_tab));
  ASSERT_TRUE(OptionsPageInfo::HasOptionsPage(options_in_view));

  // Start at the new tab page, and then open the extension options page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUINewTabURL)));
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  GURL options_url = OptionsPageInfo::GetOptionsPage(options_in_tab);
  EXPECT_TRUE(ExtensionTabUtil::OpenOptionsPage(options_in_tab, browser()));

  // Opening the options page should take the new tab and use it, so we should
  // have only one tab, and it should be open to the options page.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_TRUE(content::WaitForLoadStop(
                  browser()->tab_strip_model()->GetActiveWebContents()));
  EXPECT_EQ(options_url, GetActiveUrl(browser()));

  // Calling OpenOptionsPage again shouldn't result in any new tabs, since we
  // re-use the existing options page.
  EXPECT_TRUE(ExtensionTabUtil::OpenOptionsPage(options_in_tab, browser()));
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_TRUE(content::WaitForLoadStop(
                  browser()->tab_strip_model()->GetActiveWebContents()));
  EXPECT_EQ(options_url, GetActiveUrl(browser()));

  // Navigate to google.com (something non-newtab, non-options). Calling
  // OpenOptionsPage() should create a new tab and navigate it to the options
  // page. So we should have two total tabs, with the active tab pointing to
  // options.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("http://www.google.com/")));
  EXPECT_TRUE(ExtensionTabUtil::OpenOptionsPage(options_in_tab, browser()));
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_TRUE(content::WaitForLoadStop(
                  browser()->tab_strip_model()->GetActiveWebContents()));
  EXPECT_EQ(options_url, GetActiveUrl(browser()));

  // Navigate the tab to a different extension URL, and call OpenOptionsPage().
  // We should not reuse the current tab since it's opened to a page that isn't
  // the options page, and we don't want to arbitrarily close extension content.
  // Regression test for crbug.com/587581.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), options_in_tab->GetResourceURL("other.html")));
  EXPECT_TRUE(ExtensionTabUtil::OpenOptionsPage(options_in_tab, browser()));
  EXPECT_EQ(3, browser()->tab_strip_model()->count());
  EXPECT_TRUE(content::WaitForLoadStop(
                  browser()->tab_strip_model()->GetActiveWebContents()));
  EXPECT_EQ(options_url, GetActiveUrl(browser()));

  // If the user navigates to the options page e.g. by typing in the url, it
  // should not override the currently-open tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), options_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  EXPECT_EQ(4, browser()->tab_strip_model()->count());
  EXPECT_EQ(options_url, GetActiveUrl(browser()));

  // Test the extension that has the options page open in a view inside
  // chrome://extensions.
  // Triggering OpenOptionsPage() should create a new tab, since there are none
  // to override.
  options_url = GURL(std::string(chrome::kChromeUIExtensionsURL) +
                     "?options=" + options_in_view->id());
  EXPECT_TRUE(ExtensionTabUtil::OpenOptionsPage(options_in_view, browser()));
  EXPECT_EQ(5, browser()->tab_strip_model()->count());
  EXPECT_TRUE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents()));
  EXPECT_EQ(options_url, GetActiveUrl(browser()));

  // Calling it a second time should not create a new tab, since one already
  // exists with that options page open.
  EXPECT_TRUE(ExtensionTabUtil::OpenOptionsPage(options_in_view, browser()));
  EXPECT_EQ(5, browser()->tab_strip_model()->count());
  EXPECT_TRUE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents()));
  EXPECT_EQ(options_url, GetActiveUrl(browser()));

  // Navigate to chrome://extensions (no options). Calling OpenOptionsPage()
  // should override that tab rather than opening a new tab. crbug.com/595253.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIExtensionsURL)));
  EXPECT_TRUE(ExtensionTabUtil::OpenOptionsPage(options_in_view, browser()));
  EXPECT_EQ(5, browser()->tab_strip_model()->count());
  EXPECT_TRUE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents()));
  EXPECT_EQ(options_url, GetActiveUrl(browser()));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabUtilBrowserTest,
                       OpenSplitModeExtensionOptionsPageIncognito) {
  const Extension* options_split_extension =
      LoadExtension(test_data_dir_.AppendASCII("options_page_split_incognito"),
                    {.allow_in_incognito = true});
  ASSERT_TRUE(options_split_extension);
  ASSERT_TRUE(OptionsPageInfo::HasOptionsPage(options_split_extension));
  GURL options_url = OptionsPageInfo::GetOptionsPage(options_split_extension);

  Browser* incognito = CreateIncognitoBrowser();

  // There should be two browser windows open, regular and incognito.
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());

  // In the regular browser window, start at the new tab page, and then open the
  // extension options page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUINewTabURL)));
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_TRUE(
      ExtensionTabUtil::OpenOptionsPage(options_split_extension, browser()));

  // Opening the options page should take the new tab and use it, so we should
  // have only one tab, and it should be open to the options page.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_TRUE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents()));
  EXPECT_EQ(options_url, GetActiveUrl(browser()));

  // If the options page is already opened from a regular window, calling
  // OpenOptionsPage() from an incognito window should not refocus to the
  // options page in the regular window, but instead open the options page in
  // the incognito window.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(incognito,
                                           GURL(chrome::kChromeUINewTabURL)));
  EXPECT_EQ(1, incognito->tab_strip_model()->count());
  EXPECT_TRUE(ExtensionTabUtil::OpenOptionsPageFromAPI(options_split_extension,
                                                       incognito->profile()));
  EXPECT_EQ(1, incognito->tab_strip_model()->count());
  EXPECT_TRUE(content::WaitForLoadStop(
      incognito->tab_strip_model()->GetActiveWebContents()));
  EXPECT_EQ(options_url, GetActiveUrl(incognito));

  // Both regular and incognito windows should have one tab each.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, incognito->tab_strip_model()->count());

  // Reset the incognito browser.
  CloseBrowserSynchronously(incognito);
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  incognito = CreateIncognitoBrowser();

  // Close the regular browser.
  CloseBrowserSynchronously(browser());
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());

  // In the incognito browser, start at the new tab page, and then open the
  // extension options page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(incognito,
                                           GURL(chrome::kChromeUINewTabURL)));
  EXPECT_EQ(1, incognito->tab_strip_model()->count());
  EXPECT_TRUE(ExtensionTabUtil::OpenOptionsPageFromAPI(options_split_extension,
                                                       incognito->profile()));

  // Opening the options page should take the new tab and use it, so we should
  // have only one tab, and it should be open to the options page.
  EXPECT_EQ(1, incognito->tab_strip_model()->count());
  EXPECT_TRUE(content::WaitForLoadStop(
      incognito->tab_strip_model()->GetActiveWebContents()));
  EXPECT_EQ(options_url, GetActiveUrl(incognito));

  // Calling OpenOptionsPage again shouldn't result in any new tabs, since we
  // re-use the existing options page.
  EXPECT_TRUE(ExtensionTabUtil::OpenOptionsPageFromAPI(options_split_extension,
                                                       incognito->profile()));
  EXPECT_EQ(1, incognito->tab_strip_model()->count());
  EXPECT_TRUE(content::WaitForLoadStop(
      incognito->tab_strip_model()->GetActiveWebContents()));
  EXPECT_EQ(options_url, GetActiveUrl(incognito));

  // Navigate to google.com (something non-newtab, non-options). Calling
  // OpenOptionsPage() should create a new tab and navigate it to the options
  // page. So we should have two total tabs, with the active tab pointing to
  // options.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(incognito, GURL("http://www.google.com/")));
  EXPECT_TRUE(ExtensionTabUtil::OpenOptionsPageFromAPI(options_split_extension,
                                                       incognito->profile()));
  EXPECT_EQ(2, incognito->tab_strip_model()->count());
  EXPECT_TRUE(content::WaitForLoadStop(
      incognito->tab_strip_model()->GetActiveWebContents()));
  EXPECT_EQ(options_url, GetActiveUrl(incognito));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabUtilBrowserTest,
                       OpenSpanningModeExtensionOptionsPageIncognito) {
  const Extension* options_spanning_extension = LoadExtension(
      test_data_dir_.AppendASCII("options_page_spanning_incognito"),
      {.allow_in_incognito = true});
  ASSERT_TRUE(options_spanning_extension);
  ASSERT_TRUE(OptionsPageInfo::HasOptionsPage(options_spanning_extension));
  GURL options_url =
      OptionsPageInfo::GetOptionsPage(options_spanning_extension);

  // Start a regular browser window with two tabs, one that is non-options,
  // non-newtab and the other that is the options page.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("http://www.google.com/")));
  EXPECT_TRUE(
      ExtensionTabUtil::OpenOptionsPage(options_spanning_extension, browser()));
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_TRUE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents()));
  EXPECT_EQ(options_url, GetActiveUrl(browser()));
  // Switch to tab containing google.com such that it is the active tab.
  browser()->tab_strip_model()->SelectPreviousTab();
  EXPECT_EQ(GURL("http://www.google.com/"), GetActiveUrl(browser()));

  // Spanning mode extensions can never open pages in incognito so a regular
  // (non-OTR) profile must be used. If the options page is already opened from
  // a regular window, calling OpenOptionsPage() from an incognito window should
  // refocus to the options page in the regular window.
  Browser* incognito = CreateIncognitoBrowser();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(incognito,
                                           GURL(chrome::kChromeUINewTabURL)));
  EXPECT_EQ(1, incognito->tab_strip_model()->count());
  EXPECT_TRUE(ExtensionTabUtil::OpenOptionsPageFromAPI(
      options_spanning_extension, profile()));
  // There should be two browser windows open, regular and incognito.
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  // Ensure that the regular browser is the foreground browser.
  EXPECT_EQ(browser(), BrowserList::GetInstance()->GetLastActive());
  // The options page in the regular window should be in focus instead of
  // the tab pointing to www.google.com.
  EXPECT_TRUE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents()));
  EXPECT_EQ(options_url, GetActiveUrl(browser()));

  // Only the incognito browser should be left.
  CloseBrowserSynchronously(browser());
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());

  // Start at the new tab page in incognito and open the extension options page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(incognito,
                                           GURL(chrome::kChromeUINewTabURL)));
  EXPECT_EQ(1, incognito->tab_strip_model()->count());
  EXPECT_TRUE(ExtensionTabUtil::OpenOptionsPageFromAPI(
      options_spanning_extension, profile()));

  // Opening the options page from an incognito window should open a new regular
  // profile window, which should have one tab open to the options page.
  ASSERT_EQ(2u, chrome::GetTotalBrowserCount());
  BrowserList* browser_list = BrowserList::GetInstance();
  Browser* regular = !browser_list->get(0u)->profile()->IsOffTheRecord()
                         ? browser_list->get(0u)
                         : browser_list->get(1u);
  EXPECT_EQ(1, regular->tab_strip_model()->count());
  EXPECT_TRUE(content::WaitForLoadStop(
      regular->tab_strip_model()->GetActiveWebContents()));
  EXPECT_EQ(options_url, GetActiveUrl(regular));

  // Leave only incognito browser open.
  CloseBrowserSynchronously(regular);
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());

  // Right-clicking on an extension action icon in the toolbar and selecting
  // options should open the options page in a regular window. In this case, the
  // profile is an OTR profile instead of a non-OTR profile, as described above.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(incognito,
                                           GURL(chrome::kChromeUINewTabURL)));
  EXPECT_EQ(1, incognito->tab_strip_model()->count());
  // Because the OpenOptionsPage() call originates from an OTR window via, e.g.
  // the action menu, instead of initiated by the extension, the
  // OpenOptionsPage() version that takes a Browser* is used.
  EXPECT_TRUE(
      ExtensionTabUtil::OpenOptionsPage(options_spanning_extension, incognito));
  // There should be two browser windows open, regular and incognito.
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  browser_list = BrowserList::GetInstance();
  regular = !browser_list->get(0u)->profile()->IsOffTheRecord()
                ? browser_list->get(0u)
                : browser_list->get(1u);
  // Ensure that the regular browser is the foreground browser.
  EXPECT_EQ(regular, browser_list->GetLastActive());
  EXPECT_EQ(1, regular->tab_strip_model()->count());
  EXPECT_TRUE(content::WaitForLoadStop(
      regular->tab_strip_model()->GetActiveWebContents()));
  EXPECT_EQ(options_url, GetActiveUrl(regular));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabUtilBrowserTest, RecordNavigationScheme) {
  const std::string kHttpUrl("http://example.com");
  const std::string kHttpsUrl("https://example.com");
  const std::string kChromeUrl("chrome://settings");
  const std::string kFileUrl("file:///etc/passwd");
  const std::string kOtherUrl("data:,test");
  struct {
    std::string url;
    int expected_bucket;
  } test_cases[] = {
      {kHttpUrl, 0}, {kHttpsUrl, 0}, {kChromeUrl, 1},
      {kFileUrl, 2}, {kOtherUrl, 4},
  };
  std::string error;
  GURL url;

  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("simple_with_file"));
  ExtensionId id = extension->id();
  TestExtensionRegistryObserver observer(ExtensionRegistry::Get(profile()), id);
  // Allow file access. This will reload the extension, so we need to reset the
  // extension pointer.
  util::SetAllowFileAccess(id, profile(), true);
  extension = observer.WaitForExtensionLoaded().get();

  for (const auto& test_case : test_cases) {
    base::HistogramTester histogram_tester;
    auto result_url = ExtensionTabUtil::PrepareURLForNavigation(
        test_case.url, extension, profile());
    histogram_tester.ExpectBucketCount("Extensions.Navigation.Scheme",
                                       test_case.expected_bucket, 1);
  }
}

}  // namespace extensions
