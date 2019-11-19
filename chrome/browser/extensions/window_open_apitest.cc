// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/base_window.h"

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/window_pin_type.h"
#include "ash/public/cpp/window_properties.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "ui/aura/window.h"
#endif

using content::OpenURLParams;
using content::Referrer;
using content::WebContents;

namespace aura {
class Window;
}

namespace extensions {

class WindowOpenApiTest : public ExtensionApiTest {
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }
};

// The test uses the chrome.browserAction.openPopup API, which requires that the
// window can automatically be activated.
// See comments at BrowserActionInteractiveTest::ShouldRunPopupTest
// Fails flakily on all platforms. https://crbug.com/477691
IN_PROC_BROWSER_TEST_F(WindowOpenApiTest, DISABLED_WindowOpen) {
  extensions::ResultCatcher catcher;
  ASSERT_TRUE(LoadExtensionIncognito(test_data_dir_
      .AppendASCII("window_open").AppendASCII("spanning")));
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

bool WaitForTabsPopupsApps(Browser* browser,
                           int num_tabs,
                           int num_popups,
                           int num_apps) {
  SCOPED_TRACE(
      base::StringPrintf("WaitForTabsPopupsApps tabs:%d, popups:%d, apps:%d",
                         num_tabs, num_popups, num_apps));
  // We start with one tab and one browser already open.
  ++num_tabs;
  size_t num_browsers = static_cast<size_t>(num_popups + num_apps) + 1;

  const base::TimeDelta kWaitTime = base::TimeDelta::FromSeconds(10);
  base::TimeTicks end_time = base::TimeTicks::Now() + kWaitTime;
  while (base::TimeTicks::Now() < end_time) {
    if (chrome::GetBrowserCount(browser->profile()) == num_browsers &&
        browser->tab_strip_model()->count() == num_tabs)
      break;

    content::RunAllPendingInMessageLoop();
  }

  EXPECT_EQ(num_browsers, chrome::GetBrowserCount(browser->profile()));
  EXPECT_EQ(num_tabs, browser->tab_strip_model()->count());

  int num_popups_seen = 0;
  int num_apps_seen = 0;
  for (auto* b : *BrowserList::GetInstance()) {
    if (b == browser)
      continue;

    EXPECT_TRUE(b->is_type_popup() || b->is_type_app());
    if (b->is_type_popup())
      ++num_popups_seen;
    else if (b->is_type_app())
      ++num_apps_seen;
  }
  EXPECT_EQ(num_popups, num_popups_seen);
  EXPECT_EQ(num_apps, num_apps_seen);

  return ((num_browsers == chrome::GetBrowserCount(browser->profile())) &&
          (num_tabs == browser->tab_strip_model()->count()) &&
          (num_popups == num_popups_seen) && (num_apps == num_apps_seen));
}

IN_PROC_BROWSER_TEST_F(WindowOpenApiTest, BrowserIsApp) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("window_open").AppendASCII("browser_is_app")));

  EXPECT_TRUE(WaitForTabsPopupsApps(browser(), 0, 0, 2));

  for (auto* b : *BrowserList::GetInstance()) {
    if (b == browser())
      ASSERT_FALSE(b->is_type_app());
    else
      ASSERT_TRUE(b->is_type_app());
  }
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

// Disabled on Windows. Often times out or fails: crbug.com/177530
#if defined(OS_WIN)
#define MAYBE_PopupBlockingExtension DISABLED_PopupBlockingExtension
#else
#define MAYBE_PopupBlockingExtension PopupBlockingExtension
#endif
IN_PROC_BROWSER_TEST_F(WindowOpenApiTest, MAYBE_PopupBlockingExtension) {
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
                                   ui::PAGE_TRANSITION_TYPED, false));
  browser()->OpenURL(OpenURLParams(open_popup, Referrer(),
                                   WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                   ui::PAGE_TRANSITION_TYPED, false));

  EXPECT_TRUE(WaitForTabsPopupsApps(browser(), 3, 1, 0));
}

IN_PROC_BROWSER_TEST_F(WindowOpenApiTest, WindowArgumentsOverflow) {
  ASSERT_TRUE(RunExtensionTest("window_open/argument_overflow")) << message_;
}

IN_PROC_BROWSER_TEST_F(WindowOpenApiTest, DISABLED_WindowOpener) {
  ASSERT_TRUE(RunExtensionTest("window_open/opener")) << message_;
}

#if defined(OS_MACOSX)
// Extension popup windows are incorrectly sized on OSX, crbug.com/225601
#define MAYBE_WindowOpenSized DISABLED_WindowOpenSized
#else
#define MAYBE_WindowOpenSized WindowOpenSized
#endif
// Ensure that the width and height properties of a window opened with
// chrome.windows.create match the creation parameters. See crbug.com/173831.
IN_PROC_BROWSER_TEST_F(WindowOpenApiTest, MAYBE_WindowOpenSized) {
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
  ui_test_utils::NavigateToURL(browser(), start_url);
  WebContents* newtab = NULL;
  ASSERT_NO_FATAL_FAILURE(
      OpenWindow(browser()->tab_strip_model()->GetActiveWebContents(),
                 start_url.Resolve("newtab.html"), true, true, &newtab));

  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(newtab, "testExtensionApi()",
                                                   &result));
  EXPECT_TRUE(result);
}

// Tests that if an extension page calls window.open to an invalid extension
// URL, the browser doesn't crash.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, WindowOpenInvalidExtension) {
  const extensions::Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("uitest").AppendASCII("window_open"));
  ASSERT_TRUE(extension);

  GURL start_url = extension->GetResourceURL("/test.html");
  ui_test_utils::NavigateToURL(browser(), start_url);
  WebContents* newtab = nullptr;
  bool new_page_in_same_process = false;
  bool expect_success = false;
  GURL broken_extension_url(
      "chrome-extension://thisissurelynotavalidextensionid/newtab.html");
  ASSERT_NO_FATAL_FAILURE(OpenWindow(
      browser()->tab_strip_model()->GetActiveWebContents(),
      broken_extension_url, new_page_in_same_process, expect_success, &newtab));

  EXPECT_EQ(broken_extension_url,
            newtab->GetMainFrame()->GetLastCommittedURL());
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

  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  WebContents* newtab = NULL;
  ASSERT_NO_FATAL_FAILURE(
      OpenWindow(browser()->tab_strip_model()->GetActiveWebContents(),
                 GURL(std::string(extensions::kExtensionScheme) +
                      url::kStandardSchemeSeparator +
                      last_loaded_extension_id() + "/newtab.html"),
                 false, true, &newtab));

  // Extension API should succeed.
  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(newtab, "testExtensionApi()",
                                                   &result));
  EXPECT_TRUE(result);
}

// Tests that calling window.open for an extension URL from a non-HTTP or HTTPS
// URL on a new tab cannot access non-web-accessible resources.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest,
                       WindowOpenInaccessibleResourceFromDataURL) {
  const extensions::Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("uitest").AppendASCII("window_open"));
  ASSERT_TRUE(extension);

  ui_test_utils::NavigateToURL(browser(), GURL("data:text/html,foo"));

  // test.html is not web-accessible and should not be loaded.
  GURL extension_url(extension->GetResourceURL("test.html"));
  content::WindowedNotificationObserver windowed_observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  ASSERT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.open('" + extension_url.spec() + "');"));
  windowed_observer.Wait();
  content::NavigationController* controller =
      content::Source<content::NavigationController>(windowed_observer.source())
          .ptr();
  content::WebContents* newtab = controller->GetWebContents();
  ASSERT_TRUE(newtab);

  EXPECT_EQ(content::PAGE_TYPE_ERROR,
            newtab->GetController().GetLastCommittedEntry()->GetPageType());
  EXPECT_EQ(extension_url, newtab->GetMainFrame()->GetLastCommittedURL());
  EXPECT_FALSE(newtab->GetMainFrame()->GetSiteInstance()->GetSiteURL().SchemeIs(
      extensions::kExtensionScheme));
}

// Test that navigating to an extension URL is allowed on chrome:// and
// chrome-search:// pages, even for URLs that are not web-accessible.
// See https://crbug.com/662602.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest,
                       NavigateToInaccessibleResourceFromChromeURL) {
  // Mint an extension URL which is not web-accessible.
  const extensions::Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("uitest").AppendASCII("window_open"));
  ASSERT_TRUE(extension);
  GURL extension_url(extension->GetResourceURL("test.html"));

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate to the non-web-accessible URL from chrome:// and
  // chrome-search:// pages.  Verify that the page loads correctly.
  GURL history_url(chrome::kChromeUIHistoryURL);
  GURL ntp_url(chrome::kChromeSearchLocalNtpUrl);
  ASSERT_TRUE(history_url.SchemeIs(content::kChromeUIScheme));
  ASSERT_TRUE(ntp_url.SchemeIs(chrome::kChromeSearchScheme));
  GURL start_urls[] = {history_url, ntp_url};
  for (size_t i = 0; i < base::size(start_urls); i++) {
    ui_test_utils::NavigateToURL(browser(), start_urls[i]);
    EXPECT_EQ(start_urls[i], tab->GetMainFrame()->GetLastCommittedURL());

    content::TestNavigationObserver observer(tab);
    ASSERT_TRUE(content::ExecuteScript(
        tab, "location.href = '" + extension_url.spec() + "';"));
    observer.Wait();
    EXPECT_EQ(extension_url, tab->GetMainFrame()->GetLastCommittedURL());
    std::string result;
    ASSERT_TRUE(content::ExecuteScriptAndExtractString(
        tab, "domAutomationController.send(document.body.innerText)", &result));
    EXPECT_EQ("HOWDIE!!!", result);
  }
}

#if defined(OS_CHROMEOS)

namespace {

aura::Window* GetCurrentWindow() {
  extensions::WindowController* controller = nullptr;
  for (auto* iter :
       extensions::WindowControllerList::GetInstance()->windows()) {
    if (iter->window()->IsActive()) {
      controller = iter;
      break;
    }
  }
  EXPECT_TRUE(controller);
  return controller->window()->GetNativeWindow();
}

ash::WindowPinType GetCurrentWindowPinType() {
  ash::WindowPinType type =
      GetCurrentWindow()->GetProperty(ash::kWindowPinTypeKey);
  return type;
}

void SetCurrentWindowPinType(ash::WindowPinType type) {
  GetCurrentWindow()->SetProperty(ash::kWindowPinTypeKey, type);
}

}  // namespace

IN_PROC_BROWSER_TEST_F(WindowOpenApiTest, OpenLockedFullscreenWindow) {
  ASSERT_TRUE(RunExtensionTestWithArg("locked_fullscreen/with_permission",
                                      "openLockedFullscreenWindow"))
      << message_;

  // Make sure the newly created window is "trusted pinned" (which means that
  // it's in locked fullscreen mode).
  EXPECT_EQ(ash::WindowPinType::kTrustedPinned, GetCurrentWindowPinType());
}

IN_PROC_BROWSER_TEST_F(WindowOpenApiTest, UpdateWindowToLockedFullscreen) {
  ASSERT_TRUE(RunExtensionTestWithArg("locked_fullscreen/with_permission",
                                      "updateWindowToLockedFullscreen"))
      << message_;

  // Make sure the current window is put into the "trusted pinned" state.
  EXPECT_EQ(ash::WindowPinType::kTrustedPinned, GetCurrentWindowPinType());
}

IN_PROC_BROWSER_TEST_F(WindowOpenApiTest, RemoveLockedFullscreenFromWindow) {
  // After locking the window, do a LockedFullscreenStateChanged so the
  // command_controller state catches up as well.
  SetCurrentWindowPinType(ash::WindowPinType::kTrustedPinned);
  browser()->command_controller()->LockedFullscreenStateChanged();

  ASSERT_TRUE(RunExtensionTestWithArg("locked_fullscreen/with_permission",
                                      "removeLockedFullscreenFromWindow"))
      << message_;

  // Make sure the current window is removed from locked-fullscreen state.
  EXPECT_EQ(ash::WindowPinType::kNone, GetCurrentWindowPinType());
}

// Make sure that commands disabling code works in locked fullscreen mode.
IN_PROC_BROWSER_TEST_F(WindowOpenApiTest, VerifyCommandsInLockedFullscreen) {
  // IDC_EXIT is always enabled in regular mode so it's a perfect candidate for
  // testing.
  EXPECT_TRUE(browser()->command_controller()->IsCommandEnabled(IDC_EXIT));
  ASSERT_TRUE(RunExtensionTestWithArg("locked_fullscreen/with_permission",
                                      "updateWindowToLockedFullscreen"))
      << message_;

  // IDC_EXIT should always be disabled in locked fullscreen.
  EXPECT_FALSE(browser()->command_controller()->IsCommandEnabled(IDC_EXIT));

  // Some other disabled commands.
  EXPECT_FALSE(browser()->command_controller()->IsCommandEnabled(IDC_FIND));
  EXPECT_FALSE(
      browser()->command_controller()->IsCommandEnabled(IDC_ZOOM_PLUS));

  // Verify some whitelisted commands.
  EXPECT_TRUE(browser()->command_controller()->IsCommandEnabled(IDC_COPY));
  EXPECT_TRUE(browser()->command_controller()->IsCommandEnabled(IDC_PASTE));
}

IN_PROC_BROWSER_TEST_F(WindowOpenApiTest,
                       OpenLockedFullscreenWindowWithoutPermission) {
  ASSERT_TRUE(RunExtensionTestWithArg("locked_fullscreen/without_permission",
                                      "openLockedFullscreenWindow"))
      << message_;

  // Make sure no new windows get created (so only the one created by default
  // exists) since the call to chrome.windows.create fails on the javascript
  // side.
  EXPECT_EQ(1u,
            extensions::WindowControllerList::GetInstance()->windows().size());
}

IN_PROC_BROWSER_TEST_F(WindowOpenApiTest,
                       UpdateWindowToLockedFullscreenWithoutPermission) {
  ASSERT_TRUE(RunExtensionTestWithArg("locked_fullscreen/without_permission",
                                      "updateWindowToLockedFullscreen"))
      << message_;

  // chrome.windows.update call fails since this extension doesn't have the
  // correct permission and hence the current window has NONE as WindowPinType.
  EXPECT_EQ(ash::WindowPinType::kNone, GetCurrentWindowPinType());
}

IN_PROC_BROWSER_TEST_F(WindowOpenApiTest,
                       RemoveLockedFullscreenFromWindowWithoutPermission) {
  SetCurrentWindowPinType(ash::WindowPinType::kTrustedPinned);
  browser()->command_controller()->LockedFullscreenStateChanged();

  ASSERT_TRUE(RunExtensionTestWithArg("locked_fullscreen/without_permission",
                                      "removeLockedFullscreenFromWindow"))
      << message_;

  // The current window is still locked-fullscreen.
  EXPECT_EQ(ash::WindowPinType::kTrustedPinned, GetCurrentWindowPinType());
}
#endif  // defined(OS_CHROMEOS)

#if !defined(OS_CHROMEOS)
// Loading an extension requiring the 'lockWindowFullscreenPrivate' permission
// on non Chrome OS platforms should always fail since the API is available only
// on Chrome OS.
IN_PROC_BROWSER_TEST_F(WindowOpenApiTest,
                       OpenLockedFullscreenWindowNonChromeOS) {
  const extensions::Extension* extension = LoadExtensionWithFlags(
      test_data_dir_.AppendASCII("locked_fullscreen/with_permission"),
      ExtensionBrowserTest::kFlagIgnoreManifestWarnings);
  ASSERT_TRUE(extension);
  EXPECT_EQ(1u, extension->install_warnings().size());
  EXPECT_EQ(std::string("'lockWindowFullscreenPrivate' "
                        "is not allowed for specified platform."),
            extension->install_warnings().front().message);
}
#endif

}  // namespace extensions
