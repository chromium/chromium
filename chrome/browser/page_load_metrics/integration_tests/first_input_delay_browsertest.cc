// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/integration_tests/metric_integration_test.h"

#include "base/test/trace_event_analyzer.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "services/metrics/public/cpp/ukm_builders.h"

using ukm::builders::PageLoad;

IN_PROC_BROWSER_TEST_F(MetricIntegrationTest, FirstInputDelay) {
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

  StartTracing({"loading"});

  content::SimulateMouseClick(web_contents(), 0,
                              blink::WebMouseEvent::Button::kLeft);

  // Check web perf API.
  double expected_fid = EvalJs(web_contents(), "runtest()").ExtractDouble();
  EXPECT_GT(expected_fid, 0.0);

  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));

  // Check UKM. We compare the webexposed value to the UKM value. The webexposed
  // value will be rounded whereas the UKM value will not, so it may be off by 1
  // given that the comparison is at 1ms granularity.
  ExpectUKMPageLoadMetricNear(PageLoad::kInteractiveTiming_FirstInputDelay4Name,
                              expected_fid, 1);

  // Check UMA, which similarly may be off by one.
  ExpectUniqueUMAPageLoadMetricNear(
      "PageLoad.InteractiveTiming.FirstInputDelay4", expected_fid);
}
