// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/integration_tests/metric_integration_test.h"

#include "base/json/json_string_value_serializer.h"
#include "base/strings/strcat.h"
#include "base/test/trace_event_analyzer.h"
#include "build/build_config.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "services/metrics/public/cpp/ukm_builders.h"

using trace_analyzer::Query;
using trace_analyzer::TraceAnalyzer;
using trace_analyzer::TraceEvent;
using trace_analyzer::TraceEventVector;
using ukm::builders::PageLoad;

namespace {

void ValidateCandidate(int expected_size, const TraceEvent& event) {
  std::unique_ptr<base::Value> data;
  ASSERT_TRUE(event.GetArgAsValue("data", &data));

  const base::Optional<int> traced_size = data->FindIntKey("size");
  ASSERT_TRUE(traced_size.has_value());
  EXPECT_EQ(traced_size.value(), expected_size);

  const base::Optional<bool> traced_main_frame_flag =
      data->FindBoolKey("isMainFrame");
  ASSERT_TRUE(traced_main_frame_flag.has_value());
  EXPECT_TRUE(traced_main_frame_flag.value());
}

int GetCandidateIndex(const TraceEvent& event) {
  std::unique_ptr<base::Value> data = event.GetKnownArgAsValue("data");
  base::Optional<int> candidate_idx = data->FindIntKey("candidateIndex");
  DCHECK(candidate_idx.has_value()) << "couldn't find 'candidateIndex'";

  return candidate_idx.value();
}

bool compare_candidate_index(const TraceEvent* lhs, const TraceEvent* rhs) {
  return GetCandidateIndex(*lhs) < GetCandidateIndex(*rhs);
}

void ValidateTraceEvents(std::unique_ptr<TraceAnalyzer> analyzer) {
  TraceEventVector events;
  analyzer->FindEvents(Query::EventNameIs("largestContentfulPaint::Candidate"),
                       &events);
  EXPECT_EQ(3ul, events.size());
  std::sort(events.begin(), events.end(), compare_candidate_index);

  // LCP_0 uses green-16x16.png, of size 16 x 16.
  ValidateCandidate(16 * 16, *events[0]);
  // LCP_1 uses blue96x96.png, of size 96 x 96.
  ValidateCandidate(96 * 96, *events[1]);
  // LCP_2 uses green-256x256.png, of size 16 x 16.
  ValidateCandidate(256 * 256, *events[2]);
}

}  // namespace

IN_PROC_BROWSER_TEST_F(MetricIntegrationTest, LargestContentfulPaint) {
  Start();
  StartTracing({"loading"});
  Load("/largest_contentful_paint.html");

  // The test harness serves files from something like http://example.com:34777
  // but the port number can vary. Extract the 'window.origin' property so we
  // can compare encountered URLs to expected values.
  const std::string window_origin =
      EvalJs(web_contents(), "window.origin").ExtractString();
  const std::string image_1_url_expected =
      base::StrCat({window_origin, "/images/green-16x16.png"});
  const std::string image_2_url_expected =
      base::StrCat({window_origin, "/images/blue96x96.png"});
  const std::string image_3_url_expected =
      base::StrCat({window_origin, "/images/green-256x256.png"});

  content::EvalJsResult result = EvalJs(web_contents(), "run_test()");
  EXPECT_EQ("", result.error);

  // Verify that the JS API yielded three LCP reports. Note that, as we resolve
  // https://github.com/WICG/largest-contentful-paint/issues/41, this test may
  // need to be updated to reflect new semantics.
  const auto& list = result.value.GetList();
  const std::string expected_url[3] = {
      image_1_url_expected, image_2_url_expected, image_3_url_expected};
  base::Optional<double> lcp_timestamps[3];
  for (size_t i = 0; i < 3; i++) {
    const std::string* url = list[i].FindStringPath("url");
    EXPECT_TRUE(url);
    EXPECT_EQ(*url, expected_url[i]);
    lcp_timestamps[i] = list[i].FindDoublePath("time");
    EXPECT_TRUE(lcp_timestamps[i].has_value());
  }
  EXPECT_LT(lcp_timestamps[0], lcp_timestamps[1])
      << "The first LCP report should be before the second";
  EXPECT_LT(lcp_timestamps[1], lcp_timestamps[2])
      << "The second LCP report should be before the third";

  // Need to navigate away from the test html page to force metrics to get
  // flushed/synced.
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));

  // Check Trace Events.
  ValidateTraceEvents(StopTracingAndAnalyze());

  // Check UKM.
  // Since UKM rounds to an integer while the JS API returns a double, we'll
  // assert that the UKM and JS values are within 1.0 of each other. Comparing
  // with strict equality could round incorrectly and introduce flakiness into
  // the test.
  ExpectUKMPageLoadMetricNear(
      PageLoad::kPaintTiming_NavigationToLargestContentfulPaintName,
      lcp_timestamps[2].value(), 1.0);
  ExpectUKMPageLoadMetricNear(
      PageLoad::kPaintTiming_NavigationToLargestContentfulPaint_MainFrameName,
      lcp_timestamps[2].value(), 1.0);

  // Check UMA.
  // Similar to UKM, rounding could introduce flakiness, so use helper to
  // compare near.
  ExpectUniqueUMAPageLoadMetricNear(
      "PageLoad.PaintTiming.NavigationToLargestContentfulPaint",
      lcp_timestamps[2].value());
  ExpectUniqueUMAPageLoadMetricNear(
      "PageLoad.PaintTiming.NavigationToLargestContentfulPaint.MainFrame",
      lcp_timestamps[2].value());
}

IN_PROC_BROWSER_TEST_F(MetricIntegrationTest,
                       LargestContentfulPaint_SubframeInput) {
  Start();
  Load("/lcp_subframe_input.html");
  auto* sub = ChildFrameAt(web_contents()->GetMainFrame(), 0);
  EXPECT_EQ(EvalJs(sub, "test_step_1()").value.GetString(), "green-16x16.png");

  content::SimulateMouseClickAt(web_contents(), 0,
                                blink::WebMouseEvent::Button::kLeft,
                                gfx::Point(100, 100));

  EXPECT_EQ(EvalJs(sub, "test_step_2()").value.GetString(), "green-16x16.png");
}
