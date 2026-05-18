// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/tracing/trace_event_analyzer.h"
#include "base/values.h"
#include "cc/base/switches.h"
#include "chrome/browser/page_load_metrics/integration_tests/metric_integration_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_load_metrics/browser/features.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/browser/page_load_type.h"
#include "components/ukm/gmock_matchers.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/prerender_test_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/performance/largest_contentful_paint_type.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/public/mojom/navigation/navigation_type_for_navigation_api.mojom-shared.h"
#include "ui/gfx/geometry/point_conversions.h"

namespace page_load_metrics {
namespace {
using testing::EndsWith;
using testing::UnorderedElementsAre;
using ukm::TestUkmRecorder;
using ukm::UkmSource;
using ukm::builders::HistoryNavigation;
using ukm::builders::PageLoad;
using ukm::builders::PrerenderPageLoad;
using ukm::builders::SoftNavigation;
using ukm::mojom::UkmEntry;
using ukm::mojom::UkmEntryPtr;
using ukm::testing::HasMetric;
using ukm::testing::HasMetricWithValue;

std::map<int64_t, double> GetSoftNavigationMetrics(
    const TestUkmRecorder& ukm_recorder,
    std::string_view metric_name) {
  std::map<int64_t, double> source_id_to_metric_name;
  for (const UkmEntry* entry :
       ukm_recorder.GetEntriesByName(SoftNavigation::kEntryName)) {
    const UkmSource* source =
        ukm_recorder.GetSourceForSourceId(entry->source_id);
    if (MetricIntegrationTest::IsWebUISource(source)) {
      continue;
    }
    if (auto* rs = ukm_recorder.GetEntryMetric(entry, metric_name)) {
      source_id_to_metric_name[entry->source_id] = *rs;
    }
  }
  return source_id_to_metric_name;
}

// Similar to UkmRecorder::GetMergedEntriesByName(), but returned map is keyed
// by source URL.
std::map<GURL, UkmEntryPtr> GetMergedUkmEntries(
    const TestUkmRecorder& ukm_recorder,
    const std::string& entry_name) {
  auto entries = ukm_recorder.GetMergedEntriesByName(entry_name);
  std::map<GURL, UkmEntryPtr> result;
  for (auto& kv : entries) {
    const UkmEntry* entry = kv.second.get();
    const UkmSource* source =
        ukm_recorder.GetSourceForSourceId(entry->source_id);
    if (!source) {
      continue;
    }
    EXPECT_TRUE(source->url().is_valid());
    EXPECT_TRUE(result.emplace(source->url(), std::move(kv.second)).second);
  }
  return result;
}

void WaitForFrameReady(content::WebContents* web_contents) {
  // We should wait for the main frame's hit-test data to be ready before
  // sending the click event below to avoid flakiness.
  content::WaitForHitTestData(web_contents->GetPrimaryMainFrame());
  // Ensure the compositor thread is aware of the mouse events.
  content::MainThreadFrameObserver frame_observer(
      web_contents->GetRenderWidgetHostView()->GetRenderWidgetHost());
  frame_observer.Wait();
}

void SimulateUserInteractionAndWait(content::WebContents* web_contents,
                                    PageLoadMetricsTestWaiter* waiter,
                                    int expected_num_interactions,
                                    std::string_view element_id) {
  waiter->AddNumInteractionsExpectation(expected_num_interactions);

  EXPECT_TRUE(ExecJs(web_contents, "registerEventListeners(); "));

  WaitForFrameReady(web_contents);

  // Simulate a click on button which has default browser-driven presentation.
  content::SimulateMouseClickOrTapElementWithId(web_contents, element_id);

  EXPECT_TRUE(ExecJs(web_contents, "waitForClick();"));

  waiter->Wait();
}

void TriggerSoftNavigationAndWait(content::WebContents* web_contents,
                                  PageLoadMetricsTestWaiter* waiter,
                                  int expected_soft_nav_count,
                                  std::string_view element_id) {
  waiter->AddSoftNavigationCountExpectation(expected_soft_nav_count);
  waiter->AddSoftNavigationLargestContentfulPaintExpectation(
      expected_soft_nav_count);
  WaitForFrameReady(web_contents);
  content::SimulateMouseClickOrTapElementWithId(web_contents, element_id);

  waiter->Wait();
}

std::string JsSnippetTriggerLayoutShift(int width, int height) {
  return base::StringPrintf(R"(
    (() => {
      const div = document.createElement('div');
      div.style = 'width: %dpx; height: %dpx; ' +
                  'border: 1px; box-sizing: border-box;';
      document.getElementById('content').firstChild.before(div);
    })();
    )",
                            width, height);
}

// Triggers a layout shift by injecting a DOM element of the specified
// dimensions and waits for the layout shift metric to be observed.
void TriggerLayoutShiftAndWait(content::WebContents* web_contents,
                               PageLoadMetricsTestWaiter* waiter,
                               int width,
                               int height) {
  waiter->AddPageLayoutShiftExpectation(
      PageLoadMetricsTestWaiter::ShiftFrame::LayoutShiftOnlyInMainFrame,
      /*num_layout_shifts=*/1);
  std::ignore =
      EvalJs(web_contents, JsSnippetTriggerLayoutShift(width, height));
  waiter->Wait();
}

// Returns soft navigation data with its corresponding soft LCPs. While
// soft navigations are ordered by navigationId, their soft LCP is
// associated by interactionId.
std::string JsSnippetGetPerformanceEntries() {
  return R"(
    (() => {
      const byNavigationId = new Map();
      const byInteractionId = new Map();
      {
        const observer = new PerformanceObserver(() => {});
        observer.observe({type: 'soft-navigation',
                          buffered: true});
        for (const record of observer.takeRecords()) {
          const obj = {
            softNavigation: {
              navigationId: record.navigationId,
              startTime: record.startTime,
              interactionId: record.interactionId,
            },
          };
          byNavigationId.set(record.navigationId, obj);
          byInteractionId.set(record.interactionId, obj);
        }
      }
      {
        const observer = new PerformanceObserver(() => {});
        observer.observe({type: 'interaction-contentful-paint',
                          buffered: true});
        for (const record of observer.takeRecords()) {
          const obj = byInteractionId.get(record.interactionId);
          if (obj) {
            obj.interactionContentfulPaint = {
              renderTime: record.renderTime,
            }
          }
        }
      }
      return Array.from(byNavigationId.values());
    })();
    )";
}

// Returns web-exposed layout shift performance entries observed for the page,
// including their timing, score, input status, and associated navigationId.
std::string JsSnippetGetLayoutShiftPerformanceEntries() {
  return R"(
    (() => {
      const observer = new PerformanceObserver(() => {});
      observer.observe({type: 'layout-shift', buffered: true});
      return observer.takeRecords().map((entry) => ({
        startTime: entry.startTime,
        value: entry.value,
        hadRecentInput: entry.hadRecentInput,
        navigationId: entry.navigationId,
      }));
    })();
    )";
}

class SoftNavigationTest : public MetricIntegrationTest,
                           public testing::WithParamInterface<bool> {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnableGpuBenchmarking);
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
    // Set a default window size for consistency.
    command_line->AppendSwitchASCII(switches::kWindowSize, "800,600");

    std::vector<base::test::FeatureRef> enabled_feature_list;
    if (GetParam()) {
      // These features enable the JavaScript API which exposes soft navigations
      // to the web; that is, 'soft-navigation', 'interaction-contentful-paint'
      // entries available PerformanceObserver and the navigationId field on the
      // Performance Entry elements. Testing with these features enabled allows
      // us to compare the values with the UKM collection; disabling them allows
      // us to ensure the UKM collection happens regardless of the web API.
      enabled_feature_list.push_back(blink::features::kNavigationId);
      enabled_feature_list.push_back(
          blink::features::kSoftNavigationHeuristics);
    }
    feature_list_.InitWithFeatures(enabled_feature_list, {} /*disabled*/);
  }

  bool VerifyInpUkmAndTraceData(trace_analyzer::TraceAnalyzer& analyzer) {
    trace_analyzer::TraceEventVector events;

    // Extract the events by name EventTiming.
    analyzer.FindEvents(trace_analyzer::Query::EventNameIs("EventTiming"),
                        &events);

    // max_duration is used to record the maximum duration out of
    // pointerdown, pointerup and click.
    int max_duration = ExtractMaxInteractionDurationFromTrace(events);

    // Extract the UKM INP values from ukm_recorder.
    int64_t INP_numOfInteraction_value;
    int64_t INP_worst_value;
    int64_t INP_98th_value;

    bool extract_num_of_interaction = ExtractUKMPageLoadMetric(
        PageLoad::kInteractiveTiming_NumInteractionsName,
        &INP_numOfInteraction_value);
    bool extract_worst_interaction = ExtractUKMPageLoadMetric(
        PageLoad::
            kInteractiveTiming_WorstUserInteractionLatency_MaxEventDurationName,
        &INP_worst_value);
    bool extract_98th_interaction = ExtractUKMPageLoadMetric(
        PageLoad::
            kInteractiveTiming_UserInteractionLatency_HighPercentile2_MaxEventDurationName,
        &INP_98th_value);

    // Ensure the UKM contains all three values.
    if (!extract_num_of_interaction || !extract_worst_interaction ||
        !extract_98th_interaction) {
      return false;
    }

    // Since the INP value takes 98th percentile all interactions,
    // the 98th percentile and 100th percentile should be the same when
    // we have less than 50 interactions.
    EXPECT_EQ(INP_98th_value, INP_worst_value);

    // The duration value in trace data is rounded to 8ms
    // which means the value before rounding should be in the
    // range of plus and minus 8ms of the rounded value.
    EXPECT_GE(max_duration, INP_98th_value - 8);
    EXPECT_LE(max_duration, INP_98th_value + 8);

    // Verify that there are 5 interaction in UKM. They are
    // click to simulate user interaction,
    // click to trigger soft nav,
    // click to simulate user interaction,
    // click to trigger soft nav,
    // click to simulate user interaction in this order.
    // The first 2 is user interactions before soft nav. The next 2 is user
    // interactions during the 1st soft nav. The last 1 is that of the 2nd
    // soft nav.
    EXPECT_EQ(INP_numOfInteraction_value, 5);

    // Verify there are 2 soft nav num_of_interactions and the 1st is 2 and the
    // 2nd is 1.
    auto soft_nav_source_id_to_num_of_interactions = GetSoftNavigationMetrics(
        ukm_recorder(), SoftNavigation::kInteractiveTiming_NumInteractionsName);
    EXPECT_EQ(soft_nav_source_id_to_num_of_interactions.size(), 2u);
    EXPECT_EQ(soft_nav_source_id_to_num_of_interactions.begin()->second, 2);
    EXPECT_EQ(
        std::next(soft_nav_source_id_to_num_of_interactions.begin())->second,
        1);

    // Verify there are 2 soft nav offsets; the first could be 1 or two; the
    // second should be one.
    auto soft_nav_source_id_to_offsets = GetSoftNavigationMetrics(
        ukm_recorder(), SoftNavigation::kInteractiveTiming_INPOffsetName);
    EXPECT_EQ(soft_nav_source_id_to_offsets.size(), 2u);
    EXPECT_GT(soft_nav_source_id_to_offsets.begin()->second, 0);
    EXPECT_LT(soft_nav_source_id_to_offsets.begin()->second, 3);
    EXPECT_EQ(std::next(soft_nav_source_id_to_offsets.begin())->second, 1);

    // Verify there are 2 soft nav times.
    auto soft_nav_source_id_to_times = GetSoftNavigationMetrics(
        ukm_recorder(), SoftNavigation::kInteractiveTiming_INPOffsetName);
    EXPECT_EQ(soft_nav_source_id_to_times.size(), 2u);

    // Verify that num_of_interactions before soft nav is 2.
    int64_t INP_numOfInteraction_value_before_soft_nav;
    bool extract_num_of_interaction_before_soft_nav = ExtractUKMPageLoadMetric(
        PageLoad::kInteractiveTimingBeforeSoftNavigation_NumInteractionsName,
        &INP_numOfInteraction_value_before_soft_nav);
    EXPECT_TRUE(extract_num_of_interaction_before_soft_nav);

    EXPECT_EQ(INP_numOfInteraction_value_before_soft_nav, 2);

    // Verify that INP before soft nav exists.
    int64_t INP_before_soft_nav;
    bool extract_INP_before_soft_nav = ExtractUKMPageLoadMetric(
        PageLoad::
            kInteractiveTimingBeforeSoftNavigation_UserInteractionLatency_HighPercentile2_MaxEventDurationName,
        &INP_before_soft_nav);
    EXPECT_TRUE(extract_INP_before_soft_nav);

    // Verify that 2 soft nav INP exist.
    auto soft_nav_source_id_to_INP = GetSoftNavigationMetrics(
        ukm_recorder(),
        SoftNavigation::
            kInteractiveTiming_UserInteractionLatency_HighPercentile2_MaxEventDurationName);
    EXPECT_EQ(soft_nav_source_id_to_INP.size(), 2u);

    return true;
  }

  int64_t ExtractMaxInteractionDurationFromTrace(
      trace_analyzer::TraceEventVector events) {
    int64_t max_duration = 0;
    for (const trace_analyzer::TraceEvent* traceEvent : events) {
      // If the traceEvent doesn't contain args data, it is not
      // one of pointerdown, pointerup and click.
      if (!traceEvent->HasDictArg("data")) {
        continue;
      }
      base::DictValue data = traceEvent->GetKnownArgAsDict("data");

      // INP only consider the events with interactionID greater than 0.
      if (data.FindInt("interactionId").value_or(-1) <= 0) {
        continue;
      }
      std::string* event_name = data.FindString("type");
      EXPECT_TRUE(event_name != nullptr);
      if (event_name == nullptr) {
        continue;
      }
      // Ensure the max_duration carries the largest duration out of
      // pointerdown, pointerup and click.
      if (*event_name != "pointerdown" && *event_name != "pointerup" &&
          *event_name != "click") {
        continue;
      }
      std::optional<double> duration = data.FindDouble("duration");
      EXPECT_TRUE(duration.has_value());
      EXPECT_TRUE(std::isfinite(duration.value()));
      int64_t duration_as_int = static_cast<int64_t>(duration.value());
      if (duration_as_int > max_duration) {
        max_duration = duration_as_int;
      }
    }
    return max_duration;
  }

  void VerifySoftNavigationCount(int64_t expected_count) {
    int64_t soft_navigation_count;
    bool extract_soft_navigation_count = ExtractUKMPageLoadMetric(
        PageLoad::kSoftNavigationCountName, &soft_navigation_count);
    EXPECT_TRUE(extract_soft_navigation_count);
    EXPECT_EQ(soft_navigation_count, expected_count);
  }

  void VerifySoftNavIdsAndSoftLcpStartTimes(
      const base::ListValue& performance_entries,
      uint32_t expected_soft_nav_count) {
    bool soft_nav_heuristics_enabled = GetParam();

    // Soft navigation start times.
    auto source_id_to_start_time = GetSoftNavigationMetrics(
        ukm_recorder(), SoftNavigation::kStartTimeName);
    EXPECT_EQ(source_id_to_start_time.size(), expected_soft_nav_count);

    // Soft navigation LCP.
    auto source_id_to_lcp = GetSoftNavigationMetrics(
        ukm_recorder(),
        SoftNavigation::kPaintTiming_LargestContentfulPaintName);
    EXPECT_EQ(source_id_to_lcp.size(), expected_soft_nav_count);

    std::vector<double> soft_nav_start_times;
    for (const auto& [source_id, start_time] : source_id_to_start_time) {
      soft_nav_start_times.push_back(start_time);
    }

    // Verify that the soft navigation start times are sorted and unique.
    EXPECT_EQ(soft_nav_start_times.size(), expected_soft_nav_count);
    EXPECT_TRUE(std::is_sorted(soft_nav_start_times.begin(),
                               soft_nav_start_times.end()));
    auto it = std::adjacent_find(soft_nav_start_times.begin(),
                                 soft_nav_start_times.end());
    if (it != soft_nav_start_times.end()) {
      FAIL() << "start times are not unique: " << *it << " at index "
             << std::distance(soft_nav_start_times.begin(), it);
    }
    EXPECT_EQ(it, soft_nav_start_times.end());
    auto last =
        std::unique(soft_nav_start_times.begin(), soft_nav_start_times.end());
    EXPECT_EQ(last, soft_nav_start_times.end());

    EXPECT_EQ(source_id_to_lcp.size(), expected_soft_nav_count);
    std::vector<double> soft_nav_lcp;
    for (const auto& [source_id, lcp] : source_id_to_lcp) {
      soft_nav_lcp.push_back(lcp);
    }

    // If the SoftNavigationHeuristics flag is enabled, we verify exact values
    // in UKM against the web exposed values.
    if (soft_nav_heuristics_enabled) {
      EXPECT_EQ(performance_entries.size(), expected_soft_nav_count);
      ASSERT_EQ(performance_entries.size(), soft_nav_lcp.size());
      ASSERT_EQ(performance_entries.size(), soft_nav_start_times.size());
      for (uint32_t i = 0; i < performance_entries.size(); ++i) {
        SCOPED_TRACE(base::StringPrintf("performance_entries[%d]", i));
        const base::DictValue& timing = performance_entries[i].GetDict();
        double expected_lcp = soft_nav_lcp[i] + soft_nav_start_times[i];
        EXPECT_NEAR(*timing.FindDoubleByDottedPath(
                        "interactionContentfulPaint.renderTime"),
                    expected_lcp, 6);
        EXPECT_NEAR(*timing.FindDoubleByDottedPath("softNavigation.startTime"),
                    soft_nav_start_times[i], 6);
      }
    }

    // Also check the hard navigation LCP, and the LCP before the first soft
    // navigation.
    int64_t lcp;
    bool extract_lcp = ExtractUKMPageLoadMetric(
        PageLoad::kPaintTiming_NavigationToLargestContentfulPaint2Name, &lcp);
    EXPECT_TRUE(extract_lcp);
    EXPECT_GT(lcp, 0);
    // The hard navigation LCP should be smaller than the first soft navigation
    // start time, since the soft nav implies that there was an interaction.
    EXPECT_LT(lcp, soft_nav_start_times[0]);
    // The LCP before the first soft navigation should be the same as the hard
    // navigation LCP. The only difference is that it gets recorded as soon as
    // the first soft navigation is detected, and therefore it's less
    // susceptible to be missing, as 'regular' LCP is only recorded once the
    // page load is complete.
    int64_t lcp_before_first_soft_nav;
    bool extract_lcp_before_first_soft_nav = ExtractUKMPageLoadMetric(
        PageLoad::
            kPaintTimingBeforeSoftNavigation_NavigationToLargestContentfulPaint2Name,  // NOLINT
        &lcp_before_first_soft_nav);
    EXPECT_TRUE(extract_lcp_before_first_soft_nav);
    EXPECT_EQ(lcp_before_first_soft_nav, lcp);

    histogram_tester().ExpectUniqueSample(
        "PageLoad.BeforeSoftNavigation.LargestContentfulPaint2",
        lcp_before_first_soft_nav, 1);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// This test focuses on measuring the image LCP of a soft navigation in UKM.
IN_PROC_BROWSER_TEST_P(SoftNavigationTest, ImageLargestContentfulPaint) {
  // Start the test, load soft_navigation_basics.html and wait for
  // load, fcp, and lcp to be observed.
  Start();
  PageLoadMetricsTestWaiter waiter(web_contents());
  waiter.AddPageExpectation(PageLoadMetricsTestWaiter::TimingField::kLoadEvent);
  waiter.AddPageExpectation(
      PageLoadMetricsTestWaiter::TimingField::kFirstContentfulPaint);
  waiter.AddPageExpectation(
      PageLoadMetricsTestWaiter::TimingField::kLargestContentfulPaint);
  Load("/soft_navigation_basics.html#image");
  waiter.Wait();

  // 1st soft navigation: click on the next page button and wait for soft
  // navigation count and image lcp.
  TriggerSoftNavigationAndWait(web_contents(), &waiter, 1,
                               /*element_id=*/"next-page");

  // 2nd soft navigation: click on the next page button and wait for soft
  // navigation count and image lcp.
  TriggerSoftNavigationAndWait(web_contents(), &waiter, 2,
                               /*element_id=*/"next-page");

  base::ListValue performance_entries;
  if (GetParam()) {
    performance_entries = EvalJs(web_contents()->GetPrimaryMainFrame(),
                                 JsSnippetGetPerformanceEntries())
                              .TakeValue()
                              .TakeList();
    EXPECT_EQ(performance_entries.size(), 2ul);
  }

  // Navigate to about:blank (untracked) to ensure all UKM are recorded.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  VerifySoftNavigationCount(/*expected_count=*/2);

  VerifySoftNavIdsAndSoftLcpStartTimes(performance_entries,
                                       /*expected_soft_nav_count=*/2);

  // Verify 2 LCP discovery time timings are reported.
  auto source_id_to_lcp_discovery_time = GetSoftNavigationMetrics(
      ukm_recorder(),
      SoftNavigation::
          kPaintTiming_LargestContentfulPaintImageDiscoveryTimeName);

  EXPECT_EQ(source_id_to_lcp_discovery_time.size(), 2u);

  // Verify 2 LCP load start timings are reported.
  auto source_id_to_lcp_image_load_start = GetSoftNavigationMetrics(
      ukm_recorder(),
      SoftNavigation::kPaintTiming_LargestContentfulPaintImageLoadStartName);

  EXPECT_EQ(source_id_to_lcp_image_load_start.size(), 2u);

  // Verify 2 LCP load end timings are reported.
  auto source_id_to_lcp_image_load_end = GetSoftNavigationMetrics(
      ukm_recorder(),
      SoftNavigation::kPaintTiming_LargestContentfulPaintImageLoadEndName);

  EXPECT_EQ(source_id_to_lcp_image_load_end.size(), 2u);

  // Verify LCP types.
  auto source_id_to_lcp_type = GetSoftNavigationMetrics(
      ukm_recorder(),
      SoftNavigation::kPaintTiming_LargestContentfulPaintTypeName);

  EXPECT_EQ(source_id_to_lcp_type.size(), 2u);

  auto flag_set =
      static_cast<int64_t>(blink::LargestContentfulPaintType::kImage |
                           blink::LargestContentfulPaintType::kPNG);
  auto flag_set_with_mouseover =  // Allow mouseover to avoid flakiness.
      flag_set |
      static_cast<int64_t>(blink::LargestContentfulPaintType::kAfterMouseover);
  auto lcp_type_1 = source_id_to_lcp_type.cbegin()->second;
  auto lcp_type_2 = std::next(source_id_to_lcp_type.cbegin())->second;

  EXPECT_TRUE(lcp_type_1 == flag_set || lcp_type_1 == flag_set_with_mouseover);
  EXPECT_TRUE(lcp_type_2 == flag_set || lcp_type_2 == flag_set_with_mouseover);

  // Verify LCP BPP.
  auto source_id_to_lcp_bpp = GetSoftNavigationMetrics(
      ukm_recorder(),
      SoftNavigation::kPaintTiming_LargestContentfulPaintBPPName);
  // Bpp value is fixed for a given image.
  EXPECT_EQ(source_id_to_lcp_bpp.cbegin()->second, 23u);

  EXPECT_EQ(std::next(source_id_to_lcp_bpp.cbegin())->second, 23u);

  // Verify LCP priority.
  auto source_id_to_lcp_request_priority = GetSoftNavigationMetrics(
      ukm_recorder(),
      SoftNavigation::kPaintTiming_LargestContentfulPaintRequestPriorityName);

  // https://source.chromium.org/chromium/chromium/src/+/main:net/base/request_priority.h
  // 4 is MEDIUM request priority.
  EXPECT_EQ(source_id_to_lcp_request_priority.cbegin()->second, 4u);

  EXPECT_EQ(std::next(source_id_to_lcp_request_priority.cbegin())->second, 4u);
}

// This test focuses on measuring the text LCP of a soft navigation in UKM.
IN_PROC_BROWSER_TEST_P(SoftNavigationTest, TextLargestContentfulPaint) {
  // Start the test, load soft_navigation_basics.html and wait for
  // load, fcp, and lcp to be observed.
  Start();
  PageLoadMetricsTestWaiter waiter(web_contents());
  waiter.AddPageExpectation(PageLoadMetricsTestWaiter::TimingField::kLoadEvent);
  waiter.AddPageExpectation(
      PageLoadMetricsTestWaiter::TimingField::kFirstContentfulPaint);
  waiter.AddPageExpectation(
      PageLoadMetricsTestWaiter::TimingField::kLargestContentfulPaint);
  Load("/soft_navigation_basics.html#text");
  waiter.Wait();

  // 1st soft navigation: click on the next page button and wait for soft
  // navigation count and text lcp.
  TriggerSoftNavigationAndWait(web_contents(), &waiter, 1,
                               /*element_id=*/"next-page");

  // 2nd soft navigation: click on the next page button and wait for soft
  // navigation count and text lcp.
  TriggerSoftNavigationAndWait(web_contents(), &waiter, 2,
                               /*element_id=*/"next-page");

  base::ListValue performance_entries;
  if (GetParam()) {
    performance_entries = EvalJs(web_contents()->GetPrimaryMainFrame(),
                                 JsSnippetGetPerformanceEntries())
                              .TakeValue()
                              .TakeList();
    EXPECT_EQ(performance_entries.size(), 2ul);
  }

  // Navigate to about:blank (untracked) to ensure all UKM are recorded.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  VerifySoftNavigationCount(/*expected_count=*/2);

  VerifySoftNavIdsAndSoftLcpStartTimes(performance_entries,
                                       /*expected_soft_nav_count=*/2);

  // Verify LCP types.
  auto source_id_to_lcp_type = GetSoftNavigationMetrics(
      ukm_recorder(),
      SoftNavigation::kPaintTiming_LargestContentfulPaintTypeName);

  EXPECT_EQ(source_id_to_lcp_type.size(), 2u);

  auto flag_set =
      static_cast<int64_t>(blink::LargestContentfulPaintType::kText);
  auto flag_set_with_mouseover =  // Allow mouseover to avoid flakiness.
      flag_set |
      static_cast<int64_t>(blink::LargestContentfulPaintType::kAfterMouseover);
  auto lcp_type_1 = source_id_to_lcp_type.cbegin()->second;
  auto lcp_type_2 = std::next(source_id_to_lcp_type.cbegin())->second;

  EXPECT_TRUE(lcp_type_1 == flag_set || lcp_type_1 == flag_set_with_mouseover);
  EXPECT_TRUE(lcp_type_2 == flag_set || lcp_type_2 == flag_set_with_mouseover);
}

// This test verifies that we support soft navs triggered by the browser's
// back button, including recording to UKM. While other soft navs have
// underlying same-document navigations that originate in the renderer,
// the back-button starts a same-document navigation in the browser.
IN_PROC_BROWSER_TEST_P(SoftNavigationTest, BackButton) {
  // Load soft_navigation_basics.html and wait for lcp.
  Start();
  PageLoadMetricsTestWaiter waiter(web_contents());
  waiter.AddPageExpectation(
      PageLoadMetricsTestWaiter::TimingField::kLargestContentfulPaint);
  Load("/soft_navigation_basics.html#text");
  waiter.Wait();

  // This registers a popstate event handler to trigger a soft navigation when
  // the back button is clicked.
  ASSERT_TRUE(EvalJs(web_contents()->GetPrimaryMainFrame(),
                     "registerBackButtonHandler()")
                  .is_ok());

  // 1st soft navigation: click on the next page button.
  TriggerSoftNavigationAndWait(web_contents(), &waiter, 1,
                               /*element_id=*/"next-page");

  // 2nd soft navigation: click on the next page button.
  TriggerSoftNavigationAndWait(web_contents(), &waiter, 2,
                               /*element_id=*/"next-page");

  // Now we simulate the back button. However, a regular back-button click or
  // content::HistoryGoBack would trigger this intervention:
  // https://chromium.googlesource.com/chromium/src/+/main/docs/history_manipulation_intervention.md
  // Therefore we use -1 history offsets, which are like long-press back-button
  // menu clicks. This is simpler (and perhaps slightly more robust) than
  // avoiding the intervention by adding a user activation to the page.

  // 3rd soft navigation: going backwards in history.
  waiter.AddSoftNavigationCountExpectation(3);
  waiter.AddSoftNavigationLargestContentfulPaintExpectation(3);
  WaitForFrameReady(web_contents());
  ASSERT_TRUE(content::HistoryGoToOffset(web_contents(), -1));
  waiter.Wait();

  // 4th soft navigation: going backwards in history.
  waiter.AddSoftNavigationCountExpectation(4);
  waiter.AddSoftNavigationLargestContentfulPaintExpectation(4);
  WaitForFrameReady(web_contents());
  ASSERT_TRUE(content::HistoryGoToOffset(web_contents(), -1));
  waiter.Wait();

  base::ListValue soft_nav_lcp_list;
  if (GetParam()) {
    soft_nav_lcp_list = EvalJs(web_contents()->GetPrimaryMainFrame(),
                               JsSnippetGetPerformanceEntries())
                            .TakeValue()
                            .TakeList();
    EXPECT_EQ(soft_nav_lcp_list.size(), 4ul);
  }

  // Navigate to about:blank (untracked) to ensure all UKM are recorded.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  VerifySoftNavigationCount(/*expected_count=*/4);

  VerifySoftNavIdsAndSoftLcpStartTimes(soft_nav_lcp_list,
                                       /*expected_soft_nav_count=*/4);

  // Verify the UKM recorded URLs for the soft navigations.
  auto source_id_to_start_time =
      GetSoftNavigationMetrics(ukm_recorder(), SoftNavigation::kStartTimeName);
  EXPECT_EQ(source_id_to_start_time.size(), 4);

  std::vector<std::string> urls;
  int port = 0;
  for (const auto& [source_id, start_time] : source_id_to_start_time) {
    const UkmSource* s = ukm_recorder().GetSourceForSourceId(source_id);
    port = s->url().IntPort();
    urls.push_back(s->url().spec());
  }
  EXPECT_THAT(
      urls,
      testing::ElementsAre(
          absl::StrFormat(
              "http://example.com:%d/soft_navigation_basics.html?id=1#text",
              port),
          absl::StrFormat(
              "http://example.com:%d/soft_navigation_basics.html?id=2#text",
              port),
          absl::StrFormat(
              "http://example.com:%d/soft_navigation_basics.html?id=1#text",
              port),
          absl::StrFormat(
              "http://example.com:%d/soft_navigation_basics.html#text", port)));

  std::vector<int> navigation_types;
  for (const auto& [source_id, type] : GetSoftNavigationMetrics(
           ukm_recorder(), SoftNavigation::kNavigationTypeName)) {
    navigation_types.push_back(type);
  }
  EXPECT_THAT(
      navigation_types,
      testing::ElementsAre(
          static_cast<int>(blink::mojom::NavigationTypeForNavigationApi::kPush),
          static_cast<int>(blink::mojom::NavigationTypeForNavigationApi::kPush),
          static_cast<int>(
              blink::mojom::NavigationTypeForNavigationApi::kTraverse),
          static_cast<int>(
              blink::mojom::NavigationTypeForNavigationApi::kTraverse)));
  std::vector<int> page_load_types;
  for (const auto& [source_id, type] : GetSoftNavigationMetrics(
           ukm_recorder(), SoftNavigation::kPageLoadTypeName)) {
    page_load_types.push_back(type);
  }
  EXPECT_THAT(page_load_types,
              testing::ElementsAre(static_cast<int>(PageLoadType::kPageLoad),
                                   static_cast<int>(PageLoadType::kPageLoad),
                                   static_cast<int>(PageLoadType::kPageLoad),
                                   static_cast<int>(PageLoadType::kPageLoad)));
}

IN_PROC_BROWSER_TEST_P(SoftNavigationTest, NoSoftNavigation) {
  PageLoadMetricsTestWaiter waiter(web_contents());

  waiter.AddMinimumLargestContentfulPaintImageExpectation(1);

  Start();
  Load("/soft_navigation.html");

  waiter.Wait();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // Verify that no soft navigation metric is recorded.
  ExpectUkmEventNotRecorded(SoftNavigation::kEntryName);
}

INSTANTIATE_TEST_SUITE_P(All,
                         SoftNavigationTest,
                         ::testing::Values(false, true));

IN_PROC_BROWSER_TEST_P(SoftNavigationTest, INP_ClickWithPresentation) {
  // Add waiter to wait for the interaction is arrived in browser.
  PageLoadMetricsTestWaiter waiter(web_contents());

  // Start tracing to record tracing data.
  StartTracing({"devtools.timeline"});
  Start();
  Load("/soft_navigation.html");

  // Set up for soft navigation.
  EXPECT_TRUE(EvalJs(web_contents()->GetPrimaryMainFrame(), "setEventAndWait()")
                  .is_ok());

  // Add event listener to change color on click.
  EXPECT_TRUE(ExecJs(web_contents(), "addChangeColorEventListener();"));

  WaitForFrameReady(web_contents());

  SimulateUserInteractionAndWait(web_contents(), &waiter, 1,
                                 /*element_id=*/"div");

  // Trigger 1st soft nav.
  TriggerSoftNavigationAndWait(web_contents(), &waiter, 1,
                               /*element_id=*/"link");

  // Trigger a user interaction.
  SimulateUserInteractionAndWait(web_contents(), &waiter, 3,
                                 /*element_id=*/"div");

  // Trigger 2nd soft nav.
  TriggerSoftNavigationAndWait(web_contents(), &waiter, 2,
                               /*element_id=*/"link");

  // Trigger a user interaction.
  SimulateUserInteractionAndWait(web_contents(), &waiter, 5,
                                 /*element_id=*/"div");

  // Navigate to blank page to ensure the data gets flushed from renderer to
  // browser.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  auto analyzer = StopTracingAndAnalyze();

  ASSERT_TRUE(VerifyInpUkmAndTraceData(*analyzer));
}

// This test focuses on measuring the layout shift of soft navigations in UKM.
IN_PROC_BROWSER_TEST_P(SoftNavigationTest, LayoutShift) {
  // Start the test, load soft_navigation_basics.html and wait for
  // load, fcp, and lcp to be observed.
  Start();
  PageLoadMetricsTestWaiter waiter(web_contents());
  waiter.AddPageExpectation(PageLoadMetricsTestWaiter::TimingField::kLoadEvent);
  waiter.AddPageExpectation(
      PageLoadMetricsTestWaiter::TimingField::kFirstContentfulPaint);
  waiter.AddPageExpectation(
      PageLoadMetricsTestWaiter::TimingField::kLargestContentfulPaint);
  Load("/soft_navigation_basics.html#image");
  waiter.Wait();
  TriggerLayoutShiftAndWait(web_contents(), &waiter, /*width=*/500,
                            /*height=*/500);

  // 1st soft navigation: click on the next page button and wait for soft
  // navigation count and image lcp.
  TriggerSoftNavigationAndWait(web_contents(), &waiter, 1,
                               /*element_id=*/"next-page");
  // Since the soft navigation included a user interaction, we must wait at
  // least 500ms for the layout shift to be counted in the CLS computation.
  base::PlatformThread::Sleep(base::Milliseconds(500));
  // Trigger two layout shifts sequentially for the first soft navigation.
  TriggerLayoutShiftAndWait(web_contents(), &waiter, /*width=*/200,
                            /*height=*/200);
  TriggerLayoutShiftAndWait(web_contents(), &waiter, /*width=*/300,
                            /*height=*/300);

  // 2nd soft navigation: click on the next page button and wait for soft
  // navigation count and image lcp.
  TriggerSoftNavigationAndWait(web_contents(), &waiter, 2,
                               /*element_id=*/"next-page");

  // 3rd soft navigation: click on the next page button and wait for soft
  // navigation count and image lcp.
  TriggerSoftNavigationAndWait(web_contents(), &waiter, 3,
                               /*element_id=*/"next-page");

  // Trigger one layout shift for the third soft navigation.
  // Since the soft navigation included a user interaction, we must wait at
  // least 500ms for the layout shift to be counted in the CLS computation.
  base::PlatformThread::Sleep(base::Milliseconds(500));
  TriggerLayoutShiftAndWait(web_contents(), &waiter, /*width=*/400,
                            /*height=*/400);

  base::ListValue performance_entries;
  base::ListValue layout_shift_performance_entries;
  if (GetParam()) {
    performance_entries = EvalJs(web_contents()->GetPrimaryMainFrame(),
                                 JsSnippetGetPerformanceEntries())
                              .TakeValue()
                              .TakeList();
    EXPECT_EQ(performance_entries.size(), 3ul);
    layout_shift_performance_entries =
        EvalJs(web_contents()->GetPrimaryMainFrame(),
               JsSnippetGetLayoutShiftPerformanceEntries())
            .TakeValue()
            .TakeList();
    EXPECT_EQ(layout_shift_performance_entries.size(), 4ul);
  }

  // Navigate to about:blank (untracked) to ensure all UKM are recorded.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // Verify recorded UKM soft navigation entries.
  std::map<ukm::SourceId, UkmEntryPtr> soft_navigations =
      ukm_recorder().GetMergedEntriesByName(SoftNavigation::kEntryName);
  std::vector<const UkmEntry*> soft_navigation_entries;
  for (const auto& [source_id, entry] : soft_navigations) {
    soft_navigation_entries.emplace_back(entry.get());
  }
  ASSERT_EQ(3u, soft_navigation_entries.size());

  // For all three soft navigations, the page load type should be kPageLoad.
  for (size_t ii = 0; ii < soft_navigation_entries.size(); ++ii) {
    SCOPED_TRACE(base::StringPrintf("soft navigation entry %d", ii));
    EXPECT_THAT(
        soft_navigation_entries[ii],
        HasMetricWithValue(SoftNavigation::kPageLoadTypeName,
                           static_cast<int32_t>(PageLoadType::kPageLoad)));
  }

  // There's a CLS before soft nav recorded in UKM.
  int64_t cls_before_soft_nav;
  bool extract_cls_before_soft_nav = ExtractUKMPageLoadMetric(
      PageLoad::PageLoad::
          kLayoutInstabilityBeforeSoftNavigation_MaxCumulativeShiftScore_MainFrame_SessionWindow_Gap1000ms_Max5000msName,
      &cls_before_soft_nav);
  EXPECT_TRUE(extract_cls_before_soft_nav);
  EXPECT_GT(cls_before_soft_nav, 0);

  // There's a CLS for the first soft nav recorded in UKM.
  const int64_t* soft_nav1_cls = TestUkmRecorder::GetEntryMetric(
      soft_navigation_entries[0],
      SoftNavigation::
          kLayoutInstability_MaxCumulativeShiftScore_SessionWindow_Gap1000ms_Max5000msName);
  ASSERT_TRUE(soft_nav1_cls != nullptr);
  EXPECT_GT(*soft_nav1_cls, 0);

  // The second soft nav doesn't have a layout shift, so the value 0 is
  // reported.
  EXPECT_THAT(
      soft_navigation_entries[1],
      HasMetricWithValue(
          SoftNavigation::
              kLayoutInstability_MaxCumulativeShiftScore_SessionWindow_Gap1000ms_Max5000msName,
          0));

  // There's a CLS for the third soft nav recorded in UKM.
  const int64_t* soft_nav3_cls = TestUkmRecorder::GetEntryMetric(
      soft_navigation_entries[2],
      SoftNavigation::
          kLayoutInstability_MaxCumulativeShiftScore_SessionWindow_Gap1000ms_Max5000msName);
  ASSERT_TRUE(soft_nav3_cls != nullptr);
  EXPECT_GT(*soft_nav3_cls, 0);

  if (GetParam()) {
    // First basic verification of the PerformanceObserver-based layout shift
    // performance entries, then comparison with the UKM recordings.
    for (size_t ii = 0; ii < layout_shift_performance_entries.size(); ++ii) {
      SCOPED_TRACE(base::StringPrintf("layout shift entry %d", ii));
      const auto& dict = layout_shift_performance_entries[ii].GetDict();
      ASSERT_TRUE(dict.FindInt("navigationId").has_value());
      ASSERT_TRUE(dict.FindDouble("value").has_value());
      EXPECT_THAT(dict.FindBool("hadRecentInput"), testing::Optional(false));
    }

    // The first layout shift's navigationId is smaller than first soft nav's.
    EXPECT_THAT(
        layout_shift_performance_entries[0].GetDict().FindInt("navigationId"),
        testing::Lt(performance_entries[0].GetDict().FindIntByDottedPath(
            "softNavigation.navigationId")));

    // Layout shift before the first soft navigation.
    std::optional<double> layout_shift0_value =
        layout_shift_performance_entries[0].GetDict().FindDouble("value");
    EXPECT_EQ(LayoutShiftUkmValue(*layout_shift0_value), cls_before_soft_nav);

    // Verify layout shifts recorded during the first soft navigation.
    EXPECT_THAT(
        layout_shift_performance_entries[1].GetDict().FindInt("navigationId"),
        testing::Eq(performance_entries[0].GetDict().FindIntByDottedPath(
            "softNavigation.navigationId")));
    EXPECT_THAT(
        layout_shift_performance_entries[2].GetDict().FindInt("navigationId"),
        testing::Eq(performance_entries[0].GetDict().FindIntByDottedPath(
            "softNavigation.navigationId")));

    std::optional<double> layout_shift1_value =
        layout_shift_performance_entries[1].GetDict().FindDouble("value");
    std::optional<double> layout_shift2_value =
        layout_shift_performance_entries[2].GetDict().FindDouble("value");
    EXPECT_TRUE(layout_shift1_value.has_value());
    EXPECT_EQ(LayoutShiftUkmValue(*layout_shift1_value + *layout_shift2_value),
              *soft_nav1_cls);

    // Verify layout shifts recorded during the third soft navigation.
    EXPECT_THAT(
        layout_shift_performance_entries[3].GetDict().FindInt("navigationId"),
        testing::Eq(performance_entries[2].GetDict().FindIntByDottedPath(
            "softNavigation.navigationId")));

    std::optional<double> layout_shift3_value =
        layout_shift_performance_entries[3].GetDict().FindDouble("value");
    EXPECT_EQ(LayoutShiftUkmValue(*layout_shift3_value), *soft_nav3_cls);
  }
}

//
// Tests soft navigation metrics after prerender activations.
//
class SoftNavigationPrerenderTest : public MetricIntegrationTest {
 public:
  SoftNavigationPrerenderTest()
      : prerender_helper_(
            base::BindRepeating(&SoftNavigationPrerenderTest::web_contents,
                                base::Unretained(this))) {}

  void SetUp() override {
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
    MetricIntegrationTest::SetUp();
  }

 protected:
  content::test::PrerenderTestHelper prerender_helper_;
};

// Measures PrerenderPageLoad:SoftNavigationCount.
IN_PROC_BROWSER_TEST_F(SoftNavigationPrerenderTest, SoftNavigationCount) {
  // Start the test, navigate to an initial page.
  Start();

  auto initial_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  // Prerender the test page.
  GURL prerender_url =
      embedded_test_server()->GetURL("/soft_navigation_basics.html#image");
  prerender_helper_.AddPrerender(prerender_url);
  GURL prerender_url_after_softnav1 =
      embedded_test_server()->GetURL("/soft_navigation_basics.html?id=1#image");
  GURL prerender_url_after_softnav2 =
      embedded_test_server()->GetURL("/soft_navigation_basics.html?id=2#image");

  PageLoadMetricsTestWaiter waiter(web_contents());
  waiter.AddPageExpectation(PageLoadMetricsTestWaiter::TimingField::kLoadEvent);
  waiter.AddPageExpectation(
      PageLoadMetricsTestWaiter::TimingField::kFirstContentfulPaint);
  waiter.AddPageExpectation(
      PageLoadMetricsTestWaiter::TimingField::kLargestContentfulPaint);

  // Activate the prerendered page.
  prerender_helper_.NavigatePrimaryPage(prerender_url);

  waiter.Wait();

  // 1st soft navigation: click on the next page button and wait for soft
  // navigation count and image lcp.
  TriggerSoftNavigationAndWait(web_contents(), &waiter, 1,
                               /*element_id=*/"next-page");

  // Trigger a layout shift.
  // Since the soft navigation included a user interaction, we must wait at
  // least 500ms for the layout shift to be counted in the CLS computation.
  base::PlatformThread::Sleep(base::Milliseconds(500));
  TriggerLayoutShiftAndWait(web_contents(), &waiter, /*width=*/400,
                            /*height=*/400);

  // 2nd soft navigation: click on the next page button and wait for soft
  // navigation count and image lcp.
  TriggerSoftNavigationAndWait(web_contents(), &waiter, 2,
                               /*element_id=*/"next-page");

  // Before navigating to about:blank, collect the soft navigation data and
  // the layout shifts from the JavaScript PerformanceObserver API.
  base::ListValue performance_entries =
      EvalJs(web_contents()->GetPrimaryMainFrame(),
             JsSnippetGetPerformanceEntries())
          .TakeValue()
          .TakeList();
  EXPECT_EQ(performance_entries.size(), 2ul);
  base::ListValue layout_shift_performance_entries =
      EvalJs(web_contents()->GetPrimaryMainFrame(),
             JsSnippetGetLayoutShiftPerformanceEntries())
          .TakeValue()
          .TakeList();
  EXPECT_EQ(layout_shift_performance_entries.size(), 1ul);

  // Navigate to about:blank (untracked) to ensure all UKM are recorded.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // There are two PrerenderPageLoad entries:
  // 1. The prerendered page, which was activated and saw 2 soft navigations.
  // 2. The initiator page, which triggered the prerender.
  auto prerender_entries =
      GetMergedUkmEntries(ukm_recorder(), PrerenderPageLoad::kEntryName);
  EXPECT_EQ(2u, prerender_entries.size());

  const UkmEntry* prerendered_page = prerender_entries[prerender_url].get();
  ASSERT_TRUE(prerendered_page);
  EXPECT_THAT(prerendered_page,
              HasMetricWithValue(PrerenderPageLoad::kWasPrerenderedName, 1));
  EXPECT_THAT(prerendered_page,
              Not(HasMetric(PrerenderPageLoad::kTriggeredPrerenderName)));
  EXPECT_THAT(
      prerendered_page,
      HasMetricWithValue(PrerenderPageLoad::kSoftNavigationCountName, 2));
  EXPECT_THAT(prerendered_page,
              HasMetric(PrerenderPageLoad::kTiming_NavigationToActivationName));

  const UkmEntry* initiator_page = prerender_entries[initial_url].get();
  ASSERT_TRUE(initiator_page);
  EXPECT_THAT(
      initiator_page,
      HasMetricWithValue(PrerenderPageLoad::kTriggeredPrerenderName, 1));
  EXPECT_THAT(initiator_page,
              Not(HasMetric(PrerenderPageLoad::kWasPrerenderedName)));
  EXPECT_THAT(initiator_page,
              Not(HasMetric(PrerenderPageLoad::kSoftNavigationCountName)));

  //
  // Examine the two soft navigation events from the prerendered page.
  //
  std::map<ukm::SourceId, UkmEntryPtr> soft_navigations =
      ukm_recorder().GetMergedEntriesByName(SoftNavigation::kEntryName);
  std::vector<GURL> soft_navigation_urls;
  std::vector<const UkmEntry*> soft_navigation_entries;
  for (const auto& [source_id, entry] : soft_navigations) {
    soft_navigation_urls.push_back(
        ukm_recorder().GetSourceForSourceId(source_id)->url());
    soft_navigation_entries.emplace_back(entry.get());
  }
  ASSERT_THAT(soft_navigation_urls,
              UnorderedElementsAre(prerender_url_after_softnav1,
                                   prerender_url_after_softnav2));
  // For both soft navigations, the page load type should be PrerenderPageLoad.
  EXPECT_THAT(soft_navigation_entries[0],
              HasMetricWithValue(
                  SoftNavigation::kPageLoadTypeName,
                  static_cast<int32_t>(PageLoadType::kPrerenderPageLoad)));
  EXPECT_THAT(soft_navigation_entries[1],
              HasMetricWithValue(
                  SoftNavigation::kPageLoadTypeName,
                  static_cast<int32_t>(PageLoadType::kPrerenderPageLoad)));

  // The prerender activation time should be before the soft
  // navigation start times.
  const int64_t* prerender_activation = TestUkmRecorder::GetEntryMetric(
      prerendered_page, PrerenderPageLoad::kTiming_NavigationToActivationName);
  ASSERT_TRUE(prerender_activation != nullptr);
  const int64_t* soft_nav1_start_time = TestUkmRecorder::GetEntryMetric(
      soft_navigation_entries[0], SoftNavigation::kStartTimeName);
  ASSERT_TRUE(soft_nav1_start_time != nullptr);
  const int64_t* soft_nav2_start_time = TestUkmRecorder::GetEntryMetric(
      soft_navigation_entries[1], SoftNavigation::kStartTimeName);
  ASSERT_TRUE(soft_nav2_start_time != nullptr);
  EXPECT_LT(*prerender_activation, *soft_nav1_start_time);
  EXPECT_LT(*prerender_activation, *soft_nav2_start_time);

  // performance_entries is populated from the JavaScript PerformanceObserver
  // at the end of the (hard) navigation. Compare softNavigation.startTime
  // (in ms) with the UKM-based start times.
  ASSERT_EQ(2, performance_entries.size());
  EXPECT_NEAR(*performance_entries[0].GetDict().FindDoubleByDottedPath(
                  "softNavigation.startTime"),
              *soft_nav1_start_time, /*abs_error=*/3.0);
  EXPECT_NEAR(*performance_entries[1].GetDict().FindDoubleByDottedPath(
                  "softNavigation.startTime"),
              *soft_nav2_start_time, /*abs_error=*/3.0);

  // Compare the PerformanceObserver-based InteractionContentfulPaint (ICP)
  // renderTime with the UKM-based soft LCP. Since the UKM-based metric
  // is relative to the soft navigation start time, we add the two.
  const int64_t* soft_nav1_lcp = TestUkmRecorder::GetEntryMetric(
      soft_navigation_entries[0],
      SoftNavigation::kPaintTiming_LargestContentfulPaintName);
  ASSERT_TRUE(soft_nav1_lcp != nullptr);
  EXPECT_NEAR(*performance_entries[0].GetDict().FindDoubleByDottedPath(
                  "interactionContentfulPaint.renderTime"),
              *soft_nav1_start_time + *soft_nav1_lcp, /*abs_error=*/6.0);
  const int64_t* soft_nav2_lcp = TestUkmRecorder::GetEntryMetric(
      soft_navigation_entries[1],
      SoftNavigation::kPaintTiming_LargestContentfulPaintName);
  ASSERT_TRUE(soft_nav2_lcp != nullptr);
  EXPECT_NEAR(*performance_entries[1].GetDict().FindDoubleByDottedPath(
                  "interactionContentfulPaint.renderTime"),
              *soft_nav2_start_time + *soft_nav2_lcp, /*abs_error=*/6.0);

  // Compare the PerformanceObserver-based CLS with the UKM-based CLS.
  // There was one layout shift during the first soft navigation - this
  // means the navigationId of the layout shift match with the navigationId
  // of the soft navigation.
  ASSERT_EQ(1u, layout_shift_performance_entries.size());
  std::optional<int> layout_shift_navigation_id =
      layout_shift_performance_entries[0].GetDict().FindInt("navigationId");
  ASSERT_TRUE(layout_shift_navigation_id.has_value());
  EXPECT_THAT(layout_shift_navigation_id,
              testing::Eq(performance_entries[0].GetDict().FindIntByDottedPath(
                  "softNavigation.navigationId")));

  // Check that the UKM value and the PerformanceObserver based values for CLS
  // agree.
  std::optional<double> layout_shift_value =
      layout_shift_performance_entries[0].GetDict().FindDouble("value");
  EXPECT_TRUE(layout_shift_value.has_value());
  const int64_t* soft_nav1_cls = TestUkmRecorder::GetEntryMetric(
      soft_navigation_entries[0],
      SoftNavigation::
          kLayoutInstability_MaxCumulativeShiftScore_SessionWindow_Gap1000ms_Max5000msName);
  ASSERT_TRUE(soft_nav1_cls != nullptr);
  EXPECT_EQ(LayoutShiftUkmValue(*layout_shift_value), *soft_nav1_cls);

  // Check other PerformanceObserver based CLS performance entry fields.
  std::optional<double> layout_shift_start_time =
      layout_shift_performance_entries[0].GetDict().FindDouble("startTime");
  EXPECT_TRUE(layout_shift_start_time.has_value());
  std::optional<bool> layout_shift_had_recent_input =
      layout_shift_performance_entries[0].GetDict().FindBool("hadRecentInput");
  EXPECT_THAT(layout_shift_had_recent_input, testing::Optional(false));

  // Also check that for the second soft nav, for which no PerformanceObserver
  // based entries were found, a UKM CLS of 0 is recorded.
  EXPECT_THAT(
      soft_navigation_entries[1],
      HasMetricWithValue(
          SoftNavigation::
              kLayoutInstability_MaxCumulativeShiftScore_SessionWindow_Gap1000ms_Max5000msName,
          0));
}

//
// Tests soft navigation metrics after back-forward-cache restores.
//
class SoftNavigationBackForwardCacheTest : public MetricIntegrationTest {
 public:
  SoftNavigationBackForwardCacheTest() = default;
  ~SoftNavigationBackForwardCacheTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    MetricIntegrationTest::SetUpCommandLine(command_line);
    auto bfcache_features =
        content::GetDefaultEnabledBackForwardCacheFeaturesForTesting(
            {{features::kBackForwardCacheEmitZeroSamplesForKeyMetrics, {{}}}});
    bfcache_features.push_back(
        base::test::FeatureRefAndParams(blink::features::kNavigationId, {{}}));
    bfcache_features.push_back(base::test::FeatureRefAndParams(
        blink::features::kSoftNavigationHeuristics, {{}}));
    feature_list_for_bfcache_.InitWithFeaturesAndParameters(
        bfcache_features,
        content::GetDefaultDisabledBackForwardCacheFeaturesForTesting());
  }

  content::RenderFrameHost* top_frame_host() {
    return web_contents()->GetPrimaryMainFrame();
  }

 private:
  base::test::ScopedFeatureList feature_list_for_bfcache_;
};

IN_PROC_BROWSER_TEST_F(SoftNavigationBackForwardCacheTest,
                       SoftNavigationCount) {
  Start();
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/soft_navigation_basics.html#image"));
  GURL url_a_after_softnav1(embedded_test_server()->GetURL(
      "a.com", "/soft_navigation_basics.html?id=1#image"));
  GURL url_a_after_softnav2(embedded_test_server()->GetURL(
      "a.com", "/soft_navigation_basics.html?id=2#image"));
  GURL url_a_after_softnav3(embedded_test_server()->GetURL(
      "a.com", "/soft_navigation_basics.html?id=3#image"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/empty.html"));

  // Navigate to A.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url_a));
  content::RenderFrameHostWrapper rfh_a(top_frame_host());

  // Navigate to B.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // Go back to A - this will be the 1st bfcache restore.
  {
    PageLoadMetricsTestWaiter waiter(web_contents());
    waiter.AddPageBackForwardCacheRestoreExpectation(
        /*back_forward_timings_index=*/0,
        PageLoadMetricsTestWaiter::TimingField::
            kFirstPaintAfterBackForwardCacheRestore);
    web_contents()->GetController().GoBack();
    EXPECT_TRUE(WaitForLoadStop(web_contents()));
    EXPECT_TRUE(rfh_a->IsInPrimaryMainFrame());
    EXPECT_NE(rfh_a->GetLifecycleState(),
              content::RenderFrameHost::LifecycleState::kInBackForwardCache);

    waiter.Wait();
    // 1st soft navigation: click on the next page button and wait for soft
    // navigation count and image lcp.
    // The expected URL is url_a_after_softnav1 (with ?id=1).
    TriggerSoftNavigationAndWait(web_contents(), &waiter,
                                 /*expected_soft_nav_count=*/1,
                                 /*element_id=*/"next-page");

    // Trigger a layout shift.
    // Since the soft navigation included a user interaction, we must wait at
    // least 500ms for the layout shift to be counted in the CLS computation.
    base::PlatformThread::Sleep(base::Milliseconds(500));
    TriggerLayoutShiftAndWait(web_contents(), &waiter, /*width=*/400,
                              /*height=*/400);

    // 2nd soft navigation: click on the next page button and wait for soft
    // navigation count and image lcp.
    // The expected URL is url_a_after_softnav2 (with ?id=2).
    TriggerSoftNavigationAndWait(web_contents(), &waiter,
                                 /*expected_soft_nav_count=*/2,
                                 /*element_id=*/"next-page");
  }

  // Navigate to B again.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // Go back to A again - this will be the 2nd bfcache restore.
  // The expected URL is url_a_after_softnav2 (with ?id=2), just like it was
  // before we navigated to B.
  base::ListValue performance_entries;
  base::ListValue layout_shift_performance_entries;
  {
    PageLoadMetricsTestWaiter waiter(web_contents());
    waiter.AddPageBackForwardCacheRestoreExpectation(
        /*back_forward_timings_index=*/1,
        PageLoadMetricsTestWaiter::TimingField::
            kFirstPaintAfterBackForwardCacheRestore);
    web_contents()->GetController().GoBack();
    EXPECT_TRUE(WaitForLoadStop(web_contents()));
    EXPECT_TRUE(rfh_a->IsInPrimaryMainFrame());
    EXPECT_NE(rfh_a->GetLifecycleState(),
              content::RenderFrameHost::LifecycleState::kInBackForwardCache);

    waiter.Wait();
    // 1st soft navigation for this bfcache restore: click on the next page
    // button and wait for soft navigation count and image lcp.
    // The expected URL is url_a_after_softnav3 (with ?id=3).
    TriggerSoftNavigationAndWait(web_contents(), &waiter,
                                 /*expected_soft_nav_count=*/1,
                                 /*element_id=*/"next-page");

    // Trigger first layout shift for the third soft navigation.
    // Since the soft navigation included a user interaction, we must wait at
    // least 500ms for the layout shift to be counted in the CLS computation.
    base::PlatformThread::Sleep(base::Milliseconds(500));
    TriggerLayoutShiftAndWait(web_contents(), &waiter, /*width=*/200,
                              /*height=*/200);

    // Trigger second layout shift for the third soft navigation.
    TriggerLayoutShiftAndWait(web_contents(), &waiter, /*width=*/300,
                              /*height=*/300);

    performance_entries = EvalJs(web_contents()->GetPrimaryMainFrame(),
                                 JsSnippetGetPerformanceEntries())
                              .TakeValue()
                              .TakeList();
    EXPECT_EQ(performance_entries.size(), 3ul);
    layout_shift_performance_entries =
        EvalJs(web_contents()->GetPrimaryMainFrame(),
               JsSnippetGetLayoutShiftPerformanceEntries())
            .TakeValue()
            .TakeList();
    EXPECT_EQ(layout_shift_performance_entries.size(), 3ul);

    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  }

  //
  // We recorded UKMs for two bfcache restores, for url_a and
  // url_a_after_softnav2.
  //
  auto bfcache_restores =
      GetMergedUkmEntries(ukm_recorder(), HistoryNavigation::kEntryName);
  std::vector<GURL> bfcache_restore_urls;
  for (const auto& entry : bfcache_restores) {
    bfcache_restore_urls.push_back(entry.first);
  }
  // url_a is the URL from the 1st bfcache restore, which means it's
  // the URL that we originally (hard) navigated to.
  // Note: This EndsWith assertion is just for readability of the test.
  EXPECT_THAT(url_a.spec(), EndsWith("/soft_navigation_basics.html#image"));
  // url_a_after_softnavs is the URL from the 2nd bfcache restore, which
  // means it's the URL that we soft navigated to after the 1st bfcache
  // restore. Since there were 2 soft navigations before the 2nd bfcache
  // restore, the id is 2.
  // Note: This EndsWith assertion is just for readability of the test.
  EXPECT_THAT(url_a_after_softnav2.spec(),
              EndsWith("/soft_navigation_basics.html?id=2#image"));
  EXPECT_THAT(bfcache_restore_urls,
              UnorderedElementsAre(url_a, url_a_after_softnav2));

  // The first bfcache restore saw first paint, followed by two soft
  // navigations.
  EXPECT_THAT(
      bfcache_restores[url_a].get(),
      HasMetric(HistoryNavigation::
                    kNavigationToFirstPaintAfterBackForwardCacheRestoreName));
  EXPECT_THAT(
      bfcache_restores[url_a].get(),
      HasMetricWithValue(HistoryNavigation::kSoftNavigationCountName, 2));

  // The second bfcache restore saw first paint, followed by one soft
  // navigation.
  EXPECT_THAT(
      bfcache_restores[url_a_after_softnav2].get(),
      HasMetric(HistoryNavigation::
                    kNavigationToFirstPaintAfterBackForwardCacheRestoreName));
  EXPECT_THAT(
      bfcache_restores[url_a_after_softnav2].get(),
      HasMetricWithValue(HistoryNavigation::kSoftNavigationCountName, 1));

  //
  // We recorded three soft navigations: url_a_after_softnav{1,2,3}.
  //
  std::map<ukm::SourceId, UkmEntryPtr> soft_navigations =
      ukm_recorder().GetMergedEntriesByName(SoftNavigation::kEntryName);
  std::vector<GURL> soft_navigation_urls;
  std::vector<const UkmEntry*> soft_navigation_entries;
  for (const auto& [source_id, entry] : soft_navigations) {
    soft_navigation_urls.push_back(
        ukm_recorder().GetSourceForSourceId(source_id)->url());
    soft_navigation_entries.emplace_back(entry.get());
  }
  ASSERT_THAT(soft_navigation_urls,
              UnorderedElementsAre(url_a_after_softnav1, url_a_after_softnav2,
                                   url_a_after_softnav3));
  // For all three soft navigations, the page load
  // type should be HistoryNavigation (meaning back-forward-cache restore).
  EXPECT_THAT(soft_navigation_entries[0],
              HasMetricWithValue(
                  SoftNavigation::kPageLoadTypeName,
                  static_cast<int32_t>(PageLoadType::kHistoryNavigation)));
  EXPECT_THAT(soft_navigation_entries[1],
              HasMetricWithValue(
                  SoftNavigation::kPageLoadTypeName,
                  static_cast<int32_t>(PageLoadType::kHistoryNavigation)));
  EXPECT_THAT(soft_navigation_entries[2],
              HasMetricWithValue(
                  SoftNavigation::kPageLoadTypeName,
                  static_cast<int32_t>(PageLoadType::kHistoryNavigation)));

  // For the first bfcache restore, the first paint is followed by two soft
  // navigations. For the second bfcache restore, the first paint is followed by
  // one soft navigation.
  const int64_t* bfcache_restore1_first_paint = TestUkmRecorder::GetEntryMetric(
      bfcache_restores[url_a].get(),
      HistoryNavigation::
          kNavigationToFirstPaintAfterBackForwardCacheRestoreName);
  ASSERT_TRUE(bfcache_restore1_first_paint != nullptr);
  const int64_t* bfcache_restore2_first_paint = TestUkmRecorder::GetEntryMetric(
      bfcache_restores[url_a_after_softnav2].get(),
      HistoryNavigation::
          kNavigationToFirstPaintAfterBackForwardCacheRestoreName);
  ASSERT_TRUE(bfcache_restore2_first_paint != nullptr);

  const int64_t* soft_nav1_start_time = TestUkmRecorder::GetEntryMetric(
      soft_navigation_entries[0], SoftNavigation::kStartTimeName);
  ASSERT_TRUE(soft_nav1_start_time != nullptr);
  const int64_t* soft_nav2_start_time = TestUkmRecorder::GetEntryMetric(
      soft_navigation_entries[1], SoftNavigation::kStartTimeName);
  ASSERT_TRUE(soft_nav2_start_time != nullptr);
  const int64_t* soft_nav3_start_time = TestUkmRecorder::GetEntryMetric(
      soft_navigation_entries[2], SoftNavigation::kStartTimeName);
  ASSERT_TRUE(soft_nav3_start_time != nullptr);

  EXPECT_LT(*bfcache_restore1_first_paint, *soft_nav1_start_time);
  EXPECT_LT(*soft_nav1_start_time, *soft_nav2_start_time);
  EXPECT_LT(*bfcache_restore2_first_paint, *soft_nav3_start_time);

  // performance_entries is populated from the JavaScript PerformanceObserver
  // at the end of the (hard) navigation. Compare softNavigation.startTime
  // (in ms) with the UKM-based start times.
  ASSERT_EQ(3, performance_entries.size());
  EXPECT_NEAR(*performance_entries[0].GetDict().FindDoubleByDottedPath(
                  "softNavigation.startTime"),
              *soft_nav1_start_time, /*abs_error=*/3.0);
  EXPECT_NEAR(*performance_entries[1].GetDict().FindDoubleByDottedPath(
                  "softNavigation.startTime"),
              *soft_nav2_start_time, /*abs_error=*/3.0);
  EXPECT_NEAR(*performance_entries[2].GetDict().FindDoubleByDottedPath(
                  "softNavigation.startTime"),
              *soft_nav3_start_time, /*abs_error=*/3.0);

  // Compare the PerformanceObserver-based InteractionContentfulPaint (ICP)
  // renderTime with the UKM-based soft LCP. Since the UKM-based metric
  // is relative to the soft navigation start time, we add the two.
  const int64_t* soft_nav1_lcp = TestUkmRecorder::GetEntryMetric(
      soft_navigation_entries[0],
      SoftNavigation::kPaintTiming_LargestContentfulPaintName);
  ASSERT_TRUE(soft_nav1_lcp != nullptr);
  EXPECT_NEAR(*performance_entries[0].GetDict().FindDoubleByDottedPath(
                  "interactionContentfulPaint.renderTime"),
              *soft_nav1_start_time + *soft_nav1_lcp, /*abs_error=*/6.0);
  const int64_t* soft_nav2_lcp = TestUkmRecorder::GetEntryMetric(
      soft_navigation_entries[1],
      SoftNavigation::kPaintTiming_LargestContentfulPaintName);
  ASSERT_TRUE(soft_nav2_lcp != nullptr);
  EXPECT_NEAR(*performance_entries[1].GetDict().FindDoubleByDottedPath(
                  "interactionContentfulPaint.renderTime"),
              *soft_nav2_start_time + *soft_nav2_lcp, /*abs_error=*/6.0);
  const int64_t* soft_nav3_lcp = TestUkmRecorder::GetEntryMetric(
      soft_navigation_entries[2],
      SoftNavigation::kPaintTiming_LargestContentfulPaintName);
  ASSERT_TRUE(soft_nav3_lcp != nullptr);
  EXPECT_NEAR(*performance_entries[2].GetDict().FindDoubleByDottedPath(
                  "interactionContentfulPaint.renderTime"),
              *soft_nav3_start_time + *soft_nav3_lcp, /*abs_error=*/6.0);

  // Compare the PerformanceObserver-based CLS with the UKM-based CLS.
  // There was one layout shift during the first soft navigation and two during
  // the third soft navigation.
  ASSERT_EQ(3u, layout_shift_performance_entries.size());
  for (size_t ii = 0; ii < layout_shift_performance_entries.size(); ++ii) {
    SCOPED_TRACE(base::StringPrintf("soft navigation entry %d", ii));
    const auto& dict = layout_shift_performance_entries[ii].GetDict();
    ASSERT_TRUE(dict.FindInt("navigationId").has_value());
    ASSERT_TRUE(dict.FindDouble("value").has_value());
    EXPECT_THAT(dict.FindBool("hadRecentInput"), testing::Optional(false));
  }
  EXPECT_THAT(
      layout_shift_performance_entries[0].GetDict().FindInt("navigationId"),
      testing::Eq(performance_entries[0].GetDict().FindIntByDottedPath(
          "softNavigation.navigationId")));

  // Check that the UKM value and the PerformanceObserver based values for CLS
  // agree.
  std::optional<double> layout_shift_value =
      layout_shift_performance_entries[0].GetDict().FindDouble("value");
  const int64_t* soft_nav1_cls = TestUkmRecorder::GetEntryMetric(
      soft_navigation_entries[0],
      SoftNavigation::
          kLayoutInstability_MaxCumulativeShiftScore_SessionWindow_Gap1000ms_Max5000msName);
  ASSERT_TRUE(soft_nav1_cls != nullptr);
  EXPECT_EQ(LayoutShiftUkmValue(*layout_shift_value), *soft_nav1_cls);

  // Also check that for the second soft nav, for which no PerformanceObserver
  // based entries were found, a UKM CLS of 0 is recorded.
  EXPECT_THAT(
      soft_navigation_entries[1],
      HasMetricWithValue(
          SoftNavigation::
              kLayoutInstability_MaxCumulativeShiftScore_SessionWindow_Gap1000ms_Max5000msName,
          0));

  // Verify layout shifts recorded during the third soft navigation.
  std::optional<int> layout_shift2_navigation_id =
      layout_shift_performance_entries[1].GetDict().FindInt("navigationId");
  EXPECT_THAT(layout_shift2_navigation_id,
              testing::Eq(performance_entries[2].GetDict().FindIntByDottedPath(
                  "softNavigation.navigationId")));
  std::optional<int> layout_shift3_navigation_id =
      layout_shift_performance_entries[2].GetDict().FindInt("navigationId");
  EXPECT_THAT(layout_shift3_navigation_id,
              testing::Eq(performance_entries[2].GetDict().FindIntByDottedPath(
                  "softNavigation.navigationId")));

  std::optional<double> layout_shift2_value =
      layout_shift_performance_entries[1].GetDict().FindDouble("value");
  std::optional<double> layout_shift3_value =
      layout_shift_performance_entries[2].GetDict().FindDouble("value");

  // While UKM records the summed up / aggregated CLS for the soft navigation as
  // a single value, the PerformanceObserver reports on individual layout
  // shifts. We add the CLS values of the two layout shifts that were recorded
  // during the third soft navigation and compare the result to the UKM CLS.
  const int64_t* soft_nav3_cls = TestUkmRecorder::GetEntryMetric(
      soft_navigation_entries[2],
      SoftNavigation::
          kLayoutInstability_MaxCumulativeShiftScore_SessionWindow_Gap1000ms_Max5000msName);
  ASSERT_TRUE(soft_nav3_cls != nullptr);
  EXPECT_EQ(LayoutShiftUkmValue(*layout_shift2_value + *layout_shift3_value),
            *soft_nav3_cls);
}
}  // namespace
}  // namespace page_load_metrics
