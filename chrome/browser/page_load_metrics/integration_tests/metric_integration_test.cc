// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/integration_tests/metric_integration_test.h"

#include <string_view>

#include "base/strings/stringprintf.h"
#include "base/test/trace_event_analyzer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/tracing_controller.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/metrics/public/cpp/ukm_builders.h"

using base::CommandLine;
using base::OnceClosure;
using base::RunLoop;
using base::trace_event::TraceConfig;
using content::TracingController;
using content::WebContents;
using net::test_server::BasicHttpResponse;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;
using trace_analyzer::Query;
using trace_analyzer::TraceAnalyzer;
using trace_analyzer::TraceEvent;
using trace_analyzer::TraceEventVector;
using ukm::TestUkmRecorder;
using ukm::builders::PageLoad;
using ukm::mojom::UkmEntry;

MetricIntegrationTest::MetricIntegrationTest() {
  // TODO(crbug.com/40248833): Use HTTPS URLs in tests to avoid having to
  // disable this feature.
  feature_list_.InitAndDisableFeature(features::kHttpsUpgrades);
}

MetricIntegrationTest::~MetricIntegrationTest() = default;

void MetricIntegrationTest::SetUpOnMainThread() {
  host_resolver()->AddRule("*", "127.0.0.1");
  embedded_test_server()->ServeFilesFromSourceDirectory(
      "chrome/browser/page_load_metrics/integration_tests/data");
  embedded_test_server()->ServeFilesFromSourceDirectory(
      "third_party/blink/web_tests/external/wpt");
  content::SetupCrossSiteRedirector(embedded_test_server());

  ukm_recorder_.emplace();
  histogram_tester_.emplace();
}

void MetricIntegrationTest::ServeDelayed(const std::string& url,
                                         const std::string& content,
                                         base::TimeDelta delay) {
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&HandleRequest, url, content, delay));
}

void MetricIntegrationTest::Serve(const std::string& url,
                                  const std::string& content) {
  ServeDelayed(url, content, base::TimeDelta());
}

void MetricIntegrationTest::Start() {
  ASSERT_TRUE(embedded_test_server()->Start());
}

void MetricIntegrationTest::Load(const std::string& relative_url) {
  GURL url = embedded_test_server()->GetURL("example.com", relative_url);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
}

void MetricIntegrationTest::LoadHTML(const std::string& content) {
  Serve("/test.html", content);
  Start();
  Load("/test.html");
}

content::RenderWidgetHost* MetricIntegrationTest::GetRenderWidgetHost() {
  EXPECT_TRUE(web_contents());
  return web_contents()->GetRenderWidgetHostView()->GetRenderWidgetHost();
}

void MetricIntegrationTest::StartTracing(
    const std::vector<std::string>& categories) {
  RunLoop wait_for_tracing;
  TracingController::GetInstance()->StartTracing(
      TraceConfig(
          base::StringPrintf("{\"included_categories\": [\"%s\"]}",
                             base::JoinString(categories, "\", \"").c_str())),
      wait_for_tracing.QuitClosure());
  wait_for_tracing.Run();
}

void MetricIntegrationTest::StopTracing(std::string& trace_output) {
  RunLoop wait_for_tracing;
  TracingController::GetInstance()->StopTracing(
      TracingController::CreateStringEndpoint(base::BindOnce(
          [](OnceClosure quit_closure, std::string* output,
             std::unique_ptr<std::string> trace_str) {
            *output = *trace_str;
            std::move(quit_closure).Run();
          },
          wait_for_tracing.QuitClosure(), &trace_output)));
  wait_for_tracing.Run();
}

std::unique_ptr<TraceAnalyzer> MetricIntegrationTest::StopTracingAndAnalyze() {
  std::string trace_str;
  StopTracing(trace_str);
  return TraceAnalyzer::Create(trace_str);
}

WebContents* MetricIntegrationTest::web_contents() const {
  return browser()->tab_strip_model()->GetActiveWebContents();
}

void MetricIntegrationTest::SetUpCommandLine(CommandLine* command_line) {
  // Set a default window size for consistency.
  command_line->AppendSwitchASCII(switches::kWindowSize, "800,600");
  command_line->AppendSwitch(switches::kEnableExperimentalWebPlatformFeatures);

  content::IsolateAllSitesForTesting(command_line);
}

std::unique_ptr<HttpResponse> MetricIntegrationTest::HandleRequest(
    const std::string& relative_url,
    const std::string& content,
    base::TimeDelta delay,
    const HttpRequest& request) {
  if (request.relative_url != relative_url)
    return nullptr;
  if (!delay.is_zero())
    base::PlatformThread::Sleep(delay);
  auto response = std::make_unique<BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  response->set_content(content);
  response->set_content_type("text/html; charset=utf-8");
  return std::move(response);
}

const ukm::mojom::UkmEntryPtr MetricIntegrationTest::GetEntry() {
  auto merged_entries =
      ukm_recorder().GetMergedEntriesByName(PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());
  const auto& kv = merged_entries.begin();
  return std::move(kv->second);
}

std::vector<double> MetricIntegrationTest::GetPageLoadMetricsAsList(
    std::string_view metric_name) {
  std::vector<double> metrics;
  for (const ukm::mojom::UkmEntry* entry :
       ukm_recorder_->GetEntriesByName(ukm::builders::PageLoad::kEntryName)) {
    if (auto* rs = ukm_recorder_->GetEntryMetric(entry, metric_name)) {
      metrics.push_back(*rs);
    }
  }
  return metrics;
}

void MetricIntegrationTest::ExpectUKMPageLoadMetric(
    std::string_view metric_name,
    int64_t expected_value) {
  ukm::mojom::UkmEntryPtr entry = GetEntry();
  TestUkmRecorder::ExpectEntryMetric(entry.get(), metric_name, expected_value);
}

void MetricIntegrationTest::ExpectUKMPageLoadMetricNonExistence(
    std::string_view metric_name) {
  ukm::mojom::UkmEntryPtr entry = GetEntry();
  EXPECT_FALSE(TestUkmRecorder::EntryHasMetric(entry.get(), metric_name));
}

void MetricIntegrationTest::
    ExpectUKMPageLoadMetricNonExistenceWithExpectedPageLoadMetricsNum(
        unsigned long expected_num_page_load_metrics,
        std::string_view metric_name) {
  auto merged_entries =
      ukm_recorder().GetMergedEntriesByName(PageLoad::kEntryName);
  EXPECT_EQ(expected_num_page_load_metrics, merged_entries.size());
  for (const auto& kv : merged_entries) {
    EXPECT_FALSE(TestUkmRecorder::EntryHasMetric(kv.second.get(), metric_name));
  }
}

void MetricIntegrationTest::ExpectUkmEventNotRecorded(
    std::string_view event_name) {
  auto merged_entries = ukm_recorder().GetMergedEntriesByName(event_name);
  EXPECT_EQ(merged_entries.size(), 0u);
}

void MetricIntegrationTest::ExpectUKMPageLoadMetricGreaterThan(
    std::string_view metric_name,
    int64_t expected_value) {
  ukm::mojom::UkmEntryPtr entry = GetEntry();
  const int64_t* value =
      TestUkmRecorder::GetEntryMetric(entry.get(), metric_name);
  EXPECT_GT(*value, expected_value);
}
void MetricIntegrationTest::ExpectUKMPageLoadMetricLowerThan(
    std::string_view metric_name,
    int64_t expected_value) {
  ukm::mojom::UkmEntryPtr entry = GetEntry();
  const int64_t* value =
      TestUkmRecorder::GetEntryMetric(entry.get(), metric_name);
  EXPECT_LT(*value, expected_value);
}

void MetricIntegrationTest::ExpectUKMPageLoadMetricsInAscendingOrder(
    std::string_view metric_name1,
    std::string_view metric_name2) {
  ukm::mojom::UkmEntryPtr entry = GetEntry();
  const int64_t* value1 =
      TestUkmRecorder::GetEntryMetric(entry.get(), metric_name1);
  EXPECT_TRUE(value1 != nullptr);
  const int64_t* value2 =
      TestUkmRecorder::GetEntryMetric(entry.get(), metric_name2);
  EXPECT_TRUE(value2 != nullptr);
  EXPECT_LE(*value1, *value2);
}

int64_t MetricIntegrationTest::GetUKMPageLoadMetricFlagSet(
    std::string_view metric_name) {
  ukm::mojom::UkmEntryPtr entry = GetEntry();
  const int64_t* flag_set =
      TestUkmRecorder::GetEntryMetric(entry.get(), metric_name);
  EXPECT_TRUE(flag_set != nullptr);
  return *flag_set;
}

void MetricIntegrationTest::ExpectUKMPageLoadMetricFlagSet(
    std::string_view metric_name,
    uint32_t flag_set,
    bool expected) {
  if (expected) {
    EXPECT_EQ(GetUKMPageLoadMetricFlagSet(metric_name) &
                  static_cast<int64_t>(flag_set),
              static_cast<int64_t>(flag_set));
  } else {
    EXPECT_FALSE(GetUKMPageLoadMetricFlagSet(metric_name) &
                 static_cast<int64_t>(flag_set));
  }
}

void MetricIntegrationTest::ExpectUKMPageLoadMetricFlagSetExactMatch(
    std::string_view metric_name,
    uint32_t flag_set) {
  EXPECT_EQ(GetUKMPageLoadMetricFlagSet(metric_name),
            static_cast<int64_t>(flag_set));
}

void MetricIntegrationTest::ExpectUKMPageLoadMetricNear(
    std::string_view metric_name,
    double expected_value,
    double epsilon) {
  ukm::mojom::UkmEntryPtr entry = GetEntry();
  const int64_t* recorded =
      TestUkmRecorder::GetEntryMetric(entry.get(), metric_name);
  EXPECT_NE(recorded, nullptr);
  EXPECT_NEAR(*recorded, expected_value, epsilon);
}

void MetricIntegrationTest::ExpectUniqueUMAWithinRange(
    std::string_view metric_name,
    double expected_value,
    double below,
    double above) {
  EXPECT_EQ(histogram_tester_->GetAllSamples(metric_name).size(), 1u)
      << "There should be one sample for " << metric_name.data();

  auto bucket_min = histogram_tester().GetAllSamples(metric_name)[0].min;

  EXPECT_GE(bucket_min, expected_value - below)
      << "The sample for " << metric_name.data()
      << " is smaller than the expected range of " << below << " from "
      << expected_value;
  EXPECT_LE(bucket_min, expected_value + above)
      << "The sample for " << metric_name.data()
      << " is larger than the expected range of " << above << " from "
      << expected_value;
}

void MetricIntegrationTest::ExpectUniqueUMABucketCount(
    std::string_view metric_name,
    base::HistogramBase::Sample sample,
    base::HistogramBase::Count count) {
  histogram_tester_->ExpectBucketCount(metric_name, sample, count);
}

void MetricIntegrationTest::ExpectUniqueUMAPageLoadMetricNear(
    std::string_view metric_name,
    double expected_value) {
  EXPECT_EQ(histogram_tester_->GetAllSamples(metric_name).size(), 1u)
      << "There should be one sample for " << metric_name.data();
  // UMA uses integer buckets so check that the value is in the bucket of
  // |expected_value| or in the bucket of |expected_value| +- 1.
  EXPECT_TRUE(
      histogram_tester_->GetBucketCount(metric_name, expected_value) == 1 ||
      histogram_tester_->GetBucketCount(metric_name, expected_value + 1.0) ==
          1 ||
      histogram_tester_->GetBucketCount(metric_name, expected_value - 1.0) == 1)
      << "The sample for " << metric_name.data()
      << " is not near the expected value!";
}

void MetricIntegrationTest::ExpectUniqueUMA(std::string_view metric_name) {
  EXPECT_EQ(histogram_tester_->GetAllSamples(metric_name).size(), 1u)
      << "There should be one sample for " << metric_name.data();
}

void MetricIntegrationTest::ExpectMetricInLastUKMUpdateTraceEventNear(
    TraceAnalyzer& trace_analyzer,
    std::string_view metric_name,
    double expected_value,
    double epsilon) {
  TraceEventVector ukm_update_events;
  trace_analyzer.FindEvents(Query::EventNameIs("UkmPageLoadTimingUpdate"),
                            &ukm_update_events);
  ASSERT_GT(ukm_update_events.size(), 0ul);

  const TraceEvent* last_update_event = ukm_update_events.back();

  base::Value::Dict arg_dict;
  last_update_event->GetArgAsDict("ukm_page_load_timing_update", &arg_dict);
  std::optional<double> metric_value = arg_dict.FindDouble(metric_name);
  ASSERT_TRUE(metric_value.has_value());

  EXPECT_NEAR(expected_value, *metric_value, epsilon);
}
