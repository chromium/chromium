// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/foreground_duration_ukm_observer.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

using UkmEntry = ukm::builders::AmpPageLoad;

class AmpPageLoadMetricsBrowserTest : public InProcessBrowserTest,
                                      public testing::WithParamInterface<bool> {
 public:
  AmpPageLoadMetricsBrowserTest()
      : prerender_helper_(
            base::BindRepeating(&AmpPageLoadMetricsBrowserTest::GetWebContents,
                                base::Unretained(this))) {}

  AmpPageLoadMetricsBrowserTest(const AmpPageLoadMetricsBrowserTest&) = delete;
  AmpPageLoadMetricsBrowserTest& operator=(
      const AmpPageLoadMetricsBrowserTest&) = delete;

  ~AmpPageLoadMetricsBrowserTest() override {}

  void SetUp() override {
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
    InProcessBrowserTest::SetUp();
  }

  void PreRunTestOnMainThread() override {
    InProcessBrowserTest::PreRunTestOnMainThread();
    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  void SetUpOnMainThread() override {
    StartHttpsServer(net::EmbeddedTestServer::CERT_OK);
  }

 protected:
  void StartHttpsServer(net::EmbeddedTestServer::ServerCertificate cert) {
    https_test_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_test_server_->SetSSLConfig(cert);
    https_test_server_->ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(https_test_server_->Start());
  }

  void NavigateToURL(const GURL& url) {
    if (WithPrerendering()) {
      prerender_helper_.AddPrerender(url);
      prerender_helper_.NavigatePrimaryPage(url);
    } else {
      ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    }
  }

  void ExpectMetricValueForUrl(const GURL& url,
                               const char* metric_name,
                               const int expected_value) {
    for (const ukm::mojom::UkmEntry* entry :
         test_ukm_recorder_->GetEntriesByName(UkmEntry::kEntryName)) {
      auto* source = test_ukm_recorder_->GetSourceForSourceId(entry->source_id);
      if (source && source->url() == url) {
        test_ukm_recorder_->EntryHasMetric(entry, metric_name);
        test_ukm_recorder_->ExpectEntryMetric(entry, metric_name,
                                              expected_value);
      }
    }
  }
  void ExpectMetricCountForUrl(const GURL& url,
                               const char* metric_name,
                               const int expected_count) {
    int count = 0;
    for (const ukm::mojom::UkmEntry* entry :
         test_ukm_recorder_->GetEntriesByName(UkmEntry::kEntryName)) {
      auto* source = test_ukm_recorder_->GetSourceForSourceId(entry->source_id);
      if (source && source->url() == url &&
          test_ukm_recorder_->EntryHasMetric(entry, metric_name)) {
        count++;
      }
    }
    EXPECT_EQ(count, expected_count);
  }

  void CloseAllTabs() {
    TabStripModel* tab_strip_model = browser()->tab_strip_model();
    content::WebContentsDestroyedWatcher destroyed_watcher(
        tab_strip_model->GetActiveWebContents());
    tab_strip_model->CloseAllTabs();
    destroyed_watcher.Wait();
  }

  net::EmbeddedTestServer* https_test_server() {
    return https_test_server_.get();
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  bool WithPrerendering() { return GetParam(); }

 private:
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
  std::unique_ptr<net::EmbeddedTestServer> https_test_server_;
  content::test::PrerenderTestHelper prerender_helper_;
};

INSTANTIATE_TEST_SUITE_P(All, AmpPageLoadMetricsBrowserTest, testing::Bool());

IN_PROC_BROWSER_TEST_P(AmpPageLoadMetricsBrowserTest, NoAmp) {
  // Navigate to an empty page to inject SpeculationRules if prerendered case.
  GURL empty_url = https_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), empty_url));

  page_load_metrics::PageLoadMetricsTestWaiter waiter(GetWebContents());
  waiter.AddPageExpectation(
      page_load_metrics::PageLoadMetricsTestWaiter::TimingField::kLoadEvent);
  GURL url = https_test_server()->GetURL("/english_page.html");
  NavigateToURL(url);
  waiter.Wait();
  CloseAllTabs();
  ExpectMetricCountForUrl(url, "MainFrameAmpPageLoad", 0);
  ExpectMetricCountForUrl(url, "SubFrameAmpPageLoad", 0);
}

IN_PROC_BROWSER_TEST_P(AmpPageLoadMetricsBrowserTest, AmpMainFrame) {
  // Navigate to an empty page to inject SpeculationRules if prerendered case.
  GURL empty_url = https_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), empty_url));

  page_load_metrics::PageLoadMetricsTestWaiter waiter(GetWebContents());
  waiter.AddPageExpectation(page_load_metrics::PageLoadMetricsTestWaiter::
                                TimingField::kFirstContentfulPaint);
  GURL url = https_test_server()->GetURL("/page_load_metrics/amp_basic.html");
  NavigateToURL(url);
  waiter.Wait();
  CloseAllTabs();
  ExpectMetricValueForUrl(url, "MainFrameAmpPageLoad", 1);
  ExpectMetricCountForUrl(url, "SubFrameAmpPageLoad", 0);
}

IN_PROC_BROWSER_TEST_P(AmpPageLoadMetricsBrowserTest, AmpSubframe) {
  // Navigate to an empty page to inject SpeculationRules if prerendered case.
  GURL empty_url = https_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), empty_url));

  page_load_metrics::PageLoadMetricsTestWaiter waiter(GetWebContents());
  waiter.AddPageExpectation(page_load_metrics::PageLoadMetricsTestWaiter::
                                TimingField::kFirstContentfulPaint);
  GURL url =
      https_test_server()->GetURL("/page_load_metrics/amp_reader_mock.html");
  NavigateToURL(url);
  waiter.Wait();
  CloseAllTabs();
  ExpectMetricCountForUrl(url, "MainFrameAmpPageLoad", 0);
  ExpectMetricValueForUrl(url, "SubFrameAmpPageLoad", 1);
}

class AmpPageLoadMetricsFencedFrameBrowserTest
    : public AmpPageLoadMetricsBrowserTest {
 public:
  AmpPageLoadMetricsFencedFrameBrowserTest() = default;
  ~AmpPageLoadMetricsFencedFrameBrowserTest() override = default;
  AmpPageLoadMetricsFencedFrameBrowserTest(
      const AmpPageLoadMetricsFencedFrameBrowserTest&) = delete;

  AmpPageLoadMetricsFencedFrameBrowserTest& operator=(
      const AmpPageLoadMetricsFencedFrameBrowserTest&) = delete;

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_helper_;
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

// Currently, prerendering doesn't support FencedFrames.
// TODO(crbug.com/40228553): Add a test with prerendering.
IN_PROC_BROWSER_TEST_F(AmpPageLoadMetricsFencedFrameBrowserTest,
                       AmpFencedFrame) {
  GURL url = https_test_server()->GetURL("/english_page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  page_load_metrics::PageLoadMetricsTestWaiter waiter(GetWebContents());
  waiter.AddPageExpectation(page_load_metrics::PageLoadMetricsTestWaiter::
                                TimingField::kFirstContentfulPaint);
  const GURL kFencedFrameUrl =
      https_test_server()->GetURL("/page_load_metrics/amp_basic.html");
  content::RenderFrameHost* fenced_frame_host =
      fenced_frame_test_helper().CreateFencedFrame(
          GetWebContents()->GetPrimaryMainFrame(), kFencedFrameUrl);
  EXPECT_NE(nullptr, fenced_frame_host);

  waiter.Wait();
  CloseAllTabs();
  ExpectMetricCountForUrl(url, "MainFrameAmpPageLoad", 0);
  ExpectMetricValueForUrl(url, "SubFrameAmpPageLoad", 1);
}
