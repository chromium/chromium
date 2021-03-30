// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/prefetch_proxy_page_load_metrics_observer.h"

#include <memory>

#include "base/command_line.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/embedded_test_server_connection_listener.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "testing/gtest/include/gtest/gtest.h"

class IsolatedPrerenderPageLoadMetricsObserverBrowserTest
    : public InProcessBrowserTest {
 public:
  IsolatedPrerenderPageLoadMetricsObserverBrowserTest() = default;
  ~IsolatedPrerenderPageLoadMetricsObserverBrowserTest() override = default;

  void EnableDataSaver() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        data_reduction_proxy::switches::kEnableDataReductionProxy);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");

    embedded_test_server()->ServeFilesFromSourceDirectory(
        "chrome/test/data/subresource_loading");
    ASSERT_TRUE(embedded_test_server()->Start());

    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  void NavigateTo(const GURL& url) {
    ui_test_utils::NavigateToURL(browser(), url);
    base::RunLoop().RunUntilIdle();
  }

  void NavigateToOriginPath(const std::string& path) {
    ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("origin.com", path));
    base::RunLoop().RunUntilIdle();
  }

  void NavigateAway() {
    ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
    base::RunLoop().RunUntilIdle();
  }

  void VerifyNoUKM() {
    auto entries = ukm_recorder_->GetEntriesByName(
        ukm::builders::PrefetchProxy::kEntryName);
    EXPECT_TRUE(entries.empty());
  }

  void VerifyUKMEntry(const std::string& metric_name,
                      base::Optional<int64_t> expected_value) {
    auto entries = ukm_recorder_->GetEntriesByName(
        ukm::builders::PrefetchProxy::kEntryName);
    ASSERT_EQ(1U, entries.size());

    const auto* entry = entries.front();

    ukm_recorder_->ExpectEntrySourceHasUrl(
        entry, embedded_test_server()->GetURL("origin.com", "/index.html"));

    const int64_t* value =
        ukm::TestUkmRecorder::GetEntryMetric(entry, metric_name);
    EXPECT_EQ(value != nullptr, expected_value.has_value());

    if (!expected_value.has_value())
      return;

    EXPECT_EQ(*value, expected_value.value());
  }

  GURL GetOriginURL(const std::string& path) {
    return embedded_test_server()->GetURL("origin.com", path);
  }

 private:
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
};

IN_PROC_BROWSER_TEST_F(IsolatedPrerenderPageLoadMetricsObserverBrowserTest,
                       BeforeFCPPlumbing) {
  base::HistogramTester histogram_tester;
  NavigateToOriginPath("/index.html");
  NavigateAway();

  histogram_tester.ExpectUniqueSample(
      "PageLoad.Clients.SubresourceLoading.LoadedCSSJSBeforeFCP.Noncached", 2,
      1);
}

// TODO(http://crbug.com/1025737) Flaky on Mac.
#if defined(OS_MAC)
#define MAYBE_HistoryPlumbing DISABLED_HistoryPlumbing
#else
#define MAYBE_HistoryPlumbing HistoryPlumbing
#endif
IN_PROC_BROWSER_TEST_F(IsolatedPrerenderPageLoadMetricsObserverBrowserTest,
                       MAYBE_HistoryPlumbing) {
  base::HistogramTester histogram_tester;
  NavigateToOriginPath("/index.html");
  NavigateAway();
  histogram_tester.ExpectUniqueSample(
      "PageLoad.Clients.SubresourceLoading.HasPreviousVisitToOrigin", false, 1);
  histogram_tester.ExpectTotalCount(
      "PageLoad.Clients.SubresourceLoading.DaysSinceLastVisitToOrigin", 0);

  // Revisit and expect a 0 days-ago entry.
  NavigateToOriginPath("/index.html");
  NavigateAway();

  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.SubresourceLoading.HasPreviousVisitToOrigin", true, 1);
  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.SubresourceLoading.HasPreviousVisitToOrigin", false, 1);
  histogram_tester.ExpectUniqueSample(
      "PageLoad.Clients.SubresourceLoading.DaysSinceLastVisitToOrigin", 0, 1);
}

IN_PROC_BROWSER_TEST_F(IsolatedPrerenderPageLoadMetricsObserverBrowserTest,
                       MainFrameHadCookies_NoUKM) {
  base::HistogramTester histogram_tester;
  NavigateToOriginPath("/index.html");
  NavigateAway();

  histogram_tester.ExpectUniqueSample(
      "PageLoad.Clients.SubresourceLoading.MainFrameHadCookies", false, 1);

  VerifyNoUKM();
}

IN_PROC_BROWSER_TEST_F(IsolatedPrerenderPageLoadMetricsObserverBrowserTest,
                       MainFrameHadCookies_None) {
  EnableDataSaver();
  base::HistogramTester histogram_tester;

  NavigateToOriginPath("/index.html");
  NavigateAway();

  histogram_tester.ExpectUniqueSample(
      "PageLoad.Clients.SubresourceLoading.MainFrameHadCookies", false, 1);

  using UkmEntry = ukm::builders::PrefetchProxy;
  VerifyUKMEntry(UkmEntry::kmainpage_request_had_cookiesName, 0);
}

IN_PROC_BROWSER_TEST_F(IsolatedPrerenderPageLoadMetricsObserverBrowserTest,
                       MainFrameHadCookies_CookiesOnNextPageLoad) {
  NavigateToOriginPath("/set_cookies.html");
  base::HistogramTester histogram_tester;

  EnableDataSaver();
  NavigateToOriginPath("/index.html");
  NavigateAway();

  using UkmEntry = ukm::builders::PrefetchProxy;
  VerifyUKMEntry(UkmEntry::kmainpage_request_had_cookiesName, 1);
  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.SubresourceLoading.MainFrameHadCookies", true, 1);
  // From the first page load.
  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.SubresourceLoading.MainFrameHadCookies", false, 1);
}

IN_PROC_BROWSER_TEST_F(IsolatedPrerenderPageLoadMetricsObserverBrowserTest,
                       MainFrameHadCookies_CookiesOnRedirect) {
  NavigateToOriginPath("/set_cookies.html");
  base::HistogramTester histogram_tester;

  EnableDataSaver();
  NavigateToOriginPath("/redirect_to_index.html");
  NavigateAway();

  using UkmEntry = ukm::builders::PrefetchProxy;
  VerifyUKMEntry(UkmEntry::kmainpage_request_had_cookiesName, 1);
  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.SubresourceLoading.MainFrameHadCookies", true, 1);
  // From the first page load.
  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.SubresourceLoading.MainFrameHadCookies", false, 1);
}

namespace {
std::unique_ptr<net::test_server::HttpResponse> HandleRedirectRequest(
    const GURL& redirect_to,
    const net::test_server::HttpRequest& request) {
  if (request.GetURL().spec().find("redirect_me") != std::string::npos) {
    std::unique_ptr<net::test_server::BasicHttpResponse> response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_TEMPORARY_REDIRECT);
    response->AddCustomHeader("Location", redirect_to.spec());
    return std::move(response);
  }
  return nullptr;
}
}  // namespace

// Regression test for crbug.com/1029959.
IN_PROC_BROWSER_TEST_F(IsolatedPrerenderPageLoadMetricsObserverBrowserTest,
                       MainFrameHadCookies_CrossOriginCookiesOnRedirect) {
  net::EmbeddedTestServer redirect_server(net::EmbeddedTestServer::TYPE_HTTP);
  redirect_server.RegisterRequestHandler(
      base::BindRepeating(&HandleRedirectRequest, GetOriginURL("/index.html")));
  ASSERT_TRUE(redirect_server.Start());

  NavigateToOriginPath("/set_cookies.html");
  base::HistogramTester histogram_tester;

  EnableDataSaver();

  NavigateTo(redirect_server.GetURL("redirect.com", "/redirect_me"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  EXPECT_EQ(web_contents->GetLastCommittedURL(), GetOriginURL("/index.html"));

  NavigateAway();

  using UkmEntry = ukm::builders::PrefetchProxy;
  VerifyUKMEntry(UkmEntry::kmainpage_request_had_cookiesName, 1);
  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.SubresourceLoading.MainFrameHadCookies", true, 1);
  // From the first page load.
  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.SubresourceLoading.MainFrameHadCookies", false, 1);
}

IN_PROC_BROWSER_TEST_F(IsolatedPrerenderPageLoadMetricsObserverBrowserTest,
                       RecordNothingOnUntrackedPage) {
  EnableDataSaver();
  base::HistogramTester histogram_tester;

  NavigateAway();
  NavigateAway();

  VerifyNoUKM();
  histogram_tester.ExpectTotalCount(
      "PageLoad.Clients.SubresourceLoading.DaysSinceLastVisitToOrigin", 0);
  histogram_tester.ExpectTotalCount(
      "PageLoad.Clients.SubresourceLoading.HasPreviousVisitToOrigin", 0);
  histogram_tester.ExpectTotalCount(
      "PageLoad.Clients.SubresourceLoading.LoadedCSSJSBeforeFCP.Cached", 0);
  histogram_tester.ExpectTotalCount(
      "PageLoad.Clients.SubresourceLoading.LoadedCSSJSBeforeFCP.Noncached", 0);
  histogram_tester.ExpectTotalCount(
      "PageLoad.Clients.SubresourceLoading.MainFrameHadCookies", 0);
}
