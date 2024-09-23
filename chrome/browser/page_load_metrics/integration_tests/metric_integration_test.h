// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_INTEGRATION_TESTS_METRIC_INTEGRATION_TEST_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_INTEGRATION_TESTS_METRIC_INTEGRATION_TEST_H_

#include <string_view>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/render_widget_host.h"

namespace base {
class CommandLine;
}

namespace content {
class WebContents;
}

namespace net {
namespace test_server {
struct HttpRequest;
class HttpResponse;
}  // namespace test_server
}  // namespace net

namespace trace_analyzer {
class TraceAnalyzer;
}

// Base class for end to end integration tests of speed metrics.
// See README.md for more information.
class MetricIntegrationTest : public InProcessBrowserTest {
 public:
  MetricIntegrationTest();
  ~MetricIntegrationTest() override;

  // Override of BrowserTestBase::SetUpOnMainThread.
  void SetUpOnMainThread() override;

 protected:
  // Configures a request handler for the specified URL, which supplies
  // the specified response content.  Example:
  //
  //   Serve("/foo.html", "<html> Hello, World! </html>");
  //
  // The response is served with an HTTP 200 status code and a content
  // type of "text/html; charset=utf-8".
  void Serve(const std::string& url, const std::string& content);

  // Like Serve, but with an artificial time delay in the response.
  void ServeDelayed(const std::string& url,
                    const std::string& content,
                    base::TimeDelta delay);

  // Starts the test server.  Call this after configuring request
  // handlers, and before loading any pages.
  void Start();

  // Navigates Chrome to the specified URL in the current tab.
  void Load(const std::string& relative_url);

  // Convenience helper for Serve + Start + Load of a single HTML
  // resource at the URL "/test.html".
  void LoadHTML(const std::string& content);

  content::RenderWidgetHost* GetRenderWidgetHost();

  // Begin trace collection for the specified trace categories. The
  // trace includes events from all processes (browser and renderer).
  void StartTracing(const std::vector<std::string>& categories);

  // End trace collection and write trace output as JSON into the
  // specified string.
  void StopTracing(std::string& trace_output);

  // End trace collection and return a TraceAnalyzer which can run
  // queries on the trace output.
  std::unique_ptr<trace_analyzer::TraceAnalyzer> StopTracingAndAnalyze();

  // Returns the WebContents object for the current Chrome tab.
  content::WebContents* web_contents() const;

  // Override for command-line flags.
  void SetUpCommandLine(base::CommandLine* command_line) override;

  ukm::TestAutoSetUkmRecorder& ukm_recorder() { return *ukm_recorder_; }
  base::HistogramTester& histogram_tester() { return *histogram_tester_; }

  std::vector<double> GetPageLoadMetricsAsList(std::string_view metric_name);

  // Checks for a single UKM entry under the PageLoad event with the specified
  // metric name and value.
  void ExpectUKMPageLoadMetric(std::string_view metric_name,
                               int64_t expected_value);

  void ExpectUKMPageLoadMetricNonExistence(std::string_view metric_name);

  void ExpectUkmEventNotRecorded(std::string_view event_name);

  void ExpectUKMPageLoadMetricNonExistenceWithExpectedPageLoadMetricsNum(
      unsigned long expected_num_page_load_metrics,
      std::string_view metric_name);

  void ExpectUKMPageLoadMetricGreaterThan(std::string_view metric_name,
                                          int64_t expected_value);
  void ExpectUKMPageLoadMetricLowerThan(std::string_view metric_name,
                                        int64_t expected_value);

  void ExpectUKMPageLoadMetricsInAscendingOrder(std::string_view metric_name1,
                                                std::string_view metric_name2);

  int64_t GetUKMPageLoadMetricFlagSet(std::string_view metric_name);

  // The expected being true means ALL the bits present in the expected
  // flag_set should also be present in the flag_set retrieved from the ukm
  // metrics.
  // The expected being false means NONE of the bits present in the expected
  // flag_set should be present in the flag_set retrieved from the ukm
  // metrics.
  void ExpectUKMPageLoadMetricFlagSet(std::string_view metric_name,
                                      uint32_t flag_set,
                                      bool expected);

  void ExpectUKMPageLoadMetricFlagSetExactMatch(std::string_view metric_name,
                                                uint32_t flag_set);

  void ExpectUKMPageLoadMetricNear(std::string_view metric_name,
                                   double expected_value,
                                   double epsilon);

  // Checks that the UMA entry is in the bucket for |expected_value| or within
  // the bucket for |expected_value| +- 1.
  void ExpectUniqueUMAPageLoadMetricNear(std::string_view metric_name,
                                         double expected_value);

  // Checks that the UMA entry is in the bucket for |expected_value| or within
  // the bucket for |expected_value| +- `range`.
  void ExpectUniqueUMAWithinRange(std::string_view metric_name,
                                  double expected_value,
                                  double below,
                                  double above);

  // Checks that the UMA bucket count precisely matches the provided value.
  void ExpectUniqueUMABucketCount(std::string_view metric_name,
                                  base::Histogram::Sample sample,
                                  base::Histogram::Count count);

  // Checks that we have a single UMA entry.
  void ExpectUniqueUMA(std::string_view metric_name);

  // Checks that the value of |metric_name| in the latest timing update trace
  // event emitted by UkmPageLoadMetricsObserver is within |epsilon| of
  // |expected_value|.
  void ExpectMetricInLastUKMUpdateTraceEventNear(
      trace_analyzer::TraceAnalyzer& trace_analyzer,
      std::string_view metric_name,
      double expected_value,
      double epsilon);

 private:
  static std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const std::string& relative_url,
      const std::string& content,
      base::TimeDelta delay,
      const net::test_server::HttpRequest& request);

  const ukm::mojom::UkmEntryPtr GetEntry();

  base::test::ScopedFeatureList feature_list_;
  std::optional<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
  std::optional<base::HistogramTester> histogram_tester_;
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_INTEGRATION_TESTS_METRIC_INTEGRATION_TEST_H_
