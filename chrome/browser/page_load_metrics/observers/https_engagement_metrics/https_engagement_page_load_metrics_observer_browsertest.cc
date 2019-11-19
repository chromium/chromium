// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/https_engagement_metrics/https_engagement_page_load_metrics_observer.h"

#include "base/macros.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/https_engagement_metrics_provider.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/metrics/metrics_service.h"
#include "content/public/test/test_utils.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/ssl_server_config.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

class HttpsEngagementPageLoadMetricsBrowserTest : public InProcessBrowserTest {
 public:
  HttpsEngagementPageLoadMetricsBrowserTest()
      : metrics_provider_(new HttpsEngagementMetricsProvider()) {}
  ~HttpsEngagementPageLoadMetricsBrowserTest() override {}

  void StartHttpsServer(bool cert_error) {
    https_test_server_.reset(
        new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTPS));
    https_test_server_->SetSSLConfig(cert_error
                                         ? net::EmbeddedTestServer::CERT_EXPIRED
                                         : net::EmbeddedTestServer::CERT_OK);
    https_test_server_->ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(https_test_server_->Start());
  }

  void StartHttpServer() {
    http_test_server_.reset(
        new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTP));
    http_test_server_->ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(http_test_server_->Start());
  }

  // Navigate to a URL in a foreground tab, and close it. Return the upper bound
  // for how long the URL was open in the foreground.
  base::TimeDelta NavigateInForegroundAndCloseWithTiming(GURL target_url) {
    base::TimeTicks start = base::TimeTicks::Now();
    ui_test_utils::NavigateToURL(browser(), target_url);

    TabStripModel* tab_strip_model = browser()->tab_strip_model();
    content::WebContentsDestroyedWatcher destroyed_watcher(
        tab_strip_model->GetActiveWebContents());
    tab_strip_model->CloseAllTabs();
    destroyed_watcher.Wait();
    return (base::TimeTicks::Now() - start);
  }

  // Navigate to two URLs in the same foreground tab, and close it.
  void NavigateTwiceInTabAndClose(GURL first_url, GURL second_url) {
    ui_test_utils::NavigateToURL(browser(), first_url);
    ui_test_utils::NavigateToURL(browser(), second_url);

    TabStripModel* tab_strip_model = browser()->tab_strip_model();
    EXPECT_EQ(1, tab_strip_model->count());
    content::WebContentsDestroyedWatcher destroyed_watcher(
        tab_strip_model->GetActiveWebContents());
    tab_strip_model->CloseAllTabs();
    destroyed_watcher.Wait();
  }

  // Navigate to a URL in a foreground tab, move it to the background, then
  // close it. Return the upper bound for how long the URL was open in the
  // foreground.
  base::TimeDelta NavigateInForegroundAndCloseInBackgroundWithTiming(GURL url) {
    base::TimeTicks start = base::TimeTicks::Now();
    ui_test_utils::NavigateToURL(browser(), url);
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL(chrome::kChromeUIVersionURL),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
    base::TimeDelta upper_bound_delta = base::TimeTicks::Now() - start;

    // Make sure the correct tab is in the foreground.
    TabStripModel* tab_strip_model = browser()->tab_strip_model();
    EXPECT_EQ(2, tab_strip_model->count());
    EXPECT_EQ(url, tab_strip_model->GetWebContentsAt(0)->GetURL());
    EXPECT_NE(url, tab_strip_model->GetActiveWebContents()->GetURL());

    content::WebContentsDestroyedWatcher destroyed_watcher(
        tab_strip_model->GetWebContentsAt(0));
    EXPECT_TRUE(tab_strip_model->CloseWebContentsAt(0, 0));
    destroyed_watcher.Wait();
    EXPECT_EQ(1, tab_strip_model->count());

    return upper_bound_delta;
  }

  // Open and close a tab without ever bringing it to the foreground.
  void NavigateInBackgroundAndClose(GURL url) {
    ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIVersionURL));
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, WindowOpenDisposition::NEW_BACKGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

    // Make sure the correct tab is in the foreground.
    TabStripModel* tab_strip_model = browser()->tab_strip_model();
    EXPECT_EQ(2, tab_strip_model->count());
    EXPECT_EQ(url, tab_strip_model->GetWebContentsAt(1)->GetURL());
    EXPECT_NE(url, tab_strip_model->GetActiveWebContents()->GetURL());

    content::WebContentsDestroyedWatcher destroyed_watcher(
        tab_strip_model->GetWebContentsAt(1));
    EXPECT_TRUE(tab_strip_model->CloseWebContentsAt(1, 0));
    destroyed_watcher.Wait();
    EXPECT_EQ(1, tab_strip_model->count());
  }

  // Open a tab in the background, then bring it to the foreground. Return the
  // upper bound for how long the URL was open in the foreground.
  base::TimeDelta NavigateInBackgroundAndCloseInForegroundWithTiming(GURL url) {
    ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIVersionURL));
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, WindowOpenDisposition::NEW_BACKGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

    // Make sure the correct tab is in the foreground.
    TabStripModel* tab_strip_model = browser()->tab_strip_model();
    EXPECT_EQ(2, tab_strip_model->count());
    EXPECT_EQ(url, tab_strip_model->GetWebContentsAt(1)->GetURL());
    EXPECT_NE(url, tab_strip_model->GetActiveWebContents()->GetURL());

    // Close the foreground tab.
    base::TimeTicks start = base::TimeTicks::Now();
    content::WebContentsDestroyedWatcher destroyed_watcher(
        tab_strip_model->GetWebContentsAt(0));
    EXPECT_TRUE(tab_strip_model->CloseWebContentsAt(0, 0));
    destroyed_watcher.Wait();

    // Now the background tab should have moved to the foreground.
    EXPECT_EQ(1, tab_strip_model->count());
    EXPECT_EQ(url, tab_strip_model->GetActiveWebContents()->GetURL());

    content::WebContentsDestroyedWatcher second_watcher(
        tab_strip_model->GetActiveWebContents());
    tab_strip_model->CloseAllTabs();
    second_watcher.Wait();

    return (base::TimeTicks::Now() - start);
  }

  void FakeUserMetricsUpload() {
    metrics_provider_->ProvideCurrentSessionData(NULL);
  }

 protected:
  base::HistogramTester histogram_tester_;
  std::unique_ptr<net::EmbeddedTestServer> https_test_server_;
  std::unique_ptr<net::EmbeddedTestServer> http_test_server_;
  std::unique_ptr<HttpsEngagementMetricsProvider> metrics_provider_;

  DISALLOW_COPY_AND_ASSIGN(HttpsEngagementPageLoadMetricsBrowserTest);
};

IN_PROC_BROWSER_TEST_F(HttpsEngagementPageLoadMetricsBrowserTest,
                       Simple_Https) {
  StartHttpsServer(false);
  base::TimeDelta upper_bound = NavigateInForegroundAndCloseWithTiming(
      https_test_server_->GetURL("/simple.html"));

  // Test the page load metrics.
  histogram_tester_.ExpectTotalCount(internal::kHttpEngagementHistogram, 0);
  histogram_tester_.ExpectTotalCount(internal::kHttpsEngagementHistogram, 1);
  int32_t bucket_min =
      histogram_tester_.GetAllSamples(internal::kHttpsEngagementHistogram)[0]
          .min;
  EXPECT_GE(upper_bound.InMilliseconds(), bucket_min);
  EXPECT_LT(0, bucket_min);

  // Test the ratio metric.
  FakeUserMetricsUpload();
  histogram_tester_.ExpectTotalCount(
      internal::kHttpsEngagementSessionPercentage, 1);
  int32_t ratio_bucket =
      histogram_tester_
          .GetAllSamples(internal::kHttpsEngagementSessionPercentage)[0]
          .min;
  EXPECT_EQ(100, ratio_bucket);
}

IN_PROC_BROWSER_TEST_F(HttpsEngagementPageLoadMetricsBrowserTest, Simple_Http) {
  StartHttpServer();
  base::TimeDelta upper_bound = NavigateInForegroundAndCloseWithTiming(
      http_test_server_->GetURL("/simple.html"));

  // Test the page load metrics.
  histogram_tester_.ExpectTotalCount(internal::kHttpEngagementHistogram, 1);
  histogram_tester_.ExpectTotalCount(internal::kHttpsEngagementHistogram, 0);
  int32_t bucket_min =
      histogram_tester_.GetAllSamples(internal::kHttpEngagementHistogram)[0]
          .min;
  EXPECT_GE(upper_bound.InMilliseconds(), bucket_min);
  EXPECT_LT(0, bucket_min);

  // Test the ratio metric.
  FakeUserMetricsUpload();
  histogram_tester_.ExpectTotalCount(
      internal::kHttpsEngagementSessionPercentage, 1);
  int32_t ratio_bucket =
      histogram_tester_
          .GetAllSamples(internal::kHttpsEngagementSessionPercentage)[0]
          .min;
  EXPECT_EQ(0, ratio_bucket);
}

IN_PROC_BROWSER_TEST_F(HttpsEngagementPageLoadMetricsBrowserTest, OtherScheme) {
  NavigateInForegroundAndCloseWithTiming(GURL(chrome::kChromeUIVersionURL));
  histogram_tester_.ExpectTotalCount(internal::kHttpEngagementHistogram, 0);
  histogram_tester_.ExpectTotalCount(internal::kHttpsEngagementHistogram, 0);
  FakeUserMetricsUpload();
  histogram_tester_.ExpectTotalCount(
      internal::kHttpsEngagementSessionPercentage, 0);
}

IN_PROC_BROWSER_TEST_F(HttpsEngagementPageLoadMetricsBrowserTest,
                       SameOrigin_Https) {
  StartHttpsServer(false);
  NavigateTwiceInTabAndClose(https_test_server_->GetURL("/simple.html"),
                             https_test_server_->GetURL("/empty.html"));

  // Test the page load metrics.
  histogram_tester_.ExpectTotalCount(internal::kHttpEngagementHistogram, 0);
  histogram_tester_.ExpectTotalCount(internal::kHttpsEngagementHistogram, 2);

  // Test the ratio metric.
  FakeUserMetricsUpload();
  histogram_tester_.ExpectTotalCount(
      internal::kHttpsEngagementSessionPercentage, 1);
  int32_t ratio_bucket =
      histogram_tester_
          .GetAllSamples(internal::kHttpsEngagementSessionPercentage)[0]
          .min;
  EXPECT_EQ(100, ratio_bucket);
}

IN_PROC_BROWSER_TEST_F(HttpsEngagementPageLoadMetricsBrowserTest,
                       SameOrigin_Http) {
  StartHttpServer();
  NavigateTwiceInTabAndClose(http_test_server_->GetURL("/simple.html"),
                             http_test_server_->GetURL("/empty.html"));

  // Test the page load metrics.
  histogram_tester_.ExpectTotalCount(internal::kHttpEngagementHistogram, 2);
  histogram_tester_.ExpectTotalCount(internal::kHttpsEngagementHistogram, 0);

  // Test the ratio metric.
  FakeUserMetricsUpload();
  histogram_tester_.ExpectTotalCount(
      internal::kHttpsEngagementSessionPercentage, 1);
  int32_t ratio_bucket =
      histogram_tester_
          .GetAllSamples(internal::kHttpsEngagementSessionPercentage)[0]
          .min;
  EXPECT_EQ(0, ratio_bucket);
}

IN_PROC_BROWSER_TEST_F(HttpsEngagementPageLoadMetricsBrowserTest,
                       UncommittedLoadWithError) {
  StartHttpsServer(true);
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ui_test_utils::NavigateToURL(browser(),
                               https_test_server_->GetURL("/simple.html"));
  content::WebContentsDestroyedWatcher destroyed_watcher(
      tab_strip_model->GetActiveWebContents());
  EXPECT_TRUE(
      tab_strip_model->CloseWebContentsAt(tab_strip_model->active_index(), 0));
  destroyed_watcher.Wait();
  histogram_tester_.ExpectTotalCount(internal::kHttpEngagementHistogram, 0);
  histogram_tester_.ExpectTotalCount(internal::kHttpsEngagementHistogram, 0);
  FakeUserMetricsUpload();
  histogram_tester_.ExpectTotalCount(
      internal::kHttpsEngagementSessionPercentage, 0);
}

IN_PROC_BROWSER_TEST_F(HttpsEngagementPageLoadMetricsBrowserTest,
                       Navigate_Https) {
  StartHttpsServer(false);
  NavigateTwiceInTabAndClose(https_test_server_->GetURL("/simple.html"),
                             GURL(chrome::kChromeUIVersionURL));

  // Test the page load metrics.
  histogram_tester_.ExpectTotalCount(internal::kHttpEngagementHistogram, 0);
  histogram_tester_.ExpectTotalCount(internal::kHttpsEngagementHistogram, 1);

  // Test the ratio metric.
  FakeUserMetricsUpload();
  histogram_tester_.ExpectTotalCount(
      internal::kHttpsEngagementSessionPercentage, 1);
  int32_t ratio_bucket =
      histogram_tester_
          .GetAllSamples(internal::kHttpsEngagementSessionPercentage)[0]
          .min;
  EXPECT_EQ(100, ratio_bucket);
}

IN_PROC_BROWSER_TEST_F(HttpsEngagementPageLoadMetricsBrowserTest,
                       Navigate_Http) {
  StartHttpServer();

  // Test the page load metrics.
  NavigateTwiceInTabAndClose(http_test_server_->GetURL("/simple.html"),
                             GURL(chrome::kChromeUIVersionURL));
  histogram_tester_.ExpectTotalCount(internal::kHttpEngagementHistogram, 1);
  histogram_tester_.ExpectTotalCount(internal::kHttpsEngagementHistogram, 0);

  // Test the ratio metric.
  FakeUserMetricsUpload();
  histogram_tester_.ExpectTotalCount(
      internal::kHttpsEngagementSessionPercentage, 1);
  int32_t ratio_bucket =
      histogram_tester_
          .GetAllSamples(internal::kHttpsEngagementSessionPercentage)[0]
          .min;
  EXPECT_EQ(0, ratio_bucket);
}

IN_PROC_BROWSER_TEST_F(HttpsEngagementPageLoadMetricsBrowserTest,
                       Navigate_Both) {
  StartHttpServer();
  StartHttpsServer(false);

  NavigateTwiceInTabAndClose(http_test_server_->GetURL("/simple.html"),
                             https_test_server_->GetURL("/simple.html"));
  histogram_tester_.ExpectTotalCount(internal::kHttpEngagementHistogram, 1);
  histogram_tester_.ExpectTotalCount(internal::kHttpsEngagementHistogram, 1);
}

IN_PROC_BROWSER_TEST_F(HttpsEngagementPageLoadMetricsBrowserTest,
                       Navigate_Both_NonHtmlMainResource) {
  StartHttpServer();
  StartHttpsServer(false);
  NavigateTwiceInTabAndClose(http_test_server_->GetURL("/circle.svg"),
                             https_test_server_->GetURL("/circle.svg"));

  // Test the page load metrics.
  histogram_tester_.ExpectTotalCount(internal::kHttpEngagementHistogram, 0);
  histogram_tester_.ExpectTotalCount(internal::kHttpsEngagementHistogram, 0);

  // Test the ratio metric.
  FakeUserMetricsUpload();
  histogram_tester_.ExpectTotalCount(
      internal::kHttpsEngagementSessionPercentage, 0);
}

IN_PROC_BROWSER_TEST_F(HttpsEngagementPageLoadMetricsBrowserTest,
                       ClosedWhileHidden_Https) {
  StartHttpsServer(false);
  base::TimeDelta upper_bound =
      NavigateInForegroundAndCloseInBackgroundWithTiming(
          https_test_server_->GetURL("/simple.html"));

  // Test the page load metrics.
  histogram_tester_.ExpectTotalCount(internal::kHttpEngagementHistogram, 0);
  histogram_tester_.ExpectTotalCount(internal::kHttpsEngagementHistogram, 1);
  int32_t bucket_min =
      histogram_tester_.GetAllSamples(internal::kHttpsEngagementHistogram)[0]
          .min;
  EXPECT_GE(upper_bound.InMilliseconds(), bucket_min);
  EXPECT_LT(0, bucket_min);

  // Test the ratio metric.
  FakeUserMetricsUpload();
  histogram_tester_.ExpectTotalCount(
      internal::kHttpsEngagementSessionPercentage, 1);
  int32_t ratio_bucket =
      histogram_tester_
          .GetAllSamples(internal::kHttpsEngagementSessionPercentage)[0]
          .min;
  EXPECT_EQ(100, ratio_bucket);
}

IN_PROC_BROWSER_TEST_F(HttpsEngagementPageLoadMetricsBrowserTest,
                       ClosedWhileHidden_Http) {
  StartHttpServer();
  base::TimeDelta upper_bound =
      NavigateInForegroundAndCloseInBackgroundWithTiming(
          http_test_server_->GetURL("/simple.html"));

  // Test the page load metrics.
  histogram_tester_.ExpectTotalCount(internal::kHttpEngagementHistogram, 1);
  histogram_tester_.ExpectTotalCount(internal::kHttpsEngagementHistogram, 0);
  int32_t bucket_min =
      histogram_tester_.GetAllSamples(internal::kHttpEngagementHistogram)[0]
          .min;
  EXPECT_GE(upper_bound.InMilliseconds(), bucket_min);
  EXPECT_LT(0, bucket_min);

  // Test the ratio metric.
  FakeUserMetricsUpload();
  histogram_tester_.ExpectTotalCount(
      internal::kHttpsEngagementSessionPercentage, 1);
  int32_t ratio_bucket =
      histogram_tester_
          .GetAllSamples(internal::kHttpsEngagementSessionPercentage)[0]
          .min;
  EXPECT_EQ(0, ratio_bucket);
}

IN_PROC_BROWSER_TEST_F(HttpsEngagementPageLoadMetricsBrowserTest,
                       BackgroundThenForeground_Https) {
  StartHttpsServer(false);
  base::TimeDelta upper_bound =
      NavigateInBackgroundAndCloseInForegroundWithTiming(
          https_test_server_->GetURL("/simple.html"));

  // Test the page load metrics.
  histogram_tester_.ExpectTotalCount(internal::kHttpEngagementHistogram, 0);
  histogram_tester_.ExpectTotalCount(internal::kHttpsEngagementHistogram, 1);
  int32_t bucket_min =
      histogram_tester_.GetAllSamples(internal::kHttpsEngagementHistogram)[0]
          .min;
  EXPECT_GE(upper_bound.InMilliseconds(), bucket_min);
  EXPECT_LT(0, bucket_min);

  // Test the ratio metric.
  FakeUserMetricsUpload();
  histogram_tester_.ExpectTotalCount(
      internal::kHttpsEngagementSessionPercentage, 1);
  int32_t ratio_bucket =
      histogram_tester_
          .GetAllSamples(internal::kHttpsEngagementSessionPercentage)[0]
          .min;
  EXPECT_EQ(100, ratio_bucket);
}

IN_PROC_BROWSER_TEST_F(HttpsEngagementPageLoadMetricsBrowserTest,
                       BackgroundThenForeground_Http) {
  StartHttpServer();
  base::TimeDelta upper_bound =
      NavigateInBackgroundAndCloseInForegroundWithTiming(
          http_test_server_->GetURL("/simple.html"));

  // Test the page load metrics.
  histogram_tester_.ExpectTotalCount(internal::kHttpEngagementHistogram, 1);
  histogram_tester_.ExpectTotalCount(internal::kHttpsEngagementHistogram, 0);
  int32_t bucket_min =
      histogram_tester_.GetAllSamples(internal::kHttpEngagementHistogram)[0]
          .min;
  EXPECT_GE(upper_bound.InMilliseconds(), bucket_min);
  EXPECT_LT(0, bucket_min);

  // Test the ratio metric.
  FakeUserMetricsUpload();
  histogram_tester_.ExpectTotalCount(
      internal::kHttpsEngagementSessionPercentage, 1);
  int32_t ratio_bucket =
      histogram_tester_
          .GetAllSamples(internal::kHttpsEngagementSessionPercentage)[0]
          .min;
  EXPECT_EQ(0, ratio_bucket);
}

IN_PROC_BROWSER_TEST_F(HttpsEngagementPageLoadMetricsBrowserTest,
                       AlwaysInBackground) {
  StartHttpsServer(false);
  StartHttpServer();
  NavigateInBackgroundAndClose(https_test_server_->GetURL("/simple.html"));
  NavigateInBackgroundAndClose(http_test_server_->GetURL("/simple.html"));

  // Test the page load metrics.
  histogram_tester_.ExpectTotalCount(internal::kHttpEngagementHistogram, 0);
  histogram_tester_.ExpectTotalCount(internal::kHttpsEngagementHistogram, 0);

  // Test the ratio metric.
  FakeUserMetricsUpload();
  histogram_tester_.ExpectTotalCount(
      internal::kHttpsEngagementSessionPercentage, 0);
}

IN_PROC_BROWSER_TEST_F(HttpsEngagementPageLoadMetricsBrowserTest,
                       MultipleUploads) {
  StartHttpsServer(false);

  NavigateInForegroundAndCloseWithTiming(
      https_test_server_->GetURL("/simple.html"));
  histogram_tester_.ExpectTotalCount(
      internal::kHttpsEngagementSessionPercentage, 0);
  FakeUserMetricsUpload();
  FakeUserMetricsUpload();
  FakeUserMetricsUpload();
  histogram_tester_.ExpectTotalCount(
      internal::kHttpsEngagementSessionPercentage, 1);
}
