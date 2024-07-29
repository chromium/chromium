// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>
#include <string_view>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/install_verifier.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/web_feature_histogram_tester.h"
#include "components/embedder_support/switches.h"
#include "components/error_page/content/browser/net_error_auto_reloader.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/private_network_access_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_builder.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/private_network_access_check_result.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"

namespace {

using blink::mojom::WebFeature;
using testing::IsEmpty;

// We use a custom page that explicitly disables its own favicon (by providing
// an invalid data: URL for it) so as to prevent the browser from making an
// automatic request to /favicon.ico. This is because the automatic request
// messes with our tests, in which we want to trigger a single request from the
// web page to a resource of our choice and observe the side-effect in metrics.
constexpr char kNoFaviconPath[] = "/private_network_access/no-favicon.html";

// Same as kNoFaviconPath, except it carries a header that makes the browser
// consider it came from the `public` address space, irrespective of the fact
// that we loaded the web page from localhost.
constexpr char kTreatAsPublicAddressPath[] =
    "/private_network_access/no-favicon-treat-as-public-address.html";

GURL SecureURL(const net::EmbeddedTestServer& server, const std::string& path) {
  // Test HTTPS servers cannot lie about their hostname, so they yield URLs
  // starting with https://localhost. http://localhost is already a secure
  // context, so we do not bother instantiating an HTTPS server.
  return server.GetURL(path);
}

GURL NonSecureURL(const net::EmbeddedTestServer& server,
                  const std::string& path) {
  return server.GetURL("foo.test", path);
}

GURL LocalSecureURL(const net::EmbeddedTestServer& server) {
  return SecureURL(server, kNoFaviconPath);
}

GURL LocalNonSecureURL(const net::EmbeddedTestServer& server) {
  return NonSecureURL(server, kNoFaviconPath);
}

GURL PublicSecureURL(const net::EmbeddedTestServer& server) {
  return SecureURL(server, kTreatAsPublicAddressPath);
}

GURL PublicNonSecureURL(const net::EmbeddedTestServer& server) {
  return NonSecureURL(server, kTreatAsPublicAddressPath);
}

// Similar to LocalNonSecure() but can be fetched by any origin.
GURL LocalNonSecureWithCrossOriginCors(const net::EmbeddedTestServer& server) {
  return SecureURL(server, "/cors-ok.txt");
}

// Path to a worker script that posts a message to its creator once loaded.
constexpr char kWorkerScriptPath[] = "/workers/post_ready.js";

// Same as above, but with PNA headers set correctly for preflight requests.
constexpr char kWorkerScriptWithPnaHeadersPath[] =
    "/workers/post_ready_with_pna_headers.js";

// The returned script evaluates to a boolean indicating whether the fetch
// succeeded or not.
std::string FetchScript(const GURL& url) {
  return content::JsReplace(
      "fetch($1).then(response => true).catch(error => false)", url);
}

std::string FetchWorkerScript(std::string_view relative_url) {
  constexpr char kTemplate[] = R"(
    new Promise((resolve) => {
      const worker = new Worker($1);
      worker.addEventListener("message", () => { resolve(true); });
      worker.addEventListener("error", () => { resolve(false); });
    });
  )";
  return content::JsReplace(kTemplate, relative_url);
}

// Path to a worker script that posts a message to each client that connects.
constexpr char kSharedWorkerScriptPath[] = "/workers/shared_post_ready.js";

// Same as above, but with PNA headers set correctly for preflight requests.
constexpr char kSharedWorkerScriptWithPnaHeadersPath[] =
    "/workers/shared_post_ready_with_pna_headers.js";

// Instantiates a shared worker script from `path`.
// If it loads successfully, the worker should post a message to each client
// that connects to it to signal success.
std::string FetchSharedWorkerScript(std::string_view path) {
  constexpr char kTemplate[] = R"(
    new Promise((resolve) => {
      const worker = new SharedWorker($1);
      worker.port.addEventListener("message", () => resolve(true));
      worker.addEventListener("error", () => resolve(false));
      worker.port.start();
    })
  )";

  return content::JsReplace(kTemplate, path);
}

std::vector<WebFeature> AllAddressSpaceFeatures() {
  return {
      WebFeature::kAddressSpacePrivateSecureContextEmbeddedLocal,
      WebFeature::kAddressSpacePrivateNonSecureContextEmbeddedLocal,
      WebFeature::kAddressSpacePublicSecureContextEmbeddedLocal,
      WebFeature::kAddressSpacePublicNonSecureContextEmbeddedLocal,
      WebFeature::kAddressSpaceUnknownSecureContextEmbeddedLocal,
      WebFeature::kAddressSpaceUnknownNonSecureContextEmbeddedLocal,
      WebFeature::kAddressSpacePublicSecureContextEmbeddedPrivate,
      WebFeature::kAddressSpacePublicNonSecureContextEmbeddedPrivate,
      WebFeature::kAddressSpaceUnknownSecureContextEmbeddedPrivate,
      WebFeature::kAddressSpaceUnknownNonSecureContextEmbeddedPrivate,
      WebFeature::kAddressSpacePrivateSecureContextNavigatedToLocal,
      WebFeature::kAddressSpacePrivateNonSecureContextNavigatedToLocal,
      WebFeature::kAddressSpacePublicSecureContextNavigatedToLocal,
      WebFeature::kAddressSpacePublicNonSecureContextNavigatedToLocal,
      WebFeature::kAddressSpaceUnknownSecureContextNavigatedToLocal,
      WebFeature::kAddressSpaceUnknownNonSecureContextNavigatedToLocal,
      WebFeature::kAddressSpacePublicSecureContextNavigatedToPrivate,
      WebFeature::kAddressSpacePublicNonSecureContextNavigatedToPrivate,
      WebFeature::kAddressSpaceUnknownSecureContextNavigatedToPrivate,
      WebFeature::kAddressSpaceUnknownNonSecureContextNavigatedToPrivate,
      WebFeature::kPrivateNetworkAccessFetchedWorkerScript,
      WebFeature::kPrivateNetworkAccessFetchedSubFrame,
      WebFeature::kPrivateNetworkAccessFetchedTopFrame,
      WebFeature::kPrivateNetworkAccessWithinWorker,
      WebFeature::kPrivateNetworkAccessPreflightError,
      WebFeature::kPrivateNetworkAccessPreflightSuccess,
      WebFeature::kPrivateNetworkAccessPreflightWarning,
  };
}

// Private Network Access is a web platform specification aimed at securing
// requests made from public websites to the private network and localhost.
//
// It is mostly implemented in content/, but some of its integrations (
// (with Blink UseCounters, with chrome/-specific special schemes) cannot be
// tested in content/, however, thus we define this standalone test here.
//
// See also:
//
//  - specification: https://wicg.github.io/private-network-access.
//  - feature browsertests:
//    //content/browser/renderer_host/private_network_access_browsertest.cc
//
class PrivateNetworkAccessBrowserTestBase : public InProcessBrowserTest {
 public:
  PrivateNetworkAccessBrowserTestBase(
      std::vector<base::test::FeatureRef> enabled_features,
      std::vector<base::test::FeatureRef> disabled_features) {
    features_.InitWithFeatures(enabled_features, disabled_features);
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  // Never returns nullptr. The returned server is already Start()ed.
  //
  // NOTE: This is defined as a method on the test fixture instead of a free
  // function because GetChromeTestDataDir() is a test fixture method itself.
  // We return a unique_ptr because EmbeddedTestServer is not movable and C++17
  // support is not available at time of writing.
  std::unique_ptr<net::EmbeddedTestServer> NewServer(
      net::EmbeddedTestServer::Type server_type =
          net::EmbeddedTestServer::TYPE_HTTP) {
    std::unique_ptr<net::EmbeddedTestServer> server =
        std::make_unique<net::EmbeddedTestServer>(server_type);
    server->AddDefaultHandlers(GetChromeTestDataDir());
    EXPECT_TRUE(server->Start());
    return server;
  }

 protected:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // The public key used to verify test trial tokens.
    // See: //docs/origin_trial_integration.md
    constexpr char kOriginTrialTestPublicKey[] =
        "dRCs+TocuKkocNKa0AtZ4awrt9XKH2SQCI6o4FY6BNA=";
    command_line->AppendSwitchASCII(embedder_support::kOriginTrialPublicKey,
                                    kOriginTrialTestPublicKey);
  }

 private:
  base::test::ScopedFeatureList features_;
};

class PrivateNetworkAccessWithFeatureDisabledBrowserTest
    : public PrivateNetworkAccessBrowserTestBase {
 public:
  PrivateNetworkAccessWithFeatureDisabledBrowserTest()
      : PrivateNetworkAccessBrowserTestBase(
            {},
            {
                features::kBlockInsecurePrivateNetworkRequests,
                features::kBlockInsecurePrivateNetworkRequestsFromPrivate,
            }) {}
};

struct IsWarningOnlyTestData {
  bool is_warning_only;
};

const IsWarningOnlyTestData kIsWarningOnlyTestData[] = {{false}, {true}};

class PrivateNetworkAccessWithFeatureEnabledBrowserTest
    : public PrivateNetworkAccessBrowserTestBase {
 public:
  explicit PrivateNetworkAccessWithFeatureEnabledBrowserTest(
      bool is_warning_only = false)
      : PrivateNetworkAccessBrowserTestBase(
            {
                blink::features::kPlzDedicatedWorker,
                features::kBlockInsecurePrivateNetworkRequests,
                features::kBlockInsecurePrivateNetworkRequestsFromPrivate,
                features::kBlockInsecurePrivateNetworkRequestsDeprecationTrial,
                features::kPrivateNetworkAccessSendPreflights,
                features::kPrivateNetworkAccessForNavigations,
                features::kPrivateNetworkAccessForWorkers,
            },
            is_warning_only
                ? std::vector<base::test::FeatureRef>()
                : std::vector<base::test::FeatureRef>({
                      features::kPrivateNetworkAccessForWorkersWarningOnly,
                  })) {}
  void SetUpCommandLine(base::CommandLine* command_line) override {
    PrivateNetworkAccessBrowserTestBase::SetUpCommandLine(command_line);
  }
};

class PrivateNetworkAccessWithFeatureEnabledWorkerBrowserTest
    : public PrivateNetworkAccessWithFeatureEnabledBrowserTest,
      public testing::WithParamInterface<IsWarningOnlyTestData> {
 public:
  PrivateNetworkAccessWithFeatureEnabledWorkerBrowserTest()
      : PrivateNetworkAccessWithFeatureEnabledBrowserTest(
            GetParam().is_warning_only) {}
};

class PrivateNetworkAccessRespectPreflightResultsBrowserTest
    : public PrivateNetworkAccessBrowserTestBase,
      public testing::WithParamInterface<IsWarningOnlyTestData> {
 public:
  PrivateNetworkAccessRespectPreflightResultsBrowserTest()
      : PrivateNetworkAccessBrowserTestBase(
            {
                blink::features::kPlzDedicatedWorker,
                features::kBlockInsecurePrivateNetworkRequests,
                features::kPrivateNetworkAccessSendPreflights,
                features::kPrivateNetworkAccessRespectPreflightResults,
                features::kPrivateNetworkAccessForWorkers,
            },
            GetParam().is_warning_only
                ? std::vector<base::test::FeatureRef>()
                : std::vector<base::test::FeatureRef>({
                      features::kPrivateNetworkAccessForWorkersWarningOnly,
                  })) {}
};

// ================
// USECOUNTER TESTS
// ================
//
// UseCounters are translated into UMA histograms at the chrome/ layer, by the
// page_load_metrics component. These tests verify that UseCounters are recorded
// correctly by Private Network Access code in the right circumstances.

// This test verifies that no feature is counted for the initial navigation from
// a new tab to a page served by localhost.
//
// Regression test for https://crbug.com/1134601.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessWithFeatureDisabledBrowserTest,
                       DoesNotRecordAddressSpaceFeatureForInitialNavigation) {
  WebFeatureHistogramTester feature_histogram_tester;
  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();

  EXPECT_TRUE(content::NavigateToURL(web_contents(), PublicSecureURL(*server)));

  EXPECT_THAT(
      feature_histogram_tester.GetNonZeroCounts(AllAddressSpaceFeatures()),
      IsEmpty());
}

// This test verifies that no feature is counted for top-level navigations from
// a public page to a local page.
//
// TODO(crbug.com/40149351): Revisit this once the story around top-level
// navigations is closer to being resolved. Counting these events will help
// decide what to do.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessWithFeatureDisabledBrowserTest,
                       DoesNotRecordAddressSpaceFeatureForRegularNavigation) {
  WebFeatureHistogramTester feature_histogram_tester;
  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();

  EXPECT_TRUE(content::NavigateToURL(web_contents(), PublicSecureURL(*server)));
  EXPECT_TRUE(content::NavigateToURL(web_contents(), LocalSecureURL(*server)));

  EXPECT_THAT(
      feature_histogram_tester.GetNonZeroCounts(AllAddressSpaceFeatures()),
      IsEmpty());
}

// This test verifies that when a non-secure context served from the public
// address space loads a resource from the private network, the correct
// WebFeature
// is use-counted.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessWithFeatureDisabledBrowserTest,
                       RecordsAddressSpaceFeatureForFetchInNonSecureContext) {
  WebFeatureHistogramTester feature_histogram_tester;
  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();

  EXPECT_TRUE(
      content::NavigateToURL(web_contents(), PublicNonSecureURL(*server)));
  EXPECT_THAT(
      feature_histogram_tester.GetNonZeroCounts(AllAddressSpaceFeatures()),
      IsEmpty());

  EXPECT_EQ(true, content::EvalJs(web_contents(), R"(
    fetch("/defaultresponse").then(response => response.ok)
  )"));

  feature_histogram_tester.ExpectCounts(AddFeatureCounts(
      AllZeroFeatureCounts(AllAddressSpaceFeatures()),
      {
          {WebFeature::kAddressSpacePublicNonSecureContextEmbeddedLocal, 1},
      }));
}

// This test verifies that when the user navigates a `public` document to a
// document served by a non-public IP, no address space feature is recorded.
IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessWithFeatureEnabledBrowserTest,
    DoesNotRecordAddressSpaceFeatureForBrowserInitiatedNavigation) {
  WebFeatureHistogramTester feature_histogram_tester;
  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();

  EXPECT_TRUE(
      content::NavigateToURL(web_contents(), PublicNonSecureURL(*server)));

  EXPECT_TRUE(
      content::NavigateToURL(web_contents(), LocalNonSecureURL(*server)));

  EXPECT_THAT(
      feature_histogram_tester.GetNonZeroCounts(AllAddressSpaceFeatures()),
      IsEmpty());
}

// This test verifies that when a `public` document navigates itself to a
// document served by a non-public IP, the correct address space feature is
// recorded.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessWithFeatureDisabledBrowserTest,
                       RecordsAddressSpaceFeatureForNavigation) {
  WebFeatureHistogramTester feature_histogram_tester;
  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();

  EXPECT_TRUE(
      content::NavigateToURL(web_contents(), PublicNonSecureURL(*server)));
  EXPECT_THAT(
      feature_histogram_tester.GetNonZeroCounts(AllAddressSpaceFeatures()),
      IsEmpty());

  EXPECT_TRUE(content::NavigateToURLFromRenderer(web_contents(),
                                                 LocalNonSecureURL(*server)));

  feature_histogram_tester.ExpectCounts(AddFeatureCounts(
      AllZeroFeatureCounts(AllAddressSpaceFeatures()),
      {
          {WebFeature::kAddressSpacePublicNonSecureContextNavigatedToLocal, 1},
          {WebFeature::kPrivateNetworkAccessFetchedTopFrame, 1},
      }));
}

// This test verifies that when a `public` document navigates itself to a
// document served by a non-public IP, the correct address space feature is
// recorded, even if the target document carries a CSP `treat-as-public-address`
// directive.
IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessWithFeatureDisabledBrowserTest,
    RecordsAddressSpaceFeatureForNavigationToTreatAsPublicAddress) {
  WebFeatureHistogramTester feature_histogram_tester;
  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();

  EXPECT_TRUE(
      content::NavigateToURL(web_contents(), PublicNonSecureURL(*server)));
  EXPECT_THAT(
      feature_histogram_tester.GetNonZeroCounts(AllAddressSpaceFeatures()),
      IsEmpty());

  // Navigate to a different URL with the same CSP directive. If we just tried
  // to navigate to `PublicNonSecureURL(*server)`, nothing would happen.
  EXPECT_TRUE(content::NavigateToURLFromRenderer(
      web_contents(),
      NonSecureURL(
          *server,
          "/set-header?Content-Security-Policy: treat-as-public-address")));

  feature_histogram_tester.ExpectCounts(AddFeatureCounts(
      AllZeroFeatureCounts(AllAddressSpaceFeatures()),
      {
          {WebFeature::kAddressSpacePublicNonSecureContextNavigatedToLocal, 1},
          {WebFeature::kPrivateNetworkAccessFetchedTopFrame, 1},
      }));
}

// This test verifies that when a page embeds an empty iframe pointing to
// about:blank, no address space feature is recorded. It serves as a basis for
// comparison with the following tests, which test behavior with iframes.
IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessWithFeatureDisabledBrowserTest,
    DoesNotRecordAddressSpaceFeatureForChildAboutBlankNavigation) {
  WebFeatureHistogramTester feature_histogram_tester;
  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();

  EXPECT_TRUE(
      content::NavigateToURL(web_contents(), PublicNonSecureURL(*server)));
  EXPECT_EQ(true, content::EvalJs(web_contents(), R"(
    new Promise(resolve => {
      const child = document.createElement("iframe");
      child.src = "about:blank";
      child.onload = () => { resolve(true); };
      document.body.appendChild(child);
    })
  )"));

  EXPECT_THAT(
      feature_histogram_tester.GetNonZeroCounts(AllAddressSpaceFeatures()),
      IsEmpty());
}

// This test verifies that when a non-secure context served from the public
// address space loads a child frame from the private network, the correct
// WebFeature is use-counted.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessWithFeatureDisabledBrowserTest,
                       RecordsAddressSpaceFeatureForChildNavigation) {
  WebFeatureHistogramTester feature_histogram_tester;
  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();

  EXPECT_TRUE(
      content::NavigateToURL(web_contents(), PublicNonSecureURL(*server)));
  EXPECT_THAT(
      feature_histogram_tester.GetNonZeroCounts(AllAddressSpaceFeatures()),
      IsEmpty());

  std::string_view script_template = R"(
    new Promise(resolve => {
      const child = document.createElement("iframe");
      child.src = $1;
      child.onload = () => { resolve(true); };
      document.body.appendChild(child);
    })
  )";
  EXPECT_EQ(true,
            content::EvalJs(web_contents(),
                            content::JsReplace(script_template,
                                               LocalNonSecureURL(*server))));

  feature_histogram_tester.ExpectCounts(AddFeatureCounts(
      AllZeroFeatureCounts(AllAddressSpaceFeatures()),
      {
          {WebFeature::kAddressSpacePublicNonSecureContextNavigatedToLocal, 1},
          {WebFeature::kPrivateNetworkAccessFetchedSubFrame, 1},
      }));
}

// This test verifies that when a non-secure context served from the public
// address space loads a grand-child frame from the private network, the correct
// WebFeature is use-counted. If inheritance did not work correctly, the
// intermediate about:blank frame might confuse the address space logic.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessWithFeatureDisabledBrowserTest,
                       RecordsAddressSpaceFeatureForGrandchildNavigation) {
  WebFeatureHistogramTester feature_histogram_tester;
  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();

  EXPECT_TRUE(
      content::NavigateToURL(web_contents(), PublicNonSecureURL(*server)));
  EXPECT_THAT(
      feature_histogram_tester.GetNonZeroCounts(AllAddressSpaceFeatures()),
      IsEmpty());

  std::string_view script_template = R"(
    function addChildFrame(doc, src) {
      return new Promise(resolve => {
        const child = doc.createElement("iframe");
        child.src = src;
        child.onload = () => { resolve(child); };
        doc.body.appendChild(child);
      });
    }

    addChildFrame(document, "about:blank")
      .then(child => addChildFrame(child.contentDocument, $1))
      .then(grandchild =>  true);
  )";
  EXPECT_EQ(true,
            content::EvalJs(web_contents(),
                            content::JsReplace(script_template,
                                               LocalNonSecureURL(*server))));

  feature_histogram_tester.ExpectCounts(AddFeatureCounts(
      AllZeroFeatureCounts(AllAddressSpaceFeatures()),
      {
          {WebFeature::kAddressSpacePublicNonSecureContextNavigatedToLocal, 1},
          {WebFeature::kPrivateNetworkAccessFetchedSubFrame, 1},
      }));
}

// This test verifies that the right address space feature is recorded when a
// navigation results in a private network request. Specifically, in this test
// the document being navigated is not the one initiating the navigation (the
// latter being the "remote initiator" referenced by the test name).
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessWithFeatureDisabledBrowserTest,
                       RecordsAddressSpaceFeatureForRemoteInitiatorNavigation) {
  WebFeatureHistogramTester feature_histogram_tester;
  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();

  EXPECT_TRUE(content::NavigateToURL(
      web_contents(),
      NonSecureURL(
          *server,
          "/private_network_access/remote-initiator-navigation.html")));
  EXPECT_THAT(
      feature_histogram_tester.GetNonZeroCounts(AllAddressSpaceFeatures()),
      IsEmpty());

  EXPECT_EQ(true, content::EvalJs(web_contents(), content::JsReplace(R"(
    runTest({
      url: "/defaultresponse",
    });
  )")));

  feature_histogram_tester.ExpectCounts(AddFeatureCounts(
      AllZeroFeatureCounts(AllAddressSpaceFeatures()),
      {
          {WebFeature::kAddressSpacePublicNonSecureContextNavigatedToLocal, 1},
          {WebFeature::kPrivateNetworkAccessFetchedSubFrame, 1},
      }));
}

// This test verifies that when the initiator of a navigation is no longer
// around by the time the navigation finishes, then no address space feature is
// recorded, and importantly: the browser does not crash.
IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessWithFeatureDisabledBrowserTest,
    DoesNotRecordAddressSpaceFeatureForClosedInitiatorNavigation) {
  WebFeatureHistogramTester feature_histogram_tester;
  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();

  EXPECT_TRUE(content::NavigateToURL(
      web_contents(),
      NonSecureURL(
          *server,
          "/private_network_access/remote-initiator-navigation.html")));

  EXPECT_EQ(true, content::EvalJs(web_contents(), R"(
    runTest({
      url: new URL("/slow?3", window.location).href,
      initiatorBehavior: "close",
    });
  )"));

  EXPECT_THAT(
      feature_histogram_tester.GetNonZeroCounts(AllAddressSpaceFeatures()),
      IsEmpty());
}

// This test verifies that when the initiator of a navigation has already
// navigated itself by the time the navigation finishes, then no address space
// feature is recorded.
IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessWithFeatureDisabledBrowserTest,
    DoesNotRecordAddressSpaceFeatureForMissingInitiatorNavigation) {
  WebFeatureHistogramTester feature_histogram_tester;
  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();

  EXPECT_TRUE(content::NavigateToURL(
      web_contents(),
      NonSecureURL(
          *server,
          "/private_network_access/remote-initiator-navigation.html")));

  EXPECT_EQ(true, content::EvalJs(web_contents(), R"(
    runTest({
      url: new URL("/slow?3", window.location).href,
      initiatorBehavior: "navigate",
    });
  )"));

  EXPECT_THAT(
      feature_histogram_tester.GetNonZeroCounts(AllAddressSpaceFeatures()),
      IsEmpty());
}

// This test verifies that private network requests that are blocked are not
// use-counted.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessWithFeatureEnabledBrowserTest,
                       DoesNotRecordAddressSpaceFeatureForBlockedRequests) {
  WebFeatureHistogramTester feature_histogram_tester;
  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();

  EXPECT_TRUE(
      content::NavigateToURL(web_contents(), PublicNonSecureURL(*server)));
  EXPECT_THAT(
      feature_histogram_tester.GetNonZeroCounts(AllAddressSpaceFeatures()),
      IsEmpty());

  base::HistogramTester base_histogram_tester;

  EXPECT_EQ(true, content::EvalJs(web_contents(), R"(
    fetch("/defaultresponse").catch(() => true)
  )"));

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  base_histogram_tester.ExpectBucketCount(
      "Security.PrivateNetworkAccess.CheckResult",
      network::PrivateNetworkAccessCheckResult::kBlockedByPolicyBlock, 1);

  EXPECT_THAT(
      feature_histogram_tester.GetNonZeroCounts(AllAddressSpaceFeatures()),
      IsEmpty());
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessWithFeatureEnabledBrowserTest,
                       RecordsAddressSpaceFeatureForDeprecationTrial) {
  WebFeatureHistogramTester feature_histogram_tester;
  content::DeprecationTrialURLLoaderInterceptor interceptor;

  EXPECT_TRUE(content::NavigateToURL(web_contents(), interceptor.EnabledUrl()));

  EXPECT_EQ(
      feature_histogram_tester.GetCount(
          WebFeature::
              kPrivateNetworkAccessNonSecureContextsAllowedDeprecationTrial),
      1);
}

// This test verifies that resources proxied through a proxy on localhost can
// be fetched from documents in the public IP address space.
// Regression test for https://crbug.com/1253239.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessWithFeatureEnabledBrowserTest,
                       ProxiedResourcesAllowed) {
  auto server = NewServer();

  EXPECT_TRUE(
      content::NavigateToURL(web_contents(), PublicNonSecureURL(*server)));

  browser()->profile()->GetPrefs()->SetDict(
      proxy_config::prefs::kProxy,
      ProxyConfigDictionary::CreateFixedServers(
          server->host_port_pair().ToString(), ""));
  ProfileNetworkContextServiceFactory::GetForContext(browser()->profile())
      ->FlushProxyConfigMonitorForTesting();

  WebFeatureHistogramTester feature_histogram_tester;

  EXPECT_EQ(true, content::EvalJs(web_contents(), R"(
    fetch("/defaultresponse").then(response => response.ok)
  )"));

  EXPECT_THAT(
      feature_histogram_tester.GetNonZeroCounts(AllAddressSpaceFeatures()),
      IsEmpty());
}

// This test verifies that resources fetched from cache are subject to Private
// Network Access checks. When the fetch is blocked, it is not use-counted.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessWithFeatureEnabledBrowserTest,
                       DoesNotRecordAddressSpaceFeatureForCachedBlocked) {
  auto server = NewServer();

  EXPECT_TRUE(
      content::NavigateToURL(web_contents(), LocalNonSecureURL(*server)));

  // Load the resource a first time, to prime the HTTP cache.
  //
  // This caching hinges on the fact that `PublicNonSecureURL(*server)` is
  // same-origin with `LocalNonSecureURL(*server)` (the public one just uses
  // the `Content-Security-Policy: treat-as-public-address` header). Therefore
  // both documents share the same cache key.
  EXPECT_EQ(true, content::EvalJs(web_contents(), R"(
    fetch("/cachetime").then(response => response.ok)
  )"));

  EXPECT_TRUE(
      content::NavigateToURL(web_contents(), PublicNonSecureURL(*server)));

  WebFeatureHistogramTester feature_histogram_tester;

  EXPECT_EQ(false, content::EvalJs(web_contents(), R"(
    fetch("/cachetime").then(response => true).catch(error => false)
  )"));

  EXPECT_THAT(
      feature_histogram_tester.GetNonZeroCounts(AllAddressSpaceFeatures()),
      IsEmpty());
}

// This test verifies that resources fetched from cache are subject to Private
// Network Access checks. When the fetch is allowed, it is use-counted.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessWithFeatureEnabledBrowserTest,
                       RecordsAddressSpaceFeatureForCachedResource) {
  auto server = NewServer();

  EXPECT_TRUE(content::NavigateToURL(web_contents(), LocalSecureURL(*server)));

  // Load the resource a first time, to prime the HTTP cache.
  //
  // This caching hinges on the fact that `PublicNonSecureURL(*server)` is
  // same-origin with `LocalNonSecureURL(*server)` (the public one just uses
  // the `Content-Security-Policy: treat-as-public-address` header). Therefore
  // both documents share the same cache key.
  EXPECT_EQ(true, content::EvalJs(web_contents(), R"(
    fetch("/cachetime").then(response => response.ok)
  )"));

  EXPECT_TRUE(content::NavigateToURL(web_contents(), PublicSecureURL(*server)));

  WebFeatureHistogramTester feature_histogram_tester;

  EXPECT_EQ(true, content::EvalJs(web_contents(), R"(
    fetch("/cachetime").then(response => response.ok)
  )"));

  feature_histogram_tester.ExpectCounts(AddFeatureCounts(
      AllZeroFeatureCounts(AllAddressSpaceFeatures()),
      {
          {WebFeature::kAddressSpacePublicSecureContextEmbeddedLocal, 1},
      }));
}

// This test verifies that a UseCounter is recorded when a document makes a
// private network request to load a worker script from a non-secure context,
// even when the PNA for workers feature is disabled.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessWithFeatureDisabledBrowserTest,
                       RecordsFeatureForWorkerScriptFetchFromNonSecure) {
  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();

  EXPECT_TRUE(
      content::NavigateToURL(web_contents(), PublicNonSecureURL(*server)));

  WebFeatureHistogramTester feature_histogram_tester;

  EXPECT_EQ(true, content::EvalJs(web_contents(),
                                  FetchWorkerScript(kWorkerScriptPath)));

  feature_histogram_tester.ExpectCounts(AddFeatureCounts(
      AllZeroFeatureCounts(AllAddressSpaceFeatures()),
      {
          {WebFeature::kPrivateNetworkAccessFetchedWorkerScript, 1},
      }));
}

// This test verifies that a UseCounter is recorded when a document makes a
// private network request to load a worker script from a non-secure context,
// and the request fails due to PNA unless it's in warning-only mode.
IN_PROC_BROWSER_TEST_P(PrivateNetworkAccessWithFeatureEnabledWorkerBrowserTest,
                       RecordsFeatureForWorkerScriptFetchFromNonSecure) {
  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();

  EXPECT_TRUE(
      content::NavigateToURL(web_contents(), PublicNonSecureURL(*server)));

  WebFeatureHistogramTester feature_histogram_tester;

  EXPECT_EQ(
      GetParam().is_warning_only,
      content::EvalJs(web_contents(), FetchWorkerScript(kWorkerScriptPath)));

  feature_histogram_tester.ExpectCounts(AddFeatureCounts(
      AllZeroFeatureCounts(AllAddressSpaceFeatures()),
      {
          {WebFeature::kPrivateNetworkAccessFetchedWorkerScript, 1},
      }));
}

// This test verifies that a UseCounter is recorded when a document makes a
// private network request to load a worker script from a secure context, even
// when the PNA for workers feature is disabled.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessWithFeatureDisabledBrowserTest,
                       RecordsFeatureForWorkerScriptFetchFromSecure) {
  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();

  EXPECT_TRUE(content::NavigateToURL(web_contents(), PublicSecureURL(*server)));

  WebFeatureHistogramTester feature_histogram_tester;

  EXPECT_EQ(true, content::EvalJs(web_contents(),
                                  FetchWorkerScript(kWorkerScriptPath)));

  feature_histogram_tester.ExpectCounts(AddFeatureCounts(
      AllZeroFeatureCounts(AllAddressSpaceFeatures()),
      {
          {WebFeature::kPrivateNetworkAccessFetchedWorkerScript, 1},
      }));
}

// This test verifies that a UseCounter is recorded when a document makes a
// private network request to load a worker script from a secure context, does
// not send preflights because the request is same-origin and the origin is
// potentially trustworthy, and loads the script anyway.
IN_PROC_BROWSER_TEST_P(PrivateNetworkAccessWithFeatureEnabledWorkerBrowserTest,
                       RecordsFeatureForWorkerScriptFetchFromSecure) {
  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();

  EXPECT_TRUE(content::NavigateToURL(web_contents(), PublicSecureURL(*server)));

  WebFeatureHistogramTester feature_histogram_tester;

  EXPECT_EQ(true, content::EvalJs(web_contents(),
                                  FetchWorkerScript(kWorkerScriptPath)));

  feature_histogram_tester.ExpectCounts(AddFeatureCounts(
      AllZeroFeatureCounts(AllAddressSpaceFeatures()),
      {
          {WebFeature::kPrivateNetworkAccessFetchedWorkerScript, 1},
          {WebFeature::kPrivateNetworkAccessPreflightWarning, 1},
      }));
}

// This test verifies that a UseCounter is recorded when a document makes a
// private network request to load a worker script from a secure context.
// The request should always succeed because it same origin and the origin is
// potentially trustworthy.
IN_PROC_BROWSER_TEST_P(PrivateNetworkAccessRespectPreflightResultsBrowserTest,
                       RecordsFeatureForWorkerScriptFetchErrorFromSecure) {
  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();

  EXPECT_TRUE(content::NavigateToURL(web_contents(), PublicSecureURL(*server)));

  WebFeatureHistogramTester feature_histogram_tester;

  EXPECT_EQ(true, content::EvalJs(web_contents(),
                                  FetchWorkerScript(kWorkerScriptPath)));

  feature_histogram_tester.ExpectCounts(AddFeatureCounts(
      AllZeroFeatureCounts(AllAddressSpaceFeatures()),
      {
          {WebFeature::kPrivateNetworkAccessFetchedWorkerScript, 1},
      }));
}

// This test verifies that a UseCounter is recorded when a document makes a
// private network request to load a worker script from a secure context, does
// not send a preflight request because the request is same-origin and the
// origin is potentially trustworthy, and succeeds in loading the script.
IN_PROC_BROWSER_TEST_P(PrivateNetworkAccessRespectPreflightResultsBrowserTest,
                       RecordsFeatureForWorkerScriptFetchSuccessFromSecure) {
  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();

  EXPECT_TRUE(content::NavigateToURL(web_contents(), PublicSecureURL(*server)));

  WebFeatureHistogramTester feature_histogram_tester;

  EXPECT_EQ(true, content::EvalJs(
                      web_contents(),
                      FetchWorkerScript(kWorkerScriptWithPnaHeadersPath)));

  feature_histogram_tester.ExpectCounts(AddFeatureCounts(
      AllZeroFeatureCounts(AllAddressSpaceFeatures()),
      {
          {WebFeature::kPrivateNetworkAccessFetchedWorkerScript, 1},
      }));
}

// This test verifies that a UseCounter is recorded when a document makes a
// private network request to load a shared worker script from a non-secure
// context, even when the PNA for workers feature is disabled.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessWithFeatureDisabledBrowserTest,
                       RecordsFeatureForSharedWorkerScriptFetchFromNonSecure) {
  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();

  EXPECT_TRUE(
      content::NavigateToURL(web_contents(), PublicNonSecureURL(*server)));

  WebFeatureHistogramTester feature_histogram_tester;

  EXPECT_EQ(true,
            content::EvalJs(web_contents(),
                            FetchSharedWorkerScript(kSharedWorkerScriptPath)));

  feature_histogram_tester.ExpectCounts(AddFeatureCounts(
      AllZeroFeatureCounts(AllAddressSpaceFeatures()),
      {
          {WebFeature::kPrivateNetworkAccessFetchedWorkerScript, 1},
      }));
}

// This test verifies that a UseCounter is recorded when a document makes a
// private network request to load a shared worker script from a non-secure
// context, and the request fails due to PNA unless it's in warning-only mode.
IN_PROC_BROWSER_TEST_P(PrivateNetworkAccessWithFeatureEnabledWorkerBrowserTest,
                       RecordsFeatureForSharedWorkerScriptFetchFromNonSecure) {
  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();

  EXPECT_TRUE(
      content::NavigateToURL(web_contents(), PublicNonSecureURL(*server)));

  WebFeatureHistogramTester feature_histogram_tester;

  EXPECT_EQ(GetParam().is_warning_only,
            content::EvalJs(web_contents(),
                            FetchSharedWorkerScript(kSharedWorkerScriptPath)));

  feature_histogram_tester.ExpectCounts(AddFeatureCounts(
      AllZeroFeatureCounts(AllAddressSpaceFeatures()),
      {
          {WebFeature::kPrivateNetworkAccessFetchedWorkerScript, 1},
      }));
}

// This test verifies that a UseCounter is recorded when a document makes a
// private network request to load a shared worker script from a secure context,
// even when the PNA for workers feature is disabled.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessWithFeatureDisabledBrowserTest,
                       RecordsFeatureForSharedWorkerScriptFetchFromSecure) {
  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();

  EXPECT_TRUE(content::NavigateToURL(web_contents(), PublicSecureURL(*server)));

  WebFeatureHistogramTester feature_histogram_tester;

  EXPECT_EQ(true,
            content::EvalJs(web_contents(),
                            FetchSharedWorkerScript(kSharedWorkerScriptPath)));

  feature_histogram_tester.ExpectCounts(AddFeatureCounts(
      AllZeroFeatureCounts(AllAddressSpaceFeatures()),
      {
          {WebFeature::kPrivateNetworkAccessFetchedWorkerScript, 1},
      }));
}

// This test verifies that a UseCounter is recorded when a document makes a
// private network request to load a shared worker script from a secure context,
// does not send a preflight request because the request is same-origin and the
// origin is potentially trustworthy, and loads the script anyway.
IN_PROC_BROWSER_TEST_P(PrivateNetworkAccessWithFeatureEnabledWorkerBrowserTest,
                       RecordsFeatureForSharedWorkerScriptFetchFromSecure) {
  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();

  EXPECT_TRUE(content::NavigateToURL(web_contents(), PublicSecureURL(*server)));

  WebFeatureHistogramTester feature_histogram_tester;

  EXPECT_EQ(true,
            content::EvalJs(web_contents(),
                            FetchSharedWorkerScript(kSharedWorkerScriptPath)));

  feature_histogram_tester.ExpectCounts(AddFeatureCounts(
      AllZeroFeatureCounts(AllAddressSpaceFeatures()),
      {
          {WebFeature::kPrivateNetworkAccessFetchedWorkerScript, 1},
          {WebFeature::kPrivateNetworkAccessPreflightWarning, 1},
      }));
}

// This test verifies that a UseCounter is recorded when a document makes a
// private network request to load a shared worker script from a secure context.
// The request should always succeed because it is same-origin and the origin is
// potentially trustworthy.
IN_PROC_BROWSER_TEST_P(
    PrivateNetworkAccessRespectPreflightResultsBrowserTest,
    RecordsFeatureForSharedWorkerScriptFetchErrorFromSecure) {
  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();

  EXPECT_TRUE(content::NavigateToURL(web_contents(), PublicSecureURL(*server)));

  WebFeatureHistogramTester feature_histogram_tester;

  EXPECT_EQ(true,
            content::EvalJs(web_contents(),
                            FetchSharedWorkerScript(kSharedWorkerScriptPath)));

  feature_histogram_tester.ExpectCounts(AddFeatureCounts(
      AllZeroFeatureCounts(AllAddressSpaceFeatures()),
      {
          {WebFeature::kPrivateNetworkAccessFetchedWorkerScript, 1},
      }));
}

// This test verifies that a UseCounter is recorded when a document makes a
// private network request to load a shared worker script from a secure context,
// does not send a preflight request because the request is same-origin and the
// origin is potentially trustworthy, and succeeds in loading the script.
IN_PROC_BROWSER_TEST_P(
    PrivateNetworkAccessRespectPreflightResultsBrowserTest,
    RecordsFeatureForSharedWorkerScriptFetchSuccessFromSecure) {
  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();

  EXPECT_TRUE(content::NavigateToURL(web_contents(), PublicSecureURL(*server)));

  WebFeatureHistogramTester feature_histogram_tester;

  EXPECT_EQ(true, content::EvalJs(web_contents(),
                                  FetchSharedWorkerScript(
                                      kSharedWorkerScriptWithPnaHeadersPath)));

  feature_histogram_tester.ExpectCounts(AddFeatureCounts(
      AllZeroFeatureCounts(AllAddressSpaceFeatures()),
      {
          {WebFeature::kPrivateNetworkAccessFetchedWorkerScript, 1},
      }));
}

// This test verifies that a UseCounter is recorded when a document makes a
// private network request to load a service worker script from treat-as-public
// to local.
IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessWithFeatureEnabledBrowserTest,
    RecordsFeatureForServiceWorkerScriptFetchFromTreatAsPublicToLocal) {
  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();

  EXPECT_TRUE(content::NavigateToURL(web_contents(), PublicSecureURL(*server)));

  WebFeatureHistogramTester feature_histogram_tester;

  EXPECT_EQ(true, content::EvalJs(web_contents(), R"(
    navigator.serviceWorker.register('/service_worker/empty.js')
      .then(navigator.serviceWorker.ready)
      .then(() => true);
  )"));

  feature_histogram_tester.ExpectCounts(AddFeatureCounts(
      AllZeroFeatureCounts(AllAddressSpaceFeatures()),
      {
          {WebFeature::kPrivateNetworkAccessFetchedWorkerScript, 1},
      }));
}

// This test verifies that a UseCounter is not recorded when a document makes a
// private network request to load a service worker script from local to local.
IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessWithFeatureEnabledBrowserTest,
    ShouldNotRecordFeatureForServiceWorkerScriptFetchFromLocalToLocal) {
  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();

  EXPECT_TRUE(content::NavigateToURL(web_contents(), LocalSecureURL(*server)));

  WebFeatureHistogramTester feature_histogram_tester;

  EXPECT_EQ(true, content::EvalJs(web_contents(), R"(
    navigator.serviceWorker.register('/service_worker/empty.js')
      .then(navigator.serviceWorker.ready)
      .then(() => true);
  )"));

  feature_histogram_tester.ExpectCounts(
      AllZeroFeatureCounts(AllAddressSpaceFeatures()));
}

INSTANTIATE_TEST_SUITE_P(,
                         PrivateNetworkAccessRespectPreflightResultsBrowserTest,
                         testing::ValuesIn(kIsWarningOnlyTestData));

// Test the experimental use counter for accesses to the 0.0.0.0 IP address
// (and the corresponding `[::]` IPv6 address).
//
// In the Internet Protocol Version 4, the address 0.0.0.0 is a non-routable
// meta-address used to designate an invalid, unknown or non-applicable target.
// The real life behavior for 0.0.0.0 is different between operating systems.
// On Windows, it is unreachable, while on MacOS and Linux, 0.0.0.0 means
// all IP addresses on the local machine.
//
// In this case, 0.0.0.0 can be used to access localhost on MacOS and Linux
// and bypass Private Network Access checks, so that we would like to forbid
// fetches to 0.0.0.0. See more: https://crbug.com/1300021
#if BUILDFLAG(IS_WIN)
#define MAYBE_FetchNullIpAddressForNavigation \
  DISABLED_FetchNullIpAddressForNavigation
#else
#define MAYBE_FetchNullIpAddressForNavigation FetchNullIpAddressForNavigation
#endif
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessWithFeatureEnabledBrowserTest,
                       MAYBE_FetchNullIpAddressForNavigation) {
  WebFeatureHistogramTester feature_histogram_tester;
  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();

  EXPECT_TRUE(content::NavigateToURL(
      web_contents(), server->GetURL("0.0.0.0", kNoFaviconPath)));

  feature_histogram_tester.ExpectCounts(
      AddFeatureCounts(AllZeroFeatureCounts(AllAddressSpaceFeatures()),
                       {
                           {WebFeature::kPrivateNetworkAccessNullIpAddress, 1},
                       }));
}

#if BUILDFLAG(IS_WIN)
#define MAYBE_FetchNullIpAddressFromDocument \
  DISABLED_FetchNullIpAddressFromDocument
#else
#define MAYBE_FetchNullIpAddressFromDocument FetchNullIpAddressFromDocument
#endif
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessWithFeatureEnabledBrowserTest,
                       MAYBE_FetchNullIpAddressFromDocument) {
  WebFeatureHistogramTester feature_histogram_tester;
  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();

  EXPECT_TRUE(
      content::NavigateToURL(web_contents(), server->GetURL(kNoFaviconPath)));

  auto subresource_url =
      server->GetURL("0.0.0.0", "/set-header?Access-Control-Allow-Origin: *");
  constexpr char kSubresourceScript[] = R"(
    new Promise(resolve => {
      fetch($1).then(e => resolve(true));
    }))";
  EXPECT_EQ(true, content::EvalJs(
                      web_contents(),
                      content::JsReplace(kSubresourceScript, subresource_url)));

  feature_histogram_tester.ExpectCounts(
      AddFeatureCounts(AllZeroFeatureCounts(AllAddressSpaceFeatures()),
                       {
                           {WebFeature::kPrivateNetworkAccessNullIpAddress, 1},
                       }));
}

#if BUILDFLAG(IS_WIN)
#define MAYBE_FetchNullIpAddressFromWorker DISABLED_FetchNullIpAddressFromWorker
#else
#define MAYBE_FetchNullIpAddressFromWorker FetchNullIpAddressFromWorker
#endif
IN_PROC_BROWSER_TEST_P(PrivateNetworkAccessWithFeatureEnabledWorkerBrowserTest,
                       MAYBE_FetchNullIpAddressFromWorker) {
  WebFeatureHistogramTester feature_histogram_tester;
  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();

  EXPECT_TRUE(content::NavigateToURL(
      web_contents(), server->GetURL("/workers/fetch_from_worker.html")));

  constexpr char kWorkerScript[] = R"(
    new Promise(resolve => {
      fetch_from_worker($1);
      resolve(true);
    }))";
  auto worker_url =
      server->GetURL("0.0.0.0", "/set-header?Access-Control-Allow-Origin: *");
  EXPECT_EQ(true,
            content::EvalJs(web_contents(),
                            content::JsReplace(kWorkerScript, worker_url)));

  feature_histogram_tester.ExpectCounts(
      AddFeatureCounts(AllZeroFeatureCounts(AllAddressSpaceFeatures()),
                       {
                           {WebFeature::kPrivateNetworkAccessNullIpAddress, 1},
                       }));
}

INSTANTIATE_TEST_SUITE_P(
    ,
    PrivateNetworkAccessWithFeatureEnabledWorkerBrowserTest,
    testing::ValuesIn(kIsWarningOnlyTestData));

// ====================
// SPECIAL SCHEME TESTS
// ====================
//
// These tests verify the IP address space assigned to documents loaded from a
// variety of special URL schemes. Since these are not loaded over the network,
// an IP address space must be made up for them.

// This test verifies that the chrome-untrusted:// scheme is considered local
// for the purpose of Private Network Access computations.
// TODO(crbug.com/40195864): The NTP no longer loads a chrome-untrusted://
// iframe in all cases. Find another way to test the chrome-untrusted:// scheme.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessWithFeatureEnabledBrowserTest,
                       DISABLED_SpecialSchemeChromeUntrusted) {
  // The only way to have a page with a loaded chrome-untrusted:// url without
  // relying on platform specific or components features, is to use the
  // new-tab-page host. chrome-untrusted://new-tab-page is restricted to iframes
  // however so we load chrome://new-tab-page that embeds chrome-untrusted://
  // frame(s) by default.
  EXPECT_TRUE(
      content::NavigateToURL(web_contents(), GURL("chrome://new-tab-page")));
  content::RenderFrameHost* iframe = ChildFrameAt(web_contents(), 0);
  ASSERT_TRUE(iframe);
  EXPECT_TRUE(iframe->GetLastCommittedURL().SchemeIs(
      content::kChromeUIUntrustedScheme));

  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();
  GURL fetch_url = LocalNonSecureWithCrossOriginCors(*server);

  // TODO(crbug.com/40459152): The chrome-untrusted:// page should be kLocal,
  // and not require a Private Network Access CORS preflight. However we have
  // not yet implemented the CORS preflight mechanism, and fixing the underlying
  // issue will not change the test result. Once CORS preflight is implemented,
  // review this test and delete this comment.
  // Note: CSP is blocking javascript eval, unless we run it in an isolated
  // world.
  EXPECT_EQ(true, content::EvalJs(iframe, FetchScript(fetch_url),
                                  content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                                  content::ISOLATED_WORLD_ID_CONTENT_END));
}

// This test verifies that the devtools:// scheme is considered local for the
// purpose of Private Network Access.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessWithFeatureEnabledBrowserTest,
                       SpecialSchemeDevtools) {
  EXPECT_TRUE(content::NavigateToURL(
      web_contents(), GURL("devtools://devtools/bundled/devtools_app.html")));
  EXPECT_TRUE(
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL().SchemeIs(
          content::kChromeDevToolsScheme));

  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();
  GURL fetch_url = LocalNonSecureWithCrossOriginCors(*server);

  // TODO(crbug.com/40459152): The devtools:// page should be kLocal, and not
  // require a Private Network Access CORS preflight. However we have not yet
  // implemented the CORS preflight mechanism, and fixing the underlying issue
  // will not change the test result. Once CORS preflight is implemented, review
  // this test and delete this comment.
  EXPECT_EQ(true, content::EvalJs(web_contents(), FetchScript(fetch_url)));
}

// This test verifies that the chrome-search:// scheme is considered local for
// the purpose of Private Network Access.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessWithFeatureEnabledBrowserTest,
                       SpecialSchemeChromeSearch) {
  EXPECT_TRUE(content::NavigateToURL(
      web_contents(), GURL("chrome-search://most-visited/title.html")));
  ASSERT_TRUE(
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL().SchemeIs(
          chrome::kChromeSearchScheme));

  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();
  GURL fetch_url = LocalNonSecureWithCrossOriginCors(*server);

  // TODO(crbug.com/40459152): The chrome-search:// page should be kLocal, and
  // not require a Private Network Access CORS preflight. However we have not
  // yet implemented the CORS preflight mechanism, and fixing the underlying
  // issue will not change the test result. Once CORS preflight is implemented,
  // review this test and delete this comment. Note: CSP is blocking javascript
  // eval, unless we run it in an isolated world.
  EXPECT_EQ(true, content::EvalJs(web_contents(), FetchScript(fetch_url),
                                  content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                                  content::ISOLATED_WORLD_ID_CONTENT_END));
}

// This test verifies that the chrome-extension:// scheme is considered local
// for the purpose of Private Network Access.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessWithFeatureEnabledBrowserTest,
                       SpecialSchemeChromeExtension) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  extensions::ScopedInstallVerifierBypassForTest install_verifier_bypass;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  static constexpr char kPageFile[] = "page.html";
  constexpr char kContents[] = R"(
  <html>
    <head>
      <title>IPAddressSpace of chrome-extension:// schemes.</title>
    </head>
    <body>
    </body>
  </html>
  )";
  base::WriteFile(temp_dir.GetPath().AppendASCII(kPageFile), kContents);
  static constexpr char kWebAccessibleResources[] =
      R"([{
            "resources": ["page.html"],
            "matches": ["*://*/*"]
         }])";

  extensions::ExtensionBuilder builder("test");
  builder.SetPath(temp_dir.GetPath())
      .SetVersion("1.0")
      .SetLocation(extensions::mojom::ManifestLocation::kExternalPolicyDownload)
      .SetManifestKey("web_accessible_resources",
                      base::test::ParseJson(kWebAccessibleResources));

  extensions::ExtensionService* service =
      extensions::ExtensionSystem::Get(browser()->profile())
          ->extension_service();
  scoped_refptr<const extensions::Extension> extension = builder.Build();
  service->OnExtensionInstalled(extension.get(), syncer::StringOrdinal(), 0);

  const GURL url = extension->GetResourceURL(kPageFile);

  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
  ASSERT_TRUE(
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL().SchemeIs(
          extensions::kExtensionScheme));

  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();
  GURL fetch_url = LocalNonSecureWithCrossOriginCors(*server);

  // TODO(crbug.com/40459152): The chrome-extension:// page should be kLocal,
  // and not require a Private Network Access CORS preflight. However we have
  // not yet implemented the CORS preflight mechanism, and fixing the underlying
  // issue will not change the test result. Once CORS preflight is implemented,
  // review this test and delete this comment.
  // Note: CSP is blocking javascript eval, unless we run it in an isolated
  // world.
  EXPECT_EQ(true, content::EvalJs(web_contents(), FetchScript(fetch_url),
                                  content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                                  content::ISOLATED_WORLD_ID_CONTENT_END));
}

// =================
// AUTO-RELOAD TESTS
// =================

// Intercepts the first load to a URL and fails the request with an error.
class NetErrorInterceptor final {
 public:
  NetErrorInterceptor(GURL url, net::Error error)
      : url_(std::move(url)),
        error_(error),
        interceptor_(base::BindRepeating(&NetErrorInterceptor::Intercept,
                                         base::Unretained(this))) {}

  ~NetErrorInterceptor() = default;

  // Instances of this type are neither copyable nor movable.
  NetErrorInterceptor(const NetErrorInterceptor&) = delete;
  NetErrorInterceptor& operator=(const NetErrorInterceptor&) = delete;

 private:
  bool Intercept(content::URLLoaderInterceptor::RequestParams* params) {
    const GURL& request_url = params->url_request.url;
    if (request_url != url_ || did_intercept_) {
      return false;
    }

    did_intercept_ = true;

    network::URLLoaderCompletionStatus status;
    status.error_code = error_;
    params->client->OnComplete(status);
    return true;
  }

  const GURL url_;
  const net::Error error_;

  // Whether this instance already intercepted and failed a request.
  bool did_intercept_ = false;

  // Interceptor must be declared after all state used in `Intercept()`, to
  // avoid use-after-free at destruction time.
  const content::URLLoaderInterceptor interceptor_;
};

class PrivateNetworkAccessAutoReloadBrowserTest
    : public PrivateNetworkAccessBrowserTestBase {
 public:
  PrivateNetworkAccessAutoReloadBrowserTest()
      : PrivateNetworkAccessBrowserTestBase(
            {
                features::kBlockInsecurePrivateNetworkRequests,
                features::kBlockInsecurePrivateNetworkRequestsDeprecationTrial,
                features::kPrivateNetworkAccessForNavigations,
            },
            {}) {}

  void SetUpOnMainThread() override {
    PrivateNetworkAccessBrowserTestBase::SetUpOnMainThread();

    error_page::NetErrorAutoReloader::CreateForWebContents(web_contents());
  }
};

// This test verifies that when a document in the `local` address space fails to
// load due to a transient network error, it is auto-reloaded a short while
// later and that fetch is not blocked as a private network request.
//
// TODO(crbug.com/40225769): Test is flaky.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessAutoReloadBrowserTest,
                       DISABLED_AutoReloadWorks) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url = embedded_test_server()->GetURL("/defaultresponse");

  // There should be two navigations in total: one failed, one successful.
  content::TestNavigationObserver observer(web_contents(), 2);

  // This interceptor will only fail the first request to `url`.
  NetErrorInterceptor interceptor(url, net::ERR_UNEXPECTED);

  EXPECT_FALSE(content::NavigateToURL(web_contents(), url));

  // Observe second navigation, which succeeds.
  observer.Wait();
  EXPECT_TRUE(observer.last_navigation_succeeded());
}

// ================
// 0.0.0.0 TESTS
// ================

// This test verifies that a 0.0.0.0 subresource is blocked on a nonsecure
// public URL.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessWithFeatureEnabledBrowserTest,
                       NullIPBlockedOnNonsecure) {
  if constexpr (BUILDFLAG(IS_WIN)) {
    GTEST_SKIP() << "0.0.0.0 behavior varies across platforms and is "
                    "unreachable on Windows.";
  }

  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();
  GURL url = PublicNonSecureURL(*server);
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
  GURL subresource_url = server->GetURL("0.0.0.0", "/cors-ok.txt");
  EXPECT_EQ(false, content::EvalJs(web_contents(),
                                   content::JsReplace(R"(
    fetch($1).then(response => true).catch(error => false)
  )",
                                                      subresource_url)));
}

class PrivateNetworkAccessWithNullIPKillswitchTest
    : public PrivateNetworkAccessBrowserTestBase {
 public:
  PrivateNetworkAccessWithNullIPKillswitchTest()
      : PrivateNetworkAccessBrowserTestBase(
            {
                blink::features::kPlzDedicatedWorker,
                features::kBlockInsecurePrivateNetworkRequests,
                features::kBlockInsecurePrivateNetworkRequestsFromPrivate,
                features::kBlockInsecurePrivateNetworkRequestsDeprecationTrial,
                features::kPrivateNetworkAccessSendPreflights,
                features::kPrivateNetworkAccessForNavigations,
                features::kPrivateNetworkAccessForWorkers,
                network::features::kTreatNullIPAsPublicAddressSpace,
            },
            {}) {}
};

// This test verifies that 0.0.0.0 subresources are not blocked when the
// killswitch feature is enabled.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessWithNullIPKillswitchTest,
                       NullIPNotBlockedWithKillswitch) {
  if constexpr (BUILDFLAG(IS_WIN)) {
    GTEST_SKIP() << "0.0.0.0 behavior varies across platforms and is "
                    "unreachable on Windows.";
  }

  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();
  GURL url = PublicNonSecureURL(*server);
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
  GURL subresource_url = server->GetURL("0.0.0.0", "/cors-ok.txt");
  EXPECT_EQ(true, content::EvalJs(web_contents(),
                                  content::JsReplace(R"(
    fetch($1).then(response => response.ok)
  )",
                                                     subresource_url)));
}

}  // namespace
