// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_action_runner.h"

#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {

class CrossOriginReadBlockingExtensionTest : public ExtensionBrowserTest {
 public:
  CrossOriginReadBlockingExtensionTest() = default;

  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  const Extension* InstallExtension(
      GURL resource_to_fetch_from_declarative_content_script = GURL()) {
    bool use_declarative_content_script =
        resource_to_fetch_from_declarative_content_script.is_valid();
    const char kContentScriptManifestEntry[] = R"(
          "content_scripts": [{
            "all_frames": true,
            "match_about_blank": true,
            "matches": ["*://*/*"],
            "js": ["content_script.js"]
          }],
    )";

    const char kManifestTemplate[] = R"(
        {
          "name": "CrossOriginReadBlockingTest",
          "version": "1.0",
          "manifest_version": 2,
          "permissions": ["tabs", "*://*/*"],
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

  void VerifyContentScriptHistogramIsPresent(
      const base::HistogramTester& histograms,
      content::ResourceType resource_type) {
    VerifyContentScriptHistogram(
        histograms, testing::ElementsAre(base::Bucket(resource_type, 1)));
  }

  void VerifyContentScriptHistogramIsMissing(
      const base::HistogramTester& histograms) {
    VerifyContentScriptHistogram(histograms, testing::IsEmpty());
  }

  std::string PopString(content::DOMMessageQueue* message_queue) {
    std::string json;
    EXPECT_TRUE(message_queue->WaitForMessage(&json));
    base::JSONReader reader(base::JSON_ALLOW_TRAILING_COMMAS);
    std::unique_ptr<base::Value> value = reader.ReadToValue(json);
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

 private:
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

  std::string CreateFetchScript(const GURL& resource) {
    const char kXhrScriptTemplate[] = R"(
      fetch($1)
        .then(response => response.text())
        .then(text => domAutomationController.send(text))
        .catch(err => domAutomationController.send('error: ' + err));
    )";
    return content::JsReplace(kXhrScriptTemplate, resource);
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

    // Inject a content script that performs a cross-origin XHR to bar.com.
    EXPECT_TRUE(std::move(fetch_callback).Run(CreateFetchScript(url)));

    // Wait until the message comes back and extract result from the message.
    return PopString(&message_queue);
  }

  void VerifyContentScriptHistogram(
      const base::HistogramTester& histograms,
      testing::Matcher<std::vector<base::Bucket>> matcher) {
    // LogInitiatorSchemeBypassingDocumentBlocking is only implemented in the
    // pre-NetworkService CrossSiteDocumentResourceHandler, because we hope to
    // gather enough data before NetworkService ships.  Logging in
    // NetworkService world should be possible but would require an extra IPC
    // from NetworkService to the Browser process which seems like unnecessary
    // complexity, given that the metrics gathered won't be needed in the
    // long-term.
    if (base::FeatureList::IsEnabled(network::features::kNetworkService))
      return;

    // Verify that LogInitiatorSchemeBypassingDocumentBlocking returned early
    // for a request that wasn't from a content script.
    EXPECT_THAT(histograms.GetAllSamples(
                    "SiteIsolation.XSD.Browser.Allowed.ContentScript"),
                matcher);
  }

  TestExtensionDir dir_;
  const Extension* extension_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(CrossOriginReadBlockingExtensionTest);
};

IN_PROC_BROWSER_TEST_F(CrossOriginReadBlockingExtensionTest,
                       FromDeclarativeContentScript_NoSniffXml) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Load the test extension.
  GURL cross_site_resource(
      embedded_test_server()->GetURL("bar.com", "/nosniff.xml"));
  ASSERT_TRUE(InstallExtension(cross_site_resource));

  // Test case #1: Declarative script injected after a browser-initiated
  // navigation of the main frame.
  {
    // Monitor CORB behavior + result of the fetch.
    base::HistogramTester histograms;
    content::DOMMessageQueue message_queue;

    // Navigate to a foo.com page - this should trigger execution of the
    // |content_script| declared in the extension manifest.
    GURL page_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
    ui_test_utils::NavigateToURL(browser(), page_url);
    EXPECT_EQ(page_url, web_contents->GetMainFrame()->GetLastCommittedURL());
    EXPECT_EQ(url::Origin::Create(page_url),
              web_contents->GetMainFrame()->GetLastCommittedOrigin());

    // Extract results of the fetch done in the declarative content script.
    std::string fetch_result = PopString(&message_queue);

    // Verify that no blocking occurred.
    EXPECT_EQ("nosniff.xml - body\n", fetch_result);
    EXPECT_THAT(histograms.GetAllSamples("SiteIsolation.XSD.Browser.Blocked"),
                testing::IsEmpty());

    // Verify that LogInitiatorSchemeBypassingDocumentBlocking was called.
    VerifyContentScriptHistogramIsPresent(histograms,
                                          content::RESOURCE_TYPE_XHR);
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
    content::ExecuteScriptAsync(web_contents, kBlankSubframeInjectionScript);

    // Extract results of the fetch done in the declarative content script.
    std::string fetch_result = PopString(&message_queue);

    // Verify that no blocking occurred.
    EXPECT_EQ("nosniff.xml - body\n", fetch_result);
    EXPECT_THAT(histograms.GetAllSamples("SiteIsolation.XSD.Browser.Blocked"),
                testing::IsEmpty());

    // Verify that LogInitiatorSchemeBypassingDocumentBlocking was called.
    VerifyContentScriptHistogramIsPresent(histograms,
                                          content::RESOURCE_TYPE_XHR);
  }
}

// Test that verifies the current, baked-in (but not necessarily desirable
// behavior) where an extension that has permission to inject a content script
// to any page can also XHR (without CORS!) any cross-origin resource.
// See also https://crbug.com/846346.
IN_PROC_BROWSER_TEST_F(CrossOriginReadBlockingExtensionTest,
                       FromProgrammaticContentScript_NoSniffXml) {
  // Load the test extension.
  ASSERT_TRUE(InstallExtension());

  // Navigate to a foo.com page.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL page_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  ui_test_utils::NavigateToURL(browser(), page_url);
  EXPECT_EQ(page_url, web_contents->GetMainFrame()->GetLastCommittedURL());
  EXPECT_EQ(url::Origin::Create(page_url),
            web_contents->GetMainFrame()->GetLastCommittedOrigin());

  // Inject a content script that performs a cross-origin XHR to bar.com.
  base::HistogramTester histograms;
  GURL cross_site_resource(
      embedded_test_server()->GetURL("bar.com", "/nosniff.xml"));
  std::string fetch_result =
      FetchViaContentScript(cross_site_resource, web_contents);

  // Verify that no blocking occurred.
  EXPECT_EQ("nosniff.xml - body\n", fetch_result);
  EXPECT_THAT(histograms.GetAllSamples("SiteIsolation.XSD.Browser.Blocked"),
              testing::IsEmpty());

  // Verify that LogInitiatorSchemeBypassingDocumentBlocking was called.
  VerifyContentScriptHistogramIsPresent(histograms, content::RESOURCE_TYPE_XHR);
}

// Test that responses that would have been allowed by CORB anyway are not
// reported to LogInitiatorSchemeBypassingDocumentBlocking.
IN_PROC_BROWSER_TEST_F(CrossOriginReadBlockingExtensionTest,
                       FromProgrammaticContentScript_AllowedTextResource) {
  // Load the test extension.
  ASSERT_TRUE(InstallExtension());

  // Navigate to a foo.com page.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL page_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  ui_test_utils::NavigateToURL(browser(), page_url);
  EXPECT_EQ(page_url, web_contents->GetMainFrame()->GetLastCommittedURL());
  EXPECT_EQ(url::Origin::Create(page_url),
            web_contents->GetMainFrame()->GetLastCommittedOrigin());

  // Inject a content script that performs a cross-origin XHR to bar.com.
  //
  // StartsWith (rather than equality) is used in the verification step to
  // account for \n VS \r\n difference on Windows.
  base::HistogramTester histograms;
  GURL cross_site_resource(
      embedded_test_server()->GetURL("bar.com", "/save_page/text.txt"));
  std::string fetch_result =
      FetchViaContentScript(cross_site_resource, web_contents);

  // Verify that no blocking occurred.
  EXPECT_THAT(fetch_result,
              ::testing::StartsWith(
                  "text-object.txt: ae52dd09-9746-4b7e-86a6-6ada5e2680c2"));
  EXPECT_THAT(histograms.GetAllSamples("SiteIsolation.XSD.Browser.Blocked"),
              testing::IsEmpty());

  // Verify that we didn't call LogInitiatorSchemeBypassingDocumentBlocking
  // for a response that would have been allowed by CORB anyway.
  VerifyContentScriptHistogramIsMissing(histograms);
}

// Test that responses are blocked by CORB, but have empty response body are not
// reported to LogInitiatorSchemeBypassingDocumentBlocking.
IN_PROC_BROWSER_TEST_F(CrossOriginReadBlockingExtensionTest,
                       FromProgrammaticContentScript_EmptyAndBlocked) {
  // Load the test extension.
  ASSERT_TRUE(InstallExtension());

  // Navigate to a foo.com page.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL page_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  ui_test_utils::NavigateToURL(browser(), page_url);
  EXPECT_EQ(page_url, web_contents->GetMainFrame()->GetLastCommittedURL());
  EXPECT_EQ(url::Origin::Create(page_url),
            web_contents->GetMainFrame()->GetLastCommittedOrigin());

  // Inject a content script that performs a cross-origin XHR to bar.com.
  base::HistogramTester histograms;
  GURL cross_site_resource(
      embedded_test_server()->GetURL("bar.com", "/nosniff.empty"));
  EXPECT_EQ("", FetchViaContentScript(cross_site_resource, web_contents));

  // Verify that no blocking occurred.
  EXPECT_THAT(histograms.GetAllSamples("SiteIsolation.XSD.Browser.Blocked"),
              testing::IsEmpty());

  // Verify that we didn't call LogInitiatorSchemeBypassingDocumentBlocking
  // for a response that would have been blocked by CORB, but was empty.
  VerifyContentScriptHistogramIsMissing(histograms);
}

// Test that LogInitiatorSchemeBypassingDocumentBlocking exits early for
// requests that aren't from content scripts.
IN_PROC_BROWSER_TEST_F(CrossOriginReadBlockingExtensionTest,
                       FromBackgroundPage_NoSniffXml) {
  // Load the test extension.
  ASSERT_TRUE(InstallExtension());

  // Performs a cross-origin XHR from the background page.
  base::HistogramTester histograms;
  GURL cross_site_resource(
      embedded_test_server()->GetURL("bar.com", "/nosniff.xml"));
  std::string fetch_result = FetchViaBackgroundPage(cross_site_resource);

  // Verify that no blocking occurred.
  EXPECT_EQ("nosniff.xml - body\n", fetch_result);
  EXPECT_THAT(histograms.GetAllSamples("SiteIsolation.XSD.Browser.Blocked"),
              testing::IsEmpty());

  // Verify that LogInitiatorSchemeBypassingDocumentBlocking returned early
  // for a request that wasn't from a content script.
  VerifyContentScriptHistogramIsMissing(histograms);
}

// Test that requests from a extension page hosted in a foreground tab use
// relaxed CORB processing.
IN_PROC_BROWSER_TEST_F(CrossOriginReadBlockingExtensionTest,
                       FromForegroundPage_NoSniffXml) {
  // Load the test extension.
  ASSERT_TRUE(InstallExtension());

  // Navigate a tab to an extension page.
  ui_test_utils::NavigateToURL(browser(), GetExtensionResource("page.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(GetExtensionOrigin(),
            web_contents->GetMainFrame()->GetLastCommittedOrigin());

  // Test case #1: Fetch from a chrome-extension://... main frame.
  {
    // Perform a cross-origin XHR from the foreground extension page.
    base::HistogramTester histograms;
    GURL cross_site_resource(
        embedded_test_server()->GetURL("bar.com", "/nosniff.xml"));
    std::string fetch_result =
        FetchViaWebContents(cross_site_resource, web_contents);

    // Verify that no blocking occurred.
    EXPECT_EQ("nosniff.xml - body\n", fetch_result);
    EXPECT_THAT(histograms.GetAllSamples("SiteIsolation.XSD.Browser.Blocked"),
                testing::IsEmpty());

    // Verify that LogInitiatorSchemeBypassingDocumentBlocking returned early
    // for a request that wasn't from a content script.
    VerifyContentScriptHistogramIsMissing(histograms);
  }

  // Test case #2: Fetch from an about:srcdoc subframe of a
  // chrome-extension://... frame.
  {
    // Perform a cross-origin XHR from the foreground extension page.
    base::HistogramTester histograms;
    GURL cross_site_resource(
        embedded_test_server()->GetURL("bar.com", "/nosniff.xml"));
    std::string fetch_result =
        FetchViaSrcDocFrame(cross_site_resource, web_contents->GetMainFrame());

    // Verify that no blocking occurred.
    EXPECT_EQ("nosniff.xml - body\n", fetch_result);
    EXPECT_THAT(histograms.GetAllSamples("SiteIsolation.XSD.Browser.Blocked"),
                testing::IsEmpty());

    // Verify that LogInitiatorSchemeBypassingDocumentBlocking returned early
    // for a request that wasn't from a content script.
    VerifyContentScriptHistogramIsMissing(histograms);
  }
}

// Test that requests from an extension's service worker to the network use
// relaxed CORB processing (both in the case of requests that 1) are initiated
// by the service worker and/or 2) are ignored by the service worker and fall
// back to the network).
IN_PROC_BROWSER_TEST_F(CrossOriginReadBlockingExtensionTest,
                       FromServiceWorker_NoSniffXml) {
  // Load the test extension.
  ASSERT_TRUE(InstallExtension());

  // Register the service worker which injects "SERVICE WORKER INTERCEPT: "
  // prefix to the body of each response.
  const char kServiceWorkerScript[] = R"(
      self.addEventListener('fetch', function(event) {
          // Intercept all http requests to bar.com and inject
          // 'SERVICE WORKER INTERCEPT:' prefix.
          if (event.request.url.startsWith('http://bar.com')) {
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
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(GetExtensionOrigin(),
            web_contents->GetMainFrame()->GetLastCommittedOrigin());

  // Verify that the service worker controls the fetches.
  bool is_controlled_by_service_worker = false;
  ASSERT_TRUE(ExecuteScriptAndExtractBool(
      web_contents,
      "domAutomationController.send(!!navigator.serviceWorker.controller)",
      &is_controlled_by_service_worker));
  ASSERT_TRUE(is_controlled_by_service_worker);

  // Test case #1: Network fetch initiated by the service worker.
  //
  // This covers URLLoaderFactory owned by the ServiceWorker thread and created
  // created via CreateFactoryBundle called / posted indirectly from
  // EmbeddedWorkerInstance::StartTask::Start.
  {
    // Perform a cross-origin XHR from the foreground extension page.
    // This should be intercepted by the service worker installed above.
    base::HistogramTester histograms;
    GURL cross_site_resource_intercepted_by_service_worker(
        embedded_test_server()->GetURL("bar.com", "/nosniff.xml"));
    std::string fetch_result = FetchViaWebContents(
        cross_site_resource_intercepted_by_service_worker, web_contents);

    // Verify that no blocking occurred (and that the response really did go
    // through the service worker).
    EXPECT_EQ("SERVICE WORKER INTERCEPT: >>>nosniff.xml - body\n<<<",
              fetch_result);
    EXPECT_THAT(histograms.GetAllSamples("SiteIsolation.XSD.Browser.Blocked"),
                testing::IsEmpty());

    // Verify that LogInitiatorSchemeBypassingDocumentBlocking returned early
    // for a request that wasn't from a content script.
    VerifyContentScriptHistogramIsMissing(histograms);
  }

  // Test case #2: Network fetch used as a fallback when service worker ignores
  // the 'fetch' event.
  //
  // This covers URLLoaderFactory owned by the ServiceWorkerSubresourceLoader,
  // which can be different to the network loader factory owned by the
  // ServiceWorker thread (which is used in test case #1).
  {
    // Perform a cross-origin XHR from the foreground extension page.
    // This should be intercepted by the service worker installed above.
    base::HistogramTester histograms;
    GURL cross_site_resource_ignored_by_service_worker(
        embedded_test_server()->GetURL("other.com", "/nosniff.xml"));
    std::string fetch_result = FetchViaWebContents(
        cross_site_resource_ignored_by_service_worker, web_contents);

    // Verify that no blocking occurred.
    EXPECT_EQ("nosniff.xml - body\n", fetch_result);
    EXPECT_THAT(histograms.GetAllSamples("SiteIsolation.XSD.Browser.Blocked"),
                testing::IsEmpty());

    // Verify that LogInitiatorSchemeBypassingDocumentBlocking returned early
    // for a request that wasn't from a content script.
    VerifyContentScriptHistogramIsMissing(histograms);
  }
}

}  // namespace extensions
