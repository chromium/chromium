// Copyright 2025 The Chromium Authors
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
#include "chrome/browser/local_network_access/local_network_access_browsertest_base.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/web_feature_histogram_tester.h"
#include "components/error_page/content/browser/net_error_auto_reloader.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/permissions/permission_request_manager.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/install_default_websocket_handlers.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/local_network_access_check_result.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"

// Local Network Access browser tests testing UseCounters

namespace local_network_access {

using blink::mojom::WebFeature;
using testing::IsEmpty;

// We use a custom page that explicitly disables its own favicon (by providing
// an invalid data: URL for it) so as to prevent the browser from making an
// automatic request to /favicon.ico. This is because the automatic request
// messes with our tests, in which we want to trigger a single request from the
// web page to a resource of our choice and observe the side-effect in metrics.
constexpr char kNoFaviconPath[] = "/local_network_access/no-favicon.html";

GURL SecureURL(const net::EmbeddedTestServer& server, const std::string& path) {
  return server.GetURL(path);
}

GURL SecureURLWithHostName(const net::EmbeddedTestServer& server,
                           const std::string& path,
                           const std::string& hostname) {
  return server.GetURL(hostname, path);
}

GURL LocalSecureURL(const net::EmbeddedTestServer& server) {
  return SecureURL(server, kNoFaviconPath);
}

GURL LocalSecureURLWithHost(const net::EmbeddedTestServer& server,
                            const std::string& hostname) {
  return SecureURLWithHostName(server, kNoFaviconPath, hostname);
}

class LocalNetworkAccessCountersBrowserTest
    : public LocalNetworkAccessBrowserTestBase {};

// ================
// USECOUNTER TESTS
// ================
//
// UseCounters are translated into UMA histograms at the chrome/ layer, by the
// page_load_metrics component. These tests verify that UseCounters are recorded
// correctly by Local Network Access code in the right circumstances.

// This test verifies that no feature is counted for the initial navigation from
// a new tab to a page served by localhost.
//
// Regression test for https://crbug.com/40151532.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessCountersBrowserTest,
                       DoesNotRecordAddressSpaceFeatureForInitialNavigation) {
  WebFeatureHistogramTester feature_histogram_tester;

  EXPECT_TRUE(content::NavigateToURL(
      web_contents(), SecureURL(https_public_server(), kNoFaviconPath)));

  EXPECT_THAT(
      feature_histogram_tester.GetNonZeroCounts(AllAddressSpaceFeatures()),
      IsEmpty());
}

// This test verifies that no feature is counted for top-level navigations from
// a public page to a local page.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessCountersBrowserTest,
                       DoesNotRecordAddressSpaceFeatureForRegularNavigation) {
  WebFeatureHistogramTester feature_histogram_tester;

  EXPECT_TRUE(content::NavigateToURL(
      web_contents(), SecureURL(https_public_server(), kNoFaviconPath)));
  EXPECT_TRUE(
      content::NavigateToURL(web_contents(), LocalSecureURL(https_server())));

  EXPECT_THAT(
      feature_histogram_tester.GetNonZeroCounts(AllAddressSpaceFeatures()),
      IsEmpty());
}

// This test verifies that when a `public` document navigates itself to a
// document served by a non-public IP, the correct address space feature is
// recorded.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessCountersBrowserTest,
                       RecordsAddressSpaceFeatureForNavigation) {
  WebFeatureHistogramTester feature_histogram_tester;

  EXPECT_TRUE(content::NavigateToURL(
      web_contents(), SecureURL(https_public_server(), kNoFaviconPath)));
  EXPECT_THAT(
      feature_histogram_tester.GetNonZeroCounts(AllAddressSpaceFeatures()),
      IsEmpty());

  EXPECT_TRUE(content::NavigateToURLFromRenderer(
      web_contents(), LocalSecureURL(https_server())));

  feature_histogram_tester.ExpectCounts(AddFeatureCounts(
      AllZeroFeatureCounts(AllAddressSpaceFeatures()),
      {
          {WebFeature::kAddressSpacePublicNonSecureContextNavigatedToLocalV2,
           1},
          {WebFeature::kPrivateNetworkAccessFetchedTopFrame, 1},
      }));
}

// This test verifies that when a secure context served from the public address
// space loads a resource from the private network, the correct WebFeature is
// use-counted.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessCountersBrowserTest,
                       LocalNetworkAccessFetch) {
  WebFeatureHistogramTester feature_histogram_tester;

  EXPECT_TRUE(content::NavigateToURL(
      web_contents(), SecureURL(https_public_server(), kNoFaviconPath)));
  EXPECT_THAT(
      feature_histogram_tester.GetNonZeroCounts(AllAddressSpaceFeatures()),
      IsEmpty());

  // Enable auto-accept of LNA permission request.
  bubble_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  ASSERT_EQ(true,
            content::EvalJs(
                web_contents(),
                content::JsReplace(
                    "fetch($1).then(response => response.ok)",
                    SecureURL(https_server(),
                              "/set-header?Access-Control-Allow-Origin: *"))));

  feature_histogram_tester.ExpectCounts(AddFeatureCounts(
      AllZeroFeatureCounts(AllAddressSpaceFeatures()),
      {
          {WebFeature::kAddressSpacePublicSecureContextEmbeddedLoopbackV2, 1},
      }));
}

// This test verifies that when a page embeds an empty iframe pointing to
// about:blank, no address space feature is recorded. It serves as a basis for
// comparison with the following tests, which test behavior with iframes.
IN_PROC_BROWSER_TEST_F(
    LocalNetworkAccessCountersBrowserTest,
    DoesNotRecordAddressSpaceFeatureForChildAboutBlankNavigation) {
  WebFeatureHistogramTester feature_histogram_tester;

  EXPECT_TRUE(content::NavigateToURL(
      web_contents(), SecureURL(https_public_server(), kNoFaviconPath)));
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

// This test verifies that when a secure context served from the public
// address space loads a child frame from the local network, the correct
// WebFeature is use-counted.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessCountersBrowserTest,
                       RecordsAddressSpaceFeatureForChildNavigation) {
  WebFeatureHistogramTester feature_histogram_tester;

  // Enable auto-accept of LNA permission request.
  bubble_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  EXPECT_TRUE(content::NavigateToURL(
      web_contents(), SecureURL(https_public_server(), kNoFaviconPath)));
  EXPECT_THAT(
      feature_histogram_tester.GetNonZeroCounts(AllAddressSpaceFeatures()),
      IsEmpty());

  std::string_view script_template = R"(
    new Promise(resolve => {
      const child = document.createElement("iframe");
      child.src = $1;
      child.allow = "local-network-access";
      child.onload = () => { resolve(true); };
      document.body.appendChild(child);
    })
  )";
  EXPECT_EQ(true, content::EvalJs(
                      web_contents(),
                      content::JsReplace(script_template,
                                         LocalSecureURL(https_server()))));

  feature_histogram_tester.ExpectCounts(AddFeatureCounts(
      AllZeroFeatureCounts(AllAddressSpaceFeatures()),
      {
          {WebFeature::kAddressSpacePublicNonSecureContextNavigatedToLocalV2,
           1},
          {WebFeature::kPrivateNetworkAccessFetchedSubFrame, 1},
      }));
}

// This test verifies that when a secure context served from the public
// address space loads a grand-child frame from the local network, the correct
// WebFeature is use-counted. If inheritance did not work correctly, the
// intermediate about:blank frame might confuse the address space logic.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessCountersBrowserTest,
                       RecordsAddressSpaceFeatureForGrandchildNavigation) {
  WebFeatureHistogramTester feature_histogram_tester;

  EXPECT_TRUE(content::NavigateToURL(
      web_contents(), SecureURL(https_public_server(), kNoFaviconPath)));
  EXPECT_THAT(
      feature_histogram_tester.GetNonZeroCounts(AllAddressSpaceFeatures()),
      IsEmpty());

  // Enable auto-accept of LNA permission request.
  bubble_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  std::string_view script_template = R"(
    function addChildFrame(doc, src) {
      return new Promise(resolve => {
        const child = doc.createElement("iframe");
        child.allow = "local-network-access";
        child.src = src;
        child.onload = () => { resolve(child); };
        doc.body.appendChild(child);
      });
    }

    addChildFrame(document, "about:blank")
      .then(child => addChildFrame(child.contentDocument, $1))
      .then(grandchild =>  true);
  )";
  EXPECT_EQ(true, content::EvalJs(
                      web_contents(),
                      content::JsReplace(script_template,
                                         LocalSecureURL(https_server()))));

  feature_histogram_tester.ExpectCounts(AddFeatureCounts(
      AllZeroFeatureCounts(AllAddressSpaceFeatures()),
      {
          {WebFeature::kAddressSpacePublicNonSecureContextNavigatedToLocalV2,
           1},
          {WebFeature::kPrivateNetworkAccessFetchedSubFrame, 1},
      }));
}

// This test verifies that local network requests that are blocked are not
// use-counted.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessCountersBrowserTest,
                       DoesNotRecordAddressSpaceFeatureForBlockedRequests) {
  WebFeatureHistogramTester feature_histogram_tester;

  EXPECT_TRUE(content::NavigateToURL(
      web_contents(), SecureURL(https_public_server(), kNoFaviconPath)));
  EXPECT_THAT(
      feature_histogram_tester.GetNonZeroCounts(AllAddressSpaceFeatures()),
      IsEmpty());

  base::HistogramTester base_histogram_tester;

  bubble_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::DENY_ALL);

  EXPECT_EQ(true,
            content::EvalJs(web_contents(),
                            content::JsReplace("fetch($1).catch(() => true)",
                                               LocalSecureURLWithHost(
                                                   https_server(), "a.test"))));

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  base_histogram_tester.ExpectBucketCount(
      "Security.PrivateNetworkAccess.CheckResult",
      network::LocalNetworkAccessCheckResult::kLNAPermissionRequired, 1);

  EXPECT_THAT(
      feature_histogram_tester.GetNonZeroCounts(AllAddressSpaceFeatures()),
      IsEmpty());
}

// This test verifies that resources proxied through a proxy on localhost can
// be fetched from documents in the public IP address space.
// Regression test for https://crbug.com/40199099.
// TODO(crbug.com/465260276): Disabled for being flaky.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessCountersBrowserTest,
                       DISABLED_ProxiedResourcesAllowed) {
  EXPECT_TRUE(content::NavigateToURL(
      web_contents(), SecureURL(https_public_server(), kNoFaviconPath)));

  browser()->GetProfile()->GetPrefs()->SetDict(
      proxy_config::prefs::kProxy,
      ProxyConfigDictionary::CreateFixedServers(
          https_server().host_port_pair().ToString(), ""));
  ProfileNetworkContextServiceFactory::GetForContext(browser()->GetProfile())
      ->FlushProxyConfigMonitorForTesting();

  // FlushProxyConfigMonitorForTesting() ensures the proxy config message is
  // sent over the Mojo pipe, but does not guarantee the network service has
  // finished processing it. Without this additional flush, there is a race
  // condition where fetch() may execute before the proxy is active, causing
  // the request to go directly to localhost instead of through the proxy.
  // When that happens, the network detects a public-to-loopback access and
  // records kAddressSpacePublicSecureContextEmbeddedLoopbackV2, which fails
  // the test expectation.
  browser()
      ->GetProfile()
      ->GetDefaultStoragePartition()
      ->FlushNetworkInterfaceForTesting();

  WebFeatureHistogramTester feature_histogram_tester;

  EXPECT_EQ(true, content::EvalJs(web_contents(), R"(
    fetch("/defaultresponse").then(response => response.ok)
  )"));

  EXPECT_THAT(
      feature_histogram_tester.GetNonZeroCounts(AllAddressSpaceFeatures()),
      IsEmpty());
}

// This test verifies that a UseCounter is not recorded when a document makes a
// local network request to load a service worker script from local to local.
IN_PROC_BROWSER_TEST_F(
    LocalNetworkAccessCountersBrowserTest,
    ShouldNotRecordFeatureForServiceWorkerScriptFetchFromLocalToLocal) {
  EXPECT_TRUE(
      content::NavigateToURL(web_contents(), LocalSecureURL(https_server())));

  WebFeatureHistogramTester feature_histogram_tester;

  EXPECT_EQ(true, content::EvalJs(web_contents(), R"(
    navigator.serviceWorker.register('/service_worker/empty.js')
      .then(navigator.serviceWorker.ready)
      .then(() => true);
  )"));

  feature_histogram_tester.ExpectCounts(
      AllZeroFeatureCounts(AllAddressSpaceFeatures()));
}

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
// and bypass Local Network Access checks, so that we would like to forbid
// fetches to 0.0.0.0. See more: https://crbug.com/40058874
#if BUILDFLAG(IS_WIN)
#define MAYBE_FetchNullIpAddressForNavigation \
  DISABLED_FetchNullIpAddressForNavigation
#else
#define MAYBE_FetchNullIpAddressForNavigation FetchNullIpAddressForNavigation
#endif
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessCountersBrowserTest,
                       MAYBE_FetchNullIpAddressForNavigation) {
  WebFeatureHistogramTester feature_histogram_tester;

  EXPECT_TRUE(content::NavigateToURL(
      web_contents(), https_server().GetURL("0.0.0.0", kNoFaviconPath)));

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
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessCountersBrowserTest,
                       MAYBE_FetchNullIpAddressFromDocument) {
  WebFeatureHistogramTester feature_histogram_tester;

  EXPECT_TRUE(content::NavigateToURL(web_contents(),
                                     https_server().GetURL(kNoFaviconPath)));

  auto subresource_url = https_server().GetURL(
      "0.0.0.0", "/set-header?Access-Control-Allow-Origin: *");
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
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessCountersBrowserTest,
                       MAYBE_FetchNullIpAddressFromWorker) {
  WebFeatureHistogramTester feature_histogram_tester;

  EXPECT_TRUE(content::NavigateToURL(
      web_contents(),
      https_server().GetURL("/workers/fetch_from_worker.html")));

  constexpr char kWorkerScript[] = R"(
    new Promise(resolve => {
      fetch_from_worker($1);
      resolve(true);
    }))";
  auto worker_url = https_server().GetURL(
      "0.0.0.0", "/set-header?Access-Control-Allow-Origin: *");
  EXPECT_EQ(true,
            content::EvalJs(web_contents(),
                            content::JsReplace(kWorkerScript, worker_url)));

  feature_histogram_tester.ExpectCounts(
      AddFeatureCounts(AllZeroFeatureCounts(AllAddressSpaceFeatures()),
                       {
                           {WebFeature::kPrivateNetworkAccessNullIpAddress, 1},
                       }));
}

class LocalNetworkAccessWebSocketCountersBrowserTest
    : public LocalNetworkAccessCountersBrowserTest {
 public:
  LocalNetworkAccessWebSocketCountersBrowserTest() = default;

  net::EmbeddedTestServer& ws_server() { return ws_server_; }

  std::string WaitAndGetTitle() {
    return base::UTF16ToUTF8(watcher_->WaitAndGetTitle());
  }

 private:
  void SetUpOnMainThread() override {
    net::test_server::InstallDefaultWebSocketHandlers(&ws_server_);
    ASSERT_TRUE(ws_server_.Start());

    LocalNetworkAccessCountersBrowserTest::SetUpOnMainThread();

    watcher_ = std::make_unique<content::TitleWatcher>(web_contents(), u"PASS");
    watcher_->AlsoWaitForTitle(u"FAIL");
  }

  void TearDownOnMainThread() override { watcher_.reset(); }

  net::EmbeddedTestServer ws_server_{net::EmbeddedTestServer::Type::TYPE_HTTPS};
  std::unique_ptr<content::TitleWatcher> watcher_;
};

// When WebSocket is connected to a more-private ip address space, log a use
// counter.
// TODO(crbug.com/336429017): Flaky on Win.
#if BUILDFLAG(IS_WIN)
#define MAYBE_WebSocketConnectedPublicToLocal \
  DISABLED_WebSocketConnectedPublicToLocal
#else
#define MAYBE_WebSocketConnectedPublicToLocal WebSocketConnectedPublicToLocal
#endif
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessWebSocketCountersBrowserTest,
                       MAYBE_WebSocketConnectedPublicToLocal) {
  WebFeatureHistogramTester feature_histogram_tester;

  // Enable auto-accept of LNA permission request.
  bubble_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  LOG(ERROR) << ws_server().GetURL("/echo-with-no-extension").spec();
  EXPECT_TRUE(content::NavigateToURL(
      web_contents(),
      https_public_server().GetURL(
          "a.com",
          "/local_network_access/"
          "websocket.html"
          "?url=" +
              ws_server().GetURL("/echo-with-no-extension").spec())));

  EXPECT_EQ("PASS", WaitAndGetTitle());
  feature_histogram_tester.ExpectCounts(AddFeatureCounts(
      AllZeroFeatureCounts(AllAddressSpaceFeatures()),
      {
          {WebFeature::kPrivateNetworkAccessWebSocketConnected, 1},
      }));
}

#if BUILDFLAG(IS_WIN)
#define MAYBE_WebSocketConnectedPublicToLocalNonLocalHost \
  DISABLED_WebSocketConnectedPublicToLocalNonLocalHost
#else
#define MAYBE_WebSocketConnectedPublicToLocalNonLocalHost \
  WebSocketConnectedPublicToLocalNonLocalHost
#endif
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessWebSocketCountersBrowserTest,
                       MAYBE_WebSocketConnectedPublicToLocalNonLocalHost) {
  WebFeatureHistogramTester feature_histogram_tester;

  // Enable auto-accept of LNA permission request.
  bubble_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  LOG(ERROR) << ws_server().GetURL("b.com", "/echo-with-no-extension").spec();
  EXPECT_TRUE(content::NavigateToURL(
      web_contents(),
      https_public_server().GetURL(
          "a.com",
          "/local_network_access/"
          "websocket.html"
          "?url=" +
              ws_server().GetURL("b.com", "/echo-with-no-extension").spec())));

  EXPECT_EQ("PASS", WaitAndGetTitle());
  feature_histogram_tester.ExpectCounts(AddFeatureCounts(
      AllZeroFeatureCounts(AllAddressSpaceFeatures()),
      {
          {WebFeature::kPrivateNetworkAccessWebSocketConnected, 1},
          {WebFeature::kLocalNetworkAccessWebSocketResourceNotKnownPrivate, 0},
      }));
}

// When WebSocket is connected to the same ip address space, do not log a use
// counter.
// TODO(crbug.com/336429017): Flaky on Win.
#if BUILDFLAG(IS_WIN)
#define MAYBE_WebSocketConnectedLocalToLocal \
  DISABLED_WebSocketConnectedLocalToLocal
#else
#define MAYBE_WebSocketConnectedLocalToLocal WebSocketConnectedLocalToLocal
#endif
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessWebSocketCountersBrowserTest,
                       MAYBE_WebSocketConnectedLocalToLocal) {
  WebFeatureHistogramTester feature_histogram_tester;

  EXPECT_TRUE(content::NavigateToURL(
      web_contents(),
      https_server().GetURL(
          "a.com",
          "/local_network_access/"
          "websocket.html"
          "?url=" +
              ws_server().GetURL("/echo-with-no-extension").spec())));

  EXPECT_EQ("PASS", WaitAndGetTitle());
  feature_histogram_tester.ExpectCounts(AddFeatureCounts(
      AllZeroFeatureCounts(AllAddressSpaceFeatures()),
      {
          {WebFeature::kPrivateNetworkAccessWebSocketConnected, 0},
          {WebFeature::kLocalNetworkAccessWebSocketResourceNotKnownPrivate, 0},
      }));
}

}  // namespace local_network_access
