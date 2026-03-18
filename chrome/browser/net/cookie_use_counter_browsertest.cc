// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/net/storage_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"

namespace {

constexpr std::string_view kHost = "a.test";
constexpr std::string_view kSubdomain = "sub.a.test";

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

  void NavigateTo(const std::string_view host, const std::string_view path) {
    GURL main_url(embedded_https_test_server().GetURL(host, path));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  }

  std::string GetFrameContent() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return storage::test::GetFrameContent(web_contents->GetPrimaryMainFrame());
  }
};

IN_PROC_BROWSER_TEST_F(
    CookieUseCounterBrowserTest,
    HttpOnlyCookieShadowedByNonHttpOnlyPartitioned_BothAvailable) {
  base::HistogramTester histogram_tester;
  NavigateTo(kHost, "/set-cookie?shadowed=a;Secure;HttpOnly");
  NavigateTo(kHost, "/set-cookie?shadowed=a;Secure;Partitioned");
  NavigateTo(kHost, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "shadowed=a; shadowed=a");
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kHttpOnlyCookieShadowedByNonHttpOnlyPartitioned,
      /*expected_count=*/1);
}

IN_PROC_BROWSER_TEST_F(
    CookieUseCounterBrowserTest,
    HttpOnlyCookieShadowedByNonHttpOnlyPartitioned_HttpOnlyAvailable) {
  base::HistogramTester histogram_tester;
  NavigateTo(kHost, "/set-cookie?shadowed=a;Secure;HttpOnly");
  NavigateTo(kSubdomain,
             base::StrCat({"/set-cookie?shadowed=b;Secure;Partitioned;Domain=",
                           kSubdomain}));
  NavigateTo(kHost, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "shadowed=a");
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kHttpOnlyCookieShadowedByNonHttpOnlyPartitioned,
      /*expected_count=*/0);
}

IN_PROC_BROWSER_TEST_F(
    CookieUseCounterBrowserTest,
    HttpOnlyCookieShadowedByNonHttpOnlyPartitioned_PartitionedAvailable) {
  base::HistogramTester histogram_tester;
  NavigateTo(kSubdomain,
             base::StrCat({"/set-cookie?shadowed=a;Secure;HttpOnly;Domain=",
                           kSubdomain}));
  NavigateTo(kHost, "/set-cookie?shadowed=b;Secure;Partitioned");
  NavigateTo(kHost, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "shadowed=b");
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kHttpOnlyCookieShadowedByNonHttpOnlyPartitioned,
      /*expected_count=*/1);
}

IN_PROC_BROWSER_TEST_F(
    CookieUseCounterBrowserTest,
    HttpOnlyCookieShadowedByNonHttpOnlyPartitioned_NeitherAvailable) {
  base::HistogramTester histogram_tester;
  NavigateTo(kSubdomain,
             base::StrCat({"/set-cookie?shadowed=a;Secure;HttpOnly;Domain=",
                           kSubdomain}));
  NavigateTo(kSubdomain,
             base::StrCat({"/set-cookie?shadowed=b;Secure;Partitioned;Domain=",
                           kSubdomain}));
  NavigateTo(kHost, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kHttpOnlyCookieShadowedByNonHttpOnlyPartitioned,
      /*expected_count=*/0);
}

IN_PROC_BROWSER_TEST_F(
    CookieUseCounterBrowserTest,
    HttpOnlyCookieShadowedByNonHttpOnlyPartitioned_BothHttpOnly) {
  base::HistogramTester histogram_tester;
  NavigateTo(kHost, "/set-cookie?shadowed=a;Secure;HttpOnly");
  NavigateTo(kHost, "/set-cookie?shadowed=a;Secure;Partitioned;HttpOnly");
  NavigateTo(kHost, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "shadowed=a; shadowed=a");
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kHttpOnlyCookieShadowedByNonHttpOnlyPartitioned,
      /*expected_count=*/0);
}

IN_PROC_BROWSER_TEST_F(
    CookieUseCounterBrowserTest,
    HttpOnlyCookieShadowedByNonHttpOnlyPartitioned_NoPartitioned) {
  base::HistogramTester histogram_tester;
  NavigateTo(kHost, "/set-cookie?shadowed=a;Secure;HttpOnly");
  NavigateTo(kHost, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "shadowed=a");
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kHttpOnlyCookieShadowedByNonHttpOnlyPartitioned,
      /*expected_count=*/0);
}

IN_PROC_BROWSER_TEST_F(
    CookieUseCounterBrowserTest,
    HttpOnlyCookieShadowedByNonHttpOnlyPartitioned_NoHttpOnly) {
  base::HistogramTester histogram_tester;
  NavigateTo(kHost, "/set-cookie?shadowed=a;Secure;Partitioned");
  NavigateTo(kHost, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "shadowed=a");
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kHttpOnlyCookieShadowedByNonHttpOnlyPartitioned,
      /*expected_count=*/0);
}

IN_PROC_BROWSER_TEST_F(
    CookieUseCounterBrowserTest,
    HttpOnlyCookieShadowedByNonHttpOnlyPartitioned_NoShadowing) {
  base::HistogramTester histogram_tester;
  NavigateTo(kHost, "/set-cookie?shadowed=a;Secure;HttpOnly");
  NavigateTo(kHost, "/set-cookie?not_shadowed=a;Secure;Partitioned");
  NavigateTo(kHost, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "shadowed=a; not_shadowed=a");
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kHttpOnlyCookieShadowedByNonHttpOnlyPartitioned,
      /*expected_count=*/0);
}

}  // namespace
