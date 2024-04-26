// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/permissions/permissions_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_management_test_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_with_management_policy_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ssl/https_upgrades_interceptor.h"
#include "chrome/browser/ssl/https_upgrades_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/search/ntp_test_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/javascript_dialogs/tab_modal_dialog_manager.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/script_injection_tracker.h"
#include "extensions/common/api/content_scripts.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/externally_connectable.h"
#include "extensions/common/utils/content_script_utils.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "pdf/buildflags.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_PDF)
#include "chrome/browser/pdf/pdf_extension_test_util.h"
#include "pdf/pdf_features.h"
#endif  // BUILDFLAG(ENABLE_PDF)

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
  // before this method returns (as content::ExecJs() is synchronous).
  if (!content::ExecJs(web_contents, "1 == 1;")) {
    return false;
  }
  base::RunLoop().RunUntilIdle();
  return true;
}

// A simple extension manifest with content scripts on all pages.
constexpr char kManifest[] =
    R"({
         "name": "%s",
         "version": "1.0",
         "manifest_version": 2,
         "content_scripts": [{
           "matches": ["*://*/*"],
           "js": ["script.js"],
           "run_at": "%s"
         }]
       })";

// A (blocking) content script that pops up an alert.
constexpr char kBlockingScript[] = "alert('ALERT');";

// A (non-blocking) content script that sends a message.
constexpr char kNonBlockingScript[] = "chrome.test.sendMessage('done');";

constexpr char kNewTabOverrideManifest[] =
    R"({
         "name": "New tab override",
         "version": "0.1",
         "manifest_version": 2,
         "description": "Foo!",
         "chrome_url_overrides": {"newtab": "newtab.html"}
       })";

constexpr char kNewTabHtml[] = "<html>NewTabOverride!</html>";

}  // namespace

using ContextType = ExtensionBrowserTest::ContextType;

class ContentScriptApiTest : public ExtensionApiTest {
 public:
  explicit ContentScriptApiTest(ContextType context_type = ContextType::kNone)
      : ExtensionApiTest(context_type) {}

  ContentScriptApiTest(const ContentScriptApiTest&) = delete;
  ContentScriptApiTest& operator=(const ContentScriptApiTest&) = delete;

  ~ContentScriptApiTest() override {}

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");

    // Serve valid HTTPS from test server.
    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
    https_test_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_test_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
    https_test_server_->ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(https_test_server_->Start());

    HttpsUpgradesInterceptor::SetHttpsPortForTesting(
        https_test_server_->port());
    HttpsUpgradesInterceptor::SetHttpPortForTesting(
        embedded_test_server()->port());

    // Test extensions use these hostnames. Allow them to be loaded over
    // HTTP so that HTTPS-Upgrades feature doesn't upgrade their URLs.
    // TODO(crbug.com/40248833): Use https in these tests and remove these
    // allowlist entries.
    AllowHttpForHostnamesForTesting(
        {"a.com", "b.com", "default.test", "bar.com", "path-test.example",
         "example.com", "chromium.org", "example1.com"},
        browser()->profile()->GetPrefs());
  }

  void TearDownOnMainThread() override {
    ClearHttpAllowlistForHostnamesForTesting(browser()->profile()->GetPrefs());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    mock_cert_verifier_.SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    ExtensionApiTest::TearDownInProcessBrowserTestFixture();
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

 private:
  content::ContentMockCertVerifier mock_cert_verifier_;
  std::unique_ptr<net::EmbeddedTestServer> https_test_server_;
};

class ContentScriptApiTestWithContextType
    : public ContentScriptApiTest,
      public testing::WithParamInterface<ContextType> {
 public:
  ContentScriptApiTestWithContextType() : ContentScriptApiTest(GetParam()) {}

  ContentScriptApiTestWithContextType(
      const ContentScriptApiTestWithContextType&) = delete;
  ContentScriptApiTestWithContextType& operator=(
      const ContentScriptApiTestWithContextType&) = delete;
};

INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         ContentScriptApiTestWithContextType,
                         ::testing::Values(ContextType::kPersistentBackground));
// These tests use chrome.tabs.executeScript, which is not available in MV3 and
// above.
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         ContentScriptApiTestWithContextType,
                         ::testing::Values(ContextType::kServiceWorkerMV2));

IN_PROC_BROWSER_TEST_P(ContentScriptApiTestWithContextType, AllFrames) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("content_scripts/all_frames")) << message_;
}

IN_PROC_BROWSER_TEST_P(ContentScriptApiTestWithContextType, AboutBlankIframes) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("content_scripts/about_blank_iframes"))
      << message_;
}

IN_PROC_BROWSER_TEST_P(ContentScriptApiTestWithContextType,
                       AboutBlankAndSrcdoc) {
  // The optional "*://*/*" permission is requested after verifying that
  // content script insertion solely depends on content_scripts[*].matches.
  // The permission is needed for chrome.tabs.executeScript tests.
  auto dialog_action_reset =
      PermissionsRequestFunction::SetDialogActionForTests(
          PermissionsRequestFunction::DialogAction::kAutoConfirm);
  PermissionsRequestFunction::SetIgnoreUserGestureForTests(true);

  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("content_scripts/about_blank_srcdoc"))
      << message_;
}

IN_PROC_BROWSER_TEST_P(ContentScriptApiTestWithContextType, ExtensionIframe) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("content_scripts/extension_iframe")) << message_;
}

// TODO(crbug.com/40934824): Very flaky on multiple platforms.
IN_PROC_BROWSER_TEST_F(ContentScriptApiTest,
                       DISABLED_ContentScriptExtensionProcess) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("content_scripts/extension_process"))
      << message_;
}

IN_PROC_BROWSER_TEST_P(ContentScriptApiTestWithContextType,
                       FragmentNavigation) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  const char extension_name[] = "content_scripts/fragment";
  ASSERT_TRUE(RunExtensionTest(extension_name)) << message_;
}

IN_PROC_BROWSER_TEST_P(ContentScriptApiTestWithContextType, IsolatedWorlds) {
  // This extension runs various bits of script and tests that they all run in
  // the same isolated world.
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("content_scripts/isolated_world1")) << message_;

  // Now load a different extension, inject into same page, verify worlds aren't
  // shared.
  ASSERT_TRUE(RunExtensionTest("content_scripts/isolated_world2")) << message_;
}

IN_PROC_BROWSER_TEST_P(ContentScriptApiTestWithContextType,
                       IgnoreHostPermissions) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("content_scripts/dont_match_host_permissions"))
      << message_;
}

// crbug.com/39249 -- content scripts js should not run on view source.
IN_PROC_BROWSER_TEST_P(ContentScriptApiTestWithContextType, ViewSource) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("content_scripts/view_source")) << message_;
}

// crbug.com/126257 -- content scripts should not get injected into other
// extensions.
// TODO(crbug.com/40759559): Fix flakiness.
IN_PROC_BROWSER_TEST_P(ContentScriptApiTestWithContextType,
                       DISABLED_OtherExtensions) {
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
// TODO(crbug.com/40876652): This test can't run using a service worker-based
// extension.
IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, BlobFetch) {
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
  ASSERT_EQ(true, content::EvalJs(
                      browser()->tab_strip_model()->GetActiveWebContents(),
                      "document.getElementsByClassName('injected-once')"
                      ".length == 1"));

  // Test that a script injected at two different load process times, document
  // idle and document end, is injected exactly twice.
  ASSERT_EQ(true, content::EvalJs(
                      browser()->tab_strip_model()->GetActiveWebContents(),
                      "document.getElementsByClassName('injected-twice')"
                      ".length == 2"));
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
  EXPECT_EQ(true, content::EvalJs(
                      browser()->tab_strip_model()->GetActiveWebContents(),
                      "document.getElementById('injected') === null"));

  // `detach.js` is evaluated, and detaches the iframe.
  EXPECT_EQ(true, content::EvalJs(
                      browser()->tab_strip_model()->GetActiveWebContents(),
                      "document.getElementById('detach-evaluated') !== null"));

  // `detach2.js` isn't evaluated because the iframe is detached.
  EXPECT_EQ(
      false,
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      "document.getElementById('detach2-evaluated') !== null"));
}

// Tests that fetches made by content scripts are exempt from the page's CSP.
// Regression test for crbug.com/934819.
IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, FetchExemptFromCSP) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Create and load an extension that will inject a content script which does a
  // fetch based on the host document's "fetchUrl" url search parameter.
  static constexpr char kFetchManifest[] =
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

  static constexpr char kContentScript[] = R"(
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

// Test that content scripts that exceed the individual script size limit or the
// total extensions script limit will not be loaded/injected, and will generate
// an install warning.
IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, LargeScriptFilesNotLoaded) {
  auto single_scripts_limit_reset =
      script_parsing::CreateScopedMaxScriptLengthForTesting(800u);
  auto extension_scripts_limit_reset =
      script_parsing::CreateScopedMaxScriptsLengthPerExtensionForTesting(1000u);
  ASSERT_TRUE(StartEmbeddedTestServer());

  ResultCatcher result_catcher;
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("content_scripts/large_scripts"),
                    {.ignore_manifest_warnings = true});
  ASSERT_TRUE(extension);
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();

  std::vector<InstallWarning> expected_warnings;
  expected_warnings.emplace_back(
      l10n_util::GetStringFUTF8(IDS_EXTENSION_CONTENT_SCRIPT_FILE_TOO_LARGE,
                                u"big.js"),
      api::content_scripts::ManifestKeys::kContentScripts, "big.js");
  expected_warnings.emplace_back(
      l10n_util::GetStringFUTF8(IDS_EXTENSION_CONTENT_SCRIPT_FILE_TOO_LARGE,
                                u"inject_element_2.js"),
      api::content_scripts::ManifestKeys::kContentScripts,
      "inject_element_2.js");

  EXPECT_EQ(extension->install_warnings(), expected_warnings);
}

IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, MainWorldInjections) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("content_scripts/main_world_injections"))
      << message_;
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
    constexpr char kGetColor[] =
        R"((function() {
             let element = document.querySelector('%s');
             style = window.getComputedStyle(element);
             return style.backgroundColor;
            })();)";
    return content::EvalJs(get_active_tab(),
                           base::StringPrintf(kGetColor, query_selector))
        .ExtractString();
  };
  // Returns the number of stylesheets attached to the document.
  auto get_style_sheet_count = [&get_active_tab]() {
    constexpr char kGetStyleSheetCount[] = "document.styleSheets.length;";
    return content::EvalJs(get_active_tab(), kGetStyleSheetCount).ExtractInt();
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
           return new Promise(resolve => {
             sheet.onload = () => { resolve('success'); };
             sheet.onerror = () => { resolve('error'); };
             document.head.appendChild(sheet);
           });
         })();)";
  EXPECT_EQ("success", content::EvalJs(get_active_tab(), kLoadExtraStylesheet));
  EXPECT_EQ(kInjectedDivColor, get_element_color("#div3"));
}

IN_PROC_BROWSER_TEST_P(ContentScriptApiTestWithContextType,
                       ContentScriptCSSLocalization) {
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
      ui_test_utils::BROWSER_TEST_NO_WAIT);
  EXPECT_TRUE(catcher.GetNextResult());
}

IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, ContentScriptPermissionsApi) {
  base::AutoReset<PermissionsRequestFunction::DialogAction> dialog_action =
      PermissionsRequestFunction::SetDialogActionForTests(
          PermissionsRequestFunction::DialogAction::kAutoConfirm);
  extensions::PermissionsRequestFunction::SetIgnoreUserGestureForTests(true);
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("content_scripts/permissions")) << message_;
}

// TODO(crbug.com/40698663): Maybe push the ContextType into
// ExtensionApiTestWithManagementPolicy depending on how the conversions
// with other derived classes go. Currently, web_request_apitest.cc has a
// similar class.
class ContentScriptApiManagementPolicyTestWithContextType
    : public ExtensionApiTestWithManagementPolicy,
      public testing::WithParamInterface<ContextType> {
 public:
  ContentScriptApiManagementPolicyTestWithContextType()
      : ExtensionApiTestWithManagementPolicy(GetParam()) {}
  ~ContentScriptApiManagementPolicyTestWithContextType() override = default;
  ContentScriptApiManagementPolicyTestWithContextType(
      const ContentScriptApiManagementPolicyTestWithContextType&) = delete;
  ContentScriptApiManagementPolicyTestWithContextType& operator=(
      const ContentScriptApiManagementPolicyTestWithContextType&) = delete;
};

INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         ContentScriptApiManagementPolicyTestWithContextType,
                         ::testing::Values(ContextType::kPersistentBackground));
// These tests use chrome.tabs.executeScript, which is not available in MV3 and
// above.
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         ContentScriptApiManagementPolicyTestWithContextType,
                         ::testing::Values(ContextType::kServiceWorkerMV2));

IN_PROC_BROWSER_TEST_P(ContentScriptApiManagementPolicyTestWithContextType,
                       Policy) {
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
IN_PROC_BROWSER_TEST_P(ContentScriptApiManagementPolicyTestWithContextType,
                       PolicyWildcard) {
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
  // TODO(crbug.com/40698663): This test should be run using a service worker-
  // based extension, but we have no mechanism for doing that with a packed
  // extension.
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

IN_PROC_BROWSER_TEST_P(ContentScriptApiTestWithContextType, BypassPageCSP) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  extensions::ResultCatcher catcher;
  ASSERT_TRUE(RunExtensionTest("content_scripts/bypass_page_csp"))
      << catcher.message();
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_P(ContentScriptApiTestWithContextType,
                       BypassPageTrustedTypes) {
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
      WindowOpenDisposition::CURRENT_TAB, ui_test_utils::BROWSER_TEST_NO_WAIT);

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
      WindowOpenDisposition::CURRENT_TAB, ui_test_utils::BROWSER_TEST_NO_WAIT);

  // Now, instead of closing the dialog, just close the tab. Later scripts
  // should never get a chance to run (and we shouldn't crash).
  dialog_wait.Run();
  EXPECT_FALSE(listener.was_satisfied());
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  browser()->tab_strip_model()->CloseWebContentsAt(
      browser()->tab_strip_model()->active_index(), 0);
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
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
      WindowOpenDisposition::CURRENT_TAB, ui_test_utils::BROWSER_TEST_NO_WAIT);

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

IN_PROC_BROWSER_TEST_P(ContentScriptApiTestWithContextType,
                       CannotScriptTheNewTabPage) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  ExtensionTestMessageListener test_listener("ready",
                                             ReplyBehavior::kWillReply);
  LoadExtension(test_data_dir_.AppendASCII("content_scripts/ntp"));
  ASSERT_TRUE(test_listener.WaitUntilSatisfied());

  auto did_script_inject = [](content::WebContents* web_contents) {
    return content::EvalJs(web_contents, "document.title === 'injected';")
        .ExtractBool();
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

  // Test on an HTTP URL. HTTPS upgrades is disabled on example1.com so it
  // loads over http instead of https. example2.com loads over https.
  GURL unprotected_url1 = embedded_test_server()->GetURL(
      "example1.com", "/extensions/test_file.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), unprotected_url1));
  EXPECT_TRUE(
      did_script_inject(browser()->tab_strip_model()->GetActiveWebContents()));

  // Test on an HTTPS URL. If HTTPS-Upgrades feature is enabled, this URL is
  // upgraded to HTTPS.
  GURL unprotected_url2 = embedded_test_server()->GetURL(
      "example2.com", "/extensions/test_file.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), unprotected_url2));
  EXPECT_TRUE(
      did_script_inject(browser()->tab_strip_model()->GetActiveWebContents()));
}

IN_PROC_BROWSER_TEST_P(ContentScriptApiTestWithContextType, SameSiteCookies) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("content_scripts/request_cookies"));
  ASSERT_TRUE(extension);
  GURL url = embedded_test_server()->GetURL("a.com", "/extensions/body1.html");
  ResultCatcher catcher;
  static constexpr char kScript[] =
      R"(chrome.tabs.create({url: '%s'}, () => {
           let message = 'success';
           if (chrome.runtime.lastError)
             message = chrome.runtime.lastError.message;
           chrome.test.sendScriptResult(message);
         });)";
  base::Value result = ExecuteScriptInBackgroundPage(
      extension->id(), base::StringPrintf(kScript, url.spec().c_str()));

  EXPECT_EQ("success", result);
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_P(ContentScriptApiTestWithContextType,
                       ExecuteScriptFileSameSiteCookies) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("content_scripts/request_cookies"));
  ASSERT_TRUE(extension);
  GURL url = embedded_test_server()->GetURL("b.com", "/extensions/body1.html");
  ResultCatcher catcher;
  static constexpr char kScript[] =
      R"(chrome.tabs.create({url: '%s'}, (tab) => {
           if (chrome.runtime.lastError) {
             chrome.test.sendScriptResult(chrome.runtime.lastError.message);
             return;
           }
           chrome.tabs.executeScript(tab.id, {file: 'cookies.js'}, () => {
             let message = 'success';
             if (chrome.runtime.lastError)
               message = chrome.runtime.lastError.message;
             chrome.test.sendScriptResult(message);
           });
         });)";
  base::Value result = ExecuteScriptInBackgroundPage(
      extension->id(), base::StringPrintf(kScript, url.spec().c_str()));

  EXPECT_EQ("success", result);
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_P(ContentScriptApiTestWithContextType,
                       ExecuteScriptCodeSameSiteCookies) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("content_scripts/request_cookies"));
  ASSERT_TRUE(extension);
  GURL url = embedded_test_server()->GetURL("b.com", "/extensions/body1.html");
  ResultCatcher catcher;
  static constexpr char kScript[] =
      R"(chrome.tabs.create({url: '%s'}, (tab) => {
           if (chrome.runtime.lastError) {
             chrome.test.sendScriptResult(chrome.runtime.lastError.message);
             return;
           }
           fetch(chrome.runtime.getURL('cookies.js')).then((response) => {
             return response.text();
           }).then((text) => {
             chrome.tabs.executeScript(tab.id, {code: text}, () => {
               let message = 'success';
               if (chrome.runtime.lastError)
                 message = chrome.runtime.lastError.message;
               chrome.test.sendScriptResult(message);
             });
           }).catch((e) => {
             chrome.test.sendScriptResult(e);
           });
         });)";
  base::Value result = ExecuteScriptInBackgroundPage(
      extension->id(), base::StringPrintf(kScript, url.spec().c_str()));

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

// Regression test for https://crbug.com/1407986.
IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, ExecuteScriptForSandboxFrame) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  TestExtensionDir test_dir;
  test_dir.WriteManifest(
      R"({
           "name": "Execute Script Sandbox CSP",
           "description": "Execute scripts should work for CSP sandbox.",
           "version": "0.1",
           "manifest_version": 2,
           "permissions": ["tabs","activeTab","http://*/*","https://*/*"],
           "background": {
            "scripts": [
              "script.js"
            ]}
          })");

  test_dir.WriteFile(FILE_PATH_LITERAL("script.js"),
                     R"(
chrome.tabs.onUpdated.addListener(function(tabId, changeInfo, tab) {
  if (changeInfo.status === "complete" && tab.url) {
    chrome.tabs.executeScript(
      tabId,
      { code: 'var x = 1;' },
      () => {
        let lastError = chrome.runtime.lastError;
        if (lastError) {
          chrome.test.notifyFail(lastError.message);
        } else {
          chrome.test.notifyPass();
      }
    });
  }
});)");

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
IN_PROC_BROWSER_TEST_P(ContentScriptApiTestWithContextType, Messaging) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "content_scripts/other_extensions/message_echoer_allows_by_default")));
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "content_scripts/other_extensions/message_echoer_allows")));
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII(
          "content_scripts/other_extensions/message_echoer_denies"),
      {.ignore_manifest_warnings = true});
  ASSERT_TRUE(extension);
  std::vector<InstallWarning> expected_warnings;
  expected_warnings.emplace_back(
      manifest_errors::kManifestV2IsDeprecatedWarning);
  expected_warnings.emplace_back(
      externally_connectable_errors::kErrorNothingSpecified);
  EXPECT_EQ(extension->install_warnings(), expected_warnings);
  ASSERT_TRUE(RunExtensionTest("content_scripts/messaging")) << message_;
}

// Tests that the URLs of content scripts are set to the extension URL
// (chrome-extension://<id>/<path_to_script>) rather than the local file
// path.
// Regression test for https://crbug.com/714617.
IN_PROC_BROWSER_TEST_P(ContentScriptApiTestWithContextType, ContentScriptUrls) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  TestExtensionDir test_dir;
  test_dir.WriteManifest(
      R"({
           "name": "Content Script",
           "manifest_version": 2,
           "version": "0.1",
           "background": {
              "scripts": ["background.js"],
              "persistent": true
           },
           "content_scripts": [{
             "matches": ["*://content-script.example/*"],
             "js": ["content_script.js"]
           }],
           "permissions": ["*://*/*"]
         })");
  static constexpr char kContentScriptSrc[] =
      R"(console.error('TestMessage');
         chrome.test.notifyPass();)";
  test_dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), kContentScriptSrc);
  static constexpr char kBackgroundScriptSrc[] =
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

// Regression test for https://crbug.com/1449796 - verifying that the IPC
// verification doesn't incorrectly think that an IPC from a content script
// running in an MHTML frame is malicious (in this scenario the `source_url`
// field of the IPC may be a bit unusual and doesn't necessarily match the
// process lock).
IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, MhtmlIframe) {
  // Install a test extension.
  TestExtensionDir dir;
  const char kManifestTemplate[] = R"(
      {
        "name": "ScriptInjectionTrackerBrowserTest - Declarative",
        "version": "1.0",
        "manifest_version": 3,
        "host_permissions": ["http://foo.com/*", "file://*"],
        "content_scripts": [{
          "all_frames": true,
          "match_about_blank": true,
          "matches": ["http://foo.com/*", "file://*"],
          "js": ["content_script.js"]
        }],
        "background": {"service_worker": "background_script.js"}
      } )";
  const char kBackgroundScript[] = R"(
      chrome.runtime.onMessage.addListener(
        function(request, sender, sendResponse) {
          chrome.test.sendMessage("Got message from " + sender.url);
        }
      );
  )";
  const char kContentScript[] = R"(
      message = "Hello from frame at url = " + window.location.href;
      console.log(message);
      chrome.runtime.sendMessage({greeting: message});
  )";
  dir.WriteManifest(kManifestTemplate);
  dir.WriteFile(FILE_PATH_LITERAL("background_script.js"), kBackgroundScript);
  dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), kContentScript);
  const Extension* extension = LoadExtension(dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Navigate to a MHTML *file* that pretends to host a nested *http* subframe
  // (as well as a *cid* subframe).
  const GURL kExpectedFrame1Url = GURL("http://foo.com/frame_0.html");
  const GURL kExpectedFrame2Url = GURL("cid:frame1@foo.bar");
  ExtensionTestMessageListener listener1(base::StringPrintf(
      "Got message from %s", kExpectedFrame1Url.spec().c_str()));
  ExtensionTestMessageListener listener2(base::StringPrintf(
      "Got message from %s", kExpectedFrame2Url.spec().c_str()));
  GURL page_url = ui_test_utils::GetTestUrl(
      base::FilePath(FILE_PATH_LITERAL("extensions")),
      base::FilePath(FILE_PATH_LITERAL("mhtml-with-subframes.mht")));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));

  // Verify that the subframes are at the expected URLs:
  // * Not `file:` URLs - the URLs come from inside MHTML,
  // * URLs will match the URLs patterns from the extension manifest above.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* subframe1 = content::ChildFrameAt(web_contents, 0);
  ASSERT_TRUE(subframe1);
  EXPECT_EQ(subframe1->GetLastCommittedURL(), kExpectedFrame1Url);
  content::RenderFrameHost* subframe2 = content::ChildFrameAt(web_contents, 1);
  ASSERT_TRUE(subframe2);
  EXPECT_EQ(subframe2->GetLastCommittedURL(), kExpectedFrame2Url);

  // Verify that the content scripts have been injected.  Content script
  // injection is important even in somewhat exotic scenarios such as here
  // (MHTML frames normally don't execute any scripts), because it is important
  // that some extensions (such as accessbility aids) are able to inject content
  // scripts into all frames.
  //
  // Note that `<all_urls>` doesn't cover `cid:` subframes, so we don't wait for
  // `listener2`.
  //
  // Since `chrome.test.sendMessage` happens *after*
  // `chrome.runtime.sendMessage` this is sufficient for verifying that the IPC
  // handler didn't terminate the renderer process.
  ASSERT_TRUE(listener1.WaitUntilSatisfied());
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
    EXPECT_TRUE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
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
  EXPECT_TRUE(content::ExecJs(
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
  EXPECT_TRUE(content::ExecJs(navigating_host, script));
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
  child_frame = content::ChildFrameAt(tab->GetPrimaryMainFrame(), 0);

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

#if BUILDFLAG(ENABLE_PDF)
// A test suite for exercising the behavior of content script injection into
// PDF-related frames.
class ContentScriptRelatedPdfFrameTest : public ContentScriptRelatedFrameTest {
 public:
  ContentScriptRelatedPdfFrameTest() {
    feature_list_.InitAndEnableFeature(chrome_pdf::features::kPdfOopif);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test that content scripts can execute in the PDF embedder frame, but not in
// the PDF extension frame nor PDF content frame.
IN_PROC_BROWSER_TEST_F(ContentScriptRelatedPdfFrameTest, PdfFrames) {
  // Navigate to a full-page PDF.
  content::WebContents* tab = NavigateTab(
      embedded_test_server()->GetURL("example.com", "/pdf/test.pdf"));
  content::RenderFrameHost* primary_main_frame = tab->GetPrimaryMainFrame();

  // Wait until the PDF finishes loading.
  ASSERT_TRUE(pdf_extension_test_util::EnsurePDFHasLoaded(primary_main_frame));

  // The content script should run in the PDF embedder frame.
  EXPECT_TRUE(DidScriptRunInFrame(primary_main_frame));

  // The content script shouldn't run in the PDF extension frame.
  content::RenderFrameHost* extension_host =
      pdf_extension_test_util::GetOnlyPdfExtensionHost(tab);
  ASSERT_TRUE(extension_host);
  EXPECT_FALSE(DidScriptRunInFrame(extension_host));

  // The content script shouldn't run in the PDF content frame.
  content::RenderFrameHost* content_host =
      pdf_extension_test_util::GetOnlyPdfPluginFrame(tab);
  ASSERT_TRUE(content_host);
  EXPECT_FALSE(DidScriptRunInFrame(content_host));
}
#endif  // BUILDFLAG(ENABLE_PDF)

class ContentScriptMatchOriginAsFallbackTest
    : public ContentScriptRelatedFrameTest {
 public:
  ContentScriptMatchOriginAsFallbackTest() = default;
  ~ContentScriptMatchOriginAsFallbackTest() override = default;

  bool IncludeMatchOriginAsFallback() override { return true; }
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
class NTPInterceptionTest : public ExtensionApiTest,
                            public testing::WithParamInterface<ContextType> {
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

 private:
  net::EmbeddedTestServer https_test_server_;
};

INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         NTPInterceptionTest,
                         ::testing::Values(ContextType::kPersistentBackground));
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         NTPInterceptionTest,
                         ::testing::Values(ContextType::kServiceWorker));

// Ensure extensions can't inject a content script into the New Tab page.
// Regression test for crbug.com/844428.
IN_PROC_BROWSER_TEST_P(NTPInterceptionTest, ContentScript) {
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

  EXPECT_EQ(false, EvalJs(web_contents, "document.title !== 'Fake NTP';"));
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

class ContentScriptApiPrerenderingTest
    : public ContentScriptApiTestWithContextType {
 private:
  content::test::ScopedPrerenderFeatureList prerender_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         ContentScriptApiPrerenderingTest,
                         ::testing::Values(ContextType::kPersistentBackground));
// These tests use chrome.tabs.executeScript, which is not available in MV3 and
// above. See crbug.com/332328868.
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         ContentScriptApiPrerenderingTest,
                         ::testing::Values(ContextType::kServiceWorkerMV2));

IN_PROC_BROWSER_TEST_P(ContentScriptApiPrerenderingTest, Prerendering) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("content_scripts/prerendering")) << message_;
}

// This test is MV3-only, so it already runs using a service worker-based
// extension.
using ContentScriptApiPrerenderingMV3Test = ContentScriptApiPrerenderingTest;
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         ContentScriptApiPrerenderingMV3Test,
                         ::testing::Values(ContextType::kNone));

// Checks if injecting inline speculation rules are permitted in the manifest v3
// content_scripts.
IN_PROC_BROWSER_TEST_P(ContentScriptApiPrerenderingMV3Test, SpeculationRules) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("content_scripts/speculation_rules"))
      << message_;
}

class ContentScriptApiFencedFrameTest : public ContentScriptApiTest {
 protected:
  ContentScriptApiFencedFrameTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kFencedFrames, {{"implementation_type", "mparch"}}},
         {features::kPrivacySandboxAdsAPIsOverride, {}},
         {blink::features::kFencedFramesAPIChanges, {}},
         {blink::features::kFencedFramesDefaultMode, {}}},
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

  TestExtensionDir document_idle_extension_dir;
  document_idle_extension_dir.WriteManifest(kDocumentIdleExtensionManifest);

  document_idle_extension_dir.WriteFile(FILE_PATH_LITERAL("test.html"), R"HTML(
    <html>
      Fenced Frame Test!
      <fencedframe></fencedframe>
      <script src="navigation.js"></script>
    </html>
  )HTML");

  document_idle_extension_dir.WriteFile(
      FILE_PATH_LITERAL("navigation.js"),
      content::JsReplace(
          "const fencedframe = document.querySelector('fencedframe');"
          "fencedframe.config = new FencedFrameConfig($1);",
          fenced_frame_url));

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

class ContentScriptApiTestWithActivityLog : public ContentScriptApiTest {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnableExtensionActivityLogging);
    ContentScriptApiTest::SetUpCommandLine(command_line);
  }
};

// Tests Activity Log for content script executions.
// Regression test for https://crbug.com/1519380.
IN_PROC_BROWSER_TEST_F(ContentScriptApiTestWithActivityLog,
                       ActivityLogRecorded) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Load an extension that injects content scripts.
  base::FilePath data_dir = test_data_dir_.AppendASCII("content_scripts");
  const Extension* extension =
      LoadExtension(data_dir.AppendASCII("script_a_com"));
  ASSERT_TRUE(extension);

  // Navigate to a page where content scripts would be executed.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("a.com", "/extensions/test_file.html")));

  // Execute the test which passes when it sees exactly 1 content_script entry
  // in the activity log.
  ASSERT_TRUE(RunExtensionTest("content_scripts/activity_log/"));
}

}  // namespace extensions
