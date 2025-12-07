// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/google/browser/gws_page_load_metrics_observer.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/page_load_metrics/integration_tests/metric_integration_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/page_load_metrics/google/browser/google_url_util.h"
#include "components/page_load_metrics/google/browser/gws_abandoned_page_load_metrics_observer.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/prerender_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

std::unique_ptr<net::test_server::HttpResponse> SRPHandler(
    const net::test_server::HttpRequest& request) {
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HttpStatusCode::HTTP_OK);
  http_response->set_content_type("text/html");
  // TODO(crbug.com/436345871): Consider sending the performance mark to test
  // the metrics.
  http_response->set_content(R"(
    <html>
      <body>
        SRP Content
      </body>
    </html>
  )");
  return http_response;
}

using page_load_metrics::PageLoadMetricsTestWaiter;

class GWSPageLoadMetricsObserverBrowserTest : public MetricIntegrationTest {
 public:
  GWSPageLoadMetricsObserverBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &GWSPageLoadMetricsObserverBrowserTest::GetActiveWebContents,
            base::Unretained(this))) {}

  GWSPageLoadMetricsObserverBrowserTest(
      const GWSPageLoadMetricsObserverBrowserTest&) = delete;
  GWSPageLoadMetricsObserverBrowserTest& operator=(
      const GWSPageLoadMetricsObserverBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    MetricIntegrationTest::SetUpOnMainThread();
    embedded_test_server()->RegisterDefaultHandler(
        base::BindRepeating(&net::test_server::HandlePrefixedRequest, "/search",
                            base::BindRepeating(SRPHandler)));
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
    Start();
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  std::unique_ptr<PageLoadMetricsTestWaiter> CreatePageLoadMetricsTestWaiter() {
    return std::make_unique<PageLoadMetricsTestWaiter>(web_contents());
  }

  GURL GetSrpUrl(std::optional<std::string> query) {
    constexpr char kSRPDomain[] = "www.google.com";
    constexpr char kSRPPath[] = "/search?q=";

    GURL url(embedded_test_server()->GetURL(kSRPDomain,
                                            kSRPPath + query.value_or("")));
    EXPECT_TRUE(page_load_metrics::IsGoogleSearchResultUrl(url));
    return url;
  }

 protected:
  content::test::PrerenderTestHelper prerender_helper_;
};

IN_PROC_BROWSER_TEST_F(GWSPageLoadMetricsObserverBrowserTest,
                       PrerenderSRPAndActivate) {
  base::HistogramTester histogram_tester;

  // Navigate to an initial SRP page and wait until load event.
  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(
      PageLoadMetricsTestWaiter::TimingField::kLoadEvent);
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetSrpUrl(std::nullopt)));
  waiter->Wait();

  // Check that the prerender metrics are not recorded.
  histogram_tester.ExpectTotalCount(internal::kHistogramPrerenderHostReused, 0);
  histogram_tester.ExpectTotalCount(
      internal::kHistogramGWSPrerenderNavigationToActivation, 0);

  GURL url_srp_prerender = GetSrpUrl("prerender");
  prerender_helper_.AddPrerender(url_srp_prerender);

  // Activate the prerendered SRP on the initial WebContents.
  content::TestActivationManager activation_manager(web_contents(),
                                                    url_srp_prerender);
  ASSERT_TRUE(
      content::ExecJs(web_contents()->GetPrimaryMainFrame(),
                      content::JsReplace("location = $1", url_srp_prerender)));
  activation_manager.WaitForNavigationFinished();
  EXPECT_TRUE(activation_manager.was_activated());

  // Flush metrics.
  ASSERT_TRUE(
      content::NavigateToURL(web_contents(), GURL(url::kAboutBlankURL)));

  // Check that the prerender metrics are recorded.
  histogram_tester.ExpectTotalCount(internal::kHistogramPrerenderHostReused, 1);
  histogram_tester.ExpectTotalCount(
      internal::kHistogramGWSPrerenderNavigationToActivation, 1);
}

}  // namespace
