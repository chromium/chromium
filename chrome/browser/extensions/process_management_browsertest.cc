// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_map.h"
#include "extensions/common/manifest_handlers/web_accessible_resources_info.h"
#include "extensions/common/switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

using content::NavigationController;
using content::WebContents;

namespace extensions {

namespace {

bool IsExtensionProcessSharingAllowed() {
  // TODO(nick): Currently, process sharing is allowed even in
  // --site-per-process. Lock this down.  https://crbug.com/766267
  return true;
}

class ProcessManagementTest : public ExtensionBrowserTest {
 private:
  // This is needed for testing isolated apps, which are still experimental.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        extensions::switches::kEnableExperimentalExtensionApis);
  }

  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }
};

class ChromeWebStoreProcessTest : public ExtensionBrowserTest {
 public:
  const GURL& gallery_url() { return gallery_url_; }

  // Overrides location of Chrome Web Store gallery to a test controlled URL.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionBrowserTest::SetUpCommandLine(command_line);

    ASSERT_TRUE(embedded_test_server()->Start());
    gallery_url_ =
        embedded_test_server()->GetURL("chrome.webstore.test.com", "/");
    command_line->AppendSwitchASCII(::switches::kAppsGalleryURL,
                                    gallery_url_.spec());
  }

 private:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  GURL gallery_url_;
};

class ChromeWebStoreInIsolatedOriginTest : public ChromeWebStoreProcessTest {
 public:
  ChromeWebStoreInIsolatedOriginTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ChromeWebStoreProcessTest::SetUpCommandLine(command_line);

    // Mark the Chrome Web Store URL as an isolated origin.
    command_line->AppendSwitchASCII(::switches::kIsolateOrigins,
                                    gallery_url().spec());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeWebStoreInIsolatedOriginTest);
};

}  // namespace


// TODO(nasko): crbug.com/173137
#if defined(OS_WIN)
#define MAYBE_ProcessOverflow DISABLED_ProcessOverflow
#else
#define MAYBE_ProcessOverflow ProcessOverflow
#endif

// Ensure that an isolated app never shares a process with WebUIs, non-isolated
// extensions, and normal webpages.  None of these should ever comingle
// RenderProcessHosts even if we hit the process limit.
IN_PROC_BROWSER_TEST_F(ProcessManagementTest, MAYBE_ProcessOverflow) {
  // Set max renderers to 1 to force running out of processes.
  content::RenderProcessHost::SetMaxRendererProcessCount(1);

  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("isolated_apps/app1")));
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("isolated_apps/app2")));
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("hosted_app")));
  ASSERT_TRUE(
      LoadExtension(test_data_dir_.AppendASCII("api_test/app_process")));

  // The app under test acts on URLs whose host is "localhost",
  // so the URLs we navigate to must have host "localhost".
  GURL base_url = embedded_test_server()->GetURL(
      "/extensions/");
  GURL::Replacements replace_host;
  replace_host.SetHostStr("localhost");
  base_url = base_url.ReplaceComponents(replace_host);

  // Load an extension before adding tabs.
  const extensions::Extension* extension1 = LoadExtension(
      test_data_dir_.AppendASCII("api_test/browser_action/basics"));
  ASSERT_TRUE(extension1);
  GURL extension1_url = extension1->url();

  // Create multiple tabs for each type of renderer that might exist.
  ui_test_utils::NavigateToURL(
      browser(), base_url.Resolve("isolated_apps/app1/main.html"));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), base_url.Resolve("hosted_app/main.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), base_url.Resolve("test_file.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), base_url.Resolve("isolated_apps/app2/main.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), base_url.Resolve("api_test/app_process/path1/empty.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), base_url.Resolve("test_file_with_body.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  // Load another copy of isolated app 1.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), base_url.Resolve("isolated_apps/app1/main.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  // Load another extension.
  const extensions::Extension* extension2 = LoadExtension(
      test_data_dir_.AppendASCII("api_test/browser_action/close_background"));
  ASSERT_TRUE(extension2);
  GURL extension2_url = extension2->url();

  // Get tab processes.
  ASSERT_EQ(9, browser()->tab_strip_model()->count());
  content::RenderProcessHost* isolated1_host = browser()
                                                   ->tab_strip_model()
                                                   ->GetWebContentsAt(0)
                                                   ->GetMainFrame()
                                                   ->GetProcess();
  content::RenderProcessHost* ntp1_host = browser()
                                              ->tab_strip_model()
                                              ->GetWebContentsAt(1)
                                              ->GetMainFrame()
                                              ->GetProcess();
  content::RenderProcessHost* hosted1_host = browser()
                                                 ->tab_strip_model()
                                                 ->GetWebContentsAt(2)
                                                 ->GetMainFrame()
                                                 ->GetProcess();
  content::RenderProcessHost* web1_host = browser()
                                              ->tab_strip_model()
                                              ->GetWebContentsAt(3)
                                              ->GetMainFrame()
                                              ->GetProcess();

  content::RenderProcessHost* isolated2_host = browser()
                                                   ->tab_strip_model()
                                                   ->GetWebContentsAt(4)
                                                   ->GetMainFrame()
                                                   ->GetProcess();
  content::RenderProcessHost* ntp2_host = browser()
                                              ->tab_strip_model()
                                              ->GetWebContentsAt(5)
                                              ->GetMainFrame()
                                              ->GetProcess();
  content::RenderProcessHost* hosted2_host = browser()
                                                 ->tab_strip_model()
                                                 ->GetWebContentsAt(6)
                                                 ->GetMainFrame()
                                                 ->GetProcess();
  content::RenderProcessHost* web2_host = browser()
                                              ->tab_strip_model()
                                              ->GetWebContentsAt(7)
                                              ->GetMainFrame()
                                              ->GetProcess();

  content::RenderProcessHost* second_isolated1_host = browser()
                                                          ->tab_strip_model()
                                                          ->GetWebContentsAt(8)
                                                          ->GetMainFrame()
                                                          ->GetProcess();

  // Get extension processes.
  extensions::ProcessManager* process_manager =
      extensions::ProcessManager::Get(browser()->profile());
  content::RenderProcessHost* extension1_host =
      process_manager->GetSiteInstanceForURL(extension1_url)->GetProcess();
  content::RenderProcessHost* extension2_host =
      process_manager->GetSiteInstanceForURL(extension2_url)->GetProcess();

  // An isolated app only shares with other instances of itself, not other
  // isolated apps or anything else.
  EXPECT_EQ(isolated1_host, second_isolated1_host);
  EXPECT_NE(isolated1_host, isolated2_host);
  EXPECT_NE(isolated1_host, ntp1_host);
  EXPECT_NE(isolated1_host, hosted1_host);
  EXPECT_NE(isolated1_host, web1_host);
  EXPECT_NE(isolated1_host, extension1_host);
  EXPECT_NE(isolated2_host, ntp1_host);
  EXPECT_NE(isolated2_host, hosted1_host);
  EXPECT_NE(isolated2_host, web1_host);
  EXPECT_NE(isolated2_host, extension1_host);

  // Everything else is clannish.  WebUI only shares with other WebUI.
  EXPECT_EQ(ntp1_host, ntp2_host);
  EXPECT_NE(ntp1_host, hosted1_host);
  EXPECT_NE(ntp1_host, web1_host);
  EXPECT_NE(ntp1_host, extension1_host);

  // Hosted apps only share with each other.
  // Note that hosted2_host's app has the background permission and will use
  // process-per-site mode, but it should still share with hosted1_host's app.
  EXPECT_EQ(hosted1_host, hosted2_host);
  EXPECT_NE(hosted1_host, web1_host);
  EXPECT_NE(hosted1_host, extension1_host);

  // Web pages only share with each other.
  EXPECT_EQ(web1_host, web2_host);
  EXPECT_NE(web1_host, extension1_host);

  if (IsExtensionProcessSharingAllowed()) {
    // Extensions only share with each other ...
    EXPECT_EQ(extension1_host, extension2_host);
  } else {
    // Unless extensions are not allowed to share, even with each other.
    EXPECT_NE(extension1_host, extension2_host);
  }
}

// See
#if defined(OS_WIN)
#define MAYBE_ExtensionProcessBalancing DISABLED_ExtensionProcessBalancing
#else
#define MAYBE_ExtensionProcessBalancing ExtensionProcessBalancing
#endif
// Test to verify that the policy of maximum share of extension processes is
// properly enforced.
IN_PROC_BROWSER_TEST_F(ProcessManagementTest, MAYBE_ExtensionProcessBalancing) {
  // Set max renderers to 6 so we can expect 2 extension processes to be
  // allocated.
  content::RenderProcessHost::SetMaxRendererProcessCount(6);

  ASSERT_TRUE(embedded_test_server()->Start());

  // The app under test acts on URLs whose host is "localhost",
  // so the URLs we navigate to must have host "localhost".
  GURL base_url = embedded_test_server()->GetURL(
      "/extensions/");
  GURL::Replacements replace_host;
  replace_host.SetHostStr("localhost");
  base_url = base_url.ReplaceComponents(replace_host);

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("api_test/browser_action/none")));
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("api_test/browser_action/basics")));
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("api_test/browser_action/remove_popup")));
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("api_test/browser_action/add_popup")));
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("api_test/browser_action/no_icon")));
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("isolated_apps/app1")));
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("api_test/management/test")));

  // TODO(lukasza): It might be worth it to navigate to actual
  // chrome-extension:// URIs below (not to HTTP URIs) to make sure the 1/3rd
  // of process limit also applies to normal tabs (not just to background pages
  // and scripts).
  content::RenderProcessHost* first_renderer = ui_test_utils::NavigateToURL(
      browser(), base_url.Resolve("isolated_apps/app1/main.html"));
  content::RenderProcessHostWatcher first_renderer_watcher(
      first_renderer,
      content::RenderProcessHostWatcher::WATCH_FOR_HOST_DESTRUCTION);

  content::RenderProcessHost* second_renderer = ui_test_utils::NavigateToURL(
      browser(), base_url.Resolve("api_test/management/test/basics.html"));

  std::set<int> process_ids;
  Profile* profile = browser()->profile();
  extensions::ProcessManager* epm = extensions::ProcessManager::Get(profile);
  for (extensions::ExtensionHost* host : epm->background_hosts())
    process_ids.insert(host->render_process_host()->GetID());

  // We've loaded 5 extensions with background pages
  // (api_test/browser_action/*), 1 extension without background page
  // (api_test/management), and one isolated app. With extension process
  // sharing, we expect only 2 unique processes hosting the background
  // pages/scripts of these extensions (which extension gets assigned to which
  // process is randomized).  If extension process sharing is disabled, there is
  // no process limit, and each of the 5 background pages/scripts will be hosted
  // in a separate process.
  if (IsExtensionProcessSharingAllowed())
    EXPECT_EQ(2u, process_ids.size());
  else
    EXPECT_EQ(5u, process_ids.size());

  if (first_renderer != second_renderer) {
    // Wait for the first renderer to be torn down before verifying the number
    // of processes, else we race with the teardown here (specifically the
    // UnfreezableFrameMsg_SwapOut -> FrameHostMsg_SwapOut_ACK round trip).
    first_renderer_watcher.Wait();
  }

  // ProcessMap will always have exactly 5 entries - one for each of the
  // extensions with a background page (api_test/browser_action/*).  There won't
  // be any additional entries, since 1) the isolated app will be associated
  // with a separate content::BrowserContext and 2) the navigation to
  // api_test/management/test/basics.html navigates to a file: URI (not to a
  // chrome-extension: URI).
  extensions::ProcessMap* process_map = extensions::ProcessMap::Get(profile);
  EXPECT_EQ(5u, process_map->size());
}

IN_PROC_BROWSER_TEST_F(ProcessManagementTest,
                       NavigateExtensionTabToWebViaPost) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Load an extension.
  const extensions::Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("api_test/browser_action/popup_with_form"));
  ASSERT_TRUE(extension);

  // Navigate a tab to an extension page.
  GURL extension_url = extension->GetResourceURL("popup.html");
  ui_test_utils::NavigateToURL(browser(), extension_url);
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(extension_url, web_contents->GetLastCommittedURL());
  content::RenderProcessHost* old_process_host =
      web_contents->GetMainFrame()->GetProcess();

  // Note that the |setTimeout| call below is needed to make sure
  // ExecuteScriptAndExtractBool returns *after* a scheduled navigation has
  // already started.
  GURL web_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  std::string navigation_starting_script =
      "var form = document.getElementById('form');\n"
      "form.action = '" + web_url.spec() + "';\n"
      "form.submit();\n"
      "setTimeout(\n"
      "    function() { window.domAutomationController.send(true); },\n"
      "    0);\n";

  // Try to trigger navigation to a webpage from within the tab.
  bool ignored_script_result = false;
  content::TestNavigationObserver nav_observer(web_contents, 1);
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents, navigation_starting_script, &ignored_script_result));

  // Verify that the navigation succeeded.
  nav_observer.Wait();
  EXPECT_EQ(web_url, web_contents->GetLastCommittedURL());

  // Verify that the navigation transferred the contents to another renderer
  // process.
  content::RenderProcessHost* new_process_host =
      web_contents->GetMainFrame()->GetProcess();
  EXPECT_NE(old_process_host, new_process_host);
}

IN_PROC_BROWSER_TEST_F(ChromeWebStoreProcessTest,
                       NavigateWebTabToChromeWebStoreViaPost) {
  // Navigate a tab to a web page with a form.
  GURL web_url = embedded_test_server()->GetURL("foo.com", "/form.html");
  ui_test_utils::NavigateToURL(browser(), web_url);
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(web_url, web_contents->GetLastCommittedURL());
  content::RenderProcessHost* old_process_host =
      web_contents->GetMainFrame()->GetProcess();

  // Calculate an URL that is 1) relative to the fake (i.e. test-controlled)
  // Chrome Web Store gallery URL and 2) resolves to something that
  // embedded_test_server can actually serve (e.g. title1.html test file).
  GURL::Replacements replace_path;
  replace_path.SetPathStr("/title1.html");
  GURL cws_web_url = gallery_url().ReplaceComponents(replace_path);

  // Note that the |setTimeout| call below is needed to make sure
  // ExecuteScriptAndExtractBool returns *after* a scheduled navigation has
  // already started.
  std::string navigation_starting_script =
      "var form = document.getElementById('form');\n"
      "form.action = '" + cws_web_url.spec() + "';\n"
      "form.submit();\n"
      "setTimeout(\n"
      "    function() { window.domAutomationController.send(true); },\n"
      "    0);\n";

  // Trigger a renderer-initiated POST navigation (via the form) to a Chrome Web
  // Store gallery URL (which will commit into a chrome-extension://cws-app-id).
  bool ignored_script_result = false;
  content::TestNavigationObserver nav_observer(web_contents, 1);

  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents, navigation_starting_script, &ignored_script_result));

  // The expectation is that the store will be properly put in its own process,
  // otherwise the renderer process is going to be terminated.
  // Verify that the navigation succeeded.
  nav_observer.Wait();
  EXPECT_EQ(cws_web_url, web_contents->GetLastCommittedURL());

  // Verify that we really have the Chrome Web Store app loaded in the Web
  // Contents.
  content::RenderProcessHost* new_process_host =
      web_contents->GetMainFrame()->GetProcess();
  EXPECT_TRUE(extensions::ProcessMap::Get(profile())->Contains(
      extensions::kWebStoreAppId, new_process_host->GetID()));

  // Verify that Chrome Web Store is isolated in a separate renderer process.
  EXPECT_NE(old_process_host, new_process_host);
}

// Check that navigations to the Chrome Web Store succeed when the Chrome Web
// Store URL's origin is set as an isolated origin via the
// --isolate-origins flag.  See https://crbug.com/788837.
IN_PROC_BROWSER_TEST_F(ChromeWebStoreInIsolatedOriginTest,
                       NavigationLoadsChromeWebStore) {
  // Sanity check that a SiteInstance for a Chrome Web Store URL requires a
  // dedicated process.
  content::BrowserContext* context = browser()->profile();
  scoped_refptr<content::SiteInstance> cws_site_instance =
      content::SiteInstance::CreateForURL(context, gallery_url());
  EXPECT_TRUE(cws_site_instance->RequiresDedicatedProcess());

  // Navigate to Chrome Web Store and check that it's loaded successfully.
  ui_test_utils::NavigateToURL(browser(), gallery_url());
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(gallery_url(), web_contents->GetLastCommittedURL());

  // Verify that the Chrome Web Store hosted app is really loaded.
  content::RenderProcessHost* render_process_host =
      web_contents->GetMainFrame()->GetProcess();
  EXPECT_TRUE(extensions::ProcessMap::Get(profile())->Contains(
      extensions::kWebStoreAppId, render_process_host->GetID()));
}

// This test verifies that blocked navigations to extensions pages do not
// overwrite process-per-site map inside content/.
IN_PROC_BROWSER_TEST_F(ProcessManagementTest,
                       NavigateToBlockedExtensionPageInNewTab) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Load an extension, which will block a request for a specific page in it.
  const extensions::Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("web_request_site_process_registration"));
  ASSERT_TRUE(extension);

  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL blocked_url(extension->GetResourceURL("/blocked.html"));

  // Navigating to the blocked extension URL should be done through a redirect,
  // otherwise it will result in an OpenURL IPC from the renderer process, which
  // will initiate a navigation through the browser process.
  GURL redirect_url(
      embedded_test_server()->GetURL("/server-redirect?" + blocked_url.spec()));

  // Navigate the current tab to the test page in the extension, which will
  // create the extension process and register the webRequest blocking listener.
  ui_test_utils::NavigateToURL(browser(),
                               extension->GetResourceURL("/test.html"));

  // Open a new tab to about:blank, which will result in a new SiteInstance
  // without an explicit site URL set.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  WebContents* new_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate the new tab to an extension URL that will be blocked by
  // webRequest. It must be a renderer-initiated navigation. It also uses a
  // redirect, otherwise the regular renderer process will send an OpenURL
  // IPC to the browser due to the chrome-extension:// URL.
  std::string script =
      base::StringPrintf("location.href = '%s';", redirect_url.spec().c_str());
  content::TestNavigationObserver observer(new_web_contents);
  EXPECT_TRUE(content::ExecuteScript(new_web_contents, script));
  observer.Wait();

  EXPECT_EQ(observer.last_navigation_url(), blocked_url);
  EXPECT_FALSE(observer.last_navigation_succeeded());

  // Very subtle check for content/ internal functionality :(.
  // When a navigation is blocked, it still commits an error page. Since
  // extensions use the process-per-site model, each extension URL is registered
  // in a map from URL to a process. Creating a brand new SiteInstance for the
  // extension URL should always result in a SiteInstance that has a process and
  // the process is the same for all SiteInstances. This allows us to verify
  // that the site-to-process map for the extension hasn't been overwritten by
  // the process of the |blocked_url|.
  scoped_refptr<content::SiteInstance> new_site_instance =
      content::SiteInstance::CreateForURL(web_contents->GetBrowserContext(),
                                          extension->GetResourceURL(""));
  EXPECT_TRUE(new_site_instance->HasProcess());
  EXPECT_EQ(new_site_instance->GetProcess(),
            web_contents->GetSiteInstance()->GetProcess());

  // Ensure that reloading a blocked error page completes.
  content::TestNavigationObserver reload_observer(new_web_contents);
  new_web_contents->GetController().Reload(content::ReloadType::NORMAL, false);
  reload_observer.Wait();
  EXPECT_EQ(reload_observer.last_navigation_url(), blocked_url);
  EXPECT_FALSE(reload_observer.last_navigation_succeeded());
}

// Check that whether we can access the window object of a window.open()'d url
// to an extension is the same regardless of whether the extension is installed.
// https://crbug.com/598265.
IN_PROC_BROWSER_TEST_F(
    ProcessManagementTest,
    TestForkingBehaviorForUninstalledAndNonAccessibleExtensions) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("simple_with_icon"));
  ASSERT_TRUE(extension);
  ASSERT_FALSE(
      WebAccessibleResourcesInfo::HasWebAccessibleResources(extension));

  const GURL installed_extension = extension->url();
  const GURL nonexistent_extension("chrome-extension://" +
                                   std::string(32, 'a') + "/");
  EXPECT_NE(installed_extension, nonexistent_extension);

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("example.com", "/empty.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto can_access_window = [this, web_contents](const GURL& url) {
    bool can_access = false;
    const char kOpenNewWindow[] = "window.newWin = window.open('%s');";
    const char kGetAccess[] =
        R"(
          {
            let canAccess = false;
            try {
              window.newWin.document;
              canAccess = true;
            } catch (e) {
              canAccess = false;
            }
            window.newWin.close();
            window.domAutomationController.send(canAccess);
         }
       )";
    EXPECT_TRUE(content::ExecuteScript(
        web_contents, base::StringPrintf(kOpenNewWindow, url.spec().c_str())));

    // WaitForLoadStop() will return false on a 404, but that can happen if we
    // navigate to a blocked or nonexistent extension page.
    ignore_result(content::WaitForLoadStop(
        browser()->tab_strip_model()->GetActiveWebContents()));

    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(web_contents, kGetAccess,
                                                     &can_access));

    return can_access;
  };

  bool can_access_installed = can_access_window(installed_extension);
  bool can_access_nonexistent = can_access_window(nonexistent_extension);
  // Behavior for installed and nonexistent extensions should be equivalent.
  // We don't care much about what the result is (since if it can access it,
  // it's about:blank); only that the result is safe.
  EXPECT_EQ(can_access_installed, can_access_nonexistent);
}

}  // namespace extensions
