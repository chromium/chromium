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
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "services/metrics/public/cpp/ukm_builders.h"

using base::Bucket;
using base::Value;
using std::optional;
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
              uint64_t num_layout_shifts = 1,
              bool check_UKM_UMA_metrics = false);
  double CheckTraceData(Value::List& expectations, TraceAnalyzer&);
  void CheckSources(const Value::List& expected_sources,
                    const Value::List& trace_sources);
  void CheckUKMAndUMAMetrics(double expect_score);
  std::pair<double, double> GetCLSFromList(Value::List& entry_records_list);
  void CheckUKMAndUMAMetricsWithValues(double totalCls, double normalizedCls);

  // Perform hit test and frame waiter to ensure the frame is ready.
  void WaitForFrameReady();
};

void LayoutInstabilityTest::RunWPT(const std::string& test_file,
                                   ShiftFrame frame,
                                   uint64_t num_layout_shifts,
                                   bool check_UKM_UMA_metrics) {
  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      web_contents());
  // Wait for the layout shift in the desired frame.
  waiter->AddPageLayoutShiftExpectation(frame, num_layout_shifts);

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

  waiter->Wait();
  // Finish session.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // It compares the trace data of layout shift events with |expectations| and
  // computes a score that's used to check the UKM and UMA values below.
  double final_score = CheckTraceData(expectations, *StopTracingAndAnalyze());

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

    EXPECT_LT(i, events.size());
    Value::Dict data = events[i]->GetKnownArgAsDict("data");
    ++i;

    if (score) {
      const std::optional<double> traced_score = data.FindDouble("score");
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

std::pair<double, double> LayoutInstabilityTest::GetCLSFromList(
    Value::List& entry_records_list) {
  // cls is the normalized cls value.
  double cls = 0;

  // sessionCls is the cls score for current session window.
  double sessionCls = 0;

  // sessionDeadline is the maximum duration for current session window.
  double sessionDeadline = 0;

  // sessionGaptime is the time after one second gap of current shift.
  double sessionGaptime = 0;

  // General CLS score without normalization.
  double totalCls = 0;
  size_t entry_records_list_size = entry_records_list.size();
  for (size_t i = 0; i < entry_records_list_size; i++) {
    optional<double> record_startTime =
        entry_records_list[i].GetDict().FindDouble("startTime");
    optional<double> record_score =
        entry_records_list[i].GetDict().FindDouble("score");
    optional<double> record_hadRecentInput =
        entry_records_list[i].GetDict().FindBool("hadRecentInput");

    // Verify that the optional<double> has value.
    EXPECT_TRUE(record_startTime);
    EXPECT_TRUE(record_score);
    EXPECT_TRUE(record_hadRecentInput);

    if (*record_hadRecentInput) {
      continue;
    }

    // CLS without normalization sum all the Layout shift score
    // except the ones with hadRecentInput equal to true;
    totalCls += *record_score;
    if (sessionCls != 0 && *record_startTime < sessionDeadline &&
        *record_startTime < sessionGaptime) {
      sessionCls += *record_score;
    } else {
      sessionCls = *record_score;
      sessionDeadline = *record_startTime + 5000;
    }
    sessionGaptime = *record_startTime + 1000;

    if (sessionCls > cls) {
      cls = sessionCls;
    }
  }

  return {totalCls, cls};
}

void LayoutInstabilityTest::CheckUKMAndUMAMetricsWithValues(
    double totalCls,
    double normalizedCls) {
  // Check UKM with CLS Normalization value, and it should be the same as the
  // layout shift score.
  ExpectUKMPageLoadMetricNear(
      PageLoad::
          kLayoutInstability_MaxCumulativeShiftScore_SessionWindow_Gap1000ms_Max5000msName,
      page_load_metrics::LayoutShiftUkmValue(normalizedCls), 1);

  // Check total CLS in UKM.
  ExpectUKMPageLoadMetricNear(
      PageLoad::kLayoutInstability_CumulativeShiftScoreName,
      page_load_metrics::LayoutShiftUkmValue(totalCls), 1);

  // Check UMA with the layout shift score.
  auto samples = histogram_tester().GetAllSamples(
      "PageLoad.LayoutInstability.CumulativeShiftScore");
  EXPECT_EQ(1ul, samples.size());
  EXPECT_EQ(samples[0],
            Bucket(page_load_metrics::LayoutShiftUmaValue(normalizedCls), 1));
}

void LayoutInstabilityTest::WaitForFrameReady() {
  // We should wait for the main frame's hit-test data to be ready before
  // sending the click event below to avoid flakiness.
  content::WaitForHitTestData(web_contents()->GetPrimaryMainFrame());
  // Ensure the compositor thread is aware of the mouse events.
  content::MainThreadFrameObserver frame_observer(GetRenderWidgetHost());
  frame_observer.Wait();
}

IN_PROC_BROWSER_TEST_F(LayoutInstabilityTest, SimpleBlockMovement) {
  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      web_contents());

  waiter->AddPageLayoutShiftExpectation(ShiftFrame::LayoutShiftOnlyInMainFrame,
                                        /*num_layout_shifts=*/1);
  Start();

  // Start tracking with layout_shift related information.
  StartTracing({"loading", TRACE_DISABLED_BY_DEFAULT("layout_shift.debug")});
  Load("/simple_div_movement.html");

  // Extract the startTime and score list from watcher_entry_record.
  base::Value entry_records =
      EvalJs(web_contents(), "waitForTestFinished()").ExtractList();
  auto& entry_records_list = entry_records.GetList();

  // Verify that the entry_records_list has exactly 1 records.
  EXPECT_EQ(1ul, entry_records_list.size());

  // The first part of the expect_score contains total CLS, and the second
  // part of the expect_score contains normalized CLS.
  auto [totalCls, cls] = GetCLSFromList(entry_records_list);

  // Wait for browser to receive layout shift in browser.
  waiter->Wait();

  // Finish session.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  CheckUKMAndUMAMetricsWithValues(totalCls, cls);
}

// TODO(crbug.com/40916883): Disable this test on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_Sources_Enclosure DISABLED_Sources_Enclosure
#else
#define MAYBE_Sources_Enclosure Sources_Enclosure
#endif
IN_PROC_BROWSER_TEST_F(LayoutInstabilityTest, MAYBE_Sources_Enclosure) {
  RunWPT("sources-enclosure.html", ShiftFrame::LayoutShiftOnlyInMainFrame,
         /*num_layout_shifts=*/2);
}

// TODO(crbug.com/40250247): Fix and reenable the test.
IN_PROC_BROWSER_TEST_F(LayoutInstabilityTest, DISABLED_Sources_MaxImpact) {
  RunWPT("sources-maximpact.html");
}

// This test verifies the layout shift score in the sub-frame is recorded
// correctly in both UKM and UMA, the layout shift score in sub-frame is
// calculated by applying a sub-frame weighting factor to the total score.
IN_PROC_BROWSER_TEST_F(LayoutInstabilityTest, OOPIFSubframeWeighting) {
  RunWPT("main-frame.html", ShiftFrame::LayoutShiftOnlyInSubFrame,
         /*num_layout_shifts=*/2);

  // Check UKM.
  ExpectUKMPageLoadMetricNear(
      PageLoad::kLayoutInstability_CumulativeShiftScoreName,
      page_load_metrics::LayoutShiftUkmValue(0.03), 1);

  // Check UMA.
  ExpectUniqueUMAPageLoadMetricNear(
      "PageLoad.LayoutInstability.CumulativeShiftScore",
      page_load_metrics::LayoutShiftUmaValue(0.03));
}

IN_PROC_BROWSER_TEST_F(LayoutInstabilityTest,
                       CumulativeLayoutShift_OneSecondGap) {
  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      web_contents());

  waiter->AddPageLayoutShiftExpectation(ShiftFrame::LayoutShiftOnlyInMainFrame,
                                        /*num_layout_shifts=*/2);
  Start();

  // Start tracking with layout_shift related information.
  StartTracing({"loading", TRACE_DISABLED_BY_DEFAULT("layout_shift.debug")});
  Load("/one_second_gap.html");

  // Extract the startTime and score list from watcher_entry_record.
  base::Value entry_records =
      EvalJs(web_contents(), "waitForTestFinished()").ExtractList();
  auto& entry_records_list = entry_records.GetList();

  // Verify that the entry_records_list has exactly 2 records.
  EXPECT_EQ(2ul, entry_records_list.size());

  // The first part of the expect_score contains total CLS, and the second
  // part of the expect_score contains normalized CLS.
  auto [totalCls, cls] = GetCLSFromList(entry_records_list);

  // Verify that the two layout shifts are in different session windows.
  EXPECT_LT(cls, totalCls);

  // Wait for the second layout shift after the one second gap.
  waiter->Wait();

  // Finish session.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  CheckUKMAndUMAMetricsWithValues(totalCls, cls);
}

// TODO(crbug.com/40940689): Disable this test on Win10
#if BUILDFLAG(IS_WIN)
#define MAYBE_CumulativeLayoutShift_hadRecentInput \
  DISABLED_CumulativeLayoutShift_hadRecentInput
#else
#define MAYBE_CumulativeLayoutShift_hadRecentInput \
  CumulativeLayoutShift_hadRecentInput
#endif
IN_PROC_BROWSER_TEST_F(LayoutInstabilityTest,
                       MAYBE_CumulativeLayoutShift_hadRecentInput) {
  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      web_contents());

  waiter->AddPageLayoutShiftExpectation(ShiftFrame::LayoutShiftOnlyInMainFrame,
                                        /*num_layout_shifts=*/1);
  Start();

  // Start tracking with layout_shift related information.
  StartTracing({"loading", TRACE_DISABLED_BY_DEFAULT("layout_shift.debug")});
  Load("/had_recent_input.html");

  // Wait for hit test and and frame to be ready.
  WaitForFrameReady();

  // Click on shifter to trigger the handler, so we can have a layout shift
  // with hadRecentInput equals to true.
  content::SimulateMouseClickOrTapElementWithId(web_contents(), "shifter");

  // Extract the startTime and score list from watcher_entry_record.
  base::Value entry_records =
      EvalJs(web_contents(), "waitForTestFinished()").ExtractList();
  auto& entry_records_list = entry_records.GetList();

  // Verify that the entry_records_list has exactly 2 records.
  EXPECT_EQ(2ul, entry_records_list.size());

  optional<double> record_hadRecentInput =
      entry_records_list[1].GetDict().FindBool("hadRecentInput");

  // Verify that the optional<double> has value.
  EXPECT_TRUE(record_hadRecentInput);
  ASSERT_TRUE(*record_hadRecentInput);

  // The first part of the expect_score contains total CLS, and the second
  // part of the expect_score contains normalized CLS.
  auto [totalCls, cls] = GetCLSFromList(entry_records_list);

  // Wait for the second layout shift after the one second gap.
  waiter->Wait();

  // Finish session.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  CheckUKMAndUMAMetricsWithValues(totalCls, cls);
}
