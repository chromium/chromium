// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/integration_tests/metric_integration_test.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

IN_PROC_BROWSER_TEST_F(MetricIntegrationTest,
                       RenderBlockingResourceAndPreloadedFont) {
  Start();
  Load("/render_blocking_resource_and_preloaded_font.html");

  // Finish session.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // Actually fetch the metrics.
  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  ExpectUniqueUMA("Renderer.CriticalFonts.BlockingResourcesLoadTime");
  ExpectUniqueUMA("Renderer.CriticalFonts.PreloadedFontsLoadTime");
  ExpectUniqueUMA("Renderer.CriticalFonts.CriticalFontDelay");
}

IN_PROC_BROWSER_TEST_F(MetricIntegrationTest,
                       RenderBlockingResourceAndSlowPreloadedFont) {
  Start();
  Load("/render_blocking_resource_and_slow_preloaded_font.html");

  const std::string wait_for_resources = R"(
    (async () => {
      await new Promise(
        resolve => {
          (new PerformanceObserver(list => {
            const entries = list.getEntries();
            for (let entry of entries) {
              if (entry.name.includes(".woff")) {
                requestAnimationFrame(()=>{resolve()});
              }
            }
          })).observe(
            {type: 'resource', buffered: true})});
    })();
  )";
  ASSERT_TRUE(EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                     wait_for_resources)
                  .error.empty());

  // Finish session.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // Actually fetch the metrics.
  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  ExpectUniqueUMA("Renderer.CriticalFonts.BlockingResourcesLoadTime");
  ExpectUniqueUMA("Renderer.CriticalFonts.PreloadedFontsLoadTime");
  int script_load_time =
      histogram_tester()
          .GetAllSamples("Renderer.CriticalFonts.BlockingResourcesLoadTime")[0]
          .min;
  int font_load_time =
      histogram_tester()
          .GetAllSamples("Renderer.CriticalFonts.PreloadedFontsLoadTime")[0]
          .min;
  // The bucket size for each measurement is 200ms. Setting the margin of error
  // to a bit above that both up and down, as we're verifying a diff of two
  // measurements using their buckets, and we want to reduce flakiness.
  ExpectUniqueUMAWithinRange("Renderer.CriticalFonts.CriticalFontDelay",
                             font_load_time - script_load_time, 210.0, 210.0);
}
