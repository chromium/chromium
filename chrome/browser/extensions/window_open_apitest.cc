// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/base_window.h"

using content::OpenURLParams;
using content::Referrer;
using content::WebContents;

namespace aura {
class Window;
}

namespace extensions {

class WindowOpenApiTest : public ExtensionApiTest {
 public:
  static int CountBrowsersForType(BrowserWindowInterface::Type type) {
    return ui_test_utils::FindMatchingBrowsers(
               [type](BrowserWindowInterface* browser) {
                 return browser->GetType() == type;
               })
        .size();
  }

 protected:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }
};

bool WaitForTabsPopupsApps(Browser* browser,
                           int num_tabs,
                           int num_popups,
                           int num_app_popups) {
  SCOPED_TRACE(base::StringPrintf(
      "WaitForTabsPopupsApps tabs:%d, popups:%d, app_popups:%d", num_tabs,
      num_popups, num_app_popups));
  // We start with one tab and one browser already open.
  ++num_tabs;
  size_t num_browsers = static_cast<size_t>(num_popups + num_app_popups) + 1;

  const base::TimeDelta kWaitTime = base::Seconds(10);
  base::TimeTicks end_time = base::TimeTicks::Now() + kWaitTime;
  while (base::TimeTicks::Now() < end_time) {
    if (extensions::browsertest_util::GetWindowControllerCountInProfile(
            browser->profile()) == num_browsers &&
        browser->tab_strip_model()->count() == num_tabs) {
      break;
    }

    content::RunAllTasksUntilIdle();
  }

  EXPECT_EQ(num_browsers,
            extensions::browsertest_util::GetWindowControllerCountInProfile(
                browser->profile()));
  EXPECT_EQ(num_tabs, browser->tab_strip_model()->count());

  EXPECT_EQ(num_popups, WindowOpenApiTest::CountBrowsersForType(
                            BrowserWindowInterface::TYPE_POPUP));
  EXPECT_EQ(num_app_popups, WindowOpenApiTest::CountBrowsersForType(
                                BrowserWindowInterface::TYPE_APP_POPUP));

  return ((num_browsers ==
           extensions::browsertest_util::GetWindowControllerCountInProfile(
               browser->profile())) &&
          (num_tabs == browser->tab_strip_model()->count()) &&
          (num_popups == WindowOpenApiTest::CountBrowsersForType(
                             BrowserWindowInterface::TYPE_POPUP)) &&
          (num_app_popups == WindowOpenApiTest::CountBrowsersForType(
                                 BrowserWindowInterface::TYPE_APP_POPUP)));
}

IN_PROC_BROWSER_TEST_F(WindowOpenApiTest, BrowserIsApp) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("window_open").AppendASCII("browser_is_app")));

  constexpr int kExpectedAppPopups = 2;
  EXPECT_TRUE(WaitForTabsPopupsApps(browser(), 0, 0, kExpectedAppPopups));

  EXPECT_NE(browser()->GetType(), BrowserWindowInterface::Type::TYPE_APP_POPUP);
  EXPECT_EQ(kExpectedAppPopups,
            CountBrowsersForType(BrowserWindowInterface::Type::TYPE_APP_POPUP));
}

IN_PROC_BROWSER_TEST_F(WindowOpenApiTest, WindowOpenPopupDefault) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("window_open").AppendASCII("popup")));

  EXPECT_TRUE(WaitForTabsPopupsApps(browser(), 1, 0, 0));
}

IN_PROC_BROWSER_TEST_F(WindowOpenApiTest, WindowOpenPopupIframe) {
  base::FilePath test_data_dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
  embedded_test_server()->ServeFilesFromDirectory(test_data_dir);
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("window_open").AppendASCII("popup_iframe")));

  EXPECT_TRUE(WaitForTabsPopupsApps(browser(), 1, 0, 0));
}

IN_PROC_BROWSER_TEST_F(WindowOpenApiTest, WindowOpenPopupLarge) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("window_open").AppendASCII("popup_large")));

  // On other systems this should open a new popup window.
  EXPECT_TRUE(WaitForTabsPopupsApps(browser(), 0, 0, 1));
}

IN_PROC_BROWSER_TEST_F(WindowOpenApiTest, WindowOpenPopupSmall) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("window_open").AppendASCII("popup_small")));

  // On ChromeOS this should open a new panel (acts like a new popup window).
  // On other systems this should open a new popup window.
  EXPECT_TRUE(WaitForTabsPopupsApps(browser(), 0, 0, 1));
}

IN_PROC_BROWSER_TEST_F(WindowOpenApiTest, PopupBlockingExtension) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("window_open").AppendASCII("popup_blocking")
      .AppendASCII("extension")));

  EXPECT_TRUE(WaitForTabsPopupsApps(browser(), 5, 2, 1));
}

IN_PROC_BROWSER_TEST_F(WindowOpenApiTest, PopupBlockingHostedApp) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("window_open").AppendASCII("popup_blocking")
      .AppendASCII("hosted_app")));

  // The app being tested owns the domain a.com .  The test URLs we navigate
  // to below must be within that domain, so that they fall within the app's
  // web extent.
  GURL::Replacements replace_host;
  replace_host.SetHostStr("a.com");

  const std::string popup_app_contents_path(
      "/extensions/api_test/window_open/popup_blocking/hosted_app/");

  GURL open_tab = embedded_test_server()
                      ->GetURL(popup_app_contents_path + "open_tab.html")
                      .ReplaceComponents(replace_host);
  GURL open_popup = embedded_test_server()
                        ->GetURL(popup_app_contents_path + "open_popup.html")
                        .ReplaceComponents(replace_host);

  browser()->OpenURL(OpenURLParams(open_tab, Referrer(),
                                   WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                   ui::PAGE_TRANSITION_TYPED, false),
                     /*navigation_handle_callback=*/{});
  browser()->OpenURL(OpenURLParams(open_popup, Referrer(),
                                   WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                   ui::PAGE_TRANSITION_TYPED, false),
                     /*navigation_handle_callback=*/{});

  EXPECT_TRUE(WaitForTabsPopupsApps(browser(), 3, 1, 0));
}

IN_PROC_BROWSER_TEST_F(WindowOpenApiTest, WindowArgumentsOverflow) {
  ASSERT_TRUE(RunExtensionTest("window_open/argument_overflow")) << message_;
}

IN_PROC_BROWSER_TEST_F(WindowOpenApiTest, WindowOpener) {
  ASSERT_TRUE(RunExtensionTest("window_open/opener")) << message_;
}

// Ensure that the width and height properties of a window opened with
// chrome.windows.create match the creation parameters. See crbug.com/173831.
IN_PROC_BROWSER_TEST_F(WindowOpenApiTest, WindowOpenSized) {
  ASSERT_TRUE(RunExtensionTest("window_open/window_size")) << message_;
  EXPECT_TRUE(WaitForTabsPopupsApps(browser(), 0, 0, 1));
}

// Tests that an extension page can call window.open to an extension URL and
// the new window has extension privileges.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, WindowOpenExtension) {
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("uitest").AppendASCII("window_open")));

  GURL start_url(std::string(extensions::kExtensionScheme) +
                     url::kStandardSchemeSeparator +
                     last_loaded_extension_id() + "/test.html");
  auto* web_contents = GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(web_contents, start_url));
  WebContents* newtab = nullptr;
  ASSERT_NO_FATAL_FAILURE(OpenWindow(
      web_contents, start_url.Resolve("newtab.html"), true, true, &newtab));

  EXPECT_EQ(true, content::EvalJs(newtab, "testExtensionApi()"));
}

// Tests that if an extension page calls window.open to an invalid extension
// URL, the browser doesn't crash.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, WindowOpenInvalidExtension) {
  const extensions::Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("uitest").AppendASCII("window_open"));
  ASSERT_TRUE(extension);

  GURL start_url = extension->GetResourceURL("test.html");
  auto* web_contents = GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(web_contents, start_url));
  WebContents* newtab = nullptr;
  bool new_page_in_same_process = false;
  bool expect_success = false;
  GURL broken_extension_url(
      "chrome-extension://thisissurelynotavalidextensionid/newtab.html");
  ASSERT_NO_FATAL_FAILURE(OpenWindow(web_contents, broken_extension_url,
                                     new_page_in_same_process, expect_success,
                                     &newtab));

  EXPECT_EQ(broken_extension_url,
            newtab->GetPrimaryMainFrame()->GetLastCommittedURL());
  EXPECT_EQ(content::PAGE_TYPE_ERROR,
            newtab->GetController().GetLastCommittedEntry()->GetPageType());
}

// Tests that calling window.open from the newtab page to an extension URL
// gives the new window extension privileges - even though the opening page
// does not have extension privileges, we break the script connection, so
// there is no privilege leak.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, WindowOpenNoPrivileges) {
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("uitest").AppendASCII("window_open")));

  auto* web_contents = GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(web_contents, GURL("about:blank")));
  WebContents* newtab = nullptr;
  ASSERT_NO_FATAL_FAILURE(
      OpenWindow(web_contents,
                 GURL(std::string(extensions::kExtensionScheme) +
                      url::kStandardSchemeSeparator +
                      last_loaded_extension_id() + "/newtab.html"),
                 false, true, &newtab));

  // Extension API should succeed.
  EXPECT_EQ(true, content::EvalJs(newtab, "testExtensionApi()"));
}

// Tests that calling window.open for an extension URL from a non-HTTP or HTTPS
// URL on a new tab cannot access non-web-accessible resources.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest,
                       WindowOpenInaccessibleResourceFromDataURL) {
  const extensions::Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("uitest").AppendASCII("window_open"));
  ASSERT_TRUE(extension);

  auto* web_contents = GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(web_contents, GURL("data:text/html,foo")));

  // test.html is not web-accessible and should not be loaded.
  GURL extension_url(extension->GetResourceURL("test.html"));
  content::CreateAndLoadWebContentsObserver windowed_observer;
  ASSERT_TRUE(content::ExecJs(web_contents,
                              "window.open('" + extension_url.spec() + "');"));
  content::WebContents* newtab = windowed_observer.Wait();
  ASSERT_TRUE(newtab);

  EXPECT_EQ(content::PAGE_TYPE_ERROR,
            newtab->GetController().GetLastCommittedEntry()->GetPageType());
  EXPECT_EQ(extension_url,
            newtab->GetPrimaryMainFrame()->GetLastCommittedURL());
  EXPECT_FALSE(
      newtab->GetPrimaryMainFrame()->GetSiteInstance()->GetSiteURL().SchemeIs(
          extensions::kExtensionScheme));
}

// Test that navigating to an extension URL is allowed on chrome://.
// See https://crbug.com/662602.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest,
                       NavigateToInaccessibleResourceFromChromeURL) {
  // Mint an extension URL which is not web-accessible.
  const extensions::Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("uitest").AppendASCII("window_open"));
  ASSERT_TRUE(extension);
  GURL extension_url(extension->GetResourceURL("test.html"));

  content::WebContents* tab = GetActiveWebContents();

  // Navigate to the non-web-accessible URL from chrome:// and
  // chrome-search:// pages.  Verify that the page loads correctly.
  GURL history_url(chrome::kChromeUIHistoryURL);
  ASSERT_TRUE(history_url.SchemeIs(content::kChromeUIScheme));
  ASSERT_TRUE(NavigateToURL(tab, history_url));
  EXPECT_EQ(history_url, tab->GetPrimaryMainFrame()->GetLastCommittedURL());

  content::TestNavigationObserver observer(tab);
  ASSERT_TRUE(
      content::ExecJs(tab, "location.href = '" + extension_url.spec() + "';"));
  observer.Wait();
  EXPECT_EQ(extension_url, tab->GetPrimaryMainFrame()->GetLastCommittedURL());
  EXPECT_EQ("HOWDIE!!!", content::EvalJs(tab, "document.body.innerText"));
}

}  // namespace extensions
