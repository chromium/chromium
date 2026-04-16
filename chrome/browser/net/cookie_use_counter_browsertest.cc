// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/net/storage_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"

namespace {

constexpr std::string_view kParentHost = "a.test";
constexpr std::string_view kCookieHost = "b.test";

class CookieUseCounterBrowserTest : public InProcessBrowserTest {
 public:
  CookieUseCounterBrowserTest(const CookieUseCounterBrowserTest&) = delete;
  CookieUseCounterBrowserTest& operator=(const CookieUseCounterBrowserTest&) =
      delete;

 protected:
  CookieUseCounterBrowserTest() = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    base::FilePath path;
    base::PathService::Get(content::DIR_TEST_DATA, &path);
    embedded_https_test_server().SetSSLConfig(
        net::EmbeddedTestServer::CERT_TEST_NAMES);
    embedded_https_test_server().ServeFilesFromDirectory(path);
    embedded_https_test_server().AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  GURL GetURL(std::string_view host, std::string_view path) {
    return embedded_https_test_server().GetURL(host, path);
  }

  void NavigateTo(std::string_view host, std::string_view path) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetURL(host, path)));
  }

  void NavigateIframeTo(std::string_view host, std::string_view path) {
    ASSERT_TRUE(content::NavigateIframeToURL(web_contents(), "test",
                                             GetURL(host, path)));
  }

  void SetCookie(const GURL& url,
                 std::string_view cookie_line,
                 std::optional<net::CookiePartitionKey> cookie_partition_key =
                     std::nullopt) {
    ASSERT_TRUE(content::SetCookie(
        browser()->profile(), url, cookie_line,
        net::CookieOptions::SameSiteCookieContext::MakeInclusive(),
        cookie_partition_key));
  }

  std::string GetIframeContent() {
    content::RenderFrameHost* main_frame = browser()
                                               ->tab_strip_model()
                                               ->GetActiveWebContents()
                                               ->GetPrimaryMainFrame();
    content::RenderFrameHost* iframe_rfh = content::ChildFrameAt(main_frame, 0);
    EXPECT_TRUE(iframe_rfh);
    return storage::test::GetFrameContent(iframe_rfh);
  }
};

IN_PROC_BROWSER_TEST_F(
    CookieUseCounterBrowserTest,
    HttpOnlyCookieShadowedByNonHttpOnlyPartitioned_BothAvailable) {
  base::HistogramTester histogram_tester;
  SetCookie(GetURL(kCookieHost, "/"),
            "shadowed=a;SameSite=None;Secure;HttpOnly");
  SetCookie(
      GetURL(kCookieHost, "/"), "shadowed=b;SameSite=None;Secure;Partitioned",
      net::CookiePartitionKey::FromURLForTesting(GetURL(kParentHost, "/")));
  NavigateTo(kParentHost, "/iframe.html");
  NavigateIframeTo(kCookieHost, "/simple.html");
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kHttpOnlyCookieShadowedByNonHttpOnlyPartitioned,
      /*expected_count=*/1);
  NavigateIframeTo(kCookieHost, "/echoheader?cookie");
  EXPECT_EQ(GetIframeContent(), "shadowed=a; shadowed=b");
}

IN_PROC_BROWSER_TEST_F(
    CookieUseCounterBrowserTest,
    HttpOnlyCookieShadowedByNonHttpOnlyPartitioned_HttpOnlyAvailable) {
  base::HistogramTester histogram_tester;
  SetCookie(GetURL(kCookieHost, "/"),
            "shadowed=a;SameSite=None;Secure;HttpOnly");
  SetCookie(
      GetURL(kCookieHost, "/"), "shadowed=b;Secure;Partitioned",
      net::CookiePartitionKey::FromURLForTesting(GetURL(kParentHost, "/")));
  NavigateTo(kParentHost, "/iframe.html");
  NavigateIframeTo(kCookieHost, "/simple.html");
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kHttpOnlyCookieShadowedByNonHttpOnlyPartitioned,
      /*expected_count=*/0);
  NavigateIframeTo(kCookieHost, "/echoheader?cookie");
  EXPECT_EQ(GetIframeContent(), "shadowed=a");
}

IN_PROC_BROWSER_TEST_F(
    CookieUseCounterBrowserTest,
    HttpOnlyCookieShadowedByNonHttpOnlyPartitioned_PartitionedAvailable) {
  base::HistogramTester histogram_tester;
  SetCookie(GetURL(kCookieHost, "/"), "shadowed=a;Secure;HttpOnly");
  SetCookie(
      GetURL(kCookieHost, "/"), "shadowed=b;SameSite=None;Secure;Partitioned",
      net::CookiePartitionKey::FromURLForTesting(GetURL(kParentHost, "/")));
  NavigateTo(kParentHost, "/iframe.html");
  NavigateIframeTo(kCookieHost, "/simple.html");
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kHttpOnlyCookieShadowedByNonHttpOnlyPartitioned,
      /*expected_count=*/1);
  NavigateIframeTo(kCookieHost, "/echoheader?cookie");
  EXPECT_EQ(GetIframeContent(), "shadowed=b");
}

IN_PROC_BROWSER_TEST_F(
    CookieUseCounterBrowserTest,
    HttpOnlyCookieShadowedByNonHttpOnlyPartitioned_NeitherAvailable) {
  base::HistogramTester histogram_tester;
  SetCookie(GetURL(kCookieHost, "/"), "shadowed=a;Secure;HttpOnly");
  SetCookie(
      GetURL(kCookieHost, "/"), "shadowed=b;Secure;Partitioned",
      net::CookiePartitionKey::FromURLForTesting(GetURL(kParentHost, "/")));
  NavigateTo(kParentHost, "/iframe.html");
  NavigateIframeTo(kCookieHost, "/simple.html");
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kHttpOnlyCookieShadowedByNonHttpOnlyPartitioned,
      /*expected_count=*/0);
  NavigateIframeTo(kCookieHost, "/echoheader?cookie");
  EXPECT_EQ(GetIframeContent(), "None");
}

IN_PROC_BROWSER_TEST_F(
    CookieUseCounterBrowserTest,
    HttpOnlyCookieShadowedByNonHttpOnlyPartitioned_BothHttpOnly) {
  base::HistogramTester histogram_tester;
  SetCookie(GetURL(kCookieHost, "/"),
            "shadowed=a;SameSite=None;Secure;HttpOnly");
  SetCookie(
      GetURL(kCookieHost, "/"),
      "shadowed=b;SameSite=None;Secure;Partitioned;HttpOnly",
      net::CookiePartitionKey::FromURLForTesting(GetURL(kParentHost, "/")));
  NavigateTo(kParentHost, "/iframe.html");
  NavigateIframeTo(kCookieHost, "/simple.html");
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kHttpOnlyCookieShadowedByNonHttpOnlyPartitioned,
      /*expected_count=*/0);
  NavigateIframeTo(kCookieHost, "/echoheader?cookie");
  EXPECT_EQ(GetIframeContent(), "shadowed=a; shadowed=b");
}

IN_PROC_BROWSER_TEST_F(
    CookieUseCounterBrowserTest,
    HttpOnlyCookieShadowedByNonHttpOnlyPartitioned_NoPartitioned) {
  base::HistogramTester histogram_tester;
  SetCookie(GetURL(kCookieHost, "/"),
            "shadowed=a;SameSite=None;Secure;HttpOnly");
  NavigateTo(kParentHost, "/iframe.html");
  NavigateIframeTo(kCookieHost, "/simple.html");
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kHttpOnlyCookieShadowedByNonHttpOnlyPartitioned,
      /*expected_count=*/0);
  NavigateIframeTo(kCookieHost, "/echoheader?cookie");
  EXPECT_EQ(GetIframeContent(), "shadowed=a");
}

IN_PROC_BROWSER_TEST_F(
    CookieUseCounterBrowserTest,
    HttpOnlyCookieShadowedByNonHttpOnlyPartitioned_NoHttpOnly) {
  base::HistogramTester histogram_tester;
  SetCookie(
      GetURL(kCookieHost, "/"), "shadowed=b;SameSite=None;Secure;Partitioned",
      net::CookiePartitionKey::FromURLForTesting(GetURL(kParentHost, "/")));
  NavigateTo(kParentHost, "/iframe.html");
  NavigateIframeTo(kCookieHost, "/simple.html");
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kHttpOnlyCookieShadowedByNonHttpOnlyPartitioned,
      /*expected_count=*/0);
  NavigateIframeTo(kCookieHost, "/echoheader?cookie");
  EXPECT_EQ(GetIframeContent(), "shadowed=b");
}

IN_PROC_BROWSER_TEST_F(
    CookieUseCounterBrowserTest,
    HttpOnlyCookieShadowedByNonHttpOnlyPartitioned_NoShadowing) {
  base::HistogramTester histogram_tester;
  SetCookie(GetURL(kCookieHost, "/"),
            "shadowed=a;SameSite=None;Secure;HttpOnly");
  SetCookie(
      GetURL(kCookieHost, "/"),
      "not_shadowed=b;SameSite=None;Secure;Partitioned",
      net::CookiePartitionKey::FromURLForTesting(GetURL(kParentHost, "/")));
  NavigateTo(kParentHost, "/iframe.html");
  NavigateIframeTo(kCookieHost, "/simple.html");
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kHttpOnlyCookieShadowedByNonHttpOnlyPartitioned,
      /*expected_count=*/0);
  NavigateIframeTo(kCookieHost, "/echoheader?cookie");
  EXPECT_EQ(GetIframeContent(), "shadowed=a; not_shadowed=b");
}

IN_PROC_BROWSER_TEST_F(CookieUseCounterBrowserTest,
                       PartitionedCookiePresentV3_CountOnce) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  SetCookie(
      GetURL(kCookieHost, "/"), "/set-cookie?p1=a;Secure;Partitioned",
      net::CookiePartitionKey::FromURLForTesting(GetURL(kParentHost, "/")));
  SetCookie(
      GetURL(kCookieHost, "/"), "/set-cookie?p2=b;Secure;Partitioned",
      net::CookiePartitionKey::FromURLForTesting(GetURL(kParentHost, "/")));

  size_t entries_before =
      ukm_recorder.GetEntriesByName("PartitionedCookiePresentV3").size();

  NavigateTo(kParentHost, "/iframe.html");
  NavigateIframeTo(kCookieHost, "/simple.html");

  size_t entries_after =
      ukm_recorder.GetEntriesByName("PartitionedCookiePresentV3").size();
  size_t delta = entries_after - entries_before;

  // The request contains 2 partitioned cookies.
  EXPECT_EQ(delta, 1u);
}

}  // namespace
