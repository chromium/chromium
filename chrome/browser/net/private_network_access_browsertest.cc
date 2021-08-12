// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/strings/string_piece.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "chrome/browser/dom_distiller/tab_utils.h"
#include "chrome/browser/dom_distiller/test_distillation_observers.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/install_verifier.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/dom_distiller/content/browser/distiller_javascript_utils.h"
#include "components/dom_distiller/content/browser/test_distillability_observer.h"
#include "components/dom_distiller/core/dom_distiller_features.h"
#include "components/dom_distiller/core/dom_distiller_switches.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/embedder_support/switches.h"
#include "components/error_page/content/browser/net_error_auto_reloader.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
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
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom.h"

namespace {

using blink::mojom::WebFeature;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::Pair;

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

// The returned script evaluates to a boolean indicating whether the fetch
// succeeded or not.
std::string FetchScript(const GURL& url) {
  return content::JsReplace(
      "fetch($1).then(response => true).catch(error => false)", url);
}

constexpr char kFeatureHistogramName[] = "Blink.UseCounter.Features";

constexpr WebFeature kAllAddressSpaceFeatures[] = {
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
};

// Returns a map of WebFeature to bucket count. Skips buckets with zero counts.
std::map<WebFeature, int> GetAddressSpaceFeatureBucketCounts(
    const base::HistogramTester& tester) {
  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  std::map<WebFeature, int> counts;
  for (WebFeature feature : kAllAddressSpaceFeatures) {
    int count = tester.GetBucketCount(kFeatureHistogramName, feature);
    if (count == 0) {
      continue;
    }

    counts.emplace(feature, count);
  }
  return counts;
}

// Helper for `IsLessBucketCounts()`.
// `ASSERT_*` macros can only be used in functions that return `void`.
void AssertLe(size_t lhs, size_t rhs) {
  ASSERT_LE(lhs, rhs);
}

// Returns true if all the keys in `lhs` have lesser-than-or-equal values than
// the corresponding keys in `rhs` and `lhs != rhs`.
bool IsLessBucketCounts(const std::map<WebFeature, int>& lhs,
                        const std::map<WebFeature, int>& rhs) {
  bool lhs_has_lesser_entry = false;

  for (const auto& entry : lhs) {
    WebFeature feature = entry.first;
    int count = entry.second;

    const auto it = rhs.find(feature);
    if (it == rhs.end() || count > it->second) {
      return false;
    }

    if (count < it->second) {
      lhs_has_lesser_entry = true;
    }
  }

  // All entries in `lhs` have a corresponding entry in `rhs`.
  AssertLe(lhs.size(), rhs.size());

  // `lhs` is less if one of its entries is strictly less than the corresponding
  // `rhs` entry, or if `rhs` has some keys which `lhs` does not have.
  return lhs_has_lesser_entry || lhs.size() < rhs.size();
}

void WaitForBucketCounts(const base::HistogramTester& histogram_tester,
                         const std::map<WebFeature, int>& expected) {
  std::map<WebFeature, int> counts;

  while (true) {
    counts = GetAddressSpaceFeatureBucketCounts(histogram_tester);
    if (!IsLessBucketCounts(counts, expected)) {
      break;
    }

    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(5));
  }

  EXPECT_EQ(counts, expected);
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
      std::vector<base::Feature> enabled_features,
      std::vector<base::Feature> disabled_features) {
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

class PrivateNetworkAccessWithFeatureEnabledBrowserTest
    : public PrivateNetworkAccessBrowserTestBase {
 public:
  PrivateNetworkAccessWithFeatureEnabledBrowserTest()
      : PrivateNetworkAccessBrowserTestBase(
            {
                features::kBlockInsecurePrivateNetworkRequests,
                features::kBlockInsecurePrivateNetworkRequestsFromPrivate,
                features::kBlockInsecurePrivateNetworkRequestsDeprecationTrial,
                dom_distiller::kReaderMode,
            },
            {}) {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PrivateNetworkAccessBrowserTestBase::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableDomDistiller);
  }

 private:
  void SetUpOnMainThread() override {
    PrivateNetworkAccessBrowserTestBase::SetUpOnMainThread();
    // The distiller needs to run in an isolated environment. For tests we
    // can simply use the last value available.
    if (!dom_distiller::DistillerJavaScriptWorldIdIsSet()) {
      dom_distiller::SetDistillerJavaScriptWorldId(
          content::ISOLATED_WORLD_ID_CONTENT_END);
    }
  }
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
  base::HistogramTester histogram_tester;
  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();

  EXPECT_TRUE(content::NavigateToURL(web_contents(), PublicSecureURL(*server)));

  EXPECT_THAT(GetAddressSpaceFeatureBucketCounts(histogram_tester), IsEmpty());
}

// This test verifies that no feature is counted for top-level navigations from
// a public page to a local page.
//
// TODO(crbug.com/1129326): Revisit this once the story around top-level
// navigations is closer to being resolved. Counting these events will help
// decide what to do.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessWithFeatureDisabledBrowserTest,
                       DoesNotRecordAddressSpaceFeatureForRegularNavigation) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();

  EXPECT_TRUE(content::NavigateToURL(web_contents(), PublicSecureURL(*server)));
  EXPECT_TRUE(content::NavigateToURL(web_contents(), LocalSecureURL(*server)));

  EXPECT_THAT(GetAddressSpaceFeatureBucketCounts(histogram_tester), IsEmpty());
}

// This test verifies that when a secure context served from the public address
// space loads a resource from the local network, the correct WebFeature is
// use-counted.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessWithFeatureDisabledBrowserTest,
                       RecordsAddressSpaceFeatureForFetch) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();

  EXPECT_TRUE(content::NavigateToURL(web_contents(), PublicSecureURL(*server)));
  EXPECT_THAT(GetAddressSpaceFeatureBucketCounts(histogram_tester), IsEmpty());

  EXPECT_EQ(true, content::EvalJs(web_contents(), R"(
    fetch("/defaultresponse").then(response => response.ok)
  )"));

  WaitForBucketCounts(
      histogram_tester,
      {
          {WebFeature::kAddressSpacePublicSecureContextEmbeddedLocal, 1},
      });
}

// This test verifies that when a non-secure context served from the public
// address space loads a resource from the local network, the correct WebFeature
// is use-counted.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessWithFeatureDisabledBrowserTest,
                       RecordsAddressSpaceFeatureForFetchInNonSecureContext) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();

  EXPECT_TRUE(
      content::NavigateToURL(web_contents(), PublicNonSecureURL(*server)));
  EXPECT_THAT(GetAddressSpaceFeatureBucketCounts(histogram_tester), IsEmpty());

  EXPECT_EQ(true, content::EvalJs(web_contents(), R"(
    fetch("/defaultresponse").then(response => response.ok)
  )"));

  WaitForBucketCounts(
      histogram_tester,
      {
          {WebFeature::kAddressSpacePublicNonSecureContextEmbeddedLocal, 1},
      });
}

// This test verifies that when the user navigates a `public` document to a
// document served by a non-public IP, no address space feature is recorded.
IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessWithFeatureEnabledBrowserTest,
    DoesNotRecordAddressSpaceFeatureForBrowserInitiatedNavigation) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();

  EXPECT_TRUE(
      content::NavigateToURL(web_contents(), PublicNonSecureURL(*server)));

  EXPECT_TRUE(
      content::NavigateToURL(web_contents(), LocalNonSecureURL(*server)));

  EXPECT_THAT(GetAddressSpaceFeatureBucketCounts(histogram_tester), IsEmpty());
}

// This test verifies that when a `public` document navigates itself to a
// document served by a non-public IP, the correct address space feature is
// recorded.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessWithFeatureEnabledBrowserTest,
                       RecordsAddressSpaceFeatureForNavigation) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();

  EXPECT_TRUE(
      content::NavigateToURL(web_contents(), PublicNonSecureURL(*server)));
  EXPECT_THAT(GetAddressSpaceFeatureBucketCounts(histogram_tester), IsEmpty());

  EXPECT_TRUE(content::NavigateToURLFromRenderer(web_contents(),
                                                 LocalNonSecureURL(*server)));

  WaitForBucketCounts(
      histogram_tester,
      {
          {WebFeature::kAddressSpacePublicNonSecureContextNavigatedToLocal, 1},
      });
}

// This test verifies that when a `public` document navigates itself to a
// document served by a non-public IP, the correct address space feature is
// recorded, even if the target document carries a CSP `treat-as-public-address`
// directive.
IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessWithFeatureEnabledBrowserTest,
    RecordsAddressSpaceFeatureForNavigationToTreatAsPublicAddress) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();

  EXPECT_TRUE(
      content::NavigateToURL(web_contents(), PublicNonSecureURL(*server)));
  EXPECT_THAT(GetAddressSpaceFeatureBucketCounts(histogram_tester), IsEmpty());

  // Navigate to a different URL with the same CSP directive. If we just tried
  // to navigate to `PublicNonSecureURL(*server)`, nothing would happen.
  EXPECT_TRUE(content::NavigateToURLFromRenderer(
      web_contents(),
      NonSecureURL(
          *server,
          "/set-header?Content-Security-Policy: treat-as-public-address")));

  WaitForBucketCounts(
      histogram_tester,
      {
          {WebFeature::kAddressSpacePublicNonSecureContextNavigatedToLocal, 1},
      });
}

// This test verifies that when a page embeds an empty iframe pointing to
// about:blank, no address space feature is recorded. It serves as a basis for
// comparison with the following tests, which test behavior with iframes.
IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessWithFeatureEnabledBrowserTest,
    DoesNotRecordAddressSpaceFeatureForChildAboutBlankNavigation) {
  base::HistogramTester histogram_tester;
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

  EXPECT_THAT(GetAddressSpaceFeatureBucketCounts(histogram_tester), IsEmpty());
}

// This test verifies that when a non-secure context served from the public
// address space loads a child frame from the local network, the correct
// WebFeature is use-counted.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessWithFeatureEnabledBrowserTest,
                       RecordsAddressSpaceFeatureForChildNavigation) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();

  EXPECT_TRUE(
      content::NavigateToURL(web_contents(), PublicNonSecureURL(*server)));
  EXPECT_THAT(GetAddressSpaceFeatureBucketCounts(histogram_tester), IsEmpty());

  base::StringPiece script_template = R"(
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

  WaitForBucketCounts(
      histogram_tester,
      {
          {WebFeature::kAddressSpacePublicNonSecureContextNavigatedToLocal, 1},
      });
}

// This test verifies that when a non-secure context served from the public
// address space loads a grand-child frame from the local network, the correct
// WebFeature is use-counted. If inheritance did not work correctly, the
// intermediate about:blank frame might confuse the address space logic.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessWithFeatureEnabledBrowserTest,
                       RecordsAddressSpaceFeatureForGrandchildNavigation) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();

  EXPECT_TRUE(
      content::NavigateToURL(web_contents(), PublicNonSecureURL(*server)));
  EXPECT_THAT(GetAddressSpaceFeatureBucketCounts(histogram_tester), IsEmpty());

  base::StringPiece script_template = R"(
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

  WaitForBucketCounts(
      histogram_tester,
      {
          {WebFeature::kAddressSpacePublicNonSecureContextNavigatedToLocal, 1},
      });
}

// This test verifies that the right address space feature is recorded when a
// navigation results in a private network request. Specifically, in this test
// the document being navigated is not the one initiating the navigation (the
// latter being the "remote initiator" referenced by the test name).
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessWithFeatureEnabledBrowserTest,
                       RecordsAddressSpaceFeatureForRemoteInitiatorNavigation) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();

  EXPECT_TRUE(content::NavigateToURL(
      web_contents(),
      NonSecureURL(
          *server,
          "/private_network_access/remote-initiator-navigation.html")));
  EXPECT_THAT(GetAddressSpaceFeatureBucketCounts(histogram_tester), IsEmpty());

  EXPECT_EQ(true, content::EvalJs(web_contents(), content::JsReplace(R"(
    runTest({
      url: "/defaultresponse",
    });
  )")));

  WaitForBucketCounts(
      histogram_tester,
      {
          {WebFeature::kAddressSpacePublicNonSecureContextNavigatedToLocal, 1},
      });
}

// This test verifies that when the initiator of a navigation is no longer
// around by the time the navigation finishes, then no address space feature is
// recorded, and importantly: the browser does not crash.
IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessWithFeatureEnabledBrowserTest,
    DoesNotRecordAddressSpaceFeatureForClosedInitiatorNavigation) {
  base::HistogramTester histogram_tester;
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

  EXPECT_THAT(GetAddressSpaceFeatureBucketCounts(histogram_tester), IsEmpty());
}

// This test verifies that when the initiator of a navigation has already
// navigated itself by the time the navigation finishes, then no address space
// feature is recorded.
IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessWithFeatureEnabledBrowserTest,
    DoesNotRecordAddressSpaceFeatureForMissingInitiatorNavigation) {
  base::HistogramTester histogram_tester;
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

  EXPECT_THAT(GetAddressSpaceFeatureBucketCounts(histogram_tester), IsEmpty());
}

// This test verifies that private network requests that are blocked result in
// a WebFeature being use-counted.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessWithFeatureEnabledBrowserTest,
                       RecordsAddressSpaceFeatureForBlockedRequests) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();

  EXPECT_TRUE(
      content::NavigateToURL(web_contents(), PublicNonSecureURL(*server)));
  EXPECT_THAT(GetAddressSpaceFeatureBucketCounts(histogram_tester), IsEmpty());

  EXPECT_EQ(true, content::EvalJs(web_contents(), R"(
    fetch("/defaultresponse").catch(() => true)
  )"));

  WaitForBucketCounts(
      histogram_tester,
      {
          {WebFeature::kAddressSpacePublicNonSecureContextEmbeddedLocal, 1},
      });
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessWithFeatureEnabledBrowserTest,
                       RecordsAddressSpaceFeatureForDeprecationTrial) {
  base::HistogramTester histogram_tester;
  content::DeprecationTrialURLLoaderInterceptor interceptor;

  EXPECT_TRUE(content::NavigateToURL(web_contents(), interceptor.EnabledUrl()));

  EXPECT_EQ(
      histogram_tester.GetBucketCount(
          kFeatureHistogramName,
          WebFeature::
              kPrivateNetworkAccessNonSecureContextsAllowedDeprecationTrial),
      1);
}

// ====================
// SPECIAL SCHEME TESTS
// ====================
//
// These tests verify the IP address space assigned to documents loaded from a
// variety of special URL schemes. Since these are not loaded over the network,
// an IP address space must be made up for them.

// This test verifies that the chrome-untrusted:// scheme is considered local
// for the purpose of Private Network Access computations.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessWithFeatureEnabledBrowserTest,
                       SpecialSchemeChromeUntrusted) {
  // The only way to have a page with a loaded chrome-untrusted:// url without
  // relying on platform specific or components features, is to use the
  // new-tab-page host. chrome-untrusted://new-tab-page is restricted to iframes
  // however so we load chrome://new-tab-page that embeds chrome-untrusted://
  // frame(s) by default.
  EXPECT_TRUE(
      content::NavigateToURL(web_contents(), GURL("chrome://new-tab-page")));
  std::vector<content::RenderFrameHost*> frames =
      web_contents()->GetAllFrames();
  ASSERT_GE(frames.size(), 2u);
  content::RenderFrameHost* iframe = frames[1];
  EXPECT_TRUE(iframe->GetLastCommittedURL().SchemeIs(
      content::kChromeUIUntrustedScheme));

  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();
  GURL fetch_url = LocalNonSecureWithCrossOriginCors(*server);

  // TODO(crbug.com/591068): The chrome-untrusted:// page should be kLocal, and
  // not require a Private Network Access CORS preflight. However we have not
  // yet implemented the CORS preflight mechanism, and fixing the underlying
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
  EXPECT_TRUE(web_contents()->GetMainFrame()->GetLastCommittedURL().SchemeIs(
      content::kChromeDevToolsScheme));

  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();
  GURL fetch_url = LocalNonSecureWithCrossOriginCors(*server);

  // TODO(crbug.com/591068): The devtools:// page should be kLocal, and not
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
  ASSERT_TRUE(web_contents()->GetMainFrame()->GetLastCommittedURL().SchemeIs(
      chrome::kChromeSearchScheme));

  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();
  GURL fetch_url = LocalNonSecureWithCrossOriginCors(*server);

  // TODO(crbug.com/591068): The chrome-search:// page should be kLocal, and not
  // require a Private Network Access CORS preflight. However we have not yet
  // implemented the CORS preflight mechanism, and fixing the underlying issue
  // will not change the test result. Once CORS preflight is implemented, review
  // this test and delete this comment.
  // Note: CSP is blocking javascript eval, unless we run it in an isolated
  // world.
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

  std::vector<base::Value> resources;
  resources.emplace_back(std::string(kPageFile));
  constexpr char kContents[] = R"(
  <html>
    <head>
      <title>IPAddressSpace of chrome-extension:// schemes.</title>
    </head>
    <body>
    </body>
  </html>
  )";
  base::WriteFile(temp_dir.GetPath().AppendASCII(kPageFile), kContents,
                  sizeof(kContents) - 1);

  extensions::ExtensionBuilder builder("test");
  builder.SetPath(temp_dir.GetPath())
      .SetVersion("1.0")
      .SetLocation(extensions::mojom::ManifestLocation::kExternalPolicyDownload)
      .SetManifestKey("web_accessible_resources", std::move(resources));

  extensions::ExtensionService* service =
      extensions::ExtensionSystem::Get(browser()->profile())
          ->extension_service();
  scoped_refptr<const extensions::Extension> extension = builder.Build();
  service->OnExtensionInstalled(extension.get(), syncer::StringOrdinal(), 0);

  const GURL url = extension->GetResourceURL(kPageFile);

  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
  ASSERT_TRUE(web_contents()->GetMainFrame()->GetLastCommittedURL().SchemeIs(
      extensions::kExtensionScheme));

  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();
  GURL fetch_url = LocalNonSecureWithCrossOriginCors(*server);

  // TODO(crbug.com/591068): The chrome-extension:// page should be kLocal, and
  // not require a Private Network Access CORS preflight. However we have not
  // yet implemented the CORS preflight mechanism, and fixing the underlying
  // issue will not change the test result. Once CORS preflight is implemented,
  // review this test and delete this comment.
  // Note: CSP is blocking javascript eval, unless we run it in an isolated
  // world.
  EXPECT_EQ(true, content::EvalJs(web_contents(), FetchScript(fetch_url),
                                  content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                                  content::ISOLATED_WORLD_ID_CONTENT_END));
}

// This test verifies that the chrome-distiller:// scheme is considered public
// for the purpose of Private Network Access.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessWithFeatureEnabledBrowserTest,
                       SpecialSchemeChromeDistiller) {
  // Load the base page to be distilled. Note that HTTPS has to be used
  // otherwise the page won't be distillable.
  std::unique_ptr<net::EmbeddedTestServer> https_server =
      NewServer(net::EmbeddedTestServer::TYPE_HTTPS);
  GURL article_url = https_server->GetURL("/dom_distiller/simple_article.html");

  dom_distiller::TestDistillabilityObserver distillability_observer(
      web_contents());
  dom_distiller::DistillabilityResult expected_result;
  expected_result.is_distillable = true;
  expected_result.is_last = false;
  expected_result.is_mobile_friendly = false;

  EXPECT_TRUE(content::NavigateToURL(web_contents(), article_url));
  // This blocks until the page is found to be distillable.
  distillability_observer.WaitForResult(expected_result);

  // Distill the page. It will be placed in a new WebContents replacing the old
  // one.
  DistillCurrentPageAndView(web_contents());
  dom_distiller::DistilledPageObserver(web_contents())
      .WaitUntilFinishedLoading();

  EXPECT_TRUE(web_contents()->GetMainFrame()->GetLastCommittedURL().SchemeIs(
      dom_distiller::kDomDistillerScheme));

  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();
  GURL fetch_url = LocalNonSecureWithCrossOriginCors(*server);

  // Note: CSP is blocking javascript eval, unless we run it in an isolated
  // world.
  EXPECT_EQ(false, content::EvalJs(web_contents(), FetchScript(fetch_url),
                                   content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                                   content::ISOLATED_WORLD_ID_CONTENT_END));
}

// =================
// AUTO-RELOAD TESTS
// =================

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
  bool Intercept(content::URLLoaderInterceptor::RequestParams* params) const {
    const GURL& request_url = params->url_request.url;
    if (request_url != url_) {
      return false;
    }

    network::URLLoaderCompletionStatus status;
    status.error_code = error_;
    params->client->OnComplete(status);
    return true;
  }

  GURL url_;
  net::Error error_;

  // Interceptor must be declared after all state used in `Intercept()`, to
  // avoid use-after-free at destruction time.
  content::URLLoaderInterceptor interceptor_;
};

class PrivateNetworkAccessAutoReloadBrowserTest
    : public PrivateNetworkAccessBrowserTestBase {
 public:
  PrivateNetworkAccessAutoReloadBrowserTest()
      : PrivateNetworkAccessBrowserTestBase(
            {
                features::kBlockInsecurePrivateNetworkRequests,
                features::kBlockInsecurePrivateNetworkRequestsForNavigations,
                features::kBlockInsecurePrivateNetworkRequestsDeprecationTrial,
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
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessAutoReloadBrowserTest,
                       AutoReloadWorks) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url = embedded_test_server()->GetURL("/defaultresponse");

  // There should be two navigations in total: one failed, one successful.
  content::TestNavigationObserver observer(web_contents(), 2);

  {
    NetErrorInterceptor interceptor(url, net::ERR_UNEXPECTED);

    EXPECT_FALSE(content::NavigateToURL(web_contents(), url));
  }

  // Observe second navigation, which succeeds.
  observer.Wait();
  EXPECT_TRUE(observer.last_navigation_succeeded());
}

}  // namespace
