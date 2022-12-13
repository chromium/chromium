// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/integration_tests/metric_integration_test.h"

#include "base/test/trace_event_analyzer.h"
#include "build/build_config.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "content/public/test/browser_test.h"
#include "services/metrics/public/cpp/ukm_builders.h"

using absl::optional;
using base::Bucket;
using base::Value;
using ShiftFrame = page_load_metrics::PageLoadMetricsTestWaiter::ShiftFrame;
using trace_analyzer::Query;
using trace_analyzer::TraceAnalyzer;
using trace_analyzer::TraceEventVector;
using ukm::builders::PageLoad;

class LayoutInstabilityTest : public MetricIntegrationTest {
 protected:
  // This function will load and run the WPT, merge the layout shift scores
  // from both the main frame and sub-frame.
  // We need to specify which frame the layout shift happens and whether we
  // want to verify the layout shift UKM and UMA values.
  void RunWPT(const std::string& test_file,
              ShiftFrame frame = ShiftFrame::LayoutShiftOnlyInMainFrame,
              bool check_UKM_UMA_metrics = false);

 private:
  double CheckTraceData(Value::List& expectations, TraceAnalyzer&);
  void CheckSources(const Value::List& expected_sources,
                    const Value::List& trace_sources);
  void CheckUKMAndUMAMetrics(double expect_score);
};

void LayoutInstabilityTest::RunWPT(const std::string& test_file,
                                   ShiftFrame frame,
                                   bool check_UKM_UMA_metrics) {
  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      web_contents());
  // Wait for the layout shift in the desired frame.
  waiter->AddPageLayoutShiftExpectation(frame);

  Start();
  StartTracing({"loading", TRACE_DISABLED_BY_DEFAULT("layout_shift.debug")});
  Load("/layout-instability/" + test_file);

  // Set layout shift amount expectations from web perf API.
  base::Value::List expectations;
  if (frame == ShiftFrame::LayoutShiftOnlyInMainFrame ||
      frame == ShiftFrame::LayoutShiftOnlyInBothFrames) {
    base::Value value = EvalJs(web_contents(), "cls_run_tests").ExtractList();
    for (auto& d : value.GetList())
      expectations.Append(std::move(d));
  }
  if (frame == ShiftFrame::LayoutShiftOnlyInSubFrame ||
      frame == ShiftFrame::LayoutShiftOnlyInBothFrames) {
    content::RenderFrameHost* child_frame =
        content::ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
    base::Value value = EvalJs(child_frame, "cls_run_tests").ExtractList();
    for (auto& d : value.GetList())
      expectations.Append(std::move(d));
  }

  // It compares the trace data of layout shift events with |expectations| and
  // computes a score that's used to check the UKM and UMA values below.
  double final_score = CheckTraceData(expectations, *StopTracingAndAnalyze());

  waiter->Wait();
  // Finish session.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // We can only verify the layout shift metrics here in UKM and UMA if layout
  // shift only happens in the main frame. For layout shift happens in the
  // sub-frame, it needs to apply a sub-frame weighting factor.
  if (check_UKM_UMA_metrics) {
    DCHECK_EQ(ShiftFrame::LayoutShiftOnlyInMainFrame, frame);
    CheckUKMAndUMAMetrics(final_score);
  }
}

double LayoutInstabilityTest::CheckTraceData(Value::List& expectations,
                                             TraceAnalyzer& analyzer) {
  double final_score = 0.0;

  TraceEventVector events;
  analyzer.FindEvents(Query::EventNameIs("LayoutShift"), &events);

  size_t i = 0;
  for (const Value& expectation_value : expectations) {
    const Value::Dict& expectation = expectation_value.GetDict();

    optional<double> score = expectation.FindDouble("score");
    if (score && *score == 0.0) {
      // {score:0} expects no layout shift.
      continue;
    }

    Value::Dict data = events[i++]->GetKnownArgAsDict("data");

    if (score) {
      const absl::optional<double> traced_score = data.FindDouble("score");
      final_score += traced_score.has_value() ? traced_score.value() : 0;
      EXPECT_EQ(*score, final_score);
    }
    const Value::List* sources = expectation.FindList("sources");
    if (sources) {
      CheckSources(*sources, *data.FindList("impacted_nodes"));
    }
  }

  EXPECT_EQ(i, events.size());
  return final_score;
}

void LayoutInstabilityTest::CheckSources(const Value::List& expected_sources,
                                         const Value::List& trace_sources) {
  EXPECT_EQ(expected_sources.size(), trace_sources.size());
  size_t i = 0;
  for (const Value& expected_source : expected_sources) {
    const Value::Dict& expected_source_dict = expected_source.GetDict();
    const Value::Dict& trace_source_dict = trace_sources[i++].GetDict();
    int node_id = *trace_source_dict.FindInt("node_id");
    if (expected_source_dict.Find("node")->type() == Value::Type::NONE) {
      EXPECT_EQ(node_id, 0);
    } else {
      EXPECT_NE(node_id, 0);
      EXPECT_EQ(*expected_source_dict.FindString("debugName"),
                *trace_source_dict.FindString("debug_name"));
    }
    EXPECT_EQ(*expected_source_dict.FindList("previousRect"),
              *trace_source_dict.FindList("old_rect"));
    EXPECT_EQ(*expected_source_dict.FindList("currentRect"),
              *trace_source_dict.FindList("new_rect"));
  }
}

void LayoutInstabilityTest::CheckUKMAndUMAMetrics(double expect_score) {
  // Check UKM.
  ExpectUKMPageLoadMetric(PageLoad::kLayoutInstability_CumulativeShiftScoreName,
                          page_load_metrics::LayoutShiftUkmValue(expect_score));

  // Check UMA.
  auto samples = histogram_tester().GetAllSamples(
      "PageLoad.LayoutInstability.CumulativeShiftScore");
  EXPECT_EQ(1ul, samples.size());
  EXPECT_EQ(samples[0],
            Bucket(page_load_metrics::LayoutShiftUmaValue(expect_score), 1));
}

// TODO(crbug.com/1400401): Deflake and re-enable this test.
IN_PROC_BROWSER_TEST_F(LayoutInstabilityTest, DISABLED_SimpleBlockMovement) {
  RunWPT("simple-block-movement.html", ShiftFrame::LayoutShiftOnlyInMainFrame,
         true /* check_UKM_UMA_metrics */);
}

IN_PROC_BROWSER_TEST_F(LayoutInstabilityTest, Sources_Enclosure) {
  RunWPT("sources-enclosure.html");
}

// TODO(crbug.com/1400401): Deflake and re-enable this test.
IN_PROC_BROWSER_TEST_F(LayoutInstabilityTest, DISABLED_Sources_MaxImpact) {
  RunWPT("sources-maximpact.html");
}

// This test verifies the layout shift score in the sub-frame is recorded
// correctly in both UKM and UMA, the layout shift score in sub-frame is
// calculated by applying a sub-frame weighting factor to the total score.
IN_PROC_BROWSER_TEST_F(LayoutInstabilityTest, OOPIFSubframeWeighting) {
  RunWPT("main-frame.html", ShiftFrame::LayoutShiftOnlyInSubFrame);

  // Check UKM.
  ExpectUKMPageLoadMetricNear(
      PageLoad::kLayoutInstability_CumulativeShiftScoreName,
      page_load_metrics::LayoutShiftUkmValue(0.03), 1);

  // Check UMA.
  ExpectUniqueUMAPageLoadMetricNear(
      "PageLoad.LayoutInstability.CumulativeShiftScore",
      page_load_metrics::LayoutShiftUmaValue(0.03));
}

// TODO(crbug.com/1400401): Deflake and re-enable this test.
IN_PROC_BROWSER_TEST_F(LayoutInstabilityTest,
                       DISABLED_CumulativeLayoutShift_OneSecondGap) {
  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      web_contents());
  waiter->AddPageLayoutShiftExpectation();

  Start();
  StartTracing({"loading", TRACE_DISABLED_BY_DEFAULT("layout_shift.debug")});
  Load("/layout-instability/simple-block-movement.html");

  // Wait for the first layout shift.
  waiter->Wait();

  // Have the program sleep for 1 second to ensure the one second gap
  base::PlatformThread::Sleep(base::Milliseconds(1000));

  waiter->AddPageLayoutShiftExpectation();
  // Simulate the layout shift and this layout shift should be in the
  // new window session because it has been 1 second since last
  // layout shift. The first layout shift in simple-block-movement moves
  // the shifter to 160px and this layout shift moves the shifter to
  // 500px, so the second layout shift has 340px distance.
  const auto& result = ExecJs(web_contents(),
    "("
      "async () => {"
        "document.querySelector('#shifter').style = \"top: 500px\";"
        "await watcher.promise;"
      "}"
    ")()"
  );

  // Extract the startTime and score list from ScoreWatcher.
  base::Value entry_records =
      EvalJs(web_contents(), "watcher.get_entry_record()").ExtractList();
  const auto& entry_records_list = entry_records.GetList();

  // Verify that the entry_records_list has exactly 2 records.
  EXPECT_EQ(2ul, entry_records_list.size());

  // Extract the startTime and score from each records.
  optional<double> record_startTime_one =
      entry_records_list[0].GetDict().FindDouble("startTime");
  optional<double> record_score_one =
      entry_records_list[0].GetDict().FindDouble("score");
  optional<double> record_startTime_two =
      entry_records_list[1].GetDict().FindDouble("startTime");
  optional<double> record_score_two =
      entry_records_list[1].GetDict().FindDouble("score");

  // Verify that the optional<double> has value.
  ASSERT_TRUE(record_startTime_one);
  ASSERT_TRUE(record_score_one);
  ASSERT_TRUE(record_startTime_two);
  ASSERT_TRUE(record_score_two);

  // Verify that layout shift two happened at least 1 second after
  // layout shift one, and it has bigger score than layout shift one.
  EXPECT_GT(*record_startTime_two, *record_startTime_one + 1000);
  EXPECT_GT(*record_score_two, *record_score_one);

  // Wait for the second layout shift after the one second gap.
  waiter->Wait();
  // Finish session.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // Check UKM with CLS Normalization value, and it should be the same as the
  // second layout shift score.
  ExpectUKMPageLoadMetric(
      PageLoad::
          kLayoutInstability_MaxCumulativeShiftScore_SessionWindow_Gap1000ms_Max5000msName,
      page_load_metrics::LayoutShiftUkmValue(*record_score_two));

  // Check UMA with the second layout shift score.
  auto samples = histogram_tester().GetAllSamples(
      "PageLoad.LayoutInstability.CumulativeShiftScore");
  EXPECT_EQ(1ul, samples.size());
  EXPECT_EQ(
      samples[0],
      Bucket(page_load_metrics::LayoutShiftUmaValue(*record_score_two), 1));
}

// TODO(crbug.com/1400401): Deflake and re-enable this test.
IN_PROC_BROWSER_TEST_F(LayoutInstabilityTest,
                       DISABLED_CumulativeLayoutShift_hadRecentInput) {
  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      web_contents());
  waiter->AddPageLayoutShiftExpectation();
  Start();
  StartTracing({"loading", TRACE_DISABLED_BY_DEFAULT("layout_shift.debug")});
  Load("/layout-instability/simple-block-movement.html");

  // Wait for the first layout shift.
  waiter->Wait();

  // Let the program to sleep for one second, so the first layout shift
  // and the second layout shift will have at least one second gap.
  base::PlatformThread::Sleep(base::Milliseconds(1000));

  // Create a Performance Observer to observe first input in the program
  // and the promise will resolve when it observes first input. We are
  // leveraging the Performance Observer to ensure we received a input.
  EXPECT_TRUE(ExecJs(web_contents(),
   "waitForClick = async () => {"
      "const observePromise = new Promise(resolve => {"
        "new PerformanceObserver(e => {"
          "e.getEntries().forEach(entry => {"
            "resolve(true);"
          "})"
        "}).observe({type: 'first-input', buffered: true});"
      "});"
      "return await observePromise;"
    "};"
  ));

  // Add a event listener to shifter, so after it got clicked it will
  // simulate a layout shift and this layout shift should be in the
  // new window session because it has been 1 second since last
  // layout shift. The first layout shift in simple-block-movement moves
  // the shifter to 160px and this layout shift moves the shifter to
  // 500px, so the second layout shift has 340px distance.
 EXPECT_TRUE(ExecJs(web_contents(),
    "const element = document.getElementById('shifter');"
    "const clickHandler = async () => {"
      "document.querySelector('#shifter').style = \"top: 500px\";"
      "await watcher.promise;"
    "};"
    "element.addEventListener(\"pointerdown\", clickHandler);"
  ));

  // Simulate a click as our input and trigger the clickHandler with shifter.
  content::SimulateMouseClickOrTapElementWithId(web_contents(), "shifter");

  // Start the waitForClick Performance Observer.
  ASSERT_TRUE(EvalJs(web_contents(), "waitForClick()").ExtractBool());

  // TODO(crbug.com/1385897): We have issue with test_waiter while
  // there are multiple layout shifts. Should replace Sleep() with
  // waiter->Wait() after fixing the test_waiter for layout shifts.
  base::PlatformThread::Sleep(base::Milliseconds(1000));

  // Extract the startTime and score list from ScoreWatcher.
  base::Value entry_records =
      EvalJs(web_contents(), "watcher.get_entry_record()").ExtractList();
  const auto& entry_records_list = entry_records.GetList();

  // Verify that the entry_records_list has exactly 2 records.
  EXPECT_EQ(2ul, entry_records_list.size());

  // Extract the startTime and score from each records.
  optional<double> record_startTime_one =
      entry_records_list[0].GetDict().FindDouble("startTime");
  optional<double> record_score_one =
      entry_records_list[0].GetDict().FindDouble("score");
  optional<double> record_hadRecentInput_one =
    entry_records_list[0].GetDict().FindBool("hadRecentInput");
  optional<double> record_startTime_two =
      entry_records_list[1].GetDict().FindDouble("startTime");
  optional<double> record_score_two =
      entry_records_list[1].GetDict().FindDouble("score");
  optional<double> record_hadRecentInput_two =
    entry_records_list[1].GetDict().FindBool("hadRecentInput");

  // Verify that the optional<double> has value.
  ASSERT_TRUE(record_startTime_one);
  ASSERT_TRUE(record_score_one);
  ASSERT_TRUE(record_hadRecentInput_one);
  ASSERT_TRUE(record_startTime_two);
  ASSERT_TRUE(record_score_two);
  ASSERT_TRUE(record_hadRecentInput_two);

  // Verify that layout shift two happened at least 1 second after
  // layout shift one, and it has bigger score than layout shift one.
  EXPECT_GT(*record_startTime_two, *record_startTime_one + 1000);
  EXPECT_GT(*record_score_two, *record_score_one);

  // Verify the first layout shift doesn't have recent input, while the second
  // layout shift has.
  ASSERT_FALSE(*record_hadRecentInput_one);
  ASSERT_TRUE(*record_hadRecentInput_two);

  // Finish session.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // Check UKM with CLS Normalization value, and it should be the same as the
  // second layout shift score.
  ExpectUKMPageLoadMetric(
      PageLoad::
          kLayoutInstability_MaxCumulativeShiftScore_SessionWindow_Gap1000ms_Max5000msName,
      page_load_metrics::LayoutShiftUkmValue(*record_score_one));

  // Check normal CLS UKM.
  ExpectUKMPageLoadMetric(PageLoad::kLayoutInstability_CumulativeShiftScoreName,
                          page_load_metrics::
                          LayoutShiftUkmValue(*record_score_one));

  // Check UMA with the second layout shift score.
  auto samples = histogram_tester().GetAllSamples(
      "PageLoad.LayoutInstability.CumulativeShiftScore");
  EXPECT_EQ(1ul, samples.size());
  EXPECT_EQ(
      samples[0],
      Bucket(page_load_metrics::LayoutShiftUmaValue(*record_score_one), 1));
}
