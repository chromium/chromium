// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/integration_tests/metric_integration_test.h"

#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"

using ukm::builders::PageLoad;

IN_PROC_BROWSER_TEST_F(MetricIntegrationTest, UserTiming) {
  LoadHTML(R"HTML(
    <p>Sample website</p>
    <script>
    function runtest() {
      let results = [];
      for (let name of ['fully_loaded', 'fully_visible', 'interactive']) {
        const m = performance.mark('mark_' + name);
        results.push(m.startTime);
      }
      return results;
    }
    </script>
  )HTML");
  // Ensure that the previous page won't be stored in the back/forward cache, so
  // that the histogram will be recorded when the previous page is unloaded.
  web_contents()->GetController().GetBackForwardCache().DisableForTesting(
      content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  // Check web perf API.
  const base::Value eval_result =
      EvalJs(web_contents(), "runtest()").ExtractList();
  const double fully_loaded = eval_result.GetList()[0].GetDouble();
  EXPECT_GT(fully_loaded, 0.0);
  const double fully_visible = eval_result.GetList()[1].GetDouble();
  EXPECT_GT(fully_visible, 0.0);
  const double interactive = eval_result.GetList()[2].GetDouble();
  EXPECT_GT(interactive, 0.0);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // Check UKM. We compare the webexposed value to the UKM value. The webexposed
  // value will be rounded whereas the UKM value will not, so it may be off by 1
  // given that the comparison is at 1ms granularity. Since expected_time is a
  // double and the UKM values are int, we need leeway slightly more than 1. For
  // instance expected_value could be 2.0004 and the reported value could be 1.
  // So we use an epsilon of 1.3 to capture these cases.
  ExpectUKMPageLoadMetricNear(
      PageLoad::kPageTiming_UserTimingMarkFullyLoadedName, fully_loaded, 1.3);
  ExpectUKMPageLoadMetricNear(
      PageLoad::kPageTiming_UserTimingMarkFullyVisibleName, fully_visible, 1.3);
  ExpectUKMPageLoadMetricNear(
      PageLoad::kPageTiming_UserTimingMarkInteractiveName, interactive, 1.3);
}
