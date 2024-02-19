// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/integration_tests/metric_integration_test.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace {

std::unique_ptr<net::test_server::HttpResponse> ServePreloadHeader(
    const std::string& request_path,
    const net::test_server::HttpRequest& request) {
  if (!base::StartsWith(request.relative_url, request_path,
                        base::CompareCase::SENSITIVE)) {
    return nullptr;
  }

  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  http_response->set_code(net::HTTP_FOUND);
  http_response->AddCustomHeader("Link",
                                 "</fonts/AD.woff>; rel=preload; as=font");
  return std::move(http_response);
}

}  // namespace

IN_PROC_BROWSER_TEST_F(MetricIntegrationTest, UnusedHeaderFontPreload) {
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&ServePreloadHeader, "/header_preload.html"));
  ASSERT_TRUE(embedded_test_server()->Start());
  Load("/header_preload.html");

  const std::string wait_for_warning = R"(
    (async () => {
      await new Promise(
        resolve => {
          if (document.readyState = "complete") {
            setTimeout(resolve, 3500);
          } else {
            window.addEventListener("DOMContentLoaded", () => {
              setTimeout(resolve, 3500);
            });
          }
        });
    })();
  )";
  ASSERT_TRUE(EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                     wait_for_warning)
                  .error.empty());

  // Finish session.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // Actually fetch the metrics.
  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  int32_t unused_font_preloads = histogram_tester().GetBucketCount(
      "Renderer.Preload.UnusedResource", /*ResourceType::kFont*/ 4);
  int32_t unused_script_preloads = histogram_tester().GetBucketCount(
      "Renderer.Preload.UnusedResource", /*ResourceType::kScript*/ 3);
  ASSERT_EQ(unused_font_preloads, 1);
  ASSERT_EQ(unused_script_preloads, 0);
}

IN_PROC_BROWSER_TEST_F(MetricIntegrationTest, UnusedFontPreload) {
  Start();
  Load("/render_blocking_resource_and_preloaded_font.html");

  const std::string wait_for_warning = R"(
    (async () => {
      await new Promise(
        resolve => {
          if (document.readyState = "complete") {
            setTimeout(resolve, 3500);
          } else {
            window.addEventListener("DOMContentLoaded", () => {
              setTimeout(resolve, 3500);
            });
          }
        });
    })();
  )";
  ASSERT_TRUE(EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                     wait_for_warning)
                  .error.empty());

  // Finish session.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // Actually fetch the metrics.
  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  int32_t unused_font_preloads = histogram_tester().GetBucketCount(
      "Renderer.Preload.UnusedResource", /*ResourceType::kFont*/ 4);
  int32_t unused_script_preloads = histogram_tester().GetBucketCount(
      "Renderer.Preload.UnusedResource", /*ResourceType::kScript*/ 3);
  ASSERT_EQ(unused_font_preloads, 1);
  ASSERT_EQ(unused_script_preloads, 0);
}
