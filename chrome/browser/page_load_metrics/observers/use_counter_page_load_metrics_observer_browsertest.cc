// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/use_counter_page_load_metrics_observer.h"

#include "chrome/browser/page_load_metrics/integration_tests/metric_integration_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"

namespace {

using WebFeature = blink::mojom::WebFeature;

class UseCounterPageLoadMetricsObserverBrowserTest
    : public MetricIntegrationTest {
 public:
  ~UseCounterPageLoadMetricsObserverBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kBackForwardCache,
          {{"TimeToLiveInBackForwardCacheInSeconds", "3600"}}}},
        // Allow BackForwardCache for all devices regardless of their memory.
        {features::kBackForwardCacheMemoryControls});

    MetricIntegrationTest::SetUpCommandLine(command_line);
  }

 protected:
  content::RenderFrameHost* top_frame_host() {
    return web_contents()->GetMainFrame();
  }

  std::unique_ptr<page_load_metrics::PageLoadMetricsTestWaiter>
  CreatePageLoadMetricsTestWaiter() {
    return std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
        web_contents());
  }

  base::test::ScopedFeatureList feature_list_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(UseCounterPageLoadMetricsObserverBrowserTest,
                       RecordFeatures) {
  std::vector<WebFeature> features_0(
      {WebFeature::kFetch, WebFeature::kFetchBodyStream});
  std::vector<WebFeature> features_1({WebFeature::kWindowFind});
  page_load_metrics::mojom::PageLoadFeatures page_load_features_0;
  page_load_metrics::mojom::PageLoadFeatures page_load_features_1;
  page_load_features_0.features = features_0;
  page_load_features_1.features = features_1;

  Start();
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Navigate to A.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url_a));
  content::RenderFrameHost* rfh_a = top_frame_host();
  page_load_metrics::MetricsWebContentsObserver::RecordFeatureUsage(
      rfh_a, page_load_features_0);

  // Navigate to B.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  EXPECT_EQ(rfh_a, top_frame_host());
  EXPECT_FALSE(rfh_a->IsInBackForwardCache());
  page_load_metrics::MetricsWebContentsObserver::RecordFeatureUsage(
      rfh_a, page_load_features_1);

  // The RenderFrameHost for the page B was likely in the back-forward cache
  // just after the history navigation, but now this might be evicted due to
  // outstanding-network request.

  // Navigate to B again.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // Go back to A again.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  EXPECT_EQ(rfh_a, top_frame_host());
  EXPECT_FALSE(rfh_a->IsInBackForwardCache());

  for (auto feature : page_load_features_0.features) {
    histogram_tester().ExpectBucketCount(
        internal::kFeaturesHistogramName,
        static_cast<base::Histogram::Sample>(feature), 1);
    histogram_tester().ExpectBucketCount(
        internal::kFeaturesHistogramMainFrameName,
        static_cast<base::Histogram::Sample>(feature), 1);
  }
  for (auto feature : page_load_features_1.features) {
    histogram_tester().ExpectBucketCount(
        internal::kFeaturesHistogramName,
        static_cast<base::Histogram::Sample>(feature), 1);
    histogram_tester().ExpectBucketCount(
        internal::kFeaturesHistogramMainFrameName,
        static_cast<base::Histogram::Sample>(feature), 1);
  }
}
