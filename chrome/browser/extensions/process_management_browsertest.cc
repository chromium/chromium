// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <tuple>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
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

class ProcessManagementTest : public ExtensionBrowserTest {
 public:
  ProcessManagementTest() {
    // TODO(crbug.com/40142347): Remove this once Extensions are
    // supported with BackForwardCache.
    disabled_feature_list_.InitWithFeatures(
        {}, {features::kBackForwardCache,
             features::kProcessPerSiteUpToMainFrameThreshold});
  }

 private:
  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  base::test::ScopedFeatureList disabled_feature_list_;
};

// Domain which the Webstore hosted app is associated with in production.
constexpr char kWebstoreURL[] = "chrome.google.com";
// Domain which the new Webstore is associated with in production.
constexpr char kNewWebstoreURL[] = "chromewebstore.google.com";
// Domain for testing an overridden Webstore URL.
constexpr char kWebstoreURLOverride[] = "chrome.webstore.test.com";

class ChromeWebStoreProcessTest
    : public ExtensionApiTest,
      public testing::WithParamInterface<const char*> {
 public:
  ChromeWebStoreProcessTest() {
    // The tests need the https server to resolve the webstore domain being
    // tested and 2 related subdomains with the same eTLD+1. Add certificates
    // for each.
    UseHttpsTestServer();
    net::EmbeddedTestServer::ServerCertificateConfig cert_config;
    cert_config.dns_names = {GetParam(), GetRelatedSubdomain(),
                             GetSecondRelatedSubdomain()};
    embedded_test_server()->SetSSLConfig(cert_config);

    embedded_test_server()->ServeFilesFromSourceDirectory(
        "chrome/test/data/extensions");

    EXPECT_TRUE(embedded_test_server()->Start());
    webstore_url_ = embedded_test_server()->GetURL(GetParam(), "/");
  }
  ~ChromeWebStoreProcessTest() override = default;

  // Overrides location of Chrome Webstore to a test controlled URL.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionBrowserTest::SetUpCommandLine(command_line);

    // Only use the override if this test case is testing the override URL.
    if (GetParam() == kWebstoreURLOverride) {
      command_line->AppendSwitchASCII(::switches::kAppsGalleryURL,
                                      webstore_url().spec());
    }
  }

  // Serve up a page Chrome will detect as being associated with the Webstore.
  // For the hosted app Webstore this needs to be served from a 'webstore'
  // directory, but otherwise it can just be from the root.
  GURL GetWebstorePage() {
    GURL::Replacements replace_path;
    if (GetParam() == kWebstoreURL) {
      replace_path.SetPathStr("webstore/mock_store.html");
    } else {
      replace_path.SetPathStr("title1.html");
    }
    return webstore_url().ReplaceComponents(replace_path);
  }

  // Returns a host that is an alternate subdomain that has the same eTLD+1 as
  // the Webstore URL under test.
  const char* GetRelatedSubdomain() {
    if (GetParam() == kWebstoreURLOverride)
      return "foo.webstore.test.com";
    return "foo.google.com";
  }

  // Returns a host that is another alternate subdomain that has the same eTLD+1
  // as the Webstore URL under test, but different from that returned by
  // GetRelatedSubdomain().
  const char* GetSecondRelatedSubdomain() {
    if (GetParam() == kWebstoreURLOverride)
      return "bar.webstore.test.com";
    return "bar.google.com";
  }

  const GURL& webstore_url() { return webstore_url_; }

 private:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  GURL webstore_url_;
};

class ChromeWebStoreInIsolatedOriginTest : public ChromeWebStoreProcessTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ChromeWebStoreProcessTest::SetUpCommandLine(command_line);

    // Mark the Chrome Web Store URL as an isolated origin.
    command_line->AppendSwitchASCII(::switches::kIsolateOrigins,
                                    webstore_url().spec());
  }
};

}  // namespace

// Ensure that hosted apps, extensions, normal web sites, and WebUI never share
// a process with each other, even if we hit the process limit.
// Note: All web and hosted app URLs in this test are same-site, so Site
// Isolation is not directly involved.
IN_PROC_BROWSER_TEST_F(ProcessManagementTest, ProcessOverflow) {
  // Set max renderers to 1 to force running out of processes.
  content::RenderProcessHost::SetMaxRendererProcessCount(1);

  ASSERT_TRUE(embedded_test_server()->Start());

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
  // Tab 0: NTP 1.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUINewTabURL)));
  // Tab 1: Hosted app 1.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), base_url.Resolve("hosted_app/main.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  // Tab 2: Web page 1.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), base_url.Resolve("test_file.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Tab 3: NTP 2.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  // Tab 4: Hosted app 2.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), base_url.Resolve("api_test/app_process/path1/empty.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  // Tab 5: Web page 2.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), base_url.Resolve("test_file_with_body.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  // Tab 6: Second instance of Hosted app 1.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), base_url.Resolve("hosted_app/main.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Load another extension (in background).
  const extensions::Extension* extension2 = LoadExtension(
      test_data_dir_.AppendASCII("api_test/browser_action/close_background"));
  ASSERT_TRUE(extension2);
  GURL extension2_url = extension2->url();

  // Get tab processes.
  ASSERT_EQ(7, browser()->tab_strip_model()->count());
  content::RenderProcessHost* ntp1_host = browser()
                                              ->tab_strip_model()
                                              ->GetWebContentsAt(0)
                                              ->GetPrimaryMainFrame()
                                              ->GetProcess();
  content::RenderProcessHost* hosted1_host = browser()
                                                 ->tab_strip_model()
                                                 ->GetWebContentsAt(1)
                                                 ->GetPrimaryMainFrame()
                                                 ->GetProcess();
  content::RenderProcessHost* web1_host = browser()
                                              ->tab_strip_model()
                                              ->GetWebContentsAt(2)
                                              ->GetPrimaryMainFrame()
                                              ->GetProcess();

  content::RenderProcessHost* ntp2_host = browser()
                                              ->tab_strip_model()
                                              ->GetWebContentsAt(3)
                                              ->GetPrimaryMainFrame()
                                              ->GetProcess();
  content::RenderProcessHost* hosted2_host = browser()
                                                 ->tab_strip_model()
                                                 ->GetWebContentsAt(4)
                                                 ->GetPrimaryMainFrame()
                                                 ->GetProcess();
  content::RenderProcessHost* web2_host = browser()
                                              ->tab_strip_model()
                                              ->GetWebContentsAt(5)
                                              ->GetPrimaryMainFrame()
                                              ->GetProcess();
  content::RenderProcessHost* hosted1_second_host = browser()
                                                        ->tab_strip_model()
                                                        ->GetWebContentsAt(6)
                                                        ->GetPrimaryMainFrame()
                                                        ->GetProcess();

  // Get extension processes.
  extensions::ProcessManager* process_manager =
      extensions::ProcessManager::Get(browser()->profile());
  content::RenderProcessHost* extension1_host =
      process_manager->GetSiteInstanceForURL(extension1_url)->GetProcess();
  content::RenderProcessHost* extension2_host =
      process_manager->GetSiteInstanceForURL(extension2_url)->GetProcess();

  // WebUI only shares with other same-site WebUI.
  EXPECT_EQ(ntp1_host, ntp2_host);
  EXPECT_NE(ntp1_host, hosted1_host);
  EXPECT_NE(ntp1_host, web1_host);
  EXPECT_NE(ntp1_host, extension1_host);

  // Hosted apps only share with instances of the same app.
  // Note that hosted2_host's app has the background permission and will use
  // process-per-site mode.
  EXPECT_EQ(hosted1_host, hosted1_second_host);
  EXPECT_NE(hosted1_host, hosted2_host);
  EXPECT_NE(hosted1_host, web1_host);
  EXPECT_NE(hosted1_host, extension1_host);

  // Same-site web pages only share with each other.
  EXPECT_EQ(web1_host, web2_host);
  EXPECT_NE(web1_host, extension1_host);

  // Extensions are not allowed to share, even with each other.
  EXPECT_NE(extension1_host, extension2_host);
}

// Test that pushing both extensions and web processes past the limit creates
// the expected number of processes.
//
// Sets the process limit to 3, with 1 expected extension process when sharing
// is allowed between extensions. The test then creates 3 separate extensions,
// 3 same-site web pages, and 1 cross-site web page.
//
// With extension process sharing, there should be 1 process for all extensions,
// 2 processes for the same-site pages, and an extra process for the cross-site
// page due to Site Isolation.
//
// Without extension process sharing, there should be 3 processes for the
// extensions. The web pages should act as if there were only 1 process used by
// the extensions, so there are 2 web processes for the same-site pages, and an
// extra process for the cross-site page due to Site Isolation.
IN_PROC_BROWSER_TEST_F(ProcessManagementTest, ExtensionAndWebProcessOverflow) {
  // Set max renderers to 3, to expect a single extension process when sharing
  // is allowed.
  content::RenderProcessHost::SetMaxRendererProcessCount(3);

  ASSERT_TRUE(embedded_test_server()->Start());

  // Load 3 extensions with background processes, similar to Chrome startup.
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("api_test/browser_action/none")));
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("api_test/browser_action/basics")));
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("api_test/browser_action/add_popup")));

  // Verify the number of extension processes.
  std::set<int> process_ids;
  Profile* profile = browser()->profile();
  ProcessManager* epm = ProcessManager::Get(profile);
  for (ExtensionHost* host : epm->background_hosts()) {
    SCOPED_TRACE(testing::Message()
                 << "When testing extension: " << host->extension_id());
    // The process should be locked.
    EXPECT_TRUE(host->render_process_host()->IsProcessLockedToSiteForTesting());
    process_ids.insert(host->render_process_host()->GetID());
  }
  // Each extension is in a locked process, unavailable for sharing.
  EXPECT_EQ(3u, process_ids.size());

  // Load 3 same-site tabs after the extensions.
  GURL web_url1(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  GURL web_url2(embedded_test_server()->GetURL("foo.com", "/title2.html"));
  GURL web_url3(embedded_test_server()->GetURL("foo.com", "/title3.html"));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), web_url1, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  WebContents* web_contents1 =
      browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), web_url2, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  WebContents* web_contents2 =
      browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), web_url3, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  WebContents* web_contents3 =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Verify the number of processes across extensions and tabs.
  process_ids.insert(
      web_contents1->GetPrimaryMainFrame()->GetProcess()->GetID());
  process_ids.insert(
      web_contents2->GetPrimaryMainFrame()->GetProcess()->GetID());
  process_ids.insert(
      web_contents3->GetPrimaryMainFrame()->GetProcess()->GetID());

  // The web processes still share 2 processes as if there were a single
  // extension process (making a total of 5 processes counting the existing 3
  // extension processes). This avoids starving the web pages with a single
  // process (if the extensions pushed us past the limit on their own), or
  // increasing the process count further (if all extension processes were
  // ignored).
  EXPECT_EQ(5u, process_ids.size());

  // Add a cross-site web process.
  // Ensure bar.com has its own process by explicitly isolating it.
  content::IsolateOriginsForTesting(
      embedded_test_server(),
      browser()->tab_strip_model()->GetActiveWebContents(), {"bar.com"});
  GURL cross_site_url(
      embedded_test_server()->GetURL("bar.com", "/title1.html"));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), cross_site_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  WebContents* web_contents4 =
      browser()->tab_strip_model()->GetActiveWebContents();
  process_ids.insert(
      web_contents4->GetPrimaryMainFrame()->GetProcess()->GetID());
  // The cross-site process adds 1 more process to the total, to avoid sharing
  // with the existing web renderer processes (due to Site Isolation).
  EXPECT_EQ(6u, process_ids.size());
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), extension_url));
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(extension_url, web_contents->GetLastCommittedURL());
  content::RenderProcessHost* old_process_host =
      web_contents->GetPrimaryMainFrame()->GetProcess();

  // Note that the |setTimeout| call below is needed to make sure EvalJs returns
  // *after* a scheduled navigation has already started.
  GURL web_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  std::string navigation_starting_script =
      "var form = document.getElementById('form');\n"
      "form.action = '" +
      web_url.spec() +
      "';\n"
      "form.submit();\n"
      "new Promise(resolve => {\n"
      "  setTimeout(\n"
      "      function() { resolve(true); },\n"
      "      0);\n"
      "});";

  // Try to trigger navigation to a webpage from within the tab.
  content::TestNavigationObserver nav_observer(web_contents, 1);
  EXPECT_TRUE(content::ExecJs(web_contents, navigation_starting_script));

  // Verify that the navigation succeeded.
  nav_observer.Wait();
  EXPECT_EQ(web_url, web_contents->GetLastCommittedURL());

  // Verify that the navigation transferred the contents to another renderer
  // process.
  content::RenderProcessHost* new_process_host =
      web_contents->GetPrimaryMainFrame()->GetProcess();
  EXPECT_NE(old_process_host, new_process_host);
}

// Test that the Webstore domain is isolated from a non-webstore subdomain that
// shares the same eTLD+1.
IN_PROC_BROWSER_TEST_P(ChromeWebStoreProcessTest,
                       StoreIsolatedFromRelatedSubdomain) {
  GURL non_cws_url_1 =
      embedded_test_server()->GetURL(GetRelatedSubdomain(), "/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), non_cws_url_1));
  WebContents* non_cws_contents_1 =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(non_cws_url_1, non_cws_contents_1->GetLastCommittedURL());

  // We use window.open here to keep this as a renderer-initiated navigation, as
  // a normal browser-initiated navigation would get a process swap by default
  // (if there are remaining renderer processes available).
  auto open_url = [](GURL url, WebContents* opener) {
    content::WebContentsAddedObserver popup_observer;
    EXPECT_TRUE(
        content::EvalJs(opener, content::JsReplace("!!window.open($1);", url))
            .ExtractBool());
    WebContents* web_contents = popup_observer.GetWebContents();
    EXPECT_TRUE(content::WaitForLoadStop(web_contents));
    EXPECT_EQ(url, web_contents->GetLastCommittedURL());
    return web_contents;
  };
  // Open two pages from the initial page: One that is another non-Webstore
  // subdomain and one that is the Webstore URL under test.
  GURL non_cws_url_2 = embedded_test_server()->GetURL(
      GetSecondRelatedSubdomain(), "/title1.html");
  WebContents* non_cws_contents_2 = open_url(non_cws_url_2, non_cws_contents_1);
  WebContents* cws_contents = open_url(GetWebstorePage(), non_cws_contents_1);

  // The second non-Webstore page should have been given a different
  // WebContents.
  EXPECT_NE(non_cws_contents_1, non_cws_contents_2);
  // The two non-Webstore urls are same-site, but cross-origin. If
  // kOriginKeyedProcessesByDefault is enabled they will be placed in different
  // processes, otherwise they'll share a process.
  if (content::SiteIsolationPolicy::AreOriginKeyedProcessesEnabledByDefault()) {
    EXPECT_NE(non_cws_contents_1->GetPrimaryMainFrame()->GetProcess(),
              non_cws_contents_2->GetPrimaryMainFrame()->GetProcess());
  } else {
    EXPECT_EQ(non_cws_contents_1->GetPrimaryMainFrame()->GetProcess(),
              non_cws_contents_2->GetPrimaryMainFrame()->GetProcess());
  }

  // The Webstore page should have been given a separate WebContents and process
  // than the page that opened it.
  EXPECT_NE(non_cws_contents_1, cws_contents);
  EXPECT_NE(non_cws_contents_1->GetPrimaryMainFrame()->GetProcess(),
            cws_contents->GetPrimaryMainFrame()->GetProcess());
  EXPECT_NE(non_cws_contents_2->GetPrimaryMainFrame()->GetProcess(),
            cws_contents->GetPrimaryMainFrame()->GetProcess());
}

IN_PROC_BROWSER_TEST_P(ChromeWebStoreProcessTest,
                       NavigateWebTabToChromeWebStoreViaPost) {
  content::RenderProcessHost::SetMaxRendererProcessCount(1);
  // Navigate a tab to a web page with a form. We specifically use a page that
  // is on another subdomain with the same host as the Webstore URL under test,
  // as normally these would be allowed to share processes, but for the Webstore
  // that should never be the case.
  GURL web_url =
      embedded_test_server()->GetURL(GetRelatedSubdomain(), "/form.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), web_url));
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(web_url, web_contents->GetLastCommittedURL());
  content::RenderProcessHost* old_process_host =
      web_contents->GetPrimaryMainFrame()->GetProcess();

  GURL cws_web_url = GetWebstorePage();

  // Note that the |setTimeout| call below is needed to make sure EvalJs returns
  // *after* a scheduled navigation has already started.
  std::string navigation_starting_script = R"(
      var form = document.getElementById('form');
      form.action = $1;
      form.submit();
      new Promise(resolve => {
        setTimeout(() => { resolve(true); }, 0);
      });)";

  // Trigger a renderer-initiated POST navigation (via the form) to a Chrome
  // Webstore URL.
  content::TestNavigationObserver nav_observer(web_contents, 1);

  EXPECT_TRUE(content::ExecJs(
      web_contents,
      content::JsReplace(navigation_starting_script, cws_web_url)));

  // The expectation is that the store will be properly put in its own process,
  // otherwise the renderer process is going to be terminated.
  // Verify that the navigation succeeded.
  nav_observer.Wait();
  EXPECT_EQ(cws_web_url, web_contents->GetLastCommittedURL());

  // If this test is for the old Webstore URL, verify that we have the Webstore
  // hosted app loaded into the Web Contents.
  // TODO(crbug.com/328494022): Remove this when we get rid of using the hosted
  // app for the old Webstore.
  content::RenderProcessHost* new_process_host =
      web_contents->GetPrimaryMainFrame()->GetProcess();
  if (GetParam() == kWebstoreURL) {
    EXPECT_TRUE(extensions::ProcessMap::Get(profile())->Contains(
        extensions::kWebStoreAppId, new_process_host->GetID()));
  }

  // Verify that Webstore is isolated in a separate renderer process.
  EXPECT_NE(old_process_host, new_process_host);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ChromeWebStoreProcessTest,
                         testing::Values(kWebstoreURL,
                                         kNewWebstoreURL,
                                         kWebstoreURLOverride));

// Check that navigations to the Chrome Web Store succeed when the Chrome Web
// Store URL's origin is set as an isolated origin via the
// --isolate-origins flag.  See https://crbug.com/788837.
IN_PROC_BROWSER_TEST_P(ChromeWebStoreInIsolatedOriginTest,
                       NavigationLoadsChromeWebStore) {
  // Sanity check that a SiteInstance for a Chrome Web Store URL requires a
  // dedicated process.
  content::BrowserContext* context = browser()->profile();
  scoped_refptr<content::SiteInstance> cws_site_instance =
      content::SiteInstance::CreateForURL(context, webstore_url());
  EXPECT_TRUE(cws_site_instance->RequiresDedicatedProcess());

  GURL cws_web_url = GetWebstorePage();

  // Navigate to Chrome Web Store and check that it's loaded successfully.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), cws_web_url));
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(cws_web_url, web_contents->GetLastCommittedURL());

  // Double-check that the page has access to the restricted APIs we expect to
  // be available to the Webstore.
  EXPECT_EQ(true, content::EvalJs(web_contents,
                                  "!!chrome && !!chrome.webstorePrivate"));

  // Verify that we have the Webstore hosted app loaded into the Web Contents if
  // this is for the old Webstore URL. Note: The new Webstore and the Webstore
  // URL override are granted their powers without use of the hosted app, so we
  // don't do this check for them.
  if (GetParam() == kWebstoreURL) {
    content::RenderProcessHost* render_process_host =
        web_contents->GetPrimaryMainFrame()->GetProcess();
    EXPECT_TRUE(extensions::ProcessMap::Get(profile())->Contains(
        extensions::kWebStoreAppId, render_process_host->GetID()));
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         ChromeWebStoreInIsolatedOriginTest,
                         testing::Values(kWebstoreURL,
                                         kNewWebstoreURL,
                                         kWebstoreURLOverride));

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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), extension->GetResourceURL("/test.html")));

  // Open a new tab to about:blank, which will result in a new SiteInstance
  // without an explicit site URL set.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  WebContents* new_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate the new tab to an extension URL that will be blocked by
  // webRequest. It must be a renderer-initiated navigation. It also uses a
  // redirect, otherwise the regular renderer process will send an OpenURL
  // IPC to the browser due to the chrome-extension:// URL.
  std::string script =
      base::StringPrintf("location.href = '%s';", redirect_url.spec().c_str());
  content::TestNavigationObserver observer(new_web_contents);
  EXPECT_TRUE(content::ExecJs(new_web_contents, script));
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

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("example.com", "/empty.html")));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto can_access_window = [this, web_contents](const GURL& url) {
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
            canAccess;
         }
       )";
    EXPECT_TRUE(content::ExecJs(
        web_contents, base::StringPrintf(kOpenNewWindow, url.spec().c_str())));

    // WaitForLoadStop() will return false on a 404, but that can happen if we
    // navigate to a blocked or nonexistent extension page.
    std::ignore = content::WaitForLoadStop(
        browser()->tab_strip_model()->GetActiveWebContents());

    return content::EvalJs(web_contents, kGetAccess).ExtractBool();
  };

  bool can_access_installed = can_access_window(installed_extension);
  bool can_access_nonexistent = can_access_window(nonexistent_extension);
  // Behavior for installed and nonexistent extensions should be equivalent.
  // We don't care much about what the result is (since if it can access it,
  // it's about:blank); only that the result is safe.
  EXPECT_EQ(can_access_installed, can_access_nonexistent);
}

}  // namespace extensions
