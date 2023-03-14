// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_logging_settings.h"
#include "components/page_load_metrics/browser/observers/use_counter_page_load_metrics_observer.h"

#include "build/build_config.h"
#include "chrome/browser/page_load_metrics/integration_tests/metric_integration_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "content/public/common/content_features.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace {

using WebFeature = blink::mojom::WebFeature;

class UseCounterPageLoadMetricsObserverBrowserTest
    : public MetricIntegrationTest {
 public:
  ~UseCounterPageLoadMetricsObserverBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    feature_list_.InitWithFeaturesAndParameters(
        content::GetDefaultEnabledBackForwardCacheFeaturesForTesting(),
        content::GetDefaultDisabledBackForwardCacheFeaturesForTesting());
    MetricIntegrationTest::SetUpCommandLine(command_line);
    vmodule_switches_.InitWithSwitches("back_forward_cache_impl=1");
  }

 protected:
  content::RenderFrameHost* top_frame_host() {
    return web_contents()->GetPrimaryMainFrame();
  }

  std::unique_ptr<page_load_metrics::PageLoadMetricsTestWaiter>
  CreatePageLoadMetricsTestWaiter() {
    return std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
        web_contents());
  }

  base::test::ScopedFeatureList feature_list_;
  logging::ScopedVmoduleSwitches vmodule_switches_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(UseCounterPageLoadMetricsObserverBrowserTest,
                       RecordFeatures) {
  std::vector<WebFeature> features_0(
      {WebFeature::kFetch, WebFeature::kFetchBodyStream});
  std::vector<WebFeature> features_1({WebFeature::kWindowFind});

  Start();
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Navigate to A.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_a));
  content::RenderFrameHostWrapper rfh_a(top_frame_host());
  page_load_metrics::MetricsWebContentsObserver::RecordFeatureUsage(rfh_a.get(),
                                                                    features_0);

  // Navigate to B.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));
  ASSERT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // Go back to A.
  ASSERT_TRUE(content::HistoryGoBack(web_contents()));
  EXPECT_TRUE(rfh_a->IsInPrimaryMainFrame());

  EXPECT_NE(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);
  page_load_metrics::MetricsWebContentsObserver::RecordFeatureUsage(rfh_a.get(),
                                                                    features_1);

  // The RenderFrameHost for the page B was likely in the back-forward cache
  // just after the history navigation, but now this might be evicted due to
  // outstanding-network request.

  // Navigate to B again.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));
  ASSERT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // Go back to A again.
  ASSERT_TRUE(content::HistoryGoBack(web_contents()));
  EXPECT_TRUE(rfh_a->IsInPrimaryMainFrame());
  EXPECT_NE(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  for (auto feature : features_0) {
    histogram_tester().ExpectBucketCount(
        "Blink.UseCounter.Features",
        static_cast<base::Histogram::Sample>(feature), 1);
    histogram_tester().ExpectBucketCount(
        "Blink.UseCounter.MainFrame.Features",
        static_cast<base::Histogram::Sample>(feature), 1);
  }
  for (auto feature : features_1) {
    histogram_tester().ExpectBucketCount(
        "Blink.UseCounter.Features",
        static_cast<base::Histogram::Sample>(feature), 1);
    histogram_tester().ExpectBucketCount(
        "Blink.UseCounter.MainFrame.Features",
        static_cast<base::Histogram::Sample>(feature), 1);
  }
}
