// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/blocked_content/popup_blocker_tab_helper.h"
#include "components/embedder_support/switches.h"
#include "components/sync/model/string_ordinal.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/install_flag.h"
#include "extensions/browser/process_map.h"
#include "extensions/common/extension.h"
#include "extensions/common/file_util.h"
#include "extensions/common/switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

using content::NavigationController;
using content::RenderViewHost;
using content::SiteInstance;
using content::WebContents;
using extensions::Extension;
using extensions::WindowControllerList;

class AppApiTest : public extensions::ExtensionApiTest {
 protected:
  // Gets the base URL for files for a specific test, making sure that it uses
  // "localhost" as the hostname, since that is what the extent is declared
  // as in the test apps manifests.
  GURL GetTestBaseURL(const std::string& test_directory) {
    GURL::Replacements replace_host;
    replace_host.SetHostStr("localhost");
    GURL base_url = embedded_test_server()->GetURL(
        "/extensions/api_test/" + test_directory + "/");
    return base_url.ReplaceComponents(replace_host);
  }

  // Pass flags to make testing apps easier.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::ExtensionApiTest::SetUpCommandLine(command_line);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        embedder_support::kDisablePopupBlocking);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        extensions::switches::kAllowHTTPBackgroundPage);
  }

  void SetUpOnMainThread() override {
    extensions::ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(StartEmbeddedTestServer());
  }

  // Helper function to test that independent tabs of the named app are loaded
  // into separate processes.
  void TestAppInstancesHelper(const std::string& app_name) {
    LOG(INFO) << "Start of test.";

    extensions::ProcessMap* process_map =
        extensions::ProcessMap::Get(browser()->profile());

    ASSERT_TRUE(LoadExtension(
        test_data_dir_.AppendASCII(app_name)));
    const Extension* extension = GetSingleLoadedExtension();

    // Open two tabs in the app, one outside it.
    GURL base_url = GetTestBaseURL(app_name);

    // Test both opening a URL in a new tab, and opening a tab and then
    // navigating it.  Either way, app tabs should be considered extension
    // processes, but they have no elevated privileges and thus should not
    // have WebUI bindings.
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), base_url.Resolve("path1/empty.html"),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    LOG(INFO) << "Nav 1.";
    EXPECT_TRUE(process_map->Contains(browser()
                                          ->tab_strip_model()
                                          ->GetWebContentsAt(1)
                                          ->GetPrimaryMainFrame()
                                          ->GetProcess()
                                          ->GetID()));
    EXPECT_FALSE(browser()->tab_strip_model()->GetWebContentsAt(1)->GetWebUI());

    ui_test_utils::TabAddedWaiter tab_add(browser());
    chrome::NewTab(browser());
    tab_add.Wait();
    LOG(INFO) << "New tab.";
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), base_url.Resolve("path2/empty.html")));
    LOG(INFO) << "Nav 2.";
    EXPECT_TRUE(process_map->Contains(browser()
                                          ->tab_strip_model()
                                          ->GetWebContentsAt(2)
                                          ->GetPrimaryMainFrame()
                                          ->GetProcess()
                                          ->GetID()));
    EXPECT_FALSE(browser()->tab_strip_model()->GetWebContentsAt(2)->GetWebUI());

    // We should have opened 2 new extension tabs. Including the original blank
    // tab, we now have 3 tabs.
    ASSERT_EQ(3, browser()->tab_strip_model()->count());
    // The two app tabs don't have the background permission. To improve
    // responsiveness, they should not be in the same process unless the
    // kProcessPerSiteUpToMainFrameThreshold feature is enabled. The assumption
    // of the kProcessPerSiteUpToMainFrameThreshold is that sharing a process
    // with a threshold doesn't hurt responsiveness.
    WebContents* tab1 = browser()->tab_strip_model()->GetWebContentsAt(1);
    WebContents* tab2 = browser()->tab_strip_model()->GetWebContentsAt(2);
    if (!base::FeatureList::IsEnabled(
            features::kProcessPerSiteUpToMainFrameThreshold)) {
      EXPECT_NE(tab1->GetPrimaryMainFrame()->GetProcess(),
                tab2->GetPrimaryMainFrame()->GetProcess());
    }

    // Opening tabs with window.open should keep the page in the opener's
    // process.
    ASSERT_EQ(1u,
              extensions::browsertest_util::GetWindowControllerCountInProfile(
                  browser()->profile()));
    OpenWindow(tab1, base_url.Resolve("path1/empty.html"), true, true, nullptr);
    LOG(INFO) << "WindowOpenHelper 1.";
    OpenWindow(tab2, base_url.Resolve("path2/empty.html"), true, true, nullptr);
    LOG(INFO) << "End of test.";
    UnloadExtension(extension->id());
  }
};

// Omits the disable-popup-blocking flag so we can cover that case.
class BlockedAppApiTest : public AppApiTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::ExtensionApiTest::SetUpCommandLine(command_line);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        extensions::switches::kAllowHTTPBackgroundPage);
  }
};

// Tests that hosted apps with the background permission get a process-per-app
// model, since all pages need to be able to script the background page.
IN_PROC_BROWSER_TEST_F(AppApiTest, AppProcess) {
  extensions::ProcessMap* process_map =
      extensions::ProcessMap::Get(browser()->profile());

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("app_process")));

  // Open two tabs in the app, one outside it.
  GURL base_url = GetTestBaseURL("app_process");

  // Test both opening a URL in a new tab, and opening a tab and then navigating
  // it.  Either way, app tabs should be considered extension processes, but
  // they have no elevated privileges and thus should not have WebUI bindings.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), base_url.Resolve("path1/empty.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  EXPECT_TRUE(process_map->Contains(browser()
                                        ->tab_strip_model()
                                        ->GetWebContentsAt(1)
                                        ->GetPrimaryMainFrame()
                                        ->GetProcess()
                                        ->GetID()));
  EXPECT_FALSE(browser()->tab_strip_model()->GetWebContentsAt(1)->GetWebUI());

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), base_url.Resolve("path2/empty.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  EXPECT_TRUE(process_map->Contains(browser()
                                        ->tab_strip_model()
                                        ->GetWebContentsAt(2)
                                        ->GetPrimaryMainFrame()
                                        ->GetProcess()
                                        ->GetID()));
  EXPECT_FALSE(browser()->tab_strip_model()->GetWebContentsAt(2)->GetWebUI());

  ui_test_utils::TabAddedWaiter tab_add(browser());
  chrome::NewTab(browser());
  tab_add.Wait();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), base_url.Resolve("path3/empty.html")));
  EXPECT_FALSE(process_map->Contains(browser()
                                         ->tab_strip_model()
                                         ->GetWebContentsAt(3)
                                         ->GetPrimaryMainFrame()
                                         ->GetProcess()
                                         ->GetID()));
  EXPECT_FALSE(browser()->tab_strip_model()->GetWebContentsAt(3)->GetWebUI());

  // We should have opened 3 new extension tabs. Including the original blank
  // tab, we now have 4 tabs. Because the app_process app has the background
  // permission, all of its instances are in the same process.  Thus two tabs
  // should be part of the extension app and grouped in the same process.
  ASSERT_EQ(4, browser()->tab_strip_model()->count());
  WebContents* tab = browser()->tab_strip_model()->GetWebContentsAt(1);

  EXPECT_EQ(tab->GetPrimaryMainFrame()->GetProcess(),
            browser()
                ->tab_strip_model()
                ->GetWebContentsAt(2)
                ->GetPrimaryMainFrame()
                ->GetProcess());
  EXPECT_NE(tab->GetPrimaryMainFrame()->GetProcess(),
            browser()
                ->tab_strip_model()
                ->GetWebContentsAt(3)
                ->GetPrimaryMainFrame()
                ->GetProcess());

  // Now let's do the same using window.open. The same should happen.
  ASSERT_EQ(1u, extensions::browsertest_util::GetWindowControllerCountInProfile(
                    browser()->profile()));
  OpenWindow(tab, base_url.Resolve("path1/empty.html"), true, true, nullptr);
  OpenWindow(tab, base_url.Resolve("path2/empty.html"), true, true, nullptr);
  // TODO(creis): This should open in a new process (i.e., false for the last
  // argument), but we temporarily avoid swapping processes away from a hosted
  // app if it has an opener, because some OAuth providers make script calls
  // between non-app popups and non-app iframes in the app process.
  // See crbug.com/59285.
  OpenWindow(tab, base_url.Resolve("path3/empty.html"), true, true, nullptr);

  // Now let's have these pages navigate, into or out of the extension web
  // extent. They should switch processes.
  const GURL& app_url(base_url.Resolve("path1/empty.html"));
  const GURL& non_app_url(base_url.Resolve("path3/empty.html"));
  EXPECT_TRUE(NavigateInRenderer(
      browser()->tab_strip_model()->GetWebContentsAt(2), non_app_url));
  EXPECT_TRUE(NavigateInRenderer(
      browser()->tab_strip_model()->GetWebContentsAt(3), app_url));
  EXPECT_NE(tab->GetPrimaryMainFrame()->GetProcess(),
            browser()
                ->tab_strip_model()
                ->GetWebContentsAt(2)
                ->GetPrimaryMainFrame()
                ->GetProcess());
  EXPECT_EQ(tab->GetPrimaryMainFrame()->GetProcess(),
            browser()
                ->tab_strip_model()
                ->GetWebContentsAt(3)
                ->GetPrimaryMainFrame()
                ->GetProcess());

  // If one of the popup tabs navigates back to the app, window.opener should
  // be valid.
  EXPECT_TRUE(NavigateInRenderer(
      browser()->tab_strip_model()->GetWebContentsAt(6), app_url));
  EXPECT_EQ(tab->GetPrimaryMainFrame()->GetProcess(),
            browser()
                ->tab_strip_model()
                ->GetWebContentsAt(6)
                ->GetPrimaryMainFrame()
                ->GetProcess());
  ASSERT_EQ(true,
            content::EvalJs(browser()->tab_strip_model()->GetWebContentsAt(6),
                            "window.opener != null"));
}

// Test that hosted apps without the background permission use a process per app
// instance model, such that separate instances are in separate processes.
IN_PROC_BROWSER_TEST_F(AppApiTest, AppProcessInstances) {
  TestAppInstancesHelper("app_process_instances");
}

// Test that hosted apps with the background permission but that set
// allow_js_access to false also use a process per app instance model.
// Separate instances should be in separate processes.
IN_PROC_BROWSER_TEST_F(AppApiTest, AppProcessBackgroundInstances) {
  TestAppInstancesHelper("app_process_background_instances");
}

// Tests that app process switching works properly in the following scenario:
// 1. navigate to a page1 in the app
// 2. page1 redirects to a page2 outside the app extent (ie, "/server-redirect")
// 3. page2 redirects back to a page in the app
// The final navigation should end up in the app process.
// See http://crbug.com/61757
// Flaky.  http://crbug.com/341898
IN_PROC_BROWSER_TEST_F(AppApiTest, DISABLED_AppProcessRedirectBack) {
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("app_process")));

  // Open two tabs in the app.
  GURL base_url = GetTestBaseURL("app_process");

  chrome::NewTab(browser());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), base_url.Resolve("path1/empty.html")));
  chrome::NewTab(browser());
  // Wait until the second tab finishes its redirect train (2 hops).
  // 1. We navigate to redirect.html
  // 2. Renderer navigates and finishes, counting as a load stop.
  // 3. Renderer issues the meta refresh to navigate to server-redirect.
  // 4. Renderer is now in a "provisional load", waiting for navigation to
  //    complete.
  // 5. Browser sees a redirect response from server-redirect to empty.html, and
  //    transfers that to a new navigation, using RequestTransferURL.
  // 6. Renderer navigates to empty.html, and finishes loading, counting as the
  //    second load stop
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), base_url.Resolve("path1/redirect.html"), 2);

  // 3 tabs, including the initial about:blank. The last 2 should be the same
  // process.
  ASSERT_EQ(3, browser()->tab_strip_model()->count());
  EXPECT_EQ("/extensions/api_test/app_process/path1/empty.html",
            browser()->tab_strip_model()->GetWebContentsAt(2)->
                GetController().GetLastCommittedEntry()->GetURL().path());
  EXPECT_EQ(browser()
                ->tab_strip_model()
                ->GetWebContentsAt(1)
                ->GetPrimaryMainFrame()
                ->GetProcess(),
            browser()
                ->tab_strip_model()
                ->GetWebContentsAt(2)
                ->GetPrimaryMainFrame()
                ->GetProcess());
}

// Ensure that re-navigating to a URL after installing or uninstalling it as an
// app correctly swaps the tab to the app process.  (http://crbug.com/80621)
//
// Fails on Windows. http://crbug.com/238670
// Added logging to help diagnose the location of the problem.
IN_PROC_BROWSER_TEST_F(AppApiTest, NavigateIntoAppProcess) {
  extensions::ProcessMap* process_map =
      extensions::ProcessMap::Get(browser()->profile());

  // The app under test acts on URLs whose host is "localhost",
  // so the URLs we navigate to must have host "localhost".
  GURL base_url = GetTestBaseURL("app_process");

  // Load an app URL before loading the app.
  LOG(INFO) << "Loading path1/empty.html.";
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), base_url.Resolve("path1/empty.html")));
  LOG(INFO) << "Loading path1/empty.html - done.";
  WebContents* contents = browser()->tab_strip_model()->GetWebContentsAt(0);
  EXPECT_FALSE(process_map->Contains(
      contents->GetPrimaryMainFrame()->GetProcess()->GetID()));

  // Load app and re-navigate to the page.
  LOG(INFO) << "Loading extension.";
  const Extension* app =
      LoadExtension(test_data_dir_.AppendASCII("app_process"));
  LOG(INFO) << "Loading extension - done.";
  ASSERT_TRUE(app);
  LOG(INFO) << "Loading path1/empty.html.";
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), base_url.Resolve("path1/empty.html")));
  LOG(INFO) << "Loading path1/empty.html - done.";
  EXPECT_TRUE(process_map->Contains(
      contents->GetPrimaryMainFrame()->GetProcess()->GetID()));

  // Disable app and re-navigate to the page.
  LOG(INFO) << "Disabling extension.";
  DisableExtension(app->id());
  LOG(INFO) << "Disabling extension - done.";
  LOG(INFO) << "Loading path1/empty.html.";
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), base_url.Resolve("path1/empty.html")));
  LOG(INFO) << "Loading path1/empty.html - done.";
  EXPECT_FALSE(process_map->Contains(
      contents->GetPrimaryMainFrame()->GetProcess()->GetID()));
}

// Ensure that reloading a URL after installing or uninstalling it as an app
// correctly swaps the tab to the app process.  (http://crbug.com/80621)
//
// Added logging to help diagnose the location of the problem.
// http://crbug.com/238670
IN_PROC_BROWSER_TEST_F(AppApiTest, ReloadIntoAppProcess) {
  extensions::ProcessMap* process_map =
      extensions::ProcessMap::Get(browser()->profile());

  // The app under test acts on URLs whose host is "localhost",
  // so the URLs we navigate to must have host "localhost".
  GURL base_url = GetTestBaseURL("app_process");

  // Load app, disable it, and navigate to the page.
  LOG(INFO) << "Loading extension.";
  const Extension* app =
      LoadExtension(test_data_dir_.AppendASCII("app_process"));
  LOG(INFO) << "Loading extension - done.";
  ASSERT_TRUE(app);
  LOG(INFO) << "Disabling extension.";
  DisableExtension(app->id());
  LOG(INFO) << "Disabling extension - done.";
  LOG(INFO) << "Navigate to path1/empty.html.";
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), base_url.Resolve("path1/empty.html")));
  LOG(INFO) << "Navigate to path1/empty.html - done.";
  WebContents* contents = browser()->tab_strip_model()->GetWebContentsAt(0);
  content::NavigationController& controller = contents->GetController();
  EXPECT_FALSE(process_map->Contains(
      contents->GetPrimaryMainFrame()->GetProcess()->GetID()));
  // The test starts with about:blank, then navigates to path1/empty.html,
  // so there should be two entries.
  EXPECT_EQ(2, controller.GetEntryCount());

  // Enable app and reload the page.
  LOG(INFO) << "Enabling extension.";
  EnableExtension(app->id());
  LOG(INFO) << "Enabling extension - done.";
  content::LoadStopObserver reload_observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  LOG(INFO) << "Reloading.";
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  reload_observer.Wait();
  LOG(INFO) << "Reloading - done.";
  EXPECT_TRUE(process_map->Contains(
      contents->GetPrimaryMainFrame()->GetProcess()->GetID()));
  // Reloading, even with changing SiteInstance/process should not add any
  // more entries.
  EXPECT_EQ(2, controller.GetEntryCount());

  // Disable app and reload the page.
  LOG(INFO) << "Disabling extension.";
  DisableExtension(app->id());
  LOG(INFO) << "Disabling extension - done.";
  content::LoadStopObserver reload_observer2(
      browser()->tab_strip_model()->GetActiveWebContents());
  LOG(INFO) << "Reloading.";
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  reload_observer2.Wait();
  LOG(INFO) << "Reloading - done.";
  EXPECT_FALSE(process_map->Contains(
      contents->GetPrimaryMainFrame()->GetProcess()->GetID()));
  EXPECT_EQ(2, controller.GetEntryCount());
}

// Ensure that reloading a URL with JavaScript after installing or uninstalling
// it as an app correctly swaps the process.  (http://crbug.com/80621)
//
// Crashes on Windows and Mac. http://crbug.com/238670
// Added logging to help diagnose the location of the problem.
IN_PROC_BROWSER_TEST_F(AppApiTest, ReloadIntoAppProcessWithJavaScript) {
  extensions::ProcessMap* process_map =
      extensions::ProcessMap::Get(browser()->profile());

  // The app under test acts on URLs whose host is "localhost",
  // so the URLs we navigate to must have host "localhost".
  GURL base_url = GetTestBaseURL("app_process");

  // Load app, disable it, and navigate to the page.
  LOG(INFO) << "Loading extension.";
  const Extension* app =
      LoadExtension(test_data_dir_.AppendASCII("app_process"));
  LOG(INFO) << "Loading extension - done.";
  ASSERT_TRUE(app);
  LOG(INFO) << "Disabling extension.";
  DisableExtension(app->id());
  LOG(INFO) << "Disabling extension - done.";
  LOG(INFO) << "Navigate to path1/empty.html.";
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), base_url.Resolve("path1/empty.html")));
  LOG(INFO) << "Navigate to path1/empty.html - done.";
  WebContents* contents = browser()->tab_strip_model()->GetWebContentsAt(0);
  EXPECT_FALSE(process_map->Contains(
      contents->GetPrimaryMainFrame()->GetProcess()->GetID()));

  // Enable app and reload via JavaScript.
  LOG(INFO) << "Enabling extension.";
  EnableExtension(app->id());
  LOG(INFO) << "Enabling extension - done.";
  content::LoadStopObserver js_reload_observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  LOG(INFO) << "Executing location.reload().";
  ASSERT_TRUE(content::ExecJs(contents, "location.reload();"));
  js_reload_observer.Wait();
  LOG(INFO) << "Executing location.reload() - done.";
  EXPECT_TRUE(process_map->Contains(
      contents->GetPrimaryMainFrame()->GetProcess()->GetID()));

  // Disable app and reload via JavaScript.
  LOG(INFO) << "Disabling extension.";
  DisableExtension(app->id());
  LOG(INFO) << "Disabling extension - done.";
  content::LoadStopObserver js_reload_observer2(
      browser()->tab_strip_model()->GetActiveWebContents());
  LOG(INFO) << "Executing location = location.";
  ASSERT_TRUE(content::ExecJs(contents, "location = location;"));
  js_reload_observer2.Wait();
  LOG(INFO) << "Executing location = location - done.";
  EXPECT_FALSE(process_map->Contains(
      contents->GetPrimaryMainFrame()->GetProcess()->GetID()));
}

// Similar to the previous test, but ensure that popup blocking bypass
// isn't granted to the iframe.  See crbug.com/117446.
IN_PROC_BROWSER_TEST_F(BlockedAppApiTest, OpenAppFromIframe) {
  // Load app and start URL (not in the app).
  const Extension* app =
      LoadExtension(test_data_dir_.AppendASCII("app_process"));
  ASSERT_TRUE(app);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      GetTestBaseURL("app_process").Resolve("path3/container.html")));
  ui_test_utils::WaitForViewVisibility(browser(), VIEW_ID_CONTENT_SETTING_POPUP,
                                       true);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  blocked_content::PopupBlockerTabHelper* popup_blocker_tab_helper =
      blocked_content::PopupBlockerTabHelper::FromWebContents(tab);
  EXPECT_EQ(1u, popup_blocker_tab_helper->GetBlockedPopupsCount());
}

// Tests that if an extension launches an app via chrome.tabs.create with an URL
// that's not in the app's extent but that server redirects to it, we still end
// up with an app process. See http://crbug.com/99349 for more details.
IN_PROC_BROWSER_TEST_F(AppApiTest, ServerRedirectToAppFromExtension) {
  LoadExtension(test_data_dir_.AppendASCII("app_process"));
  const Extension* launcher =
      LoadExtension(test_data_dir_.AppendASCII("app_launcher"));

  // There should be two navigations by the time the app page is loaded.
  // 1. The extension launcher page.
  // 2. The app's URL (which includes a server redirect).
  // Note that the server redirect does not generate a navigation event.
  content::TestNavigationObserver test_navigation_observer(
      browser()->tab_strip_model()->GetActiveWebContents(),
      2);
  test_navigation_observer.StartWatchingNewWebContents();

  // Load the launcher extension, which should launch the app.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), launcher->GetResourceURL("server_redirect.html")));

  // Wait for app tab to be created and loaded.
  test_navigation_observer.Wait();

  // App has loaded, and chrome.app.isInstalled should be true.
  ASSERT_EQ(true, content::EvalJs(
                      browser()->tab_strip_model()->GetActiveWebContents(),
                      "chrome.app.isInstalled"));
}

// Tests that if an extension launches an app via chrome.tabs.create with an URL
// that's not in the app's extent but that client redirects to it, we still end
// up with an app process.
IN_PROC_BROWSER_TEST_F(AppApiTest, ClientRedirectToAppFromExtension) {
  LoadExtension(test_data_dir_.AppendASCII("app_process"));
  const Extension* launcher =
      LoadExtension(test_data_dir_.AppendASCII("app_launcher"));

  // There should be three navigations by the time the app page is loaded.
  // 1. The extension launcher page.
  // 2. The URL that the extension launches, which client redirects.
  // 3. The app's URL.
  content::TestNavigationObserver test_navigation_observer(
      browser()->tab_strip_model()->GetActiveWebContents(),
      3);
  test_navigation_observer.StartWatchingNewWebContents();

  // Load the launcher extension, which should launch the app.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), launcher->GetResourceURL("client_redirect.html")));

  // Wait for app tab to be created and loaded.
  test_navigation_observer.Wait();

  // App has loaded, and chrome.app.isInstalled should be true.
  ASSERT_EQ(true, content::EvalJs(
                      browser()->tab_strip_model()->GetActiveWebContents(),
                      "chrome.app.isInstalled"));
}

// Tests that if we have an app process (path1/container.html) with a non-app
// iframe (path3/iframe.html), then opening a link from that iframe to a new
// window to a same-origin non-app URL (path3/empty.html) should keep the window
// in the app process.
// This is in contrast to OpenAppFromIframe, since here the popup will not be
// missing special permissions and should be scriptable from the iframe.
// See http://crbug.com/92669 for more details.
IN_PROC_BROWSER_TEST_F(AppApiTest, OpenWebPopupFromWebIframe) {
  extensions::ProcessMap* process_map =
      extensions::ProcessMap::Get(browser()->profile());

  GURL base_url = GetTestBaseURL("app_process");

  // Load app and start URL (in the app).
  const Extension* app =
      LoadExtension(test_data_dir_.AppendASCII("app_process"));
  ASSERT_TRUE(app);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), base_url.Resolve("path1/container.html")));
  content::RenderProcessHost* process = browser()
                                            ->tab_strip_model()
                                            ->GetWebContentsAt(0)
                                            ->GetPrimaryMainFrame()
                                            ->GetProcess();
  EXPECT_TRUE(process_map->Contains(process->GetID()));

  // Popup window should be in the app's process.
  const WindowControllerList* active_window_list =
      WindowControllerList::GetInstance();
  EXPECT_EQ(2U, active_window_list->size());
  content::WebContents* popup_contents =
      active_window_list->get(1)->GetActiveTab();
  ASSERT_TRUE(popup_contents);
  EXPECT_TRUE(content::WaitForLoadStop(popup_contents));

  content::RenderProcessHost* popup_process =
      popup_contents->GetPrimaryMainFrame()->GetProcess();
  EXPECT_EQ(process, popup_process);
  EXPECT_TRUE(process_map->Contains(popup_process->GetID()));
}

IN_PROC_BROWSER_TEST_F(AppApiTest, ReloadAppAfterCrash) {
  extensions::ProcessMap* process_map =
      extensions::ProcessMap::Get(browser()->profile());

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("app_process")));

  GURL base_url = GetTestBaseURL("app_process");

  // Load the app, chrome.app.isInstalled should be true.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), base_url.Resolve("path1/empty.html")));
  WebContents* contents = browser()->tab_strip_model()->GetWebContentsAt(0);
  EXPECT_TRUE(process_map->Contains(
      contents->GetPrimaryMainFrame()->GetProcess()->GetID()));
  ASSERT_EQ(true, content::EvalJs(contents, "chrome.app.isInstalled"));

  // Crash the tab and reload it, chrome.app.isInstalled should still be true.
  content::CrashTab(browser()->tab_strip_model()->GetActiveWebContents());
  content::LoadStopObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  observer.Wait();
  ASSERT_EQ(true, content::EvalJs(contents, "chrome.app.isInstalled"));
}

// Test that a cross-site renderer-initiated navigation away from a hosted app
// stays in the same BrowsingInstance, so that postMessage calls to the app's
// other windows still work, and a cross-site browser-initiated navigation away
// from a hosted app switches BrowsingInstances.
IN_PROC_BROWSER_TEST_F(AppApiTest, NavigatePopupFromAppToOutsideApp) {
  extensions::ProcessMap* process_map =
      extensions::ProcessMap::Get(browser()->profile());

  GURL base_url = GetTestBaseURL("app_process");

  // Load app and start URL (in the app).
  const Extension* app =
      LoadExtension(test_data_dir_.AppendASCII("app_process"));
  ASSERT_TRUE(app);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), base_url.Resolve("path1/iframe.html")));
  content::SiteInstance* app_instance =
      browser()->tab_strip_model()->GetWebContentsAt(0)->GetSiteInstance();
  EXPECT_TRUE(process_map->Contains(app_instance->GetProcess()->GetID()));

  // Popup window should be in the app's process.
  const WindowControllerList* active_window_list =
      WindowControllerList::GetInstance();
  EXPECT_EQ(2U, active_window_list->size());
  content::WebContents* popup_contents =
      active_window_list->get(1)->GetActiveTab();
  ASSERT_TRUE(popup_contents);
  EXPECT_TRUE(content::WaitForLoadStop(popup_contents));

  SiteInstance* popup_instance = popup_contents->GetSiteInstance();
  EXPECT_EQ(app_instance, popup_instance);

  // Do a renderer-initiated navigation in the popup to a URL outside the app.
  GURL non_app_url(base_url.Resolve("path3/empty.html"));
  {
    content::TestNavigationObserver observer(popup_contents);
    EXPECT_TRUE(ExecJs(
        popup_contents,
        base::StringPrintf("location = '%s';", non_app_url.spec().c_str())));
    observer.Wait();
  }

  // The popup will stay in the same SiteInstance, even in
  // --site-per-process mode, because the popup is still same-site with its
  // opener.  Staying in same SiteInstance implies that postMessage will still
  // work.
  EXPECT_TRUE(
      app_instance->IsRelatedSiteInstance(popup_contents->GetSiteInstance()));
  EXPECT_EQ(app_instance, popup_contents->GetSiteInstance());

  // Go back in the popup.
  {
    content::TestNavigationObserver observer(popup_contents);
    popup_contents->GetController().GoBack();
    observer.Wait();
    EXPECT_EQ(app_instance, popup_contents->GetSiteInstance());
  }

  // Do a browser-initiated navigation in the popup to a same-site URL outside
  // the app.
  // TODO(alexmos): This could swap BrowsingInstances, since a
  // browser-initiated navigation breaks the scripting relationship between the
  // popup and the app, but it currently does not, since we keep the scripting
  // relationship regardless of whether the navigation is browser or
  // renderer-initiated (see https://crbug.com/828720).  Consider changing
  // this in the future as part of https://crbug.com/718516.
  {
    content::TestNavigationObserver observer(popup_contents);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        active_window_list->get(1)->GetBrowser(), non_app_url));
    observer.Wait();
    EXPECT_EQ(app_instance, popup_contents->GetSiteInstance());
    EXPECT_TRUE(
        app_instance->IsRelatedSiteInstance(popup_contents->GetSiteInstance()));
  }

  // Go back in the popup.
  {
    content::TestNavigationObserver observer(popup_contents);
    popup_contents->GetController().GoBack();
    observer.Wait();
    EXPECT_EQ(app_instance, popup_contents->GetSiteInstance());
  }

  // Do a browser-initiated navigation in the popup to a cross-site URL outside
  // the app.  This should swap BrowsingInstances.
  {
    content::TestNavigationObserver observer(popup_contents);
    GURL cross_site_url(
        embedded_test_server()->GetURL("foo.com", "/title1.html"));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        active_window_list->get(1)->GetBrowser(), cross_site_url));
    observer.Wait();
    EXPECT_NE(app_instance, popup_contents->GetSiteInstance());
    EXPECT_FALSE(
        app_instance->IsRelatedSiteInstance(popup_contents->GetSiteInstance()));
  }
}
