// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/permissions/permissions_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_management_test_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_with_management_policy_apitest.h"
#include "chrome/browser/extensions/identifiability_metrics_test_util.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/search/ntp_test_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/javascript_dialogs/tab_modal_dialog_manager.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/web_package/web_bundle_builder.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/content_script_tracker.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/identifiability_metrics.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace extensions {

namespace {

// A fake webstore domain.
const char kWebstoreDomain[] = "cws.com";

// Runs all pending tasks in the renderer associated with |web_contents|, and
// then all pending tasks in the browser process.
// Returns true on success.
bool RunAllPending(content::WebContents* web_contents) {
  // This is slight hack to achieve a RunPendingInRenderer() method. Since IPCs
  // are sent synchronously, anything started prior to this method will finish
  // before this method returns (as content::ExecuteScript() is synchronous).
  if (!content::ExecuteScript(web_contents, "1 == 1;"))
    return false;
  base::RunLoop().RunUntilIdle();
  return true;
}

// A simple extension manifest with content scripts on all pages.
const char kManifest[] =
    "{"
    "  \"name\": \"%s\","
    "  \"version\": \"1.0\","
    "  \"manifest_version\": 2,"
    "  \"content_scripts\": [{"
    "    \"matches\": [\"*://*/*\"],"
    "    \"js\": [\"script.js\"],"
    "    \"run_at\": \"%s\""
    "  }]"
    "}";

// A (blocking) content script that pops up an alert.
const char kBlockingScript[] = "alert('ALERT');";

// A (non-blocking) content script that sends a message.
const char kNonBlockingScript[] = "chrome.test.sendMessage('done');";

const char kNewTabOverrideManifest[] =
    "{"
    "  \"name\": \"New tab override\","
    "  \"version\": \"0.1\","
    "  \"manifest_version\": 2,"
    "  \"description\": \"Foo!\","
    "  \"chrome_url_overrides\": {\"newtab\": \"newtab.html\"}"
    "}";

const char kNewTabHtml[] = "<html>NewTabOverride!</html>";

}  // namespace

class ContentScriptApiTest : public ExtensionApiTest {
 public:
  ContentScriptApiTest() {}

  ContentScriptApiTest(const ContentScriptApiTest&) = delete;
  ContentScriptApiTest& operator=(const ContentScriptApiTest&) = delete;

  ~ContentScriptApiTest() override {}

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }
};

IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, ContentScriptAllFrames) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("content_scripts/all_frames")) << message_;
}

IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, ContentScriptAboutBlankIframes) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("content_scripts/about_blank_iframes"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, ContentScriptAboutBlankAndSrcdoc) {
  // The optional "*://*/*" permission is requested after verifying that
  // content script insertion solely depends on content_scripts[*].matches.
  // The permission is needed for chrome.tabs.executeScript tests.
  PermissionsRequestFunction::SetAutoConfirmForTests(true);
  PermissionsRequestFunction::SetIgnoreUserGestureForTests(true);

  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("content_scripts/about_blank_srcdoc"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, ContentScriptExtensionIframe) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("content_scripts/extension_iframe")) << message_;
}

IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, ContentScriptExtensionProcess) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("content_scripts/extension_process"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, ContentScriptFragmentNavigation) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  const char extension_name[] = "content_scripts/fragment";
  ASSERT_TRUE(RunExtensionTest(extension_name)) << message_;
}

IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, ContentScriptIsolatedWorlds) {
  // This extension runs various bits of script and tests that they all run in
  // the same isolated world.
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("content_scripts/isolated_world1")) << message_;

  // Now load a different extension, inject into same page, verify worlds aren't
  // shared.
  ASSERT_TRUE(RunExtensionTest("content_scripts/isolated_world2")) << message_;
}

IN_PROC_BROWSER_TEST_F(ContentScriptApiTest,
                       ContentScriptIgnoreHostPermissions) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("content_scripts/dont_match_host_permissions"))
      << message_;
}

// crbug.com/39249 -- content scripts js should not run on view source.
IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, ContentScriptViewSource) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("content_scripts/view_source")) << message_;
}

// crbug.com/126257 -- content scripts should not get injected into other
// extensions.
// TODO(crbug.com/1196340): Fix flakiness.
IN_PROC_BROWSER_TEST_F(ContentScriptApiTest,
                       DISABLED_ContentScriptOtherExtensions) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  // First, load extension that sets up content script.
  ASSERT_TRUE(RunExtensionTest("content_scripts/other_extensions/injector"))
      << message_;
  // Then load targeted extension to make sure its content isn't changed.
  ASSERT_TRUE(RunExtensionTest("content_scripts/other_extensions/victim"))
      << message_;
}

// https://crbug.com/825111 -- content scripts may fetch() a blob URL from their
// chrome-extension:// origin.
IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, ContentScriptBlobFetch) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("content_scripts/blob_fetch")) << message_;
}

// Test that content scripts set to run at different timings are loaded as
// expected for a few different types of pages.
IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, RunAtTimingsAllFire) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  ASSERT_TRUE(
      LoadExtension(test_data_dir_.AppendASCII("content_scripts/load_timing")));

  std::string test_paths[] = {"/extensions/test_file.html",
                              "/extensions/test_xml.xml",
                              "/extensions/test_xsl.xml"};

  for (const auto& path : test_paths) {
    ExtensionTestMessageListener listener_start("document-start-success");
    ExtensionTestMessageListener listener_end("document-end-success");
    listener_end.set_failure_message("document-end-failure");
    ExtensionTestMessageListener listener_idle("document-idle-success");
    listener_idle.set_failure_message("document-idle-failure");

    // Load the URL and make sure each script set for the different timings have
    // fired.
    const GURL url = embedded_test_server()->GetURL(path);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

    // Note: These checks don't ensure the correct ordering of injection, but
    // that is verified in the JS files themselves.
    EXPECT_TRUE(listener_start.WaitUntilSatisfied());
    EXPECT_TRUE(listener_end.WaitUntilSatisfied());
    EXPECT_TRUE(listener_idle.WaitUntilSatisfied());

    // Load the page a second time to check for any issues with cached XSL
    // resources. See: crbug.com/1041916. Note that test_xsl.xsl has
    // mock-http-headers to make sure it is cached.
    listener_start.Reset();
    listener_end.Reset();
    listener_idle.Reset();

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

    EXPECT_TRUE(listener_start.WaitUntilSatisfied());
    EXPECT_TRUE(listener_end.WaitUntilSatisfied());
    EXPECT_TRUE(listener_idle.WaitUntilSatisfied());
  }
}

IN_PROC_BROWSER_TEST_F(ContentScriptApiTest,
                       ContentScriptDuplicateScriptInjection) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  GURL url(
      base::StringPrintf("http://maps.google.com:%i/extensions/test_file.html",
                         embedded_test_server()->port()));

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "content_scripts/duplicate_script_injection")));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Test that a script that matches two separate, yet overlapping match
  // patterns is only injected once.
  bool scripts_injected_once = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send("
      "document.getElementsByClassName('injected-once')"
      ".length == 1)",
      &scripts_injected_once));
  ASSERT_TRUE(scripts_injected_once);

  // Test that a script injected at two different load process times, document
  // idle and document end, is injected exactly twice.
  bool scripts_injected_twice = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send("
      "document.getElementsByClassName('injected-twice')"
      ".length == 2)",
      &scripts_injected_twice));
  ASSERT_TRUE(scripts_injected_twice);
}

// Tests that content scripts detaching its Window during evaluation shouldn't
// crash. Regression test for https://crbug.com/1220761.
IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, DetachDuringEvaluation) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  GURL url(embedded_test_server()->GetURL(
      "document-end.example.com", "/extensions/detach_during_evaluation.html"));

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("content_scripts/detach_during_evaluation")));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // The iframe is removed by `detach.js`.
  bool iframe_removed = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send("
      "document.getElementById('injected') === null)",
      &iframe_removed));
  EXPECT_TRUE(iframe_removed);

  // `detach.js` is evaluated, and detaches the iframe.
  bool detach_evaluated = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send("
      "document.getElementById('detach-evaluated') !== null)",
      &detach_evaluated));
  EXPECT_TRUE(detach_evaluated);

  // `detach2.js` isn't evaluated because the iframe is detached.
  bool detach2_evaluated = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send("
      "document.getElementById('detach2-evaluated') !== null)",
      &detach2_evaluated));
  EXPECT_FALSE(detach2_evaluated);
}

// Tests that fetches made by content scripts are exempt from the page's CSP.
// Regression test for crbug.com/934819.
IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, FetchExemptFromCSP) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Create and load an extension that will inject a content script which does a
  // fetch based on the host document's "fetchUrl" url search parameter.
  constexpr char kFetchManifest[] =
      R"(
      {
        "name":"Fetch redirect test",
        "version":"0.0.1",
        "manifest_version": 2,
        "content_scripts": [
          {
            "matches": ["*://bar.com/*"],
            "js": ["content_script.js"],
            "run_at": "document_start"
          }
        ]
      })";
  constexpr char kContentScript[] = R"(
    let params = (new URL(document.location)).searchParams;
    let fetchUrl = params.get('fetchUrl');
    fetch(fetchUrl)
      .then(response => response.text())
      .then(text => chrome.test.sendMessage(text))
      .catch(error => chrome.test.sendMessage(error.message));
  )";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kFetchManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), kContentScript);
  ASSERT_TRUE(LoadExtension(test_dir.UnpackedPath()));

  ExtensionTestMessageListener listener;

  // The fetch will undergo a redirect. Note that the fetched file sets the
  // "Access-Control-Allow-Origin: *" header to allow for cross origin access.
  GURL fetch_url =
      embedded_test_server()->GetURL("foo.com", "/extensions/xhr.txt");
  GURL redirect_url = embedded_test_server()->GetURL(
      "bar.com", "/server-redirect?" + fetch_url.spec());

  // Navigate to a page with a CSP set that prevents resources from other
  // origins to be loaded and wait for a response from the content script.
  GURL csp_page_url = embedded_test_server()->GetURL(
      "bar.com",
      "/extensions/page_with_csp.html?fetchUrl=" + redirect_url.spec());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), csp_page_url));

  // Ensure the fetch is exempt from the page CSP and succeeds.
  ASSERT_TRUE(listener.WaitUntilSatisfied());
  EXPECT_EQ("File to request via XHR.\n", listener.message());

  // Sanity check that fetching a url which doesn't allow cross origin access
  // fails.
  listener.Reset();
  fetch_url =
      embedded_test_server()->GetURL("foo.com", "/extensions/test_file.txt");
  redirect_url = embedded_test_server()->GetURL(
      "bar.com", "/server-redirect?" + fetch_url.spec());
  csp_page_url = embedded_test_server()->GetURL(
      "bar.com",
      "/extensions/page_with_csp.html?fetchUrl=" + redirect_url.spec());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), csp_page_url));

  ASSERT_TRUE(listener.WaitUntilSatisfied());
  EXPECT_EQ("Failed to fetch", listener.message());
}

class ContentScriptCssInjectionTest : public ExtensionApiTest {
 protected:
  // TODO(rdevlin.cronin): Make a testing switch that looks like FeatureSwitch,
  // but takes in an optional value so that we don't have to do this.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    // We change the Webstore URL to be http://cws.com. We need to do this so
    // we can check that css injection is not allowed on the webstore (which
    // could lead to spoofing). Unfortunately, host_resolver seems to have
    // problems with redirecting "chrome.google.com" to the test server, so we
    // can't use the real Webstore's URL. If this changes, we could clean this
    // up.
    command_line->AppendSwitchASCII(
        ::switches::kAppsGalleryURL,
        base::StringPrintf("http://%s", kWebstoreDomain));
  }

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }
};

IN_PROC_BROWSER_TEST_F(ContentScriptCssInjectionTest,
                       ContentScriptInjectsStyles) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("content_scripts")
                                .AppendASCII("css_injection")));

  // Helper to get the active tab from the browser.
  auto get_active_tab = [browser = browser()]() {
    return browser->tab_strip_model()->GetActiveWebContents();
  };
  // Returns the background color for the element retrieved from the given
  // `query_selector`.
  auto get_element_color =
      [&get_active_tab](const char* query_selector) -> std::string {
    content::WebContents* web_contents = get_active_tab();
    SCOPED_TRACE(base::StringPrintf(
        "URL: %s; Selector: %s",
        web_contents->GetLastCommittedURL().spec().c_str(), query_selector));
    std::string color;
    constexpr char kGetColor[] =
        R"((function() {
             let element = document.querySelector('%s');
             style = window.getComputedStyle(element);
             domAutomationController.send(style.backgroundColor);
            })();)";
    if (!content::ExecuteScriptAndExtractString(
            get_active_tab(), base::StringPrintf(kGetColor, query_selector),
            &color)) {
      return "<Failed to execute>";
    }

    return color;
  };
  // Returns the number of stylesheets attached to the document.
  auto get_style_sheet_count = [&get_active_tab]() {
    int count = -1;
    constexpr char kGetStyleSheetCount[] =
        "domAutomationController.send(document.styleSheets.length);";
    if (!content::ExecuteScriptAndExtractInt(get_active_tab(),
                                             kGetStyleSheetCount, &count)) {
      return -1;
    }
    return count;
  };

  // CSS injection should be allowed on an unprivileged web page that matches
  // the patterns specified for the content script.
  GURL url =
      embedded_test_server()->GetURL("/extensions/test_file_with_body.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  constexpr char kInjectedBodyColor[] = "rgb(0, 0, 255)";  // Blue
  EXPECT_EQ(kInjectedBodyColor, get_element_color("body"));
  EXPECT_EQ(0, get_style_sheet_count())
      << "Extension-injected content scripts should not be included in "
      << "document.styleSheets.";

  // The loaded extension has an exclude match for "extensions/test_file.html",
  // so no CSS should be injected.
  url = embedded_test_server()->GetURL("/extensions/test_file.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_NE(kInjectedBodyColor, get_element_color("body"));
  EXPECT_EQ(0, get_style_sheet_count());

  // We disallow all injection on the webstore.
  GURL::Replacements replacements;
  replacements.SetHostStr(kWebstoreDomain);
  url = embedded_test_server()
            ->GetURL("/extensions/test_file_with_body.html")
            .ReplaceComponents(replacements);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_NE(kInjectedBodyColor, get_element_color("body"));
  EXPECT_EQ(0, get_style_sheet_count());

  // Check extensions override page styles if they have more specific rules.
  // Regression test for https://crbug.com/1175506.
  // This page has four divs (with ids div1, div2, div3, and div4). The page
  // specifies styles for them, but the extension has more specific styles for
  // divs 1, 2, and 3.
  // The extension styles should win by specificity, since they are in the same
  // style origin ("author").
  url = embedded_test_server()->GetURL("/extensions/test_file_with_style.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  constexpr char kInjectedDivColor[] = "rgb(0, 0, 255)";  // Blue
  constexpr char kOriginalDivColor[] = "rgb(255, 0, 0)";  // Red
  EXPECT_EQ(kInjectedDivColor, get_element_color("#div1"));
  EXPECT_EQ(kInjectedDivColor, get_element_color("#div2"));
  EXPECT_EQ(kInjectedDivColor, get_element_color("#div3"));
  EXPECT_EQ(kOriginalDivColor, get_element_color("#div4"));
  // There should be two style sheets on this website; one inline <style> tag
  // and a second included as a <link>.
  EXPECT_EQ(2, get_style_sheet_count());

  // Load an additional stylesheet dynamically (ensuring it was added to the DOM
  // later). div3 should still be styled by the extension (since that rule is
  // more specific). This ensures that stylesheets that just happen to be added
  // later don't override extension sheets of higher specificity.
  constexpr char kLoadExtraStylesheet[] =
      R"((function() {
           let sheet = document.createElement('link');
           sheet.type = 'text/css';
           sheet.rel = 'stylesheet';
           sheet.href = 'test_file_with_style2.css';
           sheet.onload = () => { domAutomationController.send('success'); };
           sheet.onerror = () => { domAutomationController.send('error'); };
           document.head.appendChild(sheet);
         })();)";
  std::string result;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      get_active_tab(), kLoadExtraStylesheet, &result));
  EXPECT_EQ("success", result);
  EXPECT_EQ(kInjectedDivColor, get_element_color("#div3"));
}

IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, ContentScriptCSSLocalization) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("content_scripts/css_l10n")) << message_;
}

IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, ContentScriptExtensionAPIs) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  const extensions::Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("content_scripts/extension_api"));

  ResultCatcher catcher;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/extensions/api_test/content_scripts/"
                                     "extension_api/functions.html")));
  EXPECT_TRUE(catcher.GetNextResult());

  // Navigate to a page that will cause a content script to run that starts
  // listening for an extension event.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/extensions/api_test/content_scripts/extension_api/events.html")));

  // Navigate to an extension page that will fire the event events.js is
  // listening for.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), extension->GetResourceURL("fire_event.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_NONE);
  EXPECT_TRUE(catcher.GetNextResult());
}

IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, ContentScriptPermissionsApi) {
  extensions::PermissionsRequestFunction::SetIgnoreUserGestureForTests(true);
  extensions::PermissionsRequestFunction::SetAutoConfirmForTests(true);
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("content_scripts/permissions")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTestWithManagementPolicy,
                       ContentScriptPolicy) {
  // Set enterprise policy to block injection to policy specified host.
  {
    ExtensionManagementPolicyUpdater pref(&policy_provider_);
    pref.AddPolicyBlockedHost("*", "*://example.com");
  }
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("content_scripts/policy")) << message_;
}

class ContentScriptPolicyStartupTest : public ExtensionApiTest {
 public:
  // We need to do this work here because the runtime host policy values are
  // checked pretty early on in the startup of the ExtensionService, which
  // happens between SetUpInProcessBrowserTestFixture and SetUpOnMainThread.
  void SetUpInProcessBrowserTestFixture() override {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();

    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);

    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
    // ExtensionManagementPolicyUpdater requires a single-threaded context to
    // call RunLoop::RunUntilIdle internally, and it isn't ready at this setup
    // moment.
    base::test::TaskEnvironment env;
    ExtensionManagementPolicyUpdater management_policy(&policy_provider_);
    management_policy.AddPolicyBlockedHost("*", "*://example.com");
  }

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

 private:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

// Regression test for: https://crbug.com/954215.
IN_PROC_BROWSER_TEST_F(ContentScriptPolicyStartupTest, RuntimeBlockedHosts) {
  // Tests that default scoped runtime blocked host policy values for the
  // ExtensionSettings policy are applied at startup.
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("content_scripts/policy")) << message_;
}

// Verifies wildcard can NOT be used for effective TLD.
IN_PROC_BROWSER_TEST_F(ExtensionApiTestWithManagementPolicy,
                       ContentScriptPolicyWildcard) {
  // Set enterprise policy to block injection to policy specified hosts.
  {
    ExtensionManagementPolicyUpdater pref(&policy_provider_);
    pref.AddPolicyBlockedHost("*", "*://example.*");
  }
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_FALSE(RunExtensionTest("content_scripts/policy")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTestWithManagementPolicy,
                       ContentScriptPolicyByExtensionId) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  base::FilePath extension_path =
      test_data_dir_.AppendASCII("content_scripts/policy");
  // Pack extension because by-extension policies aren't applied to unpacked
  // "transient" extensions.
  base::FilePath crx_path = PackExtension(extension_path);
  EXPECT_FALSE(crx_path.empty());

  // Load first time to get extension id.
  const Extension* extension = LoadExtension(crx_path);
  ASSERT_TRUE(extension);
  auto extension_id = extension->id();
  UnloadExtension(extension_id);

  // Set enterprise policy to block injection of specified extension to policy
  // specified host.
  {
    ExtensionManagementPolicyUpdater pref(&policy_provider_);
    pref.AddPolicyBlockedHost(extension_id, "*://example.com");
  }
  // Some policy updating operations are performed asynchronuosly. Wait for them
  // to complete before installing extension.
  base::RunLoop().RunUntilIdle();

  extensions::ResultCatcher catcher;
  EXPECT_TRUE(LoadExtension(crx_path));
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, ContentScriptBypassPageCSP) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  extensions::ResultCatcher catcher;
  ASSERT_TRUE(RunExtensionTest("content_scripts/bypass_page_csp"))
      << catcher.message();
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(ContentScriptApiTest,
                       ContentScriptBypassPageTrustedTypes) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  extensions::ResultCatcher catcher;
  ASSERT_TRUE(RunExtensionTest("content_scripts/bypass_page_trusted_types"))
      << catcher.message();
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Test that when injecting a blocking content script, other scripts don't run
// until the blocking script finishes.
IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, ContentScriptBlockingScript) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Load up two extensions.
  TestExtensionDir ext_dir1;
  ext_dir1.WriteManifest(
      base::StringPrintf(kManifest, "ext1", "document_start"));
  ext_dir1.WriteFile(FILE_PATH_LITERAL("script.js"), kBlockingScript);
  const Extension* ext1 = LoadExtension(ext_dir1.UnpackedPath());
  ASSERT_TRUE(ext1);

  TestExtensionDir ext_dir2;
  ext_dir2.WriteManifest(base::StringPrintf(kManifest, "ext2", "document_end"));
  ext_dir2.WriteFile(FILE_PATH_LITERAL("script.js"), kNonBlockingScript);
  const Extension* ext2 = LoadExtension(ext_dir2.UnpackedPath());
  ASSERT_TRUE(ext2);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto* js_dialog_manager =
      javascript_dialogs::TabModalDialogManager::FromWebContents(web_contents);
  base::RunLoop dialog_wait;
  js_dialog_manager->SetDialogShownCallbackForTesting(
      dialog_wait.QuitClosure());

  ExtensionTestMessageListener listener("done");
  listener.set_extension_id(ext2->id());

  // Navigate! Both extensions will try to inject.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("/empty.html"),
      WindowOpenDisposition::CURRENT_TAB, ui_test_utils::BROWSER_TEST_NONE);

  dialog_wait.Run();
  // Right now, the alert dialog is showing and blocking injection of anything
  // after it, so the listener shouldn't be satisfied.
  EXPECT_FALSE(listener.was_satisfied());
  js_dialog_manager->HandleJavaScriptDialog(web_contents, true, nullptr);

  // After closing the dialog, the rest of the scripts should be able to
  // inject.
  EXPECT_TRUE(listener.WaitUntilSatisfied());
}

// Test that closing a tab with a blocking script results in no further scripts
// running (and we don't crash).
IN_PROC_BROWSER_TEST_F(ContentScriptApiTest,
                       ContentScriptBlockingScriptTabClosed) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // We're going to close a tab in this test, so make a new one (to ensure
  // we don't close the browser).
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("/empty.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Set up the same as the previous test case.
  TestExtensionDir ext_dir1;
  ext_dir1.WriteManifest(
      base::StringPrintf(kManifest, "ext1", "document_start"));
  ext_dir1.WriteFile(FILE_PATH_LITERAL("script.js"), kBlockingScript);
  const Extension* ext1 = LoadExtension(ext_dir1.UnpackedPath());
  ASSERT_TRUE(ext1);

  TestExtensionDir ext_dir2;
  ext_dir2.WriteManifest(base::StringPrintf(kManifest, "ext2", "document_end"));
  ext_dir2.WriteFile(FILE_PATH_LITERAL("script.js"), kNonBlockingScript);
  const Extension* ext2 = LoadExtension(ext_dir2.UnpackedPath());
  ASSERT_TRUE(ext2);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto* js_dialog_manager =
      javascript_dialogs::TabModalDialogManager::FromWebContents(web_contents);
  base::RunLoop dialog_wait;
  js_dialog_manager->SetDialogShownCallbackForTesting(
      dialog_wait.QuitClosure());

  ExtensionTestMessageListener listener("done");
  listener.set_extension_id(ext2->id());

  // Navigate!
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("/empty.html"),
      WindowOpenDisposition::CURRENT_TAB, ui_test_utils::BROWSER_TEST_NONE);

  // Now, instead of closing the dialog, just close the tab. Later scripts
  // should never get a chance to run (and we shouldn't crash).
  dialog_wait.Run();
  EXPECT_FALSE(listener.was_satisfied());
  EXPECT_TRUE(browser()->tab_strip_model()->CloseWebContentsAt(
      browser()->tab_strip_model()->active_index(), 0));
  EXPECT_FALSE(listener.was_satisfied());
}

// There was a bug by which content scripts that blocked and ran on
// document_idle could be injected twice (crbug.com/431263). Test for
// regression.
IN_PROC_BROWSER_TEST_F(ContentScriptApiTest,
                       ContentScriptBlockingScriptsDontRunTwice) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Load up an extension.
  TestExtensionDir ext_dir1;
  ext_dir1.WriteManifest(
      base::StringPrintf(kManifest, "ext1", "document_idle"));
  ext_dir1.WriteFile(FILE_PATH_LITERAL("script.js"), kBlockingScript);
  const Extension* ext1 = LoadExtension(ext_dir1.UnpackedPath());
  ASSERT_TRUE(ext1);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto* js_dialog_manager =
      javascript_dialogs::TabModalDialogManager::FromWebContents(web_contents);
  base::RunLoop dialog_wait;
  js_dialog_manager->SetDialogShownCallbackForTesting(
      dialog_wait.QuitClosure());

  // Navigate!
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("/empty.html"),
      WindowOpenDisposition::CURRENT_TAB, ui_test_utils::BROWSER_TEST_NONE);

  dialog_wait.Run();

  // The extension will have injected at idle, but it should only inject once.
  js_dialog_manager->HandleJavaScriptDialog(web_contents, true, nullptr);
  EXPECT_TRUE(RunAllPending(web_contents));
  EXPECT_FALSE(js_dialog_manager->IsShowingDialogForTesting());
}

// Bug fix for crbug.com/507461.
IN_PROC_BROWSER_TEST_F(ContentScriptApiTest,
                       DocumentStartInjectionFromExtensionTabNavigation) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  TestExtensionDir new_tab_override_dir;
  new_tab_override_dir.WriteManifest(kNewTabOverrideManifest);
  new_tab_override_dir.WriteFile(FILE_PATH_LITERAL("newtab.html"), kNewTabHtml);
  const Extension* new_tab_override =
      LoadExtension(new_tab_override_dir.UnpackedPath());
  ASSERT_TRUE(new_tab_override);

  TestExtensionDir injector_dir;
  injector_dir.WriteManifest(
      base::StringPrintf(kManifest, "injector", "document_start"));
  injector_dir.WriteFile(FILE_PATH_LITERAL("script.js"), kNonBlockingScript);
  const Extension* injector = LoadExtension(injector_dir.UnpackedPath());
  ASSERT_TRUE(injector);

  ExtensionTestMessageListener listener("done");
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL("chrome://newtab"), ui::PAGE_TRANSITION_LINK));
  browser()->tab_strip_model()->ActivateTabAt(0);
  content::WebContents* tab_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_EQ(new_tab_override->GetResourceURL("newtab.html"),
            tab_contents->GetPrimaryMainFrame()->GetLastCommittedURL());
  EXPECT_FALSE(listener.was_satisfied());
  listener.Reset();

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("/empty.html"),
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(listener.was_satisfied());
}

IN_PROC_BROWSER_TEST_F(ContentScriptApiTest,
                       DontInjectContentScriptsInBackgroundPages) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  // Load two extensions, one with an iframe to a.com in its background page,
  // the other, a content script for a.com. The latter should never be able to
  // inject the script, because scripts aren't allowed to run on foreign
  // extensions' pages.
  base::FilePath data_dir = test_data_dir_.AppendASCII("content_scripts");
  ExtensionTestMessageListener iframe_loaded_listener("iframe loaded");
  ExtensionTestMessageListener content_script_listener("script injected");
  LoadExtension(data_dir.AppendASCII("script_a_com"));
  LoadExtension(data_dir.AppendASCII("background_page_iframe"));
  EXPECT_TRUE(iframe_loaded_listener.WaitUntilSatisfied());
  EXPECT_FALSE(content_script_listener.was_satisfied());
}

IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, CannotScriptTheNewTabPage) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  ExtensionTestMessageListener test_listener("ready",
                                             ReplyBehavior::kWillReply);
  LoadExtension(test_data_dir_.AppendASCII("content_scripts/ntp"));
  ASSERT_TRUE(test_listener.WaitUntilSatisfied());

  auto did_script_inject = [](content::WebContents* web_contents) {
    bool did_inject = false;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        web_contents,
        "domAutomationController.send(document.title === 'injected');",
        &did_inject));
    return did_inject;
  };

  // First, test the executeScript() method.
  ResultCatcher catcher;
  test_listener.Reply(std::string());
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
  EXPECT_EQ(ntp_test_utils::GetFinalNtpUrl(browser()->profile()),
            browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetLastCommittedURL());
  EXPECT_FALSE(
      did_script_inject(browser()->tab_strip_model()->GetActiveWebContents()));

  // Next, check content script injection.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), search::GetNewTabPageURL(profile())));
  EXPECT_FALSE(
      did_script_inject(browser()->tab_strip_model()->GetActiveWebContents()));

  // The extension should inject on "normal" urls.
  GURL unprotected_url = embedded_test_server()->GetURL(
      "example.com", "/extensions/test_file.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), unprotected_url));
  EXPECT_TRUE(
      did_script_inject(browser()->tab_strip_model()->GetActiveWebContents()));
}

IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, ContentScriptSameSiteCookies) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("content_scripts/request_cookies"));
  ASSERT_TRUE(extension);
  GURL url = embedded_test_server()->GetURL("a.com", "/extensions/body1.html");
  ResultCatcher catcher;
  constexpr char kScript[] =
      R"(chrome.tabs.create({url: '%s'}, () => {
           let message = 'success';
           if (chrome.runtime.lastError)
             message = chrome.runtime.lastError.message;
           domAutomationController.send(message);
         });)";
  std::string result = browsertest_util::ExecuteScriptInBackgroundPage(
      profile(), extension->id(),
      base::StringPrintf(kScript, url.spec().c_str()));

  EXPECT_EQ("success", result);
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, ExecuteScriptFileSameSiteCookies) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("content_scripts/request_cookies"));
  ASSERT_TRUE(extension);
  GURL url = embedded_test_server()->GetURL("b.com", "/extensions/body1.html");
  ResultCatcher catcher;
  constexpr char kScript[] =
      R"(chrome.tabs.create({url: '%s'}, (tab) => {
           if (chrome.runtime.lastError) {
             domAutomationController.send(chrome.runtime.lastError.message);
             return;
           }
           chrome.tabs.executeScript(tab.id, {file: 'cookies.js'}, () => {
             let message = 'success';
             if (chrome.runtime.lastError)
               message = chrome.runtime.lastError.message;
             domAutomationController.send(message);
           });
         });)";
  std::string result = browsertest_util::ExecuteScriptInBackgroundPage(
      profile(), extension->id(),
      base::StringPrintf(kScript, url.spec().c_str()));

  EXPECT_EQ("success", result);
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, ExecuteScriptCodeSameSiteCookies) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("content_scripts/request_cookies"));
  ASSERT_TRUE(extension);
  GURL url = embedded_test_server()->GetURL("b.com", "/extensions/body1.html");
  ResultCatcher catcher;
  constexpr char kScript[] =
      R"(chrome.tabs.create({url: '%s'}, (tab) => {
           if (chrome.runtime.lastError) {
             domAutomationController.send(chrome.runtime.lastError.message);
             return;
           }
           fetch(chrome.runtime.getURL('cookies.js')).then((response) => {
             return response.text();
           }).then((text) => {
             chrome.tabs.executeScript(tab.id, {code: text}, () => {
               let message = 'success';
               if (chrome.runtime.lastError)
                 message = chrome.runtime.lastError.message;
               domAutomationController.send(message);
             });
           }).catch((e) => {
             domAutomationController.send(e);
           });
         });)";
  std::string result = browsertest_util::ExecuteScriptInBackgroundPage(
      profile(), extension->id(),
      base::StringPrintf(kScript, url.spec().c_str()));

  EXPECT_EQ("success", result);
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Tests that extension content scripts can execute (including asynchronously
// through timeouts) in pages with Content-Security-Policy: sandbox.
// See https://crbug.com/811528.
IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, ExecuteScriptBypassingSandbox) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  TestExtensionDir test_dir;
  test_dir.WriteManifest(
      R"({
           "name": "Bypass Sandbox CSP",
           "description": "Extensions should bypass a page's CSP sandbox.",
           "version": "0.1",
           "manifest_version": 2,
           "content_scripts": [{
             "matches": ["*://example.com:*/*"],
             "js": ["script.js"]
           }]
         })");
  test_dir.WriteFile(
      FILE_PATH_LITERAL("script.js"),
      R"(window.setTimeout(() => { chrome.test.notifyPass(); }, 10);)");

  ResultCatcher catcher;
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  GURL url = embedded_test_server()->GetURL(
      "example.com", "/extensions/page_with_sandbox_csp.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Regression test for https://crbug.com/883526.
IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, InifiniteLoopInGetEffectiveURL) {
  // Create an extension that injects content scripts into about:blank frames
  // (and therefore has a chance to trigger an infinite loop in
  // ScriptContext::GetEffectiveDocumentURLForInjection()).
  TestExtensionDir test_dir;
  test_dir.WriteManifest(
      R"({
           "name": "Content scripts everywhere",
           "description": "Content scripts everywhere",
           "version": "0.1",
           "manifest_version": 2,
           "content_scripts": [{
             "matches": ["<all_urls>"],
             "all_frames": true,
             "match_about_blank": true,
             "js": ["script.js"]
           }],
           "permissions": ["*://*/*"],
         })");
  test_dir.WriteFile(FILE_PATH_LITERAL("script.js"), "console.log('blah')");

  // Create an "infinite" loop for hopping over parent/opener:
  // subframe1 ---parent---> mainFrame ---opener--> subframe1 ...
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::ExecJs(web_contents,
                              R"(
                                  var iframe = document.createElement('iframe');
                                  document.body.appendChild(iframe);
                                  window.name = 'main-frame'; )"));
  content::RenderFrameHost* subframe1 = ChildFrameAt(web_contents, 0);
  ASSERT_TRUE(
      content::ExecJs(subframe1, "var w = window.open('', 'main-frame');"));
  EXPECT_EQ(subframe1, web_contents->GetOpener());

  // Trigger GetEffectiveURL from another subframe:
  ASSERT_TRUE(content::ExecJs(web_contents,
                              R"(
                                  var iframe = document.createElement('iframe');
                                  document.body.appendChild(iframe); )"));

  // Verify that the renderer is still responsive / that the renderer didn't
  // enter an infinite loop.
  EXPECT_EQ(123, content::EvalJs(web_contents, "123"));
}

// Verifies how the messaging API works with content scripts.
IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, Test) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "content_scripts/other_extensions/message_echoer_allows_by_default")));
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "content_scripts/other_extensions/message_echoer_allows")));
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "content_scripts/other_extensions/message_echoer_denies")));
  ASSERT_TRUE(RunExtensionTest("content_scripts/messaging")) << message_;
}

// Tests that the URLs of content scripts are set to the extension URL
// (chrome-extension://<id>/<path_to_script>) rather than the local file
// path.
// Regression test for https://crbug.com/714617.
IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, ContentScriptUrls) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  TestExtensionDir test_dir;
  test_dir.WriteManifest(
      R"({
           "name": "Content Script",
           "manifest_version": 2,
           "version": "0.1",
           "background": {"scripts": ["background.js"]},
           "content_scripts": [{
             "matches": ["*://content-script.example/*"],
             "js": ["content_script.js"]
           }],
           "permissions": ["*://*/*"]
         })");
  constexpr char kContentScriptSrc[] =
      R"(console.error('TestMessage');
         chrome.test.notifyPass();)";
  test_dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), kContentScriptSrc);
  constexpr char kBackgroundScriptSrc[] =
      R"(chrome.tabs.onUpdated.addListener((id, change, tab) => {
           if (change.status !== 'complete')
             return;
           const url = new URL(tab.url);
           if (url.hostname !== 'inject-script.example')
             return;
           chrome.tabs.executeScript(id, {file: 'content_script.js'});
         });)";
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundScriptSrc);

  const Extension* const extension = LoadExtension(test_dir.UnpackedPath());

  auto load_page_and_check_error = [this, extension](const char* host) {
    SCOPED_TRACE(host);
    ResultCatcher catcher;
    content::WebContentsConsoleObserver observer(
        browser()->tab_strip_model()->GetActiveWebContents());
    auto filter =
        [](const content::WebContentsConsoleObserver::Message& message) {
          return message.message == u"TestMessage";
        };
    observer.SetFilter(base::BindRepeating(filter));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL(host, "/simple.html")));
    ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
    ASSERT_EQ(1u, observer.messages().size());
    GURL source_url(observer.messages()[0].source_id);
    ASSERT_TRUE(source_url.is_valid());
    EXPECT_EQ(kExtensionScheme, source_url.scheme_piece());
    EXPECT_EQ(extension->id(), source_url.host_piece());
  };

  // Test the script url from both a static content script specified in the
  // manifest, and a script injected through chrome.tabs.executeScript().
  load_page_and_check_error("content-script.example");
  load_page_and_check_error("inject-script.example");
}

// Verifies how the storage API works with content scripts with default access
// level.
IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, StorageApiDefaultAccessTest) {
  // The extension verifies expectations in its background context and
  // initializes state, which will be used by the content script below.
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("content_scripts/storage_api_default_access"))
      << message_;

  // Open a url to run the content script. The content script
  // then continues the test, so we need a separate ResultCatcher.
  ResultCatcher catcher;
  GURL url(embedded_test_server()->GetURL("/extensions/test_file.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Verifies how the storage API works with content scripts with untrusted access
// level.
IN_PROC_BROWSER_TEST_F(ContentScriptApiTest,
                       StorageApiAllowUntrustedAccessTest) {
  // The extension verifies expectations in its background context and
  // initializes state, which will be used by the content script below.
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(
      RunExtensionTest("content_scripts/storage_api_allow_untrusted_access"))
      << message_;

  // Open a url to run the content script. The content script
  // then continues the test, so we need a separate ResultCatcher.
  ResultCatcher catcher;
  GURL url(embedded_test_server()->GetURL("/extensions/test_file.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// A test suite designed for exercising the behavior of content script
// injection into opaque URLs (like about:blank).
class ContentScriptRelatedFrameTest : public ContentScriptApiTest {
 public:
  ContentScriptRelatedFrameTest() = default;

  ContentScriptRelatedFrameTest(const ContentScriptRelatedFrameTest&) = delete;
  ContentScriptRelatedFrameTest& operator=(
      const ContentScriptRelatedFrameTest&) = delete;

  ~ContentScriptRelatedFrameTest() override = default;

  void SetUpOnMainThread() override;

  // Whether the extension's content script should specify
  // match_origin_as_fallback as true.
  virtual bool IncludeMatchOriginAsFallback() { return false; }

  // Returns true if the extension's content script executed in the specified
  // |frame|.
  bool DidScriptRunInFrame(content::RenderFrameHost* host);

  // Navigates the current active tab to the specified |url|, ensuring the
  // navigation succeeds. Returns the active tab's WebContents.
  content::WebContents* NavigateTab(const GURL& url);

  // Opens a popup to the specified |url| from the given |opener_web_contents|.
  // Ensures the navigation succeeds, and returns the newly-opened popup's
  // WebContents.
  content::WebContents* OpenPopup(content::WebContents* opener_web_contents,
                                  const GURL& url);

  // Navigates an iframe to the specified |url| from the context of
  // |navigating_host|. The iframe is retrieved from |navigating_host| by
  // evaluating |frame_getter| (e.g., `frames[0]`).
  void NavigateIframe(content::RenderFrameHost* navigating_host,
                      const std::string& frame_getter,
                      const GURL& url);

  // Creates a new blob: URL, associated with the given `host`.
  GURL CreateBlobURL(content::RenderFrameHost* host);

  // Creates a new filesystem: URL, associated with the given `host`.
  GURL CreateFilesystemURL(content::RenderFrameHost* host);

  const GURL& about_blank() const { return about_blank_; }
  const GURL& allowed_url() const { return allowed_url_; }
  const GURL& disallowed_url() const { return disallowed_url_; }
  const GURL& allowed_url_with_iframe() const {
    return allowed_url_with_iframe_;
  }
  const GURL& disallowed_url_with_iframe() const {
    return disallowed_url_with_iframe_;
  }
  const GURL& null_document_url() const { return null_document_url_; }
  const GURL& data_url() const { return data_url_; }
  const GURL& path_specific_allowed_url() const {
    return path_specific_allowed_url_;
  }
  const GURL& matching_path_specific_iframe_url() const {
    return matching_path_specific_iframe_url_;
  }
  const GURL& non_matching_path_specific_iframe_url() const {
    return non_matching_path_specific_iframe_url_;
  }

 private:
  static constexpr char kMarkerSpanId[] = "content-script-marker";

  // The about:blank URL.
  GURL about_blank_;
  // A simple URL the extension is allowed to access.
  GURL allowed_url_;
  // A simple URL the extension is not allowed to access.
  GURL disallowed_url_;
  // A URL the extension can access with an iframe in the DOM.
  GURL allowed_url_with_iframe_;
  // A URL the extension is not allowed to access with an iframe in the DOM.
  GURL disallowed_url_with_iframe_;
  // A URL that leads to a page with an that rewrites the parent document to be
  // null.
  GURL null_document_url_;
  // A simple data URL.
  GURL data_url_;
  // A URL that matches a path-specific match pattern.
  GURL path_specific_allowed_url_;
  // A URL that matches a path-specific match pattern and has an iframe in the
  // DOM.
  GURL matching_path_specific_iframe_url_;
  // A URL that matches the domain of a path-specific match pattern - but not
  // the path component - which also has an iframe in the DOM.
  GURL non_matching_path_specific_iframe_url_;

  // The test directory used to load our extension.
  TestExtensionDir test_extension_dir_;
  // The ID of the loaded extension.
  ExtensionId extension_id_;
};

constexpr char ContentScriptRelatedFrameTest::kMarkerSpanId[];

void ContentScriptRelatedFrameTest::SetUpOnMainThread() {
  ContentScriptApiTest::SetUpOnMainThread();
  ASSERT_TRUE(StartEmbeddedTestServer());
  about_blank_ = GURL(url::kAboutBlankURL);
  allowed_url_ = embedded_test_server()->GetURL("example.com", "/simple.html");
  disallowed_url_ =
      embedded_test_server()->GetURL("chromium.org", "/simple.html");
  allowed_url_with_iframe_ =
      embedded_test_server()->GetURL("example.com", "/iframe.html");
  disallowed_url_with_iframe_ =
      embedded_test_server()->GetURL("chromium.org", "/iframe.html");
  null_document_url_ = embedded_test_server()->GetURL(
      "chromium.org", "/extensions/null_document.html");
  path_specific_allowed_url_ =
      embedded_test_server()->GetURL("path-test.example", "/simple.html");
  matching_path_specific_iframe_url_ =
      embedded_test_server()->GetURL("path-test.example", "/iframe.html");
  non_matching_path_specific_iframe_url_ =
      embedded_test_server()->GetURL("path-test.example", "/iframe_blank.html");
  data_url_ = GURL("data:text/html,<html>Hi</html>");

  constexpr char kContentScriptManifest[] =
      R"({
           "name": "Content Script injection in related frames",
           "manifest_version": 3,
           "version": "0.1",
           "content_scripts": [{
             "matches": ["http://example.com/*"],
             "js": ["script.js"],
             "run_at": "document_end",
             "all_frames": true,
             %s
             "match_about_blank": true
           }, {
             "matches": [
               "http://path-test.example/simple.html",
               "http://path-test.example/iframe.html"
             ],
             "js": ["script.js"],
             "run_at": "document_end",
             "all_frames": true,
             "match_about_blank": true
           }]
         })";
  const char* extra_property = "";
  if (IncludeMatchOriginAsFallback())
    extra_property = R"("match_origin_as_fallback": true,)";
  std::string manifest =
      base::StringPrintf(kContentScriptManifest, extra_property);
  test_extension_dir_.WriteManifest(manifest);

  std::string script = base::StringPrintf(
      R"(let span = document.createElement('span');
         span.id = '%s';
         document.body.appendChild(span);)",
      kMarkerSpanId);
  test_extension_dir_.WriteFile(FILE_PATH_LITERAL("script.js"), script);
  const Extension* extension =
      LoadExtension(test_extension_dir_.UnpackedPath());
  ASSERT_TRUE(extension);
  extension_id_ = extension->id();
}

bool ContentScriptRelatedFrameTest::DidScriptRunInFrame(
    content::RenderFrameHost* host) {
  // The WebContents needs to have stopped loading at this point for this check
  // to be guaranteed. Since the script runs at document_end (which runs
  // after DOMContentLoaded is fired, before window.onload), this check will be
  // guaranteed to run after it.
  EXPECT_FALSE(content::WebContents::FromRenderFrameHost(host)->IsLoading());
  bool did_run =
      content::EvalJs(host, content::JsReplace("!!document.getElementById($1)",
                                               kMarkerSpanId))
          .ExtractBool();
  if (did_run) {
    // Sanity check: If the content script ran in the frame, we should also have
    // tracked it properly browser-side.
    // Note that we don't just do:
    //   EXPECT_EQ(did_run, DidProcessRunContentScriptFromExtension(...))
    // because even if the given frame didn't have the script run, another frame
    // in the process may have.
    EXPECT_TRUE(ContentScriptTracker::DidProcessRunContentScriptFromExtension(
        *host->GetProcess(), extension_id_));
  }

  return did_run;
}

content::WebContents* ContentScriptRelatedFrameTest::NavigateTab(
    const GURL& url) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer(web_contents);
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(url, web_contents->GetLastCommittedURL());
  return web_contents;
}

content::WebContents* ContentScriptRelatedFrameTest::OpenPopup(
    content::WebContents* opener_web_contents,
    const GURL& url) {
  int initial_tab_count = browser()->tab_strip_model()->count();
  content::TestNavigationObserver popup_observer(nullptr /* web_contents */);
  popup_observer.StartWatchingNewWebContents();
  EXPECT_TRUE(content::ExecuteScript(
      opener_web_contents, content::JsReplace("window.open($1);", url.spec())));
  popup_observer.Wait();
  EXPECT_EQ(initial_tab_count + 1, browser()->tab_strip_model()->count());
  content::WebContents* popup =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(url, popup->GetLastCommittedURL());
  EXPECT_NE(popup, opener_web_contents);
  return popup;
}

void ContentScriptRelatedFrameTest::NavigateIframe(
    content::RenderFrameHost* navigating_host,
    const std::string& frame_getter,
    const GURL& url) {
  constexpr char kScriptTemplate[] =
      R"({
           let frame = %s;
           frame.location.href = '%s';
         })";

  std::string script = base::StringPrintf(kScriptTemplate, frame_getter.c_str(),
                                          url.spec().c_str());
  content::TestNavigationObserver navigation_observer(url);
  navigation_observer.WatchExistingWebContents();
  EXPECT_TRUE(content::ExecuteScript(navigating_host, script));
  navigation_observer.Wait();
  EXPECT_TRUE(navigation_observer.last_navigation_succeeded());

  // Also wait for the full WebContents to stop loading, in case the iframe's
  // new source has nested iframes.
  EXPECT_TRUE(content::WaitForLoadStop(
      content::WebContents::FromRenderFrameHost(navigating_host)));
}

GURL ContentScriptRelatedFrameTest::CreateBlobURL(
    content::RenderFrameHost* host) {
  constexpr char kCreateBlobURL[] =
      R"((() => {
           let content = '<html><h1>BLOB!</h1></html>';
           let blob = new Blob([content], {type: 'text/html'});
           return URL.createObjectURL(blob);
         })();)";
  std::string url_string =
      content::EvalJs(host, kCreateBlobURL).ExtractString();
  GURL url(url_string);
  EXPECT_TRUE(url.is_valid());
  EXPECT_EQ(url::kBlobScheme, url.scheme());
  EXPECT_EQ(
      url::Origin::Create(host->GetLastCommittedURL()).GetURL(),
      url::Origin::Create(url).GetTupleOrPrecursorTupleIfOpaque().GetURL());
  return url;
}

GURL ContentScriptRelatedFrameTest::CreateFilesystemURL(
    content::RenderFrameHost* host) {
  constexpr char kCreateFilesystemURL[] =
      R"((new Promise((resolve) => {
           let blob = new Blob(['<html><body>" + content + "</body></html>'],
                               {type: 'text/html'});
           window.webkitRequestFileSystem(TEMPORARY, blob.size, fs => {
             fs.root.getFile('foo.html', {create: true}, file => {
               file.createWriter(writer => {
                 writer.write(blob);
                 writer.onwriteend = () => {
                   resolve(file.toURL());
                 }
               });
             });
           });
         }));)";
  std::string url_string =
      content::EvalJs(host, kCreateFilesystemURL).ExtractString();
  GURL url(url_string);
  EXPECT_TRUE(url.is_valid());
  EXPECT_EQ(url::kFileSystemScheme, url.scheme());
  EXPECT_EQ(
      url::Origin::Create(host->GetLastCommittedURL()).GetURL(),
      url::Origin::Create(url).GetTupleOrPrecursorTupleIfOpaque().GetURL());
  return url;
}

// Injection should succeed on a popup to about:blank created by an allowed
// site.
IN_PROC_BROWSER_TEST_F(ContentScriptRelatedFrameTest,
                       MatchAboutBlank_Iframe_Allowed) {
  content::WebContents* tab = NavigateTab(allowed_url_with_iframe());
  NavigateIframe(tab->GetPrimaryMainFrame(), "frames[0]", about_blank());
  content::RenderFrameHost* render_frame_host =
      content::ChildFrameAt(tab->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(render_frame_host);
  EXPECT_EQ(about_blank(), render_frame_host->GetLastCommittedURL());
  EXPECT_TRUE(DidScriptRunInFrame(render_frame_host));
}

// Injection should fail on an iframe to about:blank created by a disallowed
// site.
IN_PROC_BROWSER_TEST_F(ContentScriptRelatedFrameTest,
                       MatchAboutBlank_Iframe_Disallowed) {
  content::WebContents* tab = NavigateTab(disallowed_url_with_iframe());
  NavigateIframe(tab->GetPrimaryMainFrame(), "frames[0]", about_blank());
  content::RenderFrameHost* render_frame_host =
      content::ChildFrameAt(tab->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(render_frame_host);
  EXPECT_EQ(about_blank(), render_frame_host->GetLastCommittedURL());
  EXPECT_FALSE(DidScriptRunInFrame(render_frame_host));
}

// Injection should succeed on a popup to about:blank created by an allowed
// site.
IN_PROC_BROWSER_TEST_F(ContentScriptRelatedFrameTest,
                       MatchAboutBlank_Popup_Allowed) {
  content::WebContents* tab = NavigateTab(allowed_url());
  content::WebContents* popup = OpenPopup(tab, about_blank());
  EXPECT_TRUE(DidScriptRunInFrame(popup->GetPrimaryMainFrame()));
}

// Injection should fail on a popup to about:blank created by a disallowed site.
IN_PROC_BROWSER_TEST_F(ContentScriptRelatedFrameTest,
                       MatchAboutBlank_Popup_Disallowed) {
  content::WebContents* tab = NavigateTab(disallowed_url());
  content::WebContents* popup = OpenPopup(tab, about_blank());
  EXPECT_FALSE(DidScriptRunInFrame(popup->GetPrimaryMainFrame()));
}

// Browser-initiated navigations do not have a separate precursor tuple, so
// injection should be disallowed.
IN_PROC_BROWSER_TEST_F(ContentScriptRelatedFrameTest,
                       MatchAboutBlank_BrowserOpened) {
  content::WebContents* tab = NavigateTab(about_blank());
  EXPECT_FALSE(DidScriptRunInFrame(tab->GetPrimaryMainFrame()));
}

// Tests injecting a content script when the iframe rewrites the parent to be
// null. This re-write causes the parent to itself become an about:blank frame
// without a parent. Regression test for https://crbug.com/963347 and
// https://crbug.com/963420.
IN_PROC_BROWSER_TEST_F(ContentScriptRelatedFrameTest,
                       MatchAboutBlank_NullParent) {
  NavigateParams navigate_params(browser(), null_document_url(),
                                 ui::PAGE_TRANSITION_TYPED);
  navigate_params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;

  // Save the WebContents instance that will be created by this navigation, as
  // the dom message that we later wait for is sent in this instance.
  content::WebContents* web_contents = nullptr;
  {
    content::WebContentsAddedObserver new_web_contents_observer;
    Navigate(&navigate_params);
    web_contents = new_web_contents_observer.GetWebContents();
  }

  content::DOMMessageQueue message_queue(web_contents);
  std::string result;
  // We can't rely on the navigation observer logic, because the frame is
  // destroyed before it finishes loading. Instead, it sends a message through
  // DOMAutomationController immediately before it (synchronously) re-writes the
  // parent.
  ASSERT_TRUE(message_queue.WaitForMessage(&result));
  EXPECT_EQ(R"("navigated")", result);
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(null_document_url(), tab->GetLastCommittedURL());
  content::RenderFrameHost* main_frame = tab->GetPrimaryMainFrame();
  // Sanity check: The main frame should have been re-written. The test passes
  // if there's no crash. Since the iframe rewrites the parent synchronously
  // after sending the "navigated" message, there's no risk of a race here.
  EXPECT_EQ("null", content::EvalJs(main_frame, "document.body.innerHTML;"));
  // The test passes if there's no crash. Previously, we didn't handle the
  // no-parent about:blank case well when there was a non-about:blank
  // precursor origin, which caused a crash during the document writing.
}

// Tests that match_about_blank does not allow extensions to inject into blob:
// URLs.
IN_PROC_BROWSER_TEST_F(ContentScriptRelatedFrameTest,
                       MatchAboutBlank_BlobFrame) {
  content::WebContents* tab = NavigateTab(allowed_url_with_iframe());
  GURL blob_url = CreateBlobURL(tab->GetPrimaryMainFrame());
  NavigateIframe(tab->GetPrimaryMainFrame(), "frames[0]", blob_url);
  content::RenderFrameHost* render_frame_host =
      content::ChildFrameAt(tab->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(render_frame_host);
  EXPECT_EQ(blob_url, render_frame_host->GetLastCommittedURL());
  EXPECT_FALSE(DidScriptRunInFrame(render_frame_host));
}

// Tests that match_about_blank does not allow extensions to inject into data:
// URLs.
IN_PROC_BROWSER_TEST_F(ContentScriptRelatedFrameTest,
                       MatchAboutBlank_DataFrame) {
  content::WebContents* tab = NavigateTab(allowed_url_with_iframe());
  NavigateIframe(tab->GetPrimaryMainFrame(), "frames[0]", data_url());
  content::RenderFrameHost* render_frame_host =
      content::ChildFrameAt(tab->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(render_frame_host);
  EXPECT_EQ(data_url(), render_frame_host->GetLastCommittedURL());
  EXPECT_FALSE(DidScriptRunInFrame(render_frame_host));
}

// Tests that content scripts can run on filesystem: URLs.
IN_PROC_BROWSER_TEST_F(ContentScriptRelatedFrameTest,
                       MatchAboutBlank_FilesystemFrame) {
  // TODO(https://crbug.com/1332598): Remove this test when removing filesystem:
  // navigation for good.
  if (!base::FeatureList::IsEnabled(blink::features::kFileSystemUrlNavigation))
    GTEST_SKIP();

  content::WebContents* tab = NavigateTab(allowed_url_with_iframe());
  GURL filesystem_url = CreateFilesystemURL(tab->GetPrimaryMainFrame());
  NavigateIframe(tab->GetPrimaryMainFrame(), "frames[0]", filesystem_url);
  content::RenderFrameHost* render_frame_host =
      content::ChildFrameAt(tab->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(render_frame_host);
  EXPECT_EQ(filesystem_url, render_frame_host->GetLastCommittedURL());

  // Even though match_about_blank won't consider filesystem: URLs when
  // determining the URL to use, URLPatterns (used in permissions and
  // content script URL pattern matching) do. As such, the content script
  // still injects into the filesystem frame.
  EXPECT_TRUE(DidScriptRunInFrame(render_frame_host));
}

// Test content script injection into iframes when the script has a
// path-specific pattern.
IN_PROC_BROWSER_TEST_F(ContentScriptRelatedFrameTest,
                       FrameInjectionWithPathSpecificMatchPattern) {
  // Open a page to the page that's same-origin with the match pattern, but
  // doesn't match.
  content::WebContents* tab =
      NavigateTab(non_matching_path_specific_iframe_url());
  // Navigate the child frame to the URL that matches the path requirement.
  NavigateIframe(tab->GetPrimaryMainFrame(), "frames[0]",
                 path_specific_allowed_url());

  content::RenderFrameHost* child_frame =
      content::ChildFrameAt(tab->GetPrimaryMainFrame(), 0);

  EXPECT_EQ(path_specific_allowed_url(), child_frame->GetLastCommittedURL());
  // The script should have ran in the child frame (which matches the pattern),
  // but not the parent frame (which doesn't match the path component).
  EXPECT_TRUE(DidScriptRunInFrame(child_frame));
  EXPECT_FALSE(DidScriptRunInFrame(tab->GetPrimaryMainFrame()));

  // Now, navigate the iframe to an about:blank URL.
  NavigateIframe(tab->GetPrimaryMainFrame(), "frames[0]", about_blank());

  // Unlike match_origin_as_fallback, match_about_blank will attempt to climb
  // the frame tree to find an ancestor with path. This results in finding the
  // parent frame, which doesn't match the script's pattern, and so the script
  // does not inject.
  EXPECT_EQ(about_blank(), child_frame->GetLastCommittedURL());
  EXPECT_FALSE(DidScriptRunInFrame(child_frame));
}

// TODO(devlin): Similar to the above test, exercise one with a frame that
// closes its own parent. This needs to use tabs.executeScript (for timing
// reasons), but is close enough to a content script test to re-use the same
// suite.

class ContentScriptMatchOriginAsFallbackTest
    : public ContentScriptRelatedFrameTest {
 public:
  ContentScriptMatchOriginAsFallbackTest() {
    feature_list_.InitAndEnableFeature(
        extensions_features::kContentScriptsMatchOriginAsFallback);
  }
  ~ContentScriptMatchOriginAsFallbackTest() override = default;

  bool IncludeMatchOriginAsFallback() override { return true; }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Inject a content script on an iframe to a data: URL on an allowed site.
IN_PROC_BROWSER_TEST_F(ContentScriptMatchOriginAsFallbackTest,
                       DataURLInjection_SimpleIframe_Allowed) {
  content::WebContents* tab = NavigateTab(allowed_url_with_iframe());
  NavigateIframe(tab->GetPrimaryMainFrame(), "frames[0]", data_url());
  content::RenderFrameHost* render_frame_host =
      content::ChildFrameAt(tab->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(render_frame_host);
  EXPECT_EQ(data_url(), render_frame_host->GetLastCommittedURL());
  EXPECT_TRUE(DidScriptRunInFrame(render_frame_host));
}

// Fail to inject a content script on an iframe to a data: URL on a protected
// site.
IN_PROC_BROWSER_TEST_F(ContentScriptMatchOriginAsFallbackTest,
                       DataURLInjection_SimpleIframe_Disallowed) {
  content::WebContents* tab = NavigateTab(disallowed_url_with_iframe());
  NavigateIframe(tab->GetPrimaryMainFrame(), "frames[0]", data_url());
  content::RenderFrameHost* render_frame_host =
      content::ChildFrameAt(tab->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(render_frame_host);
  EXPECT_EQ(data_url(), render_frame_host->GetLastCommittedURL());
  EXPECT_FALSE(DidScriptRunInFrame(render_frame_host));
}

// Inject a content script on an iframe to a blob: URL on an allowed site.
IN_PROC_BROWSER_TEST_F(ContentScriptMatchOriginAsFallbackTest,
                       BlobURLInjection_SimpleIframe_Allowed) {
  content::WebContents* tab = NavigateTab(allowed_url_with_iframe());
  GURL blob_url = CreateBlobURL(tab->GetPrimaryMainFrame());
  NavigateIframe(tab->GetPrimaryMainFrame(), "frames[0]", blob_url);
  content::RenderFrameHost* render_frame_host =
      content::ChildFrameAt(tab->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(render_frame_host);
  EXPECT_EQ(blob_url, render_frame_host->GetLastCommittedURL());
  EXPECT_TRUE(DidScriptRunInFrame(render_frame_host));
}

// Fail to inject a content script on an iframe to a blob: URL on a protected
// site.
IN_PROC_BROWSER_TEST_F(ContentScriptMatchOriginAsFallbackTest,
                       BlobURLInjection_SimpleIframe_Disallowed) {
  content::WebContents* tab = NavigateTab(disallowed_url_with_iframe());
  GURL blob_url = CreateBlobURL(tab->GetPrimaryMainFrame());
  NavigateIframe(tab->GetPrimaryMainFrame(), "frames[0]", blob_url);
  content::RenderFrameHost* render_frame_host =
      content::ChildFrameAt(tab->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(render_frame_host);
  EXPECT_EQ(blob_url, render_frame_host->GetLastCommittedURL());
  EXPECT_FALSE(DidScriptRunInFrame(render_frame_host));
}

// Inject a content script on an iframe to a filesystem: URL on an allowed site.
IN_PROC_BROWSER_TEST_F(ContentScriptMatchOriginAsFallbackTest,
                       FilesystemURLInjection_SimpleIframe_Allowed) {
  // TODO(https://crbug.com/1332598): Remove this test when removing filesystem:
  // navigation for good.
  if (!base::FeatureList::IsEnabled(blink::features::kFileSystemUrlNavigation))
    GTEST_SKIP();
  content::WebContents* tab = NavigateTab(allowed_url_with_iframe());
  GURL filesystem_url = CreateFilesystemURL(tab->GetPrimaryMainFrame());
  NavigateIframe(tab->GetPrimaryMainFrame(), "frames[0]", filesystem_url);
  content::RenderFrameHost* render_frame_host =
      content::ChildFrameAt(tab->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(render_frame_host);
  EXPECT_EQ(filesystem_url, render_frame_host->GetLastCommittedURL());
  EXPECT_TRUE(DidScriptRunInFrame(render_frame_host));
}

// Fail to inject a content script on an iframe to a filesystem: URL on a
// protected site.
IN_PROC_BROWSER_TEST_F(ContentScriptMatchOriginAsFallbackTest,
                       FilesystemURLInjection_SimpleIframe_Disallowed) {
  // TODO(https://crbug.com/1332598): Remove this test when removing filesystem:
  // navigation for good.
  if (!base::FeatureList::IsEnabled(blink::features::kFileSystemUrlNavigation))
    GTEST_SKIP();
  content::WebContents* tab = NavigateTab(disallowed_url_with_iframe());
  GURL filesystem_url = CreateFilesystemURL(tab->GetPrimaryMainFrame());
  NavigateIframe(tab->GetPrimaryMainFrame(), "frames[0]", filesystem_url);
  content::RenderFrameHost* render_frame_host =
      content::ChildFrameAt(tab->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(render_frame_host);
  EXPECT_EQ(filesystem_url, render_frame_host->GetLastCommittedURL());
  EXPECT_FALSE(DidScriptRunInFrame(render_frame_host));
}

// Inject into nested iframes with data: URLs.
IN_PROC_BROWSER_TEST_F(ContentScriptMatchOriginAsFallbackTest,
                       DataURLInjection_NestedDataIframe_SameOrigin) {
  content::WebContents* tab = NavigateTab(allowed_url_with_iframe());

  // Create a data: URL that will have an iframe to another data: URL.
  std::string nested_frame_src = "data:text/html,<html>Hello</html>";
  const std::string nested_data_html = base::StringPrintf(
      "<html><iframe name=\"nested\" src=\"%s\"></iframe></html>",
      nested_frame_src.c_str());

  const GURL data_url(base::StrCat({"data:text/html,", nested_data_html}));
  NavigateIframe(tab->GetPrimaryMainFrame(), "frames[0]", data_url);

  // The extension should have injected in both iframes, since they each
  // "belong" to the original, allowed site.
  content::RenderFrameHost* first_data =
      content::ChildFrameAt(tab->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(first_data);
  EXPECT_EQ(data_url, first_data->GetLastCommittedURL());
  EXPECT_TRUE(DidScriptRunInFrame(first_data));

  content::RenderFrameHost* nested_data = content::ChildFrameAt(first_data, 0);
  ASSERT_TRUE(nested_data);
  EXPECT_EQ(GURL(nested_frame_src), nested_data->GetLastCommittedURL());
  EXPECT_TRUE(DidScriptRunInFrame(nested_data));
}

// Test content script injection into navigated iframes to data: URLs when the
// navigator is not accessible by the extension.
IN_PROC_BROWSER_TEST_F(
    ContentScriptMatchOriginAsFallbackTest,
    DataURLInjection_NestedDataIframe_Navigation_Disallowed) {
  // Open a page to a protected site, and then navigate an iframe to an allowed
  // site with an iframe.
  content::WebContents* tab = NavigateTab(disallowed_url_with_iframe());
  NavigateIframe(tab->GetPrimaryMainFrame(), "frames[0]",
                 allowed_url_with_iframe());
  content::RenderFrameHost* example_com_frame =
      content::ChildFrameAt(tab->GetPrimaryMainFrame(), 0);
  EXPECT_EQ(allowed_url_with_iframe(),
            example_com_frame->GetLastCommittedURL());

  // Navigate the iframe within the allowed site to a data URL.
  NavigateIframe(example_com_frame, "frames[0]", data_url());

  {
    // The allowed site is the initiator of the data URL frame, and the
    // extension should inject.
    content::RenderFrameHost* data_url_host = content::FrameMatchingPredicate(
        tab->GetPrimaryPage(),
        base::BindRepeating(content::FrameHasSourceUrl, data_url()));
    ASSERT_TRUE(data_url_host);
    EXPECT_EQ(data_url(), data_url_host->GetLastCommittedURL());
    EXPECT_TRUE(DidScriptRunInFrame(data_url_host));
  }

  // Now, navigate the iframe within the allowed site to a data URL, but do so
  // from the top frame (which the extension is not allowed to access).
  NavigateIframe(tab->GetPrimaryMainFrame(), "frames[0].frames[0]", data_url());

  {
    // Since the top frame (which the extension may not access) is now the
    // initiator of the data: URL, the extension shouldn't inject.
    content::RenderFrameHost* data_url_host = content::FrameMatchingPredicate(
        tab->GetPrimaryPage(),
        base::BindRepeating(content::FrameHasSourceUrl, data_url()));
    ASSERT_TRUE(data_url_host);
    EXPECT_EQ(data_url(), data_url_host->GetLastCommittedURL());
    EXPECT_FALSE(DidScriptRunInFrame(data_url_host));
  }
}

// Test content script injection into navigated iframes to data: URLs when the
// navigator is accessible by the extension.
IN_PROC_BROWSER_TEST_F(ContentScriptMatchOriginAsFallbackTest,
                       DataURLInjection_NestedDataIframe_Navigation_Allowed) {
  // Open a page to an allowed site, and then navigate an iframe to a disallowed
  // site with an iframe.
  content::WebContents* tab = NavigateTab(allowed_url_with_iframe());
  NavigateIframe(tab->GetPrimaryMainFrame(), "frames[0]",
                 disallowed_url_with_iframe());
  content::RenderFrameHost* example_com_frame =
      content::ChildFrameAt(tab->GetPrimaryMainFrame(), 0);
  EXPECT_EQ(disallowed_url_with_iframe(),
            example_com_frame->GetLastCommittedURL());

  // Navigate the iframe within the disallowed site to a data URL.
  NavigateIframe(example_com_frame, "frames[0]", data_url());

  {
    // The disallowed site is the initiator of the data URL frame, and the
    // extension should not inject.
    content::RenderFrameHost* data_url_host = content::FrameMatchingPredicate(
        tab->GetPrimaryPage(),
        base::BindRepeating(content::FrameHasSourceUrl, data_url()));
    ASSERT_TRUE(data_url_host);
    EXPECT_TRUE(data_url_host->GetParent());
    EXPECT_EQ(data_url(), data_url_host->GetLastCommittedURL());
    EXPECT_FALSE(DidScriptRunInFrame(data_url_host));
  }

  // Now, navigate the iframe within the disallowed site to a data URL, but do
  // so from the top frame (which the extension is allowed to access).
  NavigateIframe(tab->GetPrimaryMainFrame(), "frames[0].frames[0]", data_url());

  {
    content::RenderFrameHost* data_url_host = content::FrameMatchingPredicate(
        tab->GetPrimaryPage(),
        base::BindRepeating(content::FrameHasSourceUrl, data_url()));
    ASSERT_TRUE(data_url_host);
    EXPECT_TRUE(data_url_host->GetParent());
    EXPECT_EQ(data_url(), data_url_host->GetLastCommittedURL());
    // The extension should be allowed to inject since it has access to the
    // related frame. https://crbug.com/1111028.
    EXPECT_TRUE(DidScriptRunInFrame(data_url_host));
  }
}

// Test fixture which sets a custom NTP Page.
// TODO(karandeepb): Similar logic to set up a custom NTP is used elsewhere as
// well. Abstract this away into a reusable test fixture class.
class NTPInterceptionTest : public ExtensionApiTest {
 public:
  NTPInterceptionTest()
      : https_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  NTPInterceptionTest(const NTPInterceptionTest&) = delete;
  NTPInterceptionTest& operator=(const NTPInterceptionTest&) = delete;

  // ExtensionApiTest override:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    test_data_dir_ = test_data_dir_.AppendASCII("ntp_content_script");
    https_test_server_.ServeFilesFromDirectory(test_data_dir_);
    ASSERT_TRUE(https_test_server_.Start());

    GURL ntp_url = https_test_server_.GetURL("/fake_ntp.html");
    ntp_test_utils::SetUserSelectedDefaultSearchProvider(
        profile(), https_test_server_.base_url().spec(), ntp_url.spec());
  }

  const net::EmbeddedTestServer* https_test_server() const {
    return &https_test_server_;
  }

 private:
  net::EmbeddedTestServer https_test_server_;
};

// Ensure extensions can't inject a content script into the New Tab page.
// Regression test for crbug.com/844428.
IN_PROC_BROWSER_TEST_F(NTPInterceptionTest, ContentScript) {
  // Load an extension which tries to inject a script into every frame.
  ExtensionTestMessageListener listener("ready");
  const Extension* extension = LoadExtension(test_data_dir_);
  ASSERT_TRUE(extension);
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  // Create a corresponding off the record profile for the current profile. This
  // is necessary to reproduce crbug.com/844428, which occurs in part due to
  // incorrect handling of multiple profiles by the NTP code.
  Browser* incognito_browser = CreateIncognitoBrowser(profile());
  ASSERT_TRUE(incognito_browser);

  // Ensure that the extension isn't able to inject the script into the New Tab
  // Page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUINewTabURL)));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(search::IsInstantNTP(web_contents));

  bool script_injected_in_ntp = false;
  ASSERT_TRUE(ExecuteScriptAndExtractBool(
      web_contents,
      "window.domAutomationController.send(document.title !== 'Fake NTP');",
      &script_injected_in_ntp));
  EXPECT_FALSE(script_injected_in_ntp);
}

IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, CoepFrameTest) {
  using HttpRequest = net::test_server::HttpRequest;
  using HttpResponse = net::test_server::HttpResponse;

  // We have a separate server because COEP only works in secure contexts.
  net::EmbeddedTestServer server(net::EmbeddedTestServer::TYPE_HTTPS);
  server.RegisterRequestHandler(base::BindRepeating(
      [](const HttpRequest& request) -> std::unique_ptr<HttpResponse> {
        auto response = std::make_unique<net::test_server::BasicHttpResponse>();
        response->set_content_type("text/html");
        response->AddCustomHeader("cross-origin-embedder-policy",
                                  "require-corp");
        response->set_content("<!doctpye html><html></html>");
        return response;
      }));

  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("content_scripts/coep_frame"));
  ASSERT_TRUE(extension);

  auto handle = server.StartAndReturnHandle();
  const GURL url = server.GetURL("/hello.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  const std::u16string kPassed = u"PASSED";
  const std::u16string kFailed = u"FAILED";
  content::TitleWatcher watcher(
      browser()->tab_strip_model()->GetActiveWebContents(), kPassed);
  watcher.AlsoWaitForTitle(kFailed);

  ASSERT_EQ(kPassed, watcher.WaitAndGetTitle());
}

class ContentScriptApiIdentifiabilityTest : public ContentScriptApiTest {
 public:
  void SetUpOnMainThread() override {
    identifiability_metrics_test_helper_.SetUpOnMainThread();
    ContentScriptApiTest::SetUpOnMainThread();
  }

 protected:
  IdentifiabilityMetricsTestHelper identifiability_metrics_test_helper_;
};

// TODO(crbug.com/1305273): Fix this flaky test.
// Test that identifiability study of content script injection produces the
// expected UKM events.
IN_PROC_BROWSER_TEST_F(ContentScriptApiIdentifiabilityTest,
                       DISABLED_InjectionRecorded) {
  base::RunLoop run_loop;
  identifiability_metrics_test_helper_.PrepareForTest(&run_loop);

  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("content_scripts/all_frames")) << message_;

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      identifiability_metrics_test_helper_.NavigateToBlankAndWaitForMetrics(
          web_contents, &run_loop);

  // Right now the instrumentation infra doesn't track all of the sources that
  // reported a particular surface, so we merely look for if one had it.
  // Eventually both frames should report it.
  //
  // Further, we can't actually check the UKM source ID since those events
  // are renderer-side, so use Document-generated IDs that are different than
  // the navigation IDs provided by RenderFrameHost.
  std::set<ukm::SourceId> source_ids =
      IdentifiabilityMetricsTestHelper::GetSourceIDsForSurfaceAndExtension(
          merged_entries,
          blink::IdentifiableSurface::Type::kExtensionContentScript,
          GetSingleLoadedExtension()->id());
  EXPECT_FALSE(source_ids.empty());
}

// Test that where a page doesn't get a content script injected, no
// such event is recorded.
IN_PROC_BROWSER_TEST_F(ContentScriptApiIdentifiabilityTest,
                       NoInjectionRecorded) {
  base::RunLoop run_loop;
  identifiability_metrics_test_helper_.PrepareForTest(&run_loop);

  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  identifiability_metrics_test_helper_.EnsureIdentifiabilityEventGenerated(
      web_contents);
  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      identifiability_metrics_test_helper_.NavigateToBlankAndWaitForMetrics(
          web_contents, &run_loop);
  EXPECT_FALSE(IdentifiabilityMetricsTestHelper::ContainsSurfaceOfType(
      merged_entries,
      blink::IdentifiableSurface::Type::kExtensionContentScript));
}

class SubresourceWebBundlesContentScriptApiTest : public ExtensionApiTest {
 protected:
  // Registers a request handler for static content.
  void RegisterRequestHandler(const std::string& relative_url,
                              const std::string& content_type,
                              const std::string& content,
                              bool nosniff) {
    embedded_test_server()->RegisterRequestHandler(base::BindLambdaForTesting(
        [relative_url, content_type, content,
         nosniff](const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          if (request.relative_url == relative_url) {
            auto response =
                std::make_unique<net::test_server::BasicHttpResponse>();
            response->set_code(net::HTTP_OK);
            response->set_content_type(content_type);
            response->set_content(content);
            if (nosniff) {
              response->AddCustomHeader("X-Content-Type-Options", "nosniff");
            }
            return std::move(response);
          }
          return nullptr;
        }));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SubresourceWebBundlesContentScriptApiTest,
                       SubresourceWebBundleIframe) {
  // Create an extension that injects a content script in "uuid-in-package"
  // scheme urls.
  TestExtensionDir test_dir;
  test_dir.WriteManifest(R"({
        "name": "Web Request Subresource Web Bundles Test",
        "manifest_version": 2,
        "version": "0.1",
        "permissions": ["uuid-in-package:*"],
        "content_scripts": [{
          "matches":[
            "uuid-in-package:*"
          ],
          "all_frames": true,
          "js":[
            "content_script.js"
          ]
        }]
      })");

  test_dir.WriteFile(FILE_PATH_LITERAL("content_script.js"),
                     R"(
      (() => {
        const documentUrl = document.location.toString();
        chrome.test.sendMessage(documentUrl);
      })();
      )");

  ASSERT_TRUE(LoadExtension(test_dir.UnpackedPath()));

  const std::string uuid_html_url =
      "uuid-in-package:65c6f241-f6b5-4302-9f95-9a826c4dda1c";
  web_package::WebBundleBuilder builder;
  builder.AddExchange(uuid_html_url,
                      {{":status", "200"}, {"content-type", "text/html"}},
                      "<script>console.error('hoge');</script>");
  std::vector<uint8_t> bundle = builder.CreateBundle();
  const std::string web_bundle = std::string(bundle.begin(), bundle.end());

  // For serving web bundles, "Content-Type: application/webbundle" and
  // "X-Content-Type-Options: nosniff" response headers are required.
  // https://wicg.github.io/webpackage/draft-yasskin-wpack-bundled-exchanges.html#name-serving-constraints
  RegisterRequestHandler("/test.wbn", "application/webbundle", web_bundle,
                         true /* nosniff */);

  const std::string page_html = base::StringPrintf(R"(
        <script type="webbundle">
        {
          "source": "./test.wbn",
          "scopes": ["uuid-in-package:"]
        }
        </script>
        <iframe src="%s"></iframe>
      )",
                                                   uuid_html_url.c_str());
  RegisterRequestHandler("/test.html", "text/html", page_html,
                         false /* nosniff */);
  ASSERT_TRUE(StartEmbeddedTestServer());

  ExtensionTestMessageListener listener;

  GURL page_url = embedded_test_server()->GetURL("/test.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));
  ASSERT_TRUE(listener.WaitUntilSatisfied());
  EXPECT_EQ(uuid_html_url, listener.message());
}

class ContentScriptApiPrerenderingTest : public ContentScriptApiTest {
 public:
  ContentScriptApiPrerenderingTest() {
    feature_list_.InitWithFeatures(
        {
            network::features::kPrerender2ContentSecurityPolicyExtensions,
            extensions_features::kMinimumMV3CSPWithInlineSpeculationRules,
        },
        {});
  }

 private:
  content::test::ScopedPrerenderFeatureList prerender_feature_list_;
  base::test::ScopedFeatureList feature_list_;
};

// TODO(crbug.com/1344548): Re-enable this test
IN_PROC_BROWSER_TEST_F(ContentScriptApiPrerenderingTest,
                       DISABLED_Prerendering) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("content_scripts/prerendering")) << message_;
}

// Checks if injecting inline speculation rules are permitted in the manifest v3
// content_scripts.
IN_PROC_BROWSER_TEST_F(ContentScriptApiPrerenderingTest, SpeculationRules) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("content_scripts/speculation_rules"))
      << message_;
}

class ContentScriptApiFencedFrameTest : public ContentScriptApiTest {
 protected:
  ContentScriptApiFencedFrameTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kFencedFrames, {{"implementation_type", "mparch"}}},
         {features::kPrivacySandboxAdsAPIsOverride, {}}},
        {/* disabled_features */});
    UseHttpsTestServer();
  }
  ~ContentScriptApiFencedFrameTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Inject two extensions with matching rules. Only the extension
// that matches the outermost extension's content_scripts should
// get injected.
// The documentIdle extension should execute (sending 'done').
// The documentStart extension should not-execute (sending 'fail') since it
// isn't the parent extension of the fenced frame.
IN_PROC_BROWSER_TEST_F(ContentScriptApiFencedFrameTest,
                       InjectionMatchesCorrectExtension) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  const char kDocumentIdleExtensionManifest[] =
      R"MANIFEST({
        "name": "Document Idle Extesnsion",
        "version": "0.1",
        "manifest_version": 3,
        "content_scripts": [{
          "matches": ["https://*/fenced_frames/title1.html"],
          "js": ["script.js"],
          "run_at": "document_idle",
          "all_frames": true
        }]
      })MANIFEST";

  const char kDocumentStartExtensionManifest[] =
      R"MANIFEST({
        "name": "Document Start extension",
        "version": "0.1",
        "manifest_version": 3,
        "content_scripts": [{
          "matches": ["https://*/fenced_frames/title1.html"],
          "js": ["script.js"],
          "run_at": "document_start",
          "all_frames": true
        }]
      })MANIFEST";

  GURL fenced_frame_url =
      embedded_test_server()->GetURL("a.test", "/fenced_frames/title1.html");

  const char kFencedFrameHtml[] =
      R"HTML(<html>Fenced Frame Test!<fencedframe src="%s">
          </fencedframe></html>)HTML";

  TestExtensionDir document_idle_extension_dir;
  document_idle_extension_dir.WriteManifest(kDocumentIdleExtensionManifest);
  document_idle_extension_dir.WriteFile(
      FILE_PATH_LITERAL("test.html"),
      base::StringPrintf(kFencedFrameHtml, fenced_frame_url.spec().c_str()));
  document_idle_extension_dir.WriteFile(FILE_PATH_LITERAL("script.js"),
                                        kNonBlockingScript);
  const Extension* extension =
      LoadExtension(document_idle_extension_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  TestExtensionDir document_start_extension_dir;
  const char kFailureScript[] = "chrome.test.sendMessage('fail');";

  document_start_extension_dir.WriteManifest(kDocumentStartExtensionManifest);
  document_start_extension_dir.WriteFile(FILE_PATH_LITERAL("script.js"),
                                         kFailureScript);

  ASSERT_TRUE(LoadExtension(document_start_extension_dir.UnpackedPath()));

  ExtensionTestMessageListener listener;
  content::WebContents* tab_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL extension_test_url = extension->GetResourceURL("test.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), extension_test_url));

  EXPECT_EQ(extension_test_url,
            tab_contents->GetPrimaryMainFrame()->GetLastCommittedURL());
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  EXPECT_EQ("done", listener.message());
}

}  // namespace extensions
