// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/integration_tests/metric_integration_test.h"

#include "base/test/trace_event_analyzer.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "services/metrics/public/cpp/ukm_builders.h"

using ukm::TestUkmRecorder;
using ukm::builders::InputEvent;
using ukm::builders::PageLoad;
using ukm::mojom::UkmEntry;

class TotalInputDelayIntegrationTest : public MetricIntegrationTest {
 protected:
  std::vector<int64_t> GetAllInputDelay() {
    std::vector<int64_t> input_delay_list;
    std::vector<const UkmEntry*> entries =
        ukm_recorder().GetEntriesByName(InputEvent::kEntryName);
    for (auto* const entry : entries) {
      const int64_t* metric = TestUkmRecorder::GetEntryMetric(
          entry, InputEvent::kInteractiveTiming_InputDelayName);
      input_delay_list.push_back(*metric);
    }
    return input_delay_list;
  }

  void ExpectUKMTotalInputDelayMetricNear(base::StringPiece metric_name,
                                          int64_t expected_value,
                                          int num_input_events) {
    std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
        ukm_recorder().GetMergedEntriesByName(PageLoad::kEntryName);
    EXPECT_EQ(1ul, merged_entries.size());
    const auto& kv = merged_entries.begin();
    const int64_t* metric_value =
        TestUkmRecorder::GetEntryMetric(kv->second.get(), metric_name);
    EXPECT_NE(metric_value, nullptr);
    double delta = *metric_value - expected_value;
    EXPECT_LE(delta * delta, num_input_events * num_input_events);
  }
};

IN_PROC_BROWSER_TEST_F(TotalInputDelayIntegrationTest, NoInputEvent) {
  LoadHTML(R"HTML(
    <p>Sample website</p>
  )HTML");

  StartTracing({"loading"});

  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));

  // Check UKM.
  ExpectUKMPageLoadMetric(PageLoad::kInteractiveTiming_NumInputEventsName, 0);
  ExpectUKMTotalInputDelayMetricNear(
      PageLoad::kInteractiveTiming_TotalInputDelayName, int64_t(0), 0);
  ExpectUKMTotalInputDelayMetricNear(
      PageLoad::kInteractiveTiming_TotalAdjustedInputDelayName, int64_t(0), 0);
}

// Flaky: crbug.com/1163677
IN_PROC_BROWSER_TEST_F(TotalInputDelayIntegrationTest,
                       DISABLED_MultipleInputEvents) {
  LoadHTML(R"HTML(
    <p>Sample website</p>
  )HTML");

  StartTracing({"loading"});

  // Simulate user's input.
  content::SimulateMouseClick(web_contents(), 0,
                              blink::WebMouseEvent::Button::kLeft);
  content::SimulateMouseClick(web_contents(), 0,
                              blink::WebMouseEvent::Button::kLeft);
  content::SimulateMouseClick(web_contents(), 0,
                              blink::WebMouseEvent::Button::kLeft);

  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));

  // Get all input delay recorded by UKM.
  std::vector<int64_t> input_delay_list = GetAllInputDelay();

  int num_input_events = input_delay_list.size();
  int64_t total_input_delay = 0;
  int64_t total_adjusted_input_delay = 0;
  for (int64_t input_delay : input_delay_list) {
    total_input_delay += input_delay;
    total_adjusted_input_delay +=
        std::max(int64_t(0), input_delay - int64_t(50));
  }

  // Check UKM.
  ExpectUKMPageLoadMetric(PageLoad::kInteractiveTiming_NumInputEventsName,
                          num_input_events);
  ExpectUKMTotalInputDelayMetricNear(
      PageLoad::kInteractiveTiming_TotalInputDelayName, total_input_delay,
      num_input_events);
  ExpectUKMTotalInputDelayMetricNear(
      PageLoad::kInteractiveTiming_TotalAdjustedInputDelayName,
      total_adjusted_input_delay, num_input_events);
}
