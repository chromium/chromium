// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ssl/cert_verifier_browser_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/signed_exchange_browser_test_helper.h"
#include "net/dns/mock_host_resolver.h"
#include "services/metrics/public/cpp/ukm_builders.h"

class SignedExchangePageLoadMetricsBrowserTest
    : public CertVerifierBrowserTest {
 public:
  SignedExchangePageLoadMetricsBrowserTest() {
    feature_list_.InitWithFeatures(
        {ukm::kUkmFeature, features::kSignedHTTPExchange}, {});
  }
  ~SignedExchangePageLoadMetricsBrowserTest() override {}

 protected:
  void PreRunTestOnMainThread() override {
    InProcessBrowserTest::PreRunTestOnMainThread();
    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();

    sxg_test_helper_.InstallMockCert(mock_cert_verifier());
    sxg_test_helper_.InstallMockCertChainInterceptor();
  }

  // Force navigation to a new page, so the currently tracked page load runs its
  // OnComplete callback. You should prefer to use PageLoadMetricsTestWaiter,
  // and only use NavigateToUntrackedUrl for cases where the waiter isn't
  // sufficient.
  void NavigateToUntrackedUrl() {
    ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  }

  void InstallUrlInterceptor(const GURL& url, const std::string& data_path) {
    sxg_test_helper_.InstallUrlInterceptor(url, data_path);
  }

  std::unique_ptr<page_load_metrics::PageLoadMetricsTestWaiter>
  CreatePageLoadMetricsTestWaiter() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
        web_contents);
  }

  const ukm::TestAutoSetUkmRecorder& test_ukm_recorder() const {
    return *test_ukm_recorder_;
  }

 private:
  void SetUp() override {
    // Somehow tests actually run inside InProcessBrowserTest::SetUp(),
    // so setup helper before InProcessBrowserTest::SetUp().
    sxg_test_helper_.SetUp();

    InProcessBrowserTest::SetUp();
  }

  void TearDownOnMainThread() override {
    sxg_test_helper_.TearDownOnMainThread();
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
  content::SignedExchangeBrowserTestHelper sxg_test_helper_;

  DISALLOW_COPY_AND_ASSIGN(SignedExchangePageLoadMetricsBrowserTest);
};

IN_PROC_BROWSER_TEST_F(SignedExchangePageLoadMetricsBrowserTest,
                       UkmSignedExchangeMetric) {
  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL inner_url("https://test.example.org/test/");
  const GURL url =
      embedded_test_server()->GetURL("/sxg/test.example.org_test.sxg");
  ui_test_utils::NavigateToURL(browser(), url);

  // Force navigation to another page, which should force logging of histograms
  // persisted at the end of the page load lifetime.
  NavigateToUntrackedUrl();

  using PageLoad = ukm::builders::PageLoad;
  const auto entries =
      test_ukm_recorder().GetMergedEntriesByName(PageLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const auto& kv : entries) {
    auto* const entry = kv.second.get();
    test_ukm_recorder().ExpectEntrySourceHasUrl(entry, inner_url);

    EXPECT_TRUE(test_ukm_recorder().EntryHasMetric(
        entry, PageLoad::kIsSignedExchangeInnerResponseName));
  }
}
