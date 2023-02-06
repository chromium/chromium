// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/cache_transparency_page_load_metrics_observer.h"

#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/network/public/cpp/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class CacheTransparencyPageLoadMetricsObserverBrowserTest
    : public InProcessBrowserTest {
 public:
  CacheTransparencyPageLoadMetricsObserverBrowserTest() = default;

  CacheTransparencyPageLoadMetricsObserverBrowserTest(
      const CacheTransparencyPageLoadMetricsObserverBrowserTest&) = delete;
  CacheTransparencyPageLoadMetricsObserverBrowserTest& operator=(
      const CacheTransparencyPageLoadMetricsObserverBrowserTest&) = delete;

  ~CacheTransparencyPageLoadMetricsObserverBrowserTest() override = default;

  void PreRunTestOnMainThread() override {
    InProcessBrowserTest::PreRunTestOnMainThread();
    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  void SetUp() override {
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());

    GURL pervasive_payload_url =
        embedded_test_server()->GetURL(kPervasivePayload);
    std::string pervasive_payloads_params = base::StrCat(
        {"1,", pervasive_payload_url.spec(),
         ",2478392C652868C0AAF0316A28284610DBDACF02D66A00B39F3BA75D887F4829"});

    feature_list_.InitWithFeaturesAndParameters(
        {{features::kNetworkServiceInProcess, {}},
         {network::features::kPervasivePayloadsList,
          {{"pervasive-payloads", pervasive_payloads_params}}},
         {network::features::kCacheTransparency, {}},
         {net::features::kSplitCacheByNetworkIsolationKey, {}}},
        {/* disabled_features */});

    InProcessBrowserTest::SetUp();
  }

 protected:
  void NavigateTo(const GURL& url) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  }

  void CloseAllTabs() {
    TabStripModel* tab_strip_model = browser()->tab_strip_model();
    content::WebContentsDestroyedWatcher destroyed_watcher(
        tab_strip_model->GetActiveWebContents());
    tab_strip_model->CloseAllTabs();
    destroyed_watcher.Wait();
  }

  const ukm::TestAutoSetUkmRecorder* test_ukm_recorder() const {
    return test_ukm_recorder_.get();
  }

 private:
  static constexpr char kPervasivePayload[] =
      "/cache_transparency/pervasive.js";

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
};

IN_PROC_BROWSER_TEST_F(CacheTransparencyPageLoadMetricsObserverBrowserTest,
                       PervasivePayloadRequested) {
  // Activate and wait for load event and FCP.
  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      browser()->tab_strip_model()->GetActiveWebContents());
  waiter->AddPageExpectation(
      page_load_metrics::PageLoadMetricsTestWaiter::TimingField::kLoadEvent);
  waiter->AddPageExpectation(page_load_metrics::PageLoadMetricsTestWaiter::
                                 TimingField::kFirstContentfulPaint);

  NavigateTo(
      embedded_test_server()->GetURL("/cache_transparency/pervasive2.html"));
  waiter->Wait();

  CloseAllTabs();

  // Check UKM recording
  auto entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PageLoad_CacheTransparencyEnabled::kEntryName);
  ASSERT_EQ(1u, entries.size());

  auto entries2 = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::Network_CacheTransparency2::kEntryName);
  ASSERT_EQ(1u, entries2.size());
  test_ukm_recorder()->ExpectEntryMetric(
      entries2[0],
      ukm::builders::Network_CacheTransparency2::kFoundPervasivePayloadName,
      true);
}

IN_PROC_BROWSER_TEST_F(CacheTransparencyPageLoadMetricsObserverBrowserTest,
                       PervasivePayloadNotRequested) {
  // Activate and wait for load event and FCP.
  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      browser()->tab_strip_model()->GetActiveWebContents());
  waiter->AddPageExpectation(
      page_load_metrics::PageLoadMetricsTestWaiter::TimingField::kLoadEvent);
  waiter->AddPageExpectation(page_load_metrics::PageLoadMetricsTestWaiter::
                                 TimingField::kFirstContentfulPaint);

  NavigateTo(
      embedded_test_server()->GetURL("/cache_transparency/cacheable2.html"));
  waiter->Wait();

  CloseAllTabs();

  // Check UKM recording
  auto entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PageLoad_CacheTransparencyEnabled::kEntryName);
  ASSERT_EQ(1u, entries.size());

  auto entries2 = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::Network_CacheTransparency2::kEntryName);
  ASSERT_EQ(1u, entries2.size());
  test_ukm_recorder()->ExpectEntryMetric(
      entries2[0],
      ukm::builders::Network_CacheTransparency2::kFoundPervasivePayloadName,
      false);
}
