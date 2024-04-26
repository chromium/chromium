// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/trace_event_analyzer.h"
#include "build/build_config.h"
#include "chrome/browser/page_load_metrics/integration_tests/metric_integration_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "services/metrics/public/cpp/ukm_builders.h"

using ukm::builders::PageLoad;

IN_PROC_BROWSER_TEST_F(MetricIntegrationTest, FirstInputDelay) {
  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      web_contents());
  waiter->AddPageExpectation(page_load_metrics::PageLoadMetricsTestWaiter::
                                 TimingField::kFirstInputDelay);
  LoadHTML(R"HTML(
    <p>Sample website</p>
    <script>
    runtest = async () => {
      const observePromise = new Promise(resolve => {
        new PerformanceObserver(e => {
          e.getEntries().forEach(entry => {
            const fid = entry.processingStart - entry.startTime;
            resolve(fid);
          })
        }).observe({type: 'first-input', buffered: true});
      });
      return await observePromise;
    };
    </script>
  )HTML");

  // Ensure that the previous page won't be stored in the back/forward cache, so
  // that the histogram will be recorded when the previous page is unloaded.
  // TODO(crbug.com/40189815): Investigate if this needs further fix.
  web_contents()->GetController().GetBackForwardCache().DisableForTesting(
      content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  StartTracing({"loading"});

  // We should wait for the main frame's hit-test data to be ready before
  // sending the click event below to avoid flakiness.
  content::WaitForHitTestData(web_contents()->GetPrimaryMainFrame());
  // Ensure the compositor thread is ready for mouse events.
  content::MainThreadFrameObserver frame_observer(GetRenderWidgetHost());
  frame_observer.Wait();

  content::SimulateMouseClick(web_contents(), 0,
                              blink::WebMouseEvent::Button::kLeft);

  // Check web perf API.
  double expected_fid = EvalJs(web_contents(), "runtest()").ExtractDouble();
  EXPECT_GT(expected_fid, 0.0);

  waiter->Wait();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // Check UKM. We compare the webexposed value to the UKM value. The webexposed
  // value will be rounded whereas the UKM value will not, so it may be off by 1
  // given that the comparison is at 1ms granularity. Since expected_fid is a
  // double and the UKM values are int, we need leeway slightly more than 1. For
  // instance expected_value could be 2.0004 and the reported value could be 1.
  // So we use an epsilon of 1.2 to capture these cases.
  ExpectUKMPageLoadMetricNear(PageLoad::kInteractiveTiming_FirstInputDelay4Name,
                              expected_fid, 1.2);

  // Check UMA, which similarly may be off by one.
  ExpectUniqueUMAPageLoadMetricNear(
      "PageLoad.InteractiveTiming.FirstInputDelay4", expected_fid);
}
