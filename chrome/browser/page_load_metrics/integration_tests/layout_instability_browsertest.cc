// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/integration_tests/metric_integration_test.h"

#include "base/test/trace_event_analyzer.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "content/public/test/browser_test.h"
#include "services/metrics/public/cpp/ukm_builders.h"

using absl::optional;
using base::Bucket;
using base::Value;
using trace_analyzer::Query;
using trace_analyzer::TraceAnalyzer;
using trace_analyzer::TraceEventVector;
using ukm::builders::PageLoad;

class LayoutInstabilityTest : public MetricIntegrationTest {
 protected:
  void RunWPT(const std::string& test_file, bool trace_only = false);

 private:
  double CheckTraceData(Value::List& expectations, TraceAnalyzer&);
  void CheckSources(const Value::List& expected_sources,
                    const Value::List& trace_sources);
};

void LayoutInstabilityTest::RunWPT(const std::string& test_file,
                                   bool trace_only) {
  Start();
  StartTracing({"loading", TRACE_DISABLED_BY_DEFAULT("layout_shift.debug")});
  Load("/layout-instability/" + test_file);

  // Check web perf API.
  base::Value expectations =
      EvalJs(web_contents(), "cls_run_tests").ExtractList();

  // Check trace data.
  double final_score =
      CheckTraceData(expectations.GetList(), *StopTracingAndAnalyze());
  if (trace_only)
    return;

  // Finish session.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // Check UKM.
  ExpectUKMPageLoadMetric(PageLoad::kLayoutInstability_CumulativeShiftScoreName,
                          page_load_metrics::LayoutShiftUkmValue(final_score));

  // Check UMA.
  auto samples = histogram_tester().GetAllSamples(
      "PageLoad.LayoutInstability.CumulativeShiftScore");
  EXPECT_EQ(1ul, samples.size());
  EXPECT_EQ(samples[0],
            Bucket(page_load_metrics::LayoutShiftUmaValue(final_score), 1));
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
      EXPECT_EQ(*score, data.FindDouble("score"));
      final_score = *score;
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

IN_PROC_BROWSER_TEST_F(LayoutInstabilityTest, SimpleBlockMovement) {
  RunWPT("simple-block-movement.html");
}

IN_PROC_BROWSER_TEST_F(LayoutInstabilityTest, Sources_Enclosure) {
  RunWPT("sources-enclosure.html", true);
}

IN_PROC_BROWSER_TEST_F(LayoutInstabilityTest, Sources_MaxImpact) {
  RunWPT("sources-maximpact.html", true);
}
