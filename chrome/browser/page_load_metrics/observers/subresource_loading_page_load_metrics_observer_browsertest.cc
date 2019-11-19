// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/subresource_loading_page_load_metrics_observer.h"

#include <memory>

#include "base/command_line.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/ukm/test_ukm_recorder.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "testing/gtest/include/gtest/gtest.h"

class SubresourceLoadingPageLoadMetricsObserverBrowserTest
    : public InProcessBrowserTest {
 public:
  SubresourceLoadingPageLoadMetricsObserverBrowserTest() = default;
  ~SubresourceLoadingPageLoadMetricsObserverBrowserTest() override = default;

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

  void NavigateToPath(const std::string& path) {
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

 private:
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
};

IN_PROC_BROWSER_TEST_F(SubresourceLoadingPageLoadMetricsObserverBrowserTest,
                       BeforeFCPPlumbing) {
  base::HistogramTester histogram_tester;
  NavigateToPath("/index.html");
  NavigateAway();

  histogram_tester.ExpectUniqueSample(
      "PageLoad.Clients.SubresourceLoading.LoadedCSSJSBeforeFCP.Noncached", 2,
      1);
}

// TODO(http://crbug.com/1025737) Flaky on Mac.
#if defined(OS_MACOSX)
#define MAYBE_HistoryPlumbing DISABLED_HistoryPlumbing
#else
#define MAYBE_HistoryPlumbing HistoryPlumbing
#endif
IN_PROC_BROWSER_TEST_F(SubresourceLoadingPageLoadMetricsObserverBrowserTest,
                       MAYBE_HistoryPlumbing) {
  base::HistogramTester histogram_tester;
  NavigateToPath("/index.html");
  NavigateAway();
  histogram_tester.ExpectUniqueSample(
      "PageLoad.Clients.SubresourceLoading.HasPreviousVisitToOrigin", false, 1);
  histogram_tester.ExpectTotalCount(
      "PageLoad.Clients.SubresourceLoading.DaysSinceLastVisitToOrigin", 0);
  histogram_tester.ExpectTotalCount(
      "PageLoad.Clients.SubresourceLoading.HistoryQueryTime", 1);

  // Revisit and expect a 0 days-ago entry.
  NavigateToPath("/index.html");
  NavigateAway();

  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.SubresourceLoading.HasPreviousVisitToOrigin", true, 1);
  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.SubresourceLoading.HasPreviousVisitToOrigin", false, 1);
  histogram_tester.ExpectUniqueSample(
      "PageLoad.Clients.SubresourceLoading.DaysSinceLastVisitToOrigin", 0, 1);
  histogram_tester.ExpectTotalCount(
      "PageLoad.Clients.SubresourceLoading.HistoryQueryTime", 2);
}

IN_PROC_BROWSER_TEST_F(SubresourceLoadingPageLoadMetricsObserverBrowserTest,
                       MainFrameHadCookies_NoUKM) {
  base::HistogramTester histogram_tester;
  NavigateToPath("/index.html");
  NavigateAway();

  histogram_tester.ExpectUniqueSample(
      "PageLoad.Clients.SubresourceLoading.MainFrameHadCookies", false, 1);
  histogram_tester.ExpectTotalCount(
      "PageLoad.Clients.SubresourceLoading.CookiesQueryTime", 1);

  VerifyNoUKM();
}

IN_PROC_BROWSER_TEST_F(SubresourceLoadingPageLoadMetricsObserverBrowserTest,
                       MainFrameHadCookies_None) {
  EnableDataSaver();
  base::HistogramTester histogram_tester;

  NavigateToPath("/index.html");
  NavigateAway();

  histogram_tester.ExpectUniqueSample(
      "PageLoad.Clients.SubresourceLoading.MainFrameHadCookies", false, 1);
  histogram_tester.ExpectTotalCount(
      "PageLoad.Clients.SubresourceLoading.CookiesQueryTime", 1);

  using UkmEntry = ukm::builders::PrefetchProxy;
  VerifyUKMEntry(UkmEntry::kmainpage_request_had_cookiesName, 0);
}

IN_PROC_BROWSER_TEST_F(SubresourceLoadingPageLoadMetricsObserverBrowserTest,
                       MainFrameHadCookies_CookiesOnNextPageLoad) {
  NavigateToPath("/set_cookies.html");
  base::HistogramTester histogram_tester;

  EnableDataSaver();
  NavigateToPath("/index.html");
  NavigateAway();

  using UkmEntry = ukm::builders::PrefetchProxy;
  VerifyUKMEntry(UkmEntry::kmainpage_request_had_cookiesName, 1);
  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.SubresourceLoading.MainFrameHadCookies", true, 1);
  // From the first page load.
  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.SubresourceLoading.MainFrameHadCookies", false, 1);
  histogram_tester.ExpectTotalCount(
      "PageLoad.Clients.SubresourceLoading.CookiesQueryTime", 2);
}

IN_PROC_BROWSER_TEST_F(SubresourceLoadingPageLoadMetricsObserverBrowserTest,
                       MainFrameHadCookies_CookiesOnRedirect) {
  NavigateToPath("/set_cookies.html");
  base::HistogramTester histogram_tester;

  EnableDataSaver();
  NavigateToPath("/redirect_to_index.html");
  NavigateAway();

  using UkmEntry = ukm::builders::PrefetchProxy;
  VerifyUKMEntry(UkmEntry::kmainpage_request_had_cookiesName, 1);
  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.SubresourceLoading.MainFrameHadCookies", true, 1);
  // From the first page load.
  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.SubresourceLoading.MainFrameHadCookies", false, 1);
  histogram_tester.ExpectTotalCount(
      "PageLoad.Clients.SubresourceLoading.CookiesQueryTime", 3);
}

IN_PROC_BROWSER_TEST_F(SubresourceLoadingPageLoadMetricsObserverBrowserTest,
                       RecordNothingOnUntrackedPage) {
  EnableDataSaver();
  base::HistogramTester histogram_tester;

  NavigateAway();
  NavigateAway();

  VerifyNoUKM();
  histogram_tester.ExpectTotalCount(
      "PageLoad.Clients.SubresourceLoading.CookiesQueryTime", 0);
  histogram_tester.ExpectTotalCount(
      "PageLoad.Clients.SubresourceLoading.DaysSinceLastVisitToOrigin", 0);
  histogram_tester.ExpectTotalCount(
      "PageLoad.Clients.SubresourceLoading.HasPreviousVisitToOrigin", 0);
  histogram_tester.ExpectTotalCount(
      "PageLoad.Clients.SubresourceLoading.HistoryQueryTime", 0);
  histogram_tester.ExpectTotalCount(
      "PageLoad.Clients.SubresourceLoading.LoadedCSSJSBeforeFCP.Cached", 0);
  histogram_tester.ExpectTotalCount(
      "PageLoad.Clients.SubresourceLoading.LoadedCSSJSBeforeFCP.Noncached", 0);
  histogram_tester.ExpectTotalCount(
      "PageLoad.Clients.SubresourceLoading.MainFrameHadCookies", 0);
}
