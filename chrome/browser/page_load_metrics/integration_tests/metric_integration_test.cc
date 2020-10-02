// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/integration_tests/metric_integration_test.h"

#include "base/strings/stringprintf.h"
#include "base/test/trace_event_analyzer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/tracing_controller.h"
#include "content/public/common/content_switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/metrics/public/cpp/ukm_builders.h"

using base::CommandLine;
using base::OnceClosure;
using base::RunLoop;
using base::StringPiece;
using base::TimeDelta;
using base::trace_event::TraceConfig;
using content::TracingController;
using content::WebContents;
using net::test_server::BasicHttpResponse;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;
using trace_analyzer::TraceAnalyzer;
using ukm::TestUkmRecorder;
using ukm::builders::PageLoad;
using ukm::mojom::UkmEntry;

MetricIntegrationTest::MetricIntegrationTest() = default;
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
                                         TimeDelta delay) {
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&HandleRequest, url, content, delay));
}

void MetricIntegrationTest::Serve(const std::string& url,
                                  const std::string& content) {
  ServeDelayed(url, content, TimeDelta());
}

void MetricIntegrationTest::Start() {
  ASSERT_TRUE(embedded_test_server()->Start());
}

void MetricIntegrationTest::Load(const std::string& relative_url) {
  GURL url = embedded_test_server()->GetURL("example.com", relative_url);
  ui_test_utils::NavigateToURL(browser(), url);
}

void MetricIntegrationTest::LoadHTML(const std::string& content) {
  Serve("/test.html", content);
  Start();
  Load("/test.html");
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
  return std::unique_ptr<TraceAnalyzer>(TraceAnalyzer::Create(trace_str));
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
    TimeDelta delay,
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

void MetricIntegrationTest::ExpectUKMPageLoadMetric(StringPiece metric_name,
                                                    int64_t expected_value) {
  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      ukm_recorder().GetMergedEntriesByName(PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());
  const auto& kv = merged_entries.begin();
  TestUkmRecorder::ExpectEntryMetric(kv->second.get(), metric_name,
                                     expected_value);
}

void MetricIntegrationTest::ExpectUKMPageLoadMetricNear(StringPiece metric_name,
                                                        double expected_value,
                                                        double epsilon) {
  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      ukm_recorder().GetMergedEntriesByName(PageLoad::kEntryName);

  EXPECT_EQ(1ul, merged_entries.size());
  const auto& kv = merged_entries.begin();
  const int64_t* recorded =
      TestUkmRecorder::GetEntryMetric(kv->second.get(), metric_name);
  EXPECT_NE(recorded, nullptr);
  EXPECT_NEAR(*recorded, expected_value, epsilon);
}

void MetricIntegrationTest::ExpectUniqueUMAPageLoadMetricNear(
    StringPiece metric_name,
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
