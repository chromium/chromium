// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_action_runner.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/launch_service/launch_service.h"
#include "chrome/browser/extensions/api/tabs/tabs_api.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/metrics/subprocess_metrics_provider.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/network_service_util.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/url_loader_factory_manager.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "services/network/cross_origin_read_blocking.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {

using CORBAction = network::CrossOriginReadBlocking::Action;

class CrossOriginReadBlockingExtensionTest : public ExtensionBrowserTest {
 public:
  CrossOriginReadBlockingExtensionTest() = default;

  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(embedded_test_server());
  }

 protected:
  content::WebContents* active_web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  const Extension* InstallExtension(
      GURL resource_to_fetch_from_declarative_content_script = GURL()) {
    bool use_declarative_content_script =
        resource_to_fetch_from_declarative_content_script.is_valid();
    const char kContentScriptManifestEntry[] = R"(
          "content_scripts": [{
            "all_frames": true,
            "match_about_blank": true,
            "matches": ["*://fetch-initiator.com/*"],
            "js": ["content_script.js"]
          }],
    )";

    const char kManifestTemplate[] = R"(
        {
          "name": "CrossOriginReadBlockingTest - Extension",
          "version": "1.0",
          "manifest_version": 2,
          "permissions": [
              "tabs",
              "*://fetch-initiator.com/*",
              "*://127.0.0.1/*",  // Initiator in AppCache tests.
              "*://cross-site.com/*",
              "*://other-with-permission.com/*"
              // This list intentionally does NOT include
              // other-without-permission.com.
          ],
          %s
          "background": {"scripts": ["background_script.js"]}
        } )";
    dir_.WriteManifest(base::StringPrintf(
        kManifestTemplate,
        use_declarative_content_script ? kContentScriptManifestEntry : ""));

    dir_.WriteFile(FILE_PATH_LITERAL("background_script.js"), "");
    dir_.WriteFile(FILE_PATH_LITERAL("page.html"), "<body>Hello World!</body>");

    if (use_declarative_content_script) {
      dir_.WriteFile(
          FILE_PATH_LITERAL("content_script.js"),
          CreateFetchScript(resource_to_fetch_from_declarative_content_script));
    }
    extension_ = LoadExtension(dir_.UnpackedPath());
    return extension_;
  }

  const Extension* InstallApp() {
    const char kManifest[] = R"(
        {
          "name": "CrossOriginReadBlockingTest - App",
          "version": "1.0",
          "manifest_version": 2,
          "permissions": ["*://*/*", "webview"],
          "app": {
            "background": {
              "scripts": ["background_script.js"]
            }
          }
        } )";
    dir_.WriteManifest(kManifest);

    const char kBackgroungScript[] = R"(
        chrome.app.runtime.onLaunched.addListener(function() {
          chrome.app.window.create('page.html', {}, function () {});
        });
    )";
    dir_.WriteFile(FILE_PATH_LITERAL("background_script.js"),
                   kBackgroungScript);

    const char kPage[] = R"(
        <div id="webview-tag-container"></div>
    )";
    dir_.WriteFile(FILE_PATH_LITERAL("page.html"), kPage);

    extension_ = LoadExtension(dir_.UnpackedPath());
    return extension_;
  }

  bool RegisterServiceWorkerForExtension(
      const std::string& service_worker_script) {
    const char kServiceWorkerPath[] = "service_worker.js";
    dir_.WriteFile(base::FilePath::FromUTF8Unsafe(kServiceWorkerPath).value(),
                   service_worker_script);

    const char kRegistrationScript[] = R"(
        navigator.serviceWorker.register($1).then(function() {
          // Wait until the service worker is active.
          return navigator.serviceWorker.ready;
        }).then(function(r) {
          window.domAutomationController.send('SUCCESS');
        }).catch(function(err) {
          window.domAutomationController.send('ERROR: ' + err.message);
        }); )";
    std::string registration_script =
        content::JsReplace(kRegistrationScript, kServiceWorkerPath);

    std::string result = browsertest_util::ExecuteScriptInBackgroundPage(
        browser()->profile(), extension_->id(), registration_script);
    if (result != "SUCCESS") {
      ADD_FAILURE() << "Failed to register the service worker: " << result;
      return false;
    }
    return !::testing::Test::HasFailure();
  }

  // Injects (into |web_contents|) a content_script that performs a fetch of
  // |url|. Returns the body of the response.
  //
  // The method below uses "programmatic" (rather than "declarative") way to
  // inject a content script, but the behavior and permissions of the conecnt
  // script should be the same in both cases.  See also
  // https://developer.chrome.com/extensions/content_scripts#programmatic.
  std::string FetchViaContentScript(const GURL& url,
                                    content::WebContents* web_contents) {
    return FetchHelper(
        url, base::BindOnce(
                 &CrossOriginReadBlockingExtensionTest::ExecuteContentScript,
                 base::Unretained(this), base::Unretained(web_contents)));
  }

  // Performs a fetch of |url| from the background page of the test extension.
  // Returns the body of the response.
  std::string FetchViaBackgroundPage(const GURL& url) {
    return FetchHelper(
        url, base::BindOnce(
                 &browsertest_util::ExecuteScriptInBackgroundPageNoWait,
                 base::Unretained(browser()->profile()), extension_->id()));
  }

  // Performs a fetch of |url| from |web_contents| (directly, without going
  // through content scripts).  Returns the body of the response.
  std::string FetchViaWebContents(const GURL& url,
                                  content::WebContents* web_contents) {
    return FetchHelper(
        url, base::BindOnce(
                 &CrossOriginReadBlockingExtensionTest::ExecuteRegularScript,
                 base::Unretained(this), base::Unretained(web_contents)));
  }

  // Performs a fetch of |url| from a srcdoc subframe added to |parent_frame|
  // and executing a script via <script> tag.  Returns the body of the response.
  std::string FetchViaSrcDocFrame(GURL url,
                                  content::RenderFrameHost* parent_frame) {
    return FetchHelper(
        url, base::BindOnce(
                 &CrossOriginReadBlockingExtensionTest::ExecuteInSrcDocFrame,
                 base::Unretained(this), base::Unretained(parent_frame)));
  }

  std::string PopString(content::DOMMessageQueue* message_queue) {
    std::string json;
    EXPECT_TRUE(message_queue->WaitForMessage(&json));
    base::JSONReader reader(base::JSON_ALLOW_TRAILING_COMMAS);
    std::unique_ptr<base::Value> value = reader.ReadToValueDeprecated(json);
    std::string result;
    EXPECT_TRUE(value->GetAsString(&result));
    return result;
  }

  GURL GetExtensionResource(const std::string& relative_path) {
    return extension_->GetResourceURL(relative_path);
  }

  url::Origin GetExtensionOrigin() {
    return url::Origin::Create(extension_->url());
  }

  GURL GetTestPageUrl(const std::string& hostname) {
    // Using the page below avoids a network fetch of /favicon.ico which helps
    // avoid extra synchronization hassles in the tests.
    return embedded_test_server()->GetURL(
        hostname, "/favicon/title1_with_data_uri_icon.html");
  }

  const Extension* extension() { return extension_; }

  std::string CreateFetchScript(const GURL& resource) {
    const char kFetchScriptTemplate[] = R"(
      fetch($1)
        .then(response => response.text())
        .then(text => domAutomationController.send(text))
        .catch(err => domAutomationController.send('error: ' + err));
    )";
    return content::JsReplace(kFetchScriptTemplate, resource);
  }

  // Asks the test |extension_| to inject |content_script| into |web_contents|.
  //
  // This is an implementation of FetchCallback.
  // Returns true if the content script execution started succeessfully.
  bool ExecuteContentScript(content::WebContents* web_contents,
                            const std::string& content_script) {
    int tab_id = ExtensionTabUtil::GetTabId(web_contents);
    std::string background_script = content::JsReplace(
        "chrome.tabs.executeScript($1, { code: $2 });", tab_id, content_script);
    return browsertest_util::ExecuteScriptInBackgroundPageNoWait(
        browser()->profile(), extension_->id(), background_script);
  }

 private:
  // Executes |regular_script| in |web_contents|.
  //
  // This is an implementation of FetchCallback.
  // Returns true if the script execution started succeessfully.
  bool ExecuteRegularScript(content::WebContents* web_contents,
                            const std::string& regular_script) {
    content::ExecuteScriptAsync(web_contents, regular_script);

    // Report artificial success to meet FetchCallback's requirements.
    return true;
  }

  // Injects into |parent_frame| an "srcdoc" subframe that contains/executes
  // |script_to_run_in_subframe| via <script> tag.
  //
  // This function is useful to exercise a scenario when a <script> tag may
  // execute before the browser gets a chance to see the a frame/navigation
  // commit is happening.
  //
  // This is an implementation of FetchCallback.
  // Returns true if the script execution started succeessfully.
  bool ExecuteInSrcDocFrame(content::RenderFrameHost* parent_frame,
                            const std::string& script_to_run_in_subframe) {
    static int sequence_id = 0;
    sequence_id++;
    std::string filename =
        base::StringPrintf("srcdoc_script_%d.js", sequence_id);
    dir_.WriteFile(base::FilePath::FromUTF8Unsafe(filename).value(),
                   script_to_run_in_subframe);

    // Using <script src=...></script> instead of <script>...</script> to avoid
    // extensions CSP which forbids inline scripts.
    const char kScriptTemplate[] = R"(
        var subframe = document.createElement('iframe');
        subframe.srcdoc = '<script src=' + $1 + '></script>';
        document.body.appendChild(subframe); )";
    std::string subframe_injection_script =
        content::JsReplace(kScriptTemplate, filename);
    content::ExecuteScriptAsync(parent_frame, subframe_injection_script);

    // Report artificial success to meet FetchCallback's requirements.
    return true;
  }

  // FetchCallback represents a function that executes |fetch_script|.
  //
  // |fetch_script| will include calls to |domAutomationController.send| and
  // therefore instances of FetchCallback should not inject their own calls to
  // |domAutomationController.send| (e.g. this constraint rules out
  // browsertest_util::ExecuteScriptInBackgroundPage and/or
  // content::ExecuteScript).
  //
  // The function should return true if script execution started successfully.
  //
  // Currently used "implementations":
  // - CrossOriginReadBlockingExtensionTest::ExecuteContentScript(web_contents)
  // - CrossOriginReadBlockingExtensionTest::ExecuteRegularScript(web_contents)
  // - browsertest_util::ExecuteScriptInBackgroundPageNoWait(profile, ext_id)
  using FetchCallback =
      base::OnceCallback<bool(const std::string& fetch_script)>;

  // Returns response body of a fetch of |url| initiated via |fetch_callback|.
  std::string FetchHelper(const GURL& url, FetchCallback fetch_callback) {
    content::DOMMessageQueue message_queue;

    // Inject a content script that performs a cross-origin fetch to
    // cross-site.com.
    EXPECT_TRUE(std::move(fetch_callback).Run(CreateFetchScript(url)));

    // Wait until the message comes back and extract result from the message.
    return PopString(&message_queue);
  }

  TestExtensionDir dir_;
  const Extension* extension_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(CrossOriginReadBlockingExtensionTest);
};

enum class AllowlistingParam {
  kAllowlisted,
  kNotAllowlisted,
};

class CrossOriginReadBlockingExtensionAllowlistingTest
    : public CrossOriginReadBlockingExtensionTest,
      public ::testing::WithParamInterface<AllowlistingParam> {
 public:
  using Base = CrossOriginReadBlockingExtensionTest;

  CrossOriginReadBlockingExtensionAllowlistingTest() {}

  bool IsExtensionAllowlisted() {
    return GetParam() == AllowlistingParam::kAllowlisted;
  }

  const Extension* InstallExtension(
      GURL resource_to_fetch_from_declarative_content_script = GURL()) {
    const Extension* extension = Base::InstallExtension(
        resource_to_fetch_from_declarative_content_script);

    if (IsExtensionAllowlisted()) {
      URLLoaderFactoryManager::AddExtensionToAllowlistForTesting(*extension);
    } else {
      URLLoaderFactoryManager::RemoveExtensionFromAllowlistForTesting(
          *extension);
    }

    return extension;
  }

  bool AreContentScriptFetchesExpectedToBeBlocked() {
    return !IsExtensionAllowlisted();
  }

  bool IsCorbExpectedToBeTurnedOffAltogether() {
    return IsExtensionAllowlisted();
  }

  void VerifyFetchFromContentScriptWasBlocked(
      const base::HistogramTester& histograms) {
    // Make sure that histograms logged in other processes (e.g. in
    // NetworkService process) get synced.
    SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

    histograms.ExpectBucketCount("SiteIsolation.XSD.Browser.Action",
                                 CORBAction::kResponseStarted, 1);
    histograms.ExpectBucketCount("SiteIsolation.XSD.Browser.Action",
                                 CORBAction::kBlockedWithoutSniffing, 1);
  }

  void VerifyFetchFromContentScriptWasAllowed(
      const base::HistogramTester& histograms,
      bool expecting_sniffing = false) {
    // Make sure that histograms logged in other processes (e.g. in
    // NetworkService process) get synced.
    SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

    if (IsCorbExpectedToBeTurnedOffAltogether()) {
      EXPECT_EQ(0u,
                histograms.GetTotalCountsForPrefix("SiteIsolation.XSD.Browser")
                    .size());
      return;
    }

    histograms.ExpectBucketCount("SiteIsolation.XSD.Browser.Action",
                                 CORBAction::kResponseStarted, 1);
    histograms.ExpectBucketCount("SiteIsolation.XSD.Browser.Action",
                                 expecting_sniffing
                                     ? CORBAction::kAllowedAfterSniffing
                                     : CORBAction::kAllowedWithoutSniffing,
                                 1);
  }

  // Verifies results of fetching a CORB-eligible resource from a content
  // script.  Expectations differ depending on the following:
  // 1. Non-NetworkService: Fetches from content scripts should never be blocked
  // 2. NetworkService + allowlisted extension: Fetches from content scripts
  //                                            should not be blocked
  // 3. NetworkService + other extension: Fetches from content scripts should
  //                                      be blocked
  void VerifyFetchFromContentScript(const base::HistogramTester& histograms,
                                    const std::string& actual_fetch_result,
                                    const std::string& expected_fetch_result) {
    if (AreContentScriptFetchesExpectedToBeBlocked()) {
      // Verify the fetch was blocked.
      EXPECT_EQ(std::string(), actual_fetch_result);
      VerifyFetchFromContentScriptWasBlocked(histograms);
    } else {
      // Verify the fetch was allowed.
      EXPECT_EQ(expected_fetch_result, actual_fetch_result);
      VerifyFetchFromContentScriptWasAllowed(histograms);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CrossOriginReadBlockingExtensionAllowlistingTest);
};

IN_PROC_BROWSER_TEST_P(CrossOriginReadBlockingExtensionAllowlistingTest,
                       FromDeclarativeContentScript_NoSniffXml) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL cross_site_resource(
      embedded_test_server()->GetURL("cross-site.com", "/nosniff.xml"));

  ASSERT_TRUE(InstallExtension(cross_site_resource));

  // Test case #1: Declarative script injected after a browser-initiated
  // navigation of the main frame.
  {
    // Monitor CORB behavior + result of the fetch.
    base::HistogramTester histograms;
    content::DOMMessageQueue message_queue;

    // Navigate to a fetch-initiator.com page - this should trigger execution of
    // the |content_script| declared in the extension manifest.
    GURL page_url = GetTestPageUrl("fetch-initiator.com");
    ui_test_utils::NavigateToURL(browser(), page_url);
    EXPECT_EQ(page_url,
              active_web_contents()->GetMainFrame()->GetLastCommittedURL());
    EXPECT_EQ(url::Origin::Create(page_url),
              active_web_contents()->GetMainFrame()->GetLastCommittedOrigin());

    // Extract results of the fetch done in the declarative content script.
    std::string fetch_result = PopString(&message_queue);

    // Verify whether the fetch worked or not (expectations differ depending on
    // various factors - see the body of VerifyFetchFromContentScript).
    VerifyFetchFromContentScript(histograms, fetch_result,
                                 "nosniff.xml - body\n");
  }

  // Test case #2: Declarative script injected after a renderer-initiated
  // creation of an about:blank frame.
  {
    // Monitor CORB behavior + result of the fetch.
    base::HistogramTester histograms;
    content::DOMMessageQueue message_queue;

    // Inject an about:blank subframe - this should trigger execution of the
    // |content_script| declared in the extension manifest.
    const char kBlankSubframeInjectionScript[] = R"(
        var subframe = document.createElement('iframe');
        document.body.appendChild(subframe); )";
    content::ExecuteScriptAsync(active_web_contents(),
                                kBlankSubframeInjectionScript);

    // Extract results of the fetch done in the declarative content script.
    std::string fetch_result = PopString(&message_queue);

    // Verify whether the fetch worked or not (expectations differ depending on
    // various factors - see the body of VerifyFetchFromContentScript).
    VerifyFetchFromContentScript(histograms, fetch_result,
                                 "nosniff.xml - body\n");
  }
}

// Test that verifies the current, baked-in (but not necessarily desirable
// behavior) where an extension that has permission to inject a content script
// to any page can also fetch (without CORS!) any cross-origin resource.
// See also https://crbug.com/846346.
IN_PROC_BROWSER_TEST_P(CrossOriginReadBlockingExtensionAllowlistingTest,
                       FromProgrammaticContentScript_NoSniffXml) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(InstallExtension());

  // Navigate to a fetch-initiator.com page.
  GURL page_url = GetTestPageUrl("fetch-initiator.com");
  ui_test_utils::NavigateToURL(browser(), page_url);
  ASSERT_EQ(page_url,
            active_web_contents()->GetMainFrame()->GetLastCommittedURL());
  ASSERT_EQ(url::Origin::Create(page_url),
            active_web_contents()->GetMainFrame()->GetLastCommittedOrigin());

  // Inject a content script that performs a cross-origin fetch to
  // cross-site.com.
  base::HistogramTester histograms;
  GURL cross_site_resource(
      embedded_test_server()->GetURL("cross-site.com", "/nosniff.xml"));
  std::string fetch_result =
      FetchViaContentScript(cross_site_resource, active_web_contents());

  // Verify whether the fetch worked or not (expectations differ depending on
  // various factors - see the body of VerifyFetchFromContentScript).
  VerifyFetchFromContentScript(histograms, fetch_result,
                               "nosniff.xml - body\n");
}

// Test that verifies CORS-allowed fetches work for targets that are not
// covered by the extension permissions.
IN_PROC_BROWSER_TEST_P(CrossOriginReadBlockingExtensionAllowlistingTest,
                       FromProgrammaticContentScript_NoPermissionToTarget) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(InstallExtension());

  // Navigate to a fetch-initiator.com page.
  GURL page_url = GetTestPageUrl("fetch-initiator.com");
  ui_test_utils::NavigateToURL(browser(), page_url);
  ASSERT_EQ(page_url,
            active_web_contents()->GetMainFrame()->GetLastCommittedURL());
  ASSERT_EQ(url::Origin::Create(page_url),
            active_web_contents()->GetMainFrame()->GetLastCommittedOrigin());

  // Inject a content script that performs a cross-origin fetch to
  // cross-site.com.
  base::HistogramTester histograms;
  GURL cross_site_resource(embedded_test_server()->GetURL(
      "other-without-permission.com", "/cors-ok.txt"));
  std::string fetch_result =
      FetchViaContentScript(cross_site_resource, active_web_contents());

  // Verify whether the fetch worked or not.
  EXPECT_EQ("cors-ok.txt - body\n", fetch_result);
  VerifyFetchFromContentScriptWasAllowed(histograms);
}

// Tests that same-origin fetches (same-origin relative to the webpage the
// content script is injected into) are allowed.  See also
// https://crbug.com/918660.
IN_PROC_BROWSER_TEST_P(CrossOriginReadBlockingExtensionAllowlistingTest,
                       FromProgrammaticContentScript_SameOrigin) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(InstallExtension());

  // Navigate to a fetch-initiator.com page.
  GURL page_url = GetTestPageUrl("fetch-initiator.com");
  ui_test_utils::NavigateToURL(browser(), page_url);
  ASSERT_EQ(page_url,
            active_web_contents()->GetMainFrame()->GetLastCommittedURL());
  ASSERT_EQ(url::Origin::Create(page_url),
            active_web_contents()->GetMainFrame()->GetLastCommittedOrigin());

  // Inject a content script that performs a same-origin fetch to
  // fetch-initiator.com.
  base::HistogramTester histograms;
  GURL same_origin_resource(
      embedded_test_server()->GetURL("fetch-initiator.com", "/nosniff.xml"));
  std::string fetch_result =
      FetchViaContentScript(same_origin_resource, active_web_contents());

  // Verify that no blocking occurred.
  EXPECT_THAT(fetch_result, ::testing::StartsWith("nosniff.xml - body"));
  VerifyFetchFromContentScriptWasAllowed(histograms,
                                         false /* expecting_sniffing */);
}

// Test that responses that would have been allowed by CORB anyway are not
// reported to LogInitiatorSchemeBypassingDocumentBlocking.
IN_PROC_BROWSER_TEST_P(CrossOriginReadBlockingExtensionAllowlistingTest,
                       FromProgrammaticContentScript_AllowedTextResource) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(InstallExtension());

  // Navigate to a fetch-initiator.com page.
  GURL page_url = GetTestPageUrl("fetch-initiator.com");
  ui_test_utils::NavigateToURL(browser(), page_url);
  ASSERT_EQ(page_url,
            active_web_contents()->GetMainFrame()->GetLastCommittedURL());
  ASSERT_EQ(url::Origin::Create(page_url),
            active_web_contents()->GetMainFrame()->GetLastCommittedOrigin());

  // Inject a content script that performs a cross-origin fetch to
  // cross-site.com.
  //
  // StartsWith (rather than equality) is used in the verification step to
  // account for \n VS \r\n difference on Windows.
  base::HistogramTester histograms;
  GURL cross_site_resource(
      embedded_test_server()->GetURL("cross-site.com", "/save_page/text.txt"));
  std::string fetch_result =
      FetchViaContentScript(cross_site_resource, active_web_contents());

  // Verify that no blocking occurred.
  EXPECT_THAT(fetch_result,
              ::testing::StartsWith(
                  "text-object.txt: ae52dd09-9746-4b7e-86a6-6ada5e2680c2"));
  VerifyFetchFromContentScriptWasAllowed(histograms,
                                         true /* expecting_sniffing */);
}

// Test that responses are blocked by CORB, but have empty response body are not
// reported to LogInitiatorSchemeBypassingDocumentBlocking.
IN_PROC_BROWSER_TEST_P(CrossOriginReadBlockingExtensionAllowlistingTest,
                       FromProgrammaticContentScript_EmptyAndBlocked) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(InstallExtension());

  // Navigate to a fetch-initiator.com page.
  GURL page_url = GetTestPageUrl("fetch-initiator.com");
  ui_test_utils::NavigateToURL(browser(), page_url);
  ASSERT_EQ(page_url,
            active_web_contents()->GetMainFrame()->GetLastCommittedURL());
  ASSERT_EQ(url::Origin::Create(page_url),
            active_web_contents()->GetMainFrame()->GetLastCommittedOrigin());

  // Inject a content script that performs a cross-origin fetch to
  // cross-site.com.
  base::HistogramTester histograms;
  GURL cross_site_resource(
      embedded_test_server()->GetURL("cross-site.com", "/nosniff.empty"));
  EXPECT_EQ(std::string(),
            FetchViaContentScript(cross_site_resource, active_web_contents()));

  // Verify whether blocking occurred or not.
  if (AreContentScriptFetchesExpectedToBeBlocked())
    VerifyFetchFromContentScriptWasBlocked(histograms);
  else
    VerifyFetchFromContentScriptWasAllowed(histograms);
}

// Test that LogInitiatorSchemeBypassingDocumentBlocking exits early for
// requests that aren't from content scripts.
IN_PROC_BROWSER_TEST_F(CrossOriginReadBlockingExtensionTest,
                       FromBackgroundPage_NoSniffXml) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(InstallExtension());

  // Performs a cross-origin fetch from the background page.
  base::HistogramTester histograms;
  GURL cross_site_resource(
      embedded_test_server()->GetURL("cross-site.com", "/nosniff.xml"));
  std::string fetch_result = FetchViaBackgroundPage(cross_site_resource);

  // Verify that no blocking occurred.
  EXPECT_EQ("nosniff.xml - body\n", fetch_result);
}

// Test that requests from a extension page hosted in a foreground tab use
// relaxed CORB processing.
IN_PROC_BROWSER_TEST_F(CrossOriginReadBlockingExtensionTest,
                       FromForegroundPage_NoSniffXml) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(InstallExtension());

  // Navigate a tab to an extension page.
  ui_test_utils::NavigateToURL(browser(), GetExtensionResource("page.html"));
  ASSERT_EQ(GetExtensionOrigin(),
            active_web_contents()->GetMainFrame()->GetLastCommittedOrigin());

  // Test case #1: Fetch from a chrome-extension://... main frame.
  {
    // Perform a cross-origin fetch from the foreground extension page.
    base::HistogramTester histograms;
    GURL cross_site_resource(
        embedded_test_server()->GetURL("cross-site.com", "/nosniff.xml"));
    std::string fetch_result =
        FetchViaWebContents(cross_site_resource, active_web_contents());

    // Verify that no blocking occurred.
    EXPECT_EQ("nosniff.xml - body\n", fetch_result);
  }

  // Test case #2: Fetch from an about:srcdoc subframe of a
  // chrome-extension://... frame.
  {
    // Perform a cross-origin fetch from the foreground extension page.
    base::HistogramTester histograms;
    GURL cross_site_resource(
        embedded_test_server()->GetURL("cross-site.com", "/nosniff.xml"));
    std::string fetch_result = FetchViaSrcDocFrame(
        cross_site_resource, active_web_contents()->GetMainFrame());

    // Verify that no blocking occurred.
    EXPECT_EQ("nosniff.xml - body\n", fetch_result);
  }
}

// Test that requests from an extension's service worker to the network use
// relaxed CORB processing (both in the case of requests that 1) are initiated
// by the service worker and/or 2) are ignored by the service worker and fall
// back to the network).
IN_PROC_BROWSER_TEST_F(CrossOriginReadBlockingExtensionTest,
                       FromServiceWorker_NoSniffXml) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(InstallExtension());

  // Register the service worker which injects "SERVICE WORKER INTERCEPT: "
  // prefix to the body of each response.
  const char kServiceWorkerScript[] = R"(
      self.addEventListener('fetch', function(event) {
          // Intercept all http requests to cross-site.com and inject
          // 'SERVICE WORKER INTERCEPT:' prefix.
          if (event.request.url.startsWith('http://cross-site.com')) {
            event.respondWith(
                // By using the 'fetch' call below, the service worker initiates
                // a network request that will go through the URLLoaderFactory
                // created via CreateFactoryBundle called / posted indirectly
                // from EmbeddedWorkerInstance::StartTask::Start.
                fetch(event.request)
                    .then(response => response.text())
                    .then(text => new Response(
                        'SERVICE WORKER INTERCEPT: >>>' + text + '<<<')));
          }

          // Let the request go directly to the network in all the other cases,
          // like:
          // - loading the extension resources like page.html (avoiding going
          //   through the service worker is required for correctness of test
          //   setup),
          // - handling the cross-origin fetch to other.com in test case #2.
          // Note that these requests will use the URLLoaderFactory owned by
          // ServiceWorkerSubresourceLoader which can be different to the
          // network loader factory owned by the ServiceWorker thread (which is
          // used for fetch intiated by the service worker above).
      }); )";
  ASSERT_TRUE(RegisterServiceWorkerForExtension(kServiceWorkerScript));

  // Navigate a tab to an extension page.
  ui_test_utils::NavigateToURL(browser(), GetExtensionResource("page.html"));
  ASSERT_EQ(GetExtensionOrigin(),
            active_web_contents()->GetMainFrame()->GetLastCommittedOrigin());

  // Verify that the service worker controls the fetches.
  bool is_controlled_by_service_worker = false;
  ASSERT_TRUE(ExecuteScriptAndExtractBool(
      active_web_contents(),
      "domAutomationController.send(!!navigator.serviceWorker.controller)",
      &is_controlled_by_service_worker));
  ASSERT_TRUE(is_controlled_by_service_worker);

  // Test case #1: Network fetch initiated by the service worker.
  //
  // This covers URLLoaderFactory owned by the ServiceWorker thread and created
  // created via CreateFactoryBundle called / posted indirectly from
  // EmbeddedWorkerInstance::StartTask::Start.
  {
    // Perform a cross-origin fetch from the foreground extension page.
    // This should be intercepted by the service worker installed above.
    base::HistogramTester histograms;
    GURL cross_site_resource_intercepted_by_service_worker(
        embedded_test_server()->GetURL("cross-site.com", "/nosniff.xml"));
    std::string fetch_result =
        FetchViaWebContents(cross_site_resource_intercepted_by_service_worker,
                            active_web_contents());

    // Verify that no blocking occurred (and that the response really did go
    // through the service worker).
    EXPECT_EQ("SERVICE WORKER INTERCEPT: >>>nosniff.xml - body\n<<<",
              fetch_result);
  }

  // Test case #2: Network fetch used as a fallback when service worker ignores
  // the 'fetch' event.
  //
  // This covers URLLoaderFactory owned by the ServiceWorkerSubresourceLoader,
  // which can be different to the network loader factory owned by the
  // ServiceWorker thread (which is used in test case #1).
  {
    // Perform a cross-origin fetch from the foreground extension page.
    // This should be intercepted by the service worker installed above.
    base::HistogramTester histograms;
    GURL cross_site_resource_ignored_by_service_worker(
        embedded_test_server()->GetURL("other-with-permission.com",
                                       "/nosniff.xml"));
    std::string fetch_result = FetchViaWebContents(
        cross_site_resource_ignored_by_service_worker, active_web_contents());

    // Verify that no blocking occurred.
    EXPECT_EQ("nosniff.xml - body\n", fetch_result);
  }
}

class ReadyToCommitWaiter : public content::WebContentsObserver {
 public:
  explicit ReadyToCommitWaiter(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}

  ~ReadyToCommitWaiter() override {}

  void Wait() { run_loop_.Run(); }

  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override {
    run_loop_.Quit();
  }

 private:
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(ReadyToCommitWaiter);
};

IN_PROC_BROWSER_TEST_F(CrossOriginReadBlockingExtensionTest,
                       ProgrammaticContentScriptVsWebUI) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(InstallExtension());

  // Try to inject a content script just as we are about to commit a WebUI page.
  // This will cause ExecuteCodeInTabFunction::CanExecuteScriptOnPage to execute
  // while RenderFrameHost::GetLastCommittedOrigin() still corresponds to the
  // old page.
  {
    // Initiate navigating a new, blank tab (this avoids process swaps which
    // would otherwise occur when navigating to a WebUI pages from either the
    // NTP or from a web page).  This simulates choosing "Settings" from the
    // main menu.
    GURL web_ui_url("chrome://settings");
    NavigateParams nav_params(
        browser(), web_ui_url,
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_GENERATED));
    nav_params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    content::WebContentsAddedObserver new_web_contents_observer;
    Navigate(&nav_params);

    // Capture the new WebContents.
    content::WebContents* new_web_contents =
        new_web_contents_observer.GetWebContents();
    ReadyToCommitWaiter ready_to_commit_waiter(new_web_contents);
    content::TestNavigationObserver navigation_observer(new_web_contents, 1);

    // Repro of https://crbug.com/894766 requires that no cross-process swap
    // takes place - this is what happens when navigating an initial/blank tab.

    // Wait until ReadyToCommit happens.
    //
    // For the repro to happen, content script injection needs to run
    // 1) after RenderFrameHostImpl::CommitNavigation
    //    (which runs in the same revolution of the message pump as
    //    WebContentsObserver::ReadyToCommitNavigation)
    // 2) before RenderFrameHostImpl::DidCommitProvisionalLoad
    //    (task posted from ReadyToCommitNavigation above should execute
    //     before any IPC responses that come after PostTask call).
    ready_to_commit_waiter.Wait();
    DCHECK_NE(web_ui_url,
              active_web_contents()->GetMainFrame()->GetLastCommittedURL());

    // Inject the content script (simulating chrome.tabs.executeScript, but
    // using TabsExecuteScriptFunction directly to ensure the right timing).
    int tab_id = ExtensionTabUtil::GetTabId(active_web_contents());
    const char kArgsTemplate[] = R"(
        [%d, {"code": "
            var p = document.createElement('p');
            p.innerText = 'content script injection succeeded unexpectedly';
            p.id = 'content-script-injection-result';
            document.body.appendChild(p);
        "}] )";
    std::string args = base::StringPrintf(kArgsTemplate, tab_id);
    auto function = base::MakeRefCounted<TabsExecuteScriptFunction>();
    function->set_extension(extension());
    std::string actual_error =
        extension_function_test_utils::RunFunctionAndReturnError(
            function.get(), args, browser());
    std::string expected_error =
        "Cannot access contents of url \"chrome://settings/\". "
        "Extension manifest must request permission to access this host.";
    EXPECT_EQ(expected_error, actual_error);

    // Wait until the navigation completes.
    navigation_observer.Wait();
    EXPECT_EQ(web_ui_url,
              active_web_contents()->GetMainFrame()->GetLastCommittedURL());
  }

  // Check if the injection above succeeded (it shouldn't have, because of
  // renderer-side checks).
  const char kInjectionVerificationScript[] = R"(
      domAutomationController.send(
          !!document.getElementById('content-script-injection-result')); )";
  bool has_content_script_run = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(active_web_contents(),
                                                   kInjectionVerificationScript,
                                                   &has_content_script_run));
  EXPECT_FALSE(has_content_script_run);

  // Try to fetch a WebUI resource (i.e. verify that the unsucessful content
  // script injection above didn't clobber the WebUI-specific URLLoaderFactory).
  const char kScript[] = R"(
      var img = document.createElement('img');
      img.src = 'chrome://resources/images/arrow_down.svg';
      img.onload = () => domAutomationController.send('LOADED');
      img.onerror = e => domAutomationController.send('ERROR: ' + e);
  )";
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(active_web_contents(),
                                                     kScript, &result));
  EXPECT_EQ("LOADED", result);
}

IN_PROC_BROWSER_TEST_P(CrossOriginReadBlockingExtensionAllowlistingTest,
                       ProgrammaticContentScriptVsAppCache) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(InstallExtension());

  // Set up http server serving files from content/test/data (which conveniently
  // already contains appcache-related test files, unlike chrome/test/data).
  net::EmbeddedTestServer content_test_data_server;
  content_test_data_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(content_test_data_server.Start());

  // Load the main page twice. The second navigation should have AppCache
  // initialized for the page.
  //
  // Note that localhost / 127.0.0.1 need to be used, because Application Cache
  // is restricted to secure contexts.
  GURL main_url = content_test_data_server.GetURL(
      "127.0.0.1", "/appcache/simple_page_with_manifest.html");
  ui_test_utils::NavigateToURL(browser(), main_url);
  base::string16 expected_title = base::ASCIIToUTF16("AppCache updated");
  content::TitleWatcher title_watcher(active_web_contents(), expected_title);
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  ui_test_utils::NavigateToURL(browser(), main_url);

  // Turn off the server and sanity check that the resource is still available.
  const char kScriptTemplate[] = R"(
      new Promise(function (resolve, reject) {
          var img = document.createElement('img');
          img.src = '/appcache/' + $1;
          img.onload = _ => resolve('IMG LOADED');
          img.onerror = reject;
      })
  )";
  ASSERT_TRUE(content_test_data_server.ShutdownAndWaitUntilComplete());
  EXPECT_EQ("IMG LOADED",
            content::EvalJs(active_web_contents(),
                            content::JsReplace(kScriptTemplate, "logo.png")));

  // Inject a content script and verify that this doesn't negatively impact
  // AppCache (i.e. verify that
  // RenderFrameHostImpl::MarkInitiatorsAsRequiringSeparateURLLoaderFactory
  // does not clobber the default URLLoaderFactory).
  {
    SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
    base::HistogramTester histograms;
    GURL cross_site_resource(
        embedded_test_server()->GetURL("cross-site.com", "/nosniff.xml"));
    std::string fetch_result =
        FetchViaContentScript(cross_site_resource, active_web_contents());

    // Verify whether the fetch worked or not (expectations differ depending on
    // various factors - see the body of VerifyFetchFromContentScript).
    VerifyFetchFromContentScript(histograms, fetch_result,
                                 "nosniff.xml - body\n");
  }
  // Using a different image, to bypass renderer-side caching.
  EXPECT_EQ("IMG LOADED",
            content::EvalJs(active_web_contents(),
                            content::JsReplace(kScriptTemplate, "logo2.png")));

  // Crash the network service and wait for things to come back up.  This (and
  // the remaining part of the test) only makes sense if 1) the network service
  // is enabled and running in a separate process and 2) the frame has at least
  // one network-bound URLLoaderFactory (i.e. the test extension is
  // allowlisted).
  if (!content::IsOutOfProcessNetworkService() || !IsExtensionAllowlisted())
    return;
  SimulateNetworkServiceCrash();
  active_web_contents()
      ->GetMainFrame()
      ->FlushNetworkAndNavigationInterfacesForTesting();

  // Make sure that both requests still work - the code should have recovered
  // from the crash by 1) refreshing the URLLoaderFactory for the content script
  // and 2) without cloberring the default factory for the AppCache.
  {
    SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
    base::HistogramTester histograms;
    GURL cross_site_resource(
        embedded_test_server()->GetURL("cross-site.com", "/nosniff.xml"));
    std::string fetch_result =
        FetchViaContentScript(cross_site_resource, active_web_contents());

    // Verify whether the fetch worked or not (expectations differ depending on
    // various factors - see the body of VerifyFetchFromContentScript).
    VerifyFetchFromContentScript(histograms, fetch_result,
                                 "nosniff.xml - body\n");
  }
  // Using a different image, to bypass renderer-side caching.
  EXPECT_EQ("IMG LOADED",
            content::EvalJs(active_web_contents(),
                            content::JsReplace(kScriptTemplate, "logo3.png")));
}

IN_PROC_BROWSER_TEST_F(CrossOriginReadBlockingExtensionTest,
                       WebViewContentScript) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Load the test app.
  const Extension* app = InstallApp();
  ASSERT_TRUE(app);

  // Launch the test app and grab its WebContents.
  content::WebContents* app_contents = nullptr;
  {
    content::WebContentsAddedObserver new_contents_observer;
    apps::LaunchService::Get(browser()->profile())
        ->OpenApplication(apps::AppLaunchParams(
            app->id(), LaunchContainer::kLaunchContainerNone,
            WindowOpenDisposition::NEW_WINDOW,
            apps::mojom::AppLaunchSource::kSourceTest));
    app_contents = new_contents_observer.GetWebContents();
  }
  ASSERT_TRUE(content::WaitForLoadStop(app_contents));

  // Inject a <webview> script and declare desire to inject
  // cross-origin-fetching content scripts into the guest.
  const char kWebViewInjectionScriptTemplate[] = R"(
      document.querySelector('#webview-tag-container').innerHTML =
          '<webview style="width: 100px; height: 100px;"></webview>';
      var webview = document.querySelector('webview');
      webview.addContentScripts([{
          name: 'rule',
          matches: ['*://*/*'],
          js: { code: $1 },
          run_at: 'document_start'}]);
  )";
  GURL cross_site_resource(
      embedded_test_server()->GetURL("cross-site.com", "/nosniff.xml"));
  std::string web_view_injection_script = content::JsReplace(
      kWebViewInjectionScriptTemplate, CreateFetchScript(cross_site_resource));
  ASSERT_TRUE(ExecuteScript(app_contents, web_view_injection_script));

  // Navigate <webview>, which should trigger content script execution.
  GURL guest_url(
      embedded_test_server()->GetURL("fetch-initiator.com", "/title1.html"));
  const char kWebViewNavigationScriptTemplate[] = R"(
      var webview = document.querySelector('webview');
      webview.src = $1;
  )";
  std::string web_view_navigation_script =
      content::JsReplace(kWebViewNavigationScriptTemplate, guest_url);
  {
    content::DOMMessageQueue queue;
    base::HistogramTester histograms;
    content::ExecuteScriptAsync(app_contents, web_view_navigation_script);
    std::string fetch_result = PopString(&queue);

    // Verify that no CORB blocking occurred.
    EXPECT_EQ("nosniff.xml - body\n", fetch_result);
  }
}

IN_PROC_BROWSER_TEST_P(CrossOriginReadBlockingExtensionAllowlistingTest,
                       OriginHeaderInCrossOriginGetRequest) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(InstallExtension());

  // Navigate to a fetch-initiator.com page.
  GURL page_url = GetTestPageUrl("fetch-initiator.com");
  ui_test_utils::NavigateToURL(browser(), page_url);
  ASSERT_EQ(page_url,
            active_web_contents()->GetMainFrame()->GetLastCommittedURL());
  ASSERT_EQ(url::Origin::Create(page_url),
            active_web_contents()->GetMainFrame()->GetLastCommittedOrigin());

  // Inject a content script that performs a cross-origin GET fetch to
  // cross-site.com.
  GURL cross_site_resource(
      embedded_test_server()->GetURL("cross-site.com", "/echoall"));
  const char* kScriptTemplate = R"(
      fetch($1, {method: 'GET', mode:'cors'})
          .then(response => response.text())
          .then(text => domAutomationController.send(text))
          .catch(err => domAutomationController.send('ERROR: ' + err));
  )";
  content::DOMMessageQueue message_queue;
  ExecuteContentScript(
      active_web_contents(),
      content::JsReplace(kScriptTemplate, cross_site_resource));
  std::string fetch_result = PopString(&message_queue);

  // Verify if the fetch was blocked + what the Origin header was.
  if (AreContentScriptFetchesExpectedToBeBlocked()) {
    // TODO(lukasza): https://crbug.com/953315: No CORB blocking should occur
    // for the CORS-mode request - the test expectations should be the same,
    // regardless of AreContentScriptFetchesExpectedToBeBlocked.
    EXPECT_EQ("", fetch_result);
  } else if (IsExtensionAllowlisted()) {
    // Legacy behavior - no Origin: header is present in GET CORS requests from
    // content scripts based on the extension permissions.
    EXPECT_THAT(fetch_result, ::testing::Not(::testing::HasSubstr("Origin:")));
  } else {
    // TODO(lukasza): https://crbug.com/920638: Non-allowlisted extension
    // should use the website's origin in the CORS request.
    // TODO: EXPECT_THAT(fetch_result,
    //                   ::testing::HasSubstr("Origin:
    //                   http://fetch-initiator.com"));
    EXPECT_THAT(fetch_result, ::testing::Not(::testing::HasSubstr("Origin:")));
  }

  // Regression test against https://crbug.com/944704.
  EXPECT_THAT(fetch_result,
              ::testing::Not(::testing::HasSubstr("Origin: chrome-extension")));
}

IN_PROC_BROWSER_TEST_P(CrossOriginReadBlockingExtensionAllowlistingTest,
                       OriginHeaderInCrossOriginPostRequest) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(InstallExtension());

  // Navigate to a fetch-initiator.com page.
  GURL page_url = GetTestPageUrl("fetch-initiator.com");
  ui_test_utils::NavigateToURL(browser(), page_url);
  ASSERT_EQ(page_url,
            active_web_contents()->GetMainFrame()->GetLastCommittedURL());
  ASSERT_EQ(url::Origin::Create(page_url),
            active_web_contents()->GetMainFrame()->GetLastCommittedOrigin());

  // Inject a content script that performs a cross-origin POST fetch to
  // cross-site.com.
  GURL cross_site_resource(
      embedded_test_server()->GetURL("cross-site.com", "/echoall"));
  const char* kScriptTemplate = R"(
      fetch($1, {method: 'POST', mode:'cors'})
          .then(response => response.text())
          .then(text => domAutomationController.send(text))
          .catch(err => domAutomationController.send('ERROR: ' + err));
  )";
  content::DOMMessageQueue message_queue;
  ExecuteContentScript(
      active_web_contents(),
      content::JsReplace(kScriptTemplate, cross_site_resource));
  std::string fetch_result = PopString(&message_queue);

  // Verify if the fetch was blocked + what the Origin header was.
  if (AreContentScriptFetchesExpectedToBeBlocked()) {
    // TODO(lukasza): https://crbug.com/953315: No CORB blocking should occur
    // for the CORS-mode request - the test expectations should be the same,
    // regardless of AreContentScriptFetchesExpectedToBeBlocked.
    EXPECT_EQ("", fetch_result);
  } else {
    EXPECT_THAT(fetch_result,
                ::testing::HasSubstr("Origin: http://fetch-initiator.com"));
  }

  // Regression test against https://crbug.com/944704.
  EXPECT_THAT(fetch_result,
              ::testing::Not(::testing::HasSubstr("Origin: chrome-extension")));
}

IN_PROC_BROWSER_TEST_P(CrossOriginReadBlockingExtensionAllowlistingTest,
                       OriginHeaderInSameOriginPostRequest) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(InstallExtension());

  // Navigate to a fetch-initiator.com page.
  GURL page_url = GetTestPageUrl("fetch-initiator.com");
  ui_test_utils::NavigateToURL(browser(), page_url);
  ASSERT_EQ(page_url,
            active_web_contents()->GetMainFrame()->GetLastCommittedURL());
  ASSERT_EQ(url::Origin::Create(page_url),
            active_web_contents()->GetMainFrame()->GetLastCommittedOrigin());

  // Inject a content script that performs a same-origin POST fetch to
  // fetch-initiator.com.
  GURL same_origin_resource(
      embedded_test_server()->GetURL("fetch-initiator.com", "/echoall"));
  const char* kScriptTemplate = R"(
      fetch($1, {method: 'POST', mode:'cors'})
          .then(response => response.text())
          .then(text => domAutomationController.send(text))
          .catch(err => domAutomationController.send('ERROR: ' + err));
  )";
  content::DOMMessageQueue message_queue;
  ExecuteContentScript(
      active_web_contents(),
      content::JsReplace(kScriptTemplate, same_origin_resource));
  std::string fetch_result = PopString(&message_queue);

  // Verify the Origin header.
  //
  // According to the Fetch spec, POST should always set the Origin header (even
  // for same-origin requests).
  EXPECT_THAT(fetch_result,
              ::testing::HasSubstr("Origin: http://fetch-initiator.com"));

  // Regression test against https://crbug.com/944704.
  EXPECT_THAT(fetch_result,
              ::testing::Not(::testing::HasSubstr("Origin: chrome-extension")));
}

IN_PROC_BROWSER_TEST_P(CrossOriginReadBlockingExtensionAllowlistingTest,
                       RequestHeaders_InSameOriginFetch_FromContentScript) {
  // Sec-Fetch-Site only works on secure origins - setting up a https test
  // server to help with this.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.AddDefaultHandlers(GetChromeTestDataDir());
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  net::test_server::ControllableHttpResponse subresource_request(
      &https_server, "/subresource");
  ASSERT_TRUE(https_server.Start());

  // Load the test extension.
  ASSERT_TRUE(InstallExtension());

  // Navigate to https test page.
  GURL page_url = https_server.GetURL("/title1.html");
  ui_test_utils::NavigateToURL(browser(), page_url);
  ASSERT_EQ(page_url,
            active_web_contents()->GetMainFrame()->GetLastCommittedURL());
  ASSERT_EQ(url::Origin::Create(page_url),
            active_web_contents()->GetMainFrame()->GetLastCommittedOrigin());

  // Inject a content script that performs a same-origin GET fetch.
  GURL same_origin_resource(https_server.GetURL("/subresource"));
  EXPECT_EQ(url::Origin::Create(page_url),
            url::Origin::Create(same_origin_resource));
  const char* kScriptTemplate = R"(
      fetch($1, {method: 'GET', mode: 'no-cors'}) )";
  ExecuteContentScript(
      active_web_contents(),
      content::JsReplace(kScriptTemplate, same_origin_resource));

  // Verify the Referrer and Sec-Fetch-* header values.
  subresource_request.WaitForRequest();
  EXPECT_THAT(
      subresource_request.http_request()->headers,
      testing::IsSupersetOf({testing::Pair("Referer", page_url.spec().c_str()),
                             testing::Pair("Sec-Fetch-Mode", "no-cors"),
                             testing::Pair("Sec-Fetch-Site", "same-origin")}));
}

IN_PROC_BROWSER_TEST_P(CrossOriginReadBlockingExtensionAllowlistingTest,
                       RequestHeaders_InSameOriginXhr_FromContentScript) {
  // Sec-Fetch-Site only works on secure origins - setting up a https test
  // server to help with this.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.AddDefaultHandlers(GetChromeTestDataDir());
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  net::test_server::ControllableHttpResponse subresource_request(
      &https_server, "/subresource");
  ASSERT_TRUE(https_server.Start());

  // Load the test extension.
  ASSERT_TRUE(InstallExtension());

  // Navigate to https test page.
  GURL page_url = https_server.GetURL("/title1.html");
  ui_test_utils::NavigateToURL(browser(), page_url);
  ASSERT_EQ(page_url,
            active_web_contents()->GetMainFrame()->GetLastCommittedURL());
  ASSERT_EQ(url::Origin::Create(page_url),
            active_web_contents()->GetMainFrame()->GetLastCommittedOrigin());

  // Inject a content script that performs a same-origin GET XHR.
  GURL same_origin_resource(https_server.GetURL("/subresource"));
  EXPECT_EQ(url::Origin::Create(page_url),
            url::Origin::Create(same_origin_resource));
  const char* kScriptTemplate = R"(
    var req = new XMLHttpRequest();
    req.open('GET', $1, true);
    req.send(null); )";
  ExecuteContentScript(
      active_web_contents(),
      content::JsReplace(kScriptTemplate, same_origin_resource));

  // Verify the Referrer and Sec-Fetch-* header values.
  subresource_request.WaitForRequest();
  EXPECT_THAT(
      subresource_request.http_request()->headers,
      testing::IsSupersetOf({testing::Pair("Referer", page_url.spec().c_str()),
                             testing::Pair("Sec-Fetch-Mode", "cors"),
                             testing::Pair("Sec-Fetch-Site", "same-origin")}));
}

IN_PROC_BROWSER_TEST_P(CrossOriginReadBlockingExtensionAllowlistingTest,
                       CorsFromContentScript) {
  std::string cors_resource_path = "/cors-subresource-to-intercept";
  net::test_server::ControllableHttpResponse cors_request(
      embedded_test_server(), cors_resource_path);
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(InstallExtension());

  // Navigate to test page.
  GURL page_url = GetTestPageUrl("fetch-initiator.com");
  url::Origin page_origin = url::Origin::Create(page_url);
  std::string page_origin_string = page_origin.Serialize();
  ui_test_utils::NavigateToURL(browser(), page_url);
  ASSERT_EQ(page_url,
            active_web_contents()->GetMainFrame()->GetLastCommittedURL());
  ASSERT_EQ(page_origin,
            active_web_contents()->GetMainFrame()->GetLastCommittedOrigin());

  // Inject a content script that performs a cross-origin GET fetch.
  content::DOMMessageQueue message_queue;
  GURL cors_resource_url(
      embedded_test_server()->GetURL("cross-site.com", cors_resource_path));
  EXPECT_TRUE(ExecuteContentScript(active_web_contents(),
                                   CreateFetchScript(cors_resource_url)));

  // Verify the request headers (e.g. Origin and Sec-Fetch-Site headers).
  cors_request.WaitForRequest();
  if (IsExtensionAllowlisted()) {
    // Content scripts of allowlisted extensions should be exempted from CORS,
    // based on the websites the extension has permission for, via extension
    // manifest.  Therefore, there should be no "Origin" header.
    EXPECT_THAT(
        cors_request.http_request()->headers,
        testing::Not(testing::Contains(testing::Pair("Origin", testing::_))));
  } else {
#if 0
    // TODO(lukasza): https://crbug.com/920638:
    //
    // Content scripts of non-allowlisted extensions should participate in
    // regular CORS, just as if the request was issued from the webpage that the
    // content script got injected into.  Therefore we should expect the Origin
    // header to be present and have the right value.
    EXPECT_THAT(
        cors_request.http_request()->headers,
        testing::Contains(testing::Pair("Origin", page_origin_string.c_str())));
#endif
  }

  // Respond with Access-Control-Allow-Origin that matches the origin of the web
  // page.
  cors_request.Send("HTTP/1.1 200 OK\r\n");
  cors_request.Send("Content-Type: text/xml; charset=utf-8\r\n");
  cors_request.Send("X-Content-Type-Options: nosniff\r\n");
  cors_request.Send("Access-Control-Allow-Origin: " + page_origin_string +
                    "\r\n");
  cors_request.Send("\r\n");
  cors_request.Send("cors-allowed-body");
  cors_request.Done();

  // Verify that no CORB blocking occurred.
  //
  // CORB blocks responses based on Access-Control-Allow-Origin, oblivious to
  // whether the Origin request header was present (and/or if the extension is
  // exempted from CORS).  The Access-Control-Allow-Origin header is compared
  // with the request_initiator of the fetch (the origin of |page_url|) and the
  // test responds with "*" which matches all origins.
  std::string fetch_result = PopString(&message_queue);
  EXPECT_EQ("cors-allowed-body", fetch_result);
}

INSTANTIATE_TEST_SUITE_P(Allowlisted,
                         CrossOriginReadBlockingExtensionAllowlistingTest,
                         ::testing::Values(AllowlistingParam::kAllowlisted));
INSTANTIATE_TEST_SUITE_P(NotAllowlisted,
                         CrossOriginReadBlockingExtensionAllowlistingTest,
                         ::testing::Values(AllowlistingParam::kNotAllowlisted));

}  // namespace extensions
