// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/gws_hp_page_load_metrics_observer.h"

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/after_startup_task_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/page_load_metrics/integration_tests/metric_integration_test.h"
#include "chrome/browser/page_load_metrics/observers/histogram_suffixes.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_navigation_throttle.h"
#include "content/public/test/test_navigation_throttle_inserter.h"
#include "net/test/embedded_test_server/request_handler_util.h"

namespace {

constexpr char kGoogleHomepageUrl[] = "www.google.com";
constexpr char kNonGoogleHomepageUrl[] = "www.example.com";

std::unique_ptr<net::test_server::HttpResponse> RequestHandler(
    const net::test_server::HttpRequest& request) {
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HttpStatusCode::HTTP_OK);
  http_response->set_content_type("text/html");
  http_response->set_content("<html><body>Request Handled</body></html>");
  return http_response;
}

}  // namespace
using page_load_metrics::PageLoadMetricsTestWaiter;

class GWSHpPageLoadMetricsObserverBrowserTest : public MetricIntegrationTest {
 public:
  GWSHpPageLoadMetricsObserverBrowserTest() = default;

  ~GWSHpPageLoadMetricsObserverBrowserTest() override = default;

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

 protected:
  std::unique_ptr<PageLoadMetricsTestWaiter> CreatePageLoadMetricsTestWaiter() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return std::make_unique<PageLoadMetricsTestWaiter>(web_contents);
  }

  void SetUpOnMainThread() override {
    MetricIntegrationTest::SetUpOnMainThread();
    embedded_test_server()->RegisterDefaultHandler(
        base::BindRepeating(&RequestHandler));
    Start();

    // Wait until the browser init is complete.
    AfterStartupTaskUtils::StartMonitoringStartup();
  }
};

IN_PROC_BROWSER_TEST_F(GWSHpPageLoadMetricsObserverBrowserTest,
                       FirstNavigationHomepage) {
  base::HistogramTester histogram_tester;
  // Navigation #1: Navigate to HP.
  auto url = embedded_test_server()->GetURL(kGoogleHomepageUrl, "/");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));

  url = embedded_test_server()->GetURL(kNonGoogleHomepageUrl, "/");
  // Navigate to a non-HP page to flush the metrics.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string suffix = internal::kSuffixFirstNavigation;

  histogram_tester.ExpectTotalCount(
      internal::kHistogramGWSHpParseStart + suffix, 1);
  histogram_tester.ExpectTotalCount(
      internal::kHistogramGWSHpConnectStart + suffix, 1);
  histogram_tester.ExpectTotalCount(
      internal::kHistogramGWSHpDomainLookupStart + suffix, 1);
  histogram_tester.ExpectTotalCount(
      internal::kHistogramGWSHpDomainLookupEnd + suffix, 1);
}

IN_PROC_BROWSER_TEST_F(GWSHpPageLoadMetricsObserverBrowserTest,
                       SubsequentNavigationHomepage) {
  base::HistogramTester histogram_tester;
  auto url = embedded_test_server()->GetURL(kNonGoogleHomepageUrl, "/");
  // Navigate to a non-SRP page to flush the metrics.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));

  // SRP Navigation #2: Navigate to SRP.
  url = embedded_test_server()->GetURL(kGoogleHomepageUrl, "/");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));

  url = embedded_test_server()->GetURL(kNonGoogleHomepageUrl, "/");
  // Navigate to a non-SRP page to flush the metrics.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string suffix = internal::kSuffixSubsequentNavigation;

  histogram_tester.ExpectTotalCount(
      internal::kHistogramGWSHpParseStart + suffix, 1);
  histogram_tester.ExpectTotalCount(
      internal::kHistogramGWSHpConnectStart + suffix, 1);
  histogram_tester.ExpectTotalCount(
      internal::kHistogramGWSHpDomainLookupStart + suffix, 1);
  histogram_tester.ExpectTotalCount(
      internal::kHistogramGWSHpDomainLookupEnd + suffix, 1);
}

IN_PROC_BROWSER_TEST_F(GWSHpPageLoadMetricsObserverBrowserTest, NoHomepage) {
  base::HistogramTester histogram_tester;
  // Navigation #1: Navigate to HP.
  auto url = embedded_test_server()->GetURL(kNonGoogleHomepageUrl, "/");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));

  url = embedded_test_server()->GetURL(kNonGoogleHomepageUrl, "/");
  // Navigate to a non-HP page to flush the metrics.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string suffix = internal::kSuffixFirstNavigation;

  histogram_tester.ExpectTotalCount(
      internal::kHistogramGWSHpParseStart + suffix, 0);
  histogram_tester.ExpectTotalCount(
      internal::kHistogramGWSHpConnectStart + suffix, 0);
  histogram_tester.ExpectTotalCount(
      internal::kHistogramGWSHpDomainLookupStart + suffix, 0);
  histogram_tester.ExpectTotalCount(
      internal::kHistogramGWSHpDomainLookupEnd + suffix, 0);
}
