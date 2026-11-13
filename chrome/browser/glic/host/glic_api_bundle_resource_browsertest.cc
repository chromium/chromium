// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atomic>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/glic/host/guest_source.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace glic {
namespace {

// Matches `blink::V8CodeCache::GetMetadataType` in
// `third_party/blink/renderer/bindings/core/v8/v8_code_cache.h`.
constexpr int kV8CacheGetMetadataNone = 0;
constexpr int kV8CacheGetMetadataHotTimestamp = 1;
constexpr int kV8CacheGetMetadataCompileHintsHotTimestamp = 3;
constexpr int kV8CacheGetMetadataCodeCache = 5;

constexpr char kHtmlScriptTagClientHtml[] = R"html(
<!doctype html>
<html>
<head>
<script id="glic-api-script" async src="chrome__glic_api.js"></script>
</head>
<body>
<script type="module">
import {createGlicHostRegistryOnLoad} from '/glic/api_boot.js';
const registry = await createGlicHostRegistryOnLoad();
registry.registerWebClient({
  initialize: async () => {},
  notifyPanelWillOpen: async () => ({startingMode: 0}),
});
</script>
</body>
</html>
)html";

// Minimal guest page whose only >=1KB script is `chrome__glic_api.js`, so
// `WebCore.Scripts.V8CodeCacheMetadata.*` samples come exclusively from the
// intercepted Glic API bundle resource.
constexpr char kV8CacheClientHtml[] = R"html(
<!doctype html>
<html>
<head>
<script src="chrome__glic_api.js"></script>
</head>
<body>
<script>
window.internalAutoGlicBoot(window).registerWebClient({
  initialize: async () => {},
  notifyPanelWillOpen: async () => ({startingMode: 0}),
});
</script>
</body>
</html>
)html";

class GlicApiBundleResourceBrowserTestBase : public GlicBrowserTest {
 public:
  GlicApiBundleResourceBrowserTestBase(
      std::vector<base::test::FeatureRef> enabled_features,
      std::vector<base::test::FeatureRef> disabled_features) {
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
    GetGlicHttpsTestServer()->SetCertHostnames(
        {"login.corp.google.com", "127.0.0.1"});
  }

  void SetUpOnMainThread() override {
    GetGlicHttpsTestServer()->RegisterRequestMonitor(base::BindRepeating(
        &GlicApiBundleResourceBrowserTestBase::OnServerRequest,
        base::Unretained(this)));
    GetGlicHttpsTestServer()->RegisterRequestHandler(base::BindRepeating(
        &GlicApiBundleResourceBrowserTestBase::HandleCustomRequest,
        base::Unretained(this)));
    GlicBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("login.corp.google.com", "127.0.0.1");
  }

  int bundle_server_hits() const {
    return bundle_server_hits_.load(std::memory_order_relaxed);
  }

  void ResetBundleServerHits() {
    bundle_server_hits_.store(0, std::memory_order_relaxed);
  }

 private:
  void OnServerRequest(const net::test_server::HttpRequest& request) {
    const GURL url = request.GetURL();
    if (base::EndsWith(url.path(), kGlicApiBundleResourcePathSuffix,
                       base::CompareCase::SENSITIVE)) {
      bundle_server_hits_.fetch_add(1, std::memory_order_relaxed);
    }
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleCustomRequest(
      const net::test_server::HttpRequest& request) {
    const GURL url = request.GetURL();
    const std::string_view path = url.path();
    if (path == "/empty_server_script/client.html") {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->set_content_type("text/html");
      response->set_content(kHtmlScriptTagClientHtml);
      return response;
    }
    if (path == "/nested/v8_cache_client.html") {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->set_content_type("text/html");
      response->set_content(kV8CacheClientHtml);
      return response;
    }
    if (path == "/empty_guest_origin.html") {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->set_content_type("text/html");
      response->set_content("<!doctype html><html><body>Empty</body></html>");
      return response;
    }
    return nullptr;
  }

  base::test::ScopedFeatureList feature_list_;
  std::atomic<int> bundle_server_hits_{0};
};

class GlicApiBundleResourceBrowserTest
    : public GlicApiBundleResourceBrowserTestBase {
 public:
  GlicApiBundleResourceBrowserTest()
      : GlicApiBundleResourceBrowserTestBase(
            /*enabled_features=*/{features::kGlicNoWebview,
                                  features::kGlicApiBundleResource},
            /*disabled_features=*/{features::kGeicApiBundleResource}) {}
};

class GlicApiBundleWebviewUnchangedBrowserTest
    : public GlicApiBundleResourceBrowserTestBase {
 public:
  GlicApiBundleWebviewUnchangedBrowserTest()
      : GlicApiBundleResourceBrowserTestBase(
            /*enabled_features=*/{features::kGlicApiBundleResource},
            /*disabled_features=*/{features::kGlicNoWebview}) {}
};

class GlicApiBundleV8CodeCacheBrowserTest
    : public GlicApiBundleResourceBrowserTestBase {
 public:
  GlicApiBundleV8CodeCacheBrowserTest()
      : GlicApiBundleResourceBrowserTestBase(
            /*enabled_features=*/{features::kGlicNoWebview,
                                  features::kGlicApiBundleResource},
            /*disabled_features=*/{}) {
    SetGlicPagePath("/nested/v8_cache_client.html");
  }
};

class GlicApiHtmlScriptTagNewChromeBrowserTest
    : public GlicApiBundleResourceBrowserTestBase {
 public:
  GlicApiHtmlScriptTagNewChromeBrowserTest()
      : GlicApiBundleResourceBrowserTestBase(
            /*enabled_features=*/{features::kGlicNoWebview,
                                  features::kGlicApiBundleResource},
            /*disabled_features=*/{}) {
    SetGlicPagePath("/empty_server_script/client.html");
  }
};

IN_PROC_BROWSER_TEST_F(GlicApiBundleResourceBrowserTest,
                       LoadsBundleDirectlyWithoutBootstrapPostMessage) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));

  content::WebContents* guest_contents = instance->host().web_client_contents();
  ASSERT_TRUE(guest_contents);

  EXPECT_EQ(bundle_server_hits(), 0);
  // Default test page is `/glic/browser_tests/minimal_client.html`, so the
  // relative `chrome__glic_api.js` script resolves to
  // `/glic/browser_tests/chrome__glic_api.js` rather than root.
  EXPECT_EQ("/glic/browser_tests/chrome__glic_api.js",
            content::EvalJs(guest_contents, R"js(
              (() => {
                const s = document.querySelector(
                    'script[src="chrome__glic_api.js"]');
                return s ? new URL(s.src).pathname : '';
              })()
            )js"));

  // Verify response headers and that any `/chrome__glic_api.js` path suffix is
  // intercepted (while non-matching filenames are not).
  const std::string expected_body = GetGuestAPISource();
  EXPECT_EQ(
      base::Value(base::DictValue()
                      .Set("status", 200)
                      .Set("contentType", "text/javascript; charset=utf-8")
                      .Set("nosniff", "nosniff")
                      .Set("cacheControl", "")
                      .Set("relBody", expected_body)
                      .Set("rootStatus", 200)
                      .Set("rootBody", expected_body)
                      .Set("deepStatus", 200)
                      .Set("deepBody", expected_body)
                      .Set("nonMatchingStatus", 404)),
      content::EvalJs(guest_contents, R"js(
          (async () => {
            const resp = await fetch('chrome__glic_api.js');
            const rootResp = await fetch('/chrome__glic_api.js');
            const deepResp = await fetch('/u/1/glic/chrome__glic_api.js?v=1');
            const nonMatchingResp = await fetch('/not_chrome__glic_api.js');
            return {
              status: resp.status,
              contentType: resp.headers.get('content-type') ?? '',
              nosniff: resp.headers.get('x-content-type-options') ?? '',
              cacheControl: resp.headers.get('cache-control') ?? '',
              relBody: await resp.text(),
              rootStatus: rootResp.status,
              rootBody: await rootResp.text(),
              deepStatus: deepResp.status,
              deepBody: await deepResp.text(),
              nonMatchingStatus: nonMatchingResp.status,
            };
          })()
        )js"));
  EXPECT_EQ(bundle_server_hits(), 0);
}

IN_PROC_BROWSER_TEST_F(GlicApiHtmlScriptTagNewChromeBrowserTest,
                       InterceptsHtmlHeadScriptBeforeEmptyServerEndpoint) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));

  content::WebContents* guest_contents = instance->host().web_client_contents();
  ASSERT_TRUE(guest_contents);

  EXPECT_EQ(bundle_server_hits(), 0);
  EXPECT_EQ(1, content::EvalJs(guest_contents, R"js(
      document.querySelectorAll('script[src="chrome__glic_api.js"]').length
    )js"));
}

IN_PROC_BROWSER_TEST_F(GlicApiBundleResourceBrowserTest,
                       RejectsBundleResourceOutsideAllowedGuestMainFrame) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));
  content::WebContents* guest_contents = instance->host().web_client_contents();
  ASSERT_TRUE(guest_contents);
  ResetBundleServerHits();

  // 1. A normal browser tab on the exact same guest origin must not receive
  // Chrome's Glic API bundle.
  const GURL same_origin_page =
      GetGlicHttpsTestServer()->GetURL("/empty_guest_origin.html");
  tabs::TabInterface* normal_tab = CreateAndActivateTab(same_origin_page);
  EXPECT_EQ(404, content::EvalJs(normal_tab->GetContents(), R"js(
      fetch('chrome__glic_api.js').then(r => r.status)
    )js"));
  EXPECT_EQ(bundle_server_hits(), 1);

  // 2. A same-origin child iframe inside the Glic guest must not receive
  // Chrome's Glic API bundle.
  ASSERT_TRUE(
      content::ExecJs(guest_contents, content::JsReplace(R"js(
      new Promise(resolve => {
        const f = document.createElement('iframe');
        f.src = $1;
        f.onload = resolve;
        document.body.appendChild(f);
      })
    )js",
                                                         same_origin_page)));
  content::RenderFrameHost* child_rfh =
      content::ChildFrameAt(guest_contents->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(child_rfh);
  EXPECT_EQ(404, content::EvalJs(child_rfh, R"js(
      fetch('chrome__glic_api.js').then(r => r.status)
    )js"));
  EXPECT_EQ(bundle_server_hits(), 2);

  // 3. Navigating the Glic guest main frame to an origin where the Glic API is
  // not allowed (e.g., login page) must not expose Chrome's Glic API bundle.
  const GURL disallowed_origin_page = GetGlicHttpsTestServer()->GetURL(
      "login.corp.google.com", "/empty_guest_origin.html");
  ASSERT_TRUE(content::NavigateToURL(guest_contents, disallowed_origin_page));
  EXPECT_EQ(404, content::EvalJs(guest_contents, R"js(
      fetch('chrome__glic_api.js').then(r => r.status)
    )js"));
  EXPECT_EQ(bundle_server_hits(), 3);
}

IN_PROC_BROWSER_TEST_F(
    GlicApiBundleWebviewUnchangedBrowserTest,
    WebviewModeUsesBootstrapPostMessageWithoutInterceptingResource) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));

  content::WebContents* guest_contents = instance->host().web_client_contents();
  ASSERT_TRUE(guest_contents);

  EXPECT_GE(bundle_server_hits(), 1);
  EXPECT_EQ(404, content::EvalJs(guest_contents, R"js(
      fetch('chrome__glic_api.js').then(r => r.status)
    )js"));
  EXPECT_EQ("function", content::EvalJs(guest_contents, R"js(
      typeof window.internalAutoGlicBoot
    )js"));
}

IN_PROC_BROWSER_TEST_F(GlicApiBundleV8CodeCacheBrowserTest,
                       CachesV8BytecodeAcrossGuestLoads) {
  coordinator().GetWebContentsWarmingPoolForTesting().Shutdown();

  auto open_in_fresh_guest_and_record = [&]() {
    tabs::TabInterface* tab = CreateAndActivateTab(GURL("about:blank"));
    ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
    ASSERT_OK(WaitForGlicClient(instance));
    content::FetchHistogramsFromChildProcesses();
    metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
    GetTabListInterface()->CloseTab(tab->GetHandle());
  };

  // Load 1: Cold cache -> records hot timestamp for `chrome__glic_api.js`.
  {
    base::HistogramTester histogram_tester;
    ASSERT_NO_FATAL_FAILURE(open_in_fresh_guest_and_record());
    EXPECT_GE(
        histogram_tester.GetBucketCount(
            "WebCore.Scripts.V8CodeCacheMetadata.Get", kV8CacheGetMetadataNone),
        1);
  }

  // Load 2: Hot timestamp present -> produces and persists V8 bytecode.
  {
    base::HistogramTester histogram_tester;
    ASSERT_NO_FATAL_FAILURE(open_in_fresh_guest_and_record());
    const int hot_timestamp_hits =
        histogram_tester.GetBucketCount(
            "WebCore.Scripts.V8CodeCacheMetadata.Get",
            kV8CacheGetMetadataHotTimestamp) +
        histogram_tester.GetBucketCount(
            "WebCore.Scripts.V8CodeCacheMetadata.Get",
            kV8CacheGetMetadataCompileHintsHotTimestamp);
    EXPECT_GE(hot_timestamp_hits, 1);
    EXPECT_GE(histogram_tester.GetTotalCountForPrefix(
                  "V8.ProduceCodeCacheMicroseconds"),
              1);
  }

  // Load 3: Cached V8 bytecode is validated via source hash and consumed.
  {
    base::HistogramTester histogram_tester;
    ASSERT_NO_FATAL_FAILURE(open_in_fresh_guest_and_record());
    EXPECT_GE(histogram_tester.GetBucketCount(
                  "WebCore.Scripts.V8CodeCacheMetadata.Get",
                  kV8CacheGetMetadataCodeCache),
              1);
  }

  EXPECT_EQ(bundle_server_hits(), 0);
}

}  // namespace
}  // namespace glic
