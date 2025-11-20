// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <string_view>
#include <vector>

#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/trace_event_analyzer.h"
#include "base/values.h"
#include "cc/base/switches.h"
#include "chrome/browser/page_load_metrics/integration_tests/metric_integration_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/performance/largest_contentful_paint_type.h"
#include "third_party/blink/public/common/switches.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

using page_load_metrics::PageLoadMetricsTestWaiter;
using TimingField = page_load_metrics::PageLoadMetricsTestWaiter::TimingField;

namespace {
std::map<int64_t, double> GetSoftNavigationMetrics(
    const ukm::TestUkmRecorder& ukm_recorder,
    std::string_view metric_name) {
  std::map<int64_t, double> source_id_to_metric_name;
  for (const ukm::mojom::UkmEntry* entry : ukm_recorder.GetEntriesByName(
           ukm::builders::SoftNavigation::kEntryName)) {
    if (auto* rs = ukm_recorder.GetEntryMetric(entry, metric_name)) {
      source_id_to_metric_name[entry->source_id] = *rs;
    }
  }
  return source_id_to_metric_name;
}

class SoftNavigationTest : public MetricIntegrationTest,
                           public testing::WithParamInterface<bool> {
 public:
  void SetUpOnMainThread() override {
    MetricIntegrationTest::SetUpOnMainThread();
  }

  void PreRunTestOnMainThread() override {
    InProcessBrowserTest::PreRunTestOnMainThread();
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnableGpuBenchmarking);
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
    std::vector<base::test::FeatureRef> enabled_feature_list = {
        blink::features::kSoftNavigationDetection,
        blink::features::kNavigationId,
        blink::features::kSoftNavigationDetectionAdvancedPaintAttribution};
    if (GetParam()) {
      enabled_feature_list.push_back(
          blink::features::kSoftNavigationHeuristics);
    }
    feature_list_.InitWithFeatures(enabled_feature_list, {} /*disabled*/);
  }

  std::unique_ptr<PageLoadMetricsTestWaiter> CreatePageLoadMetricsTestWaiter(
      const char* observer_name,
      content::WebContents* web_contents = nullptr) {
    if (!web_contents) {
      web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    }
    return std::make_unique<PageLoadMetricsTestWaiter>(web_contents,
                                                       observer_name);
  }

  void SimulateMouseDownElementWithId(const std::string& id) {
    gfx::Point point = gfx::ToFlooredPoint(
        GetCenterCoordinatesOfElementWithId(web_contents(), id));
    blink::WebMouseEvent click_event(
        blink::WebInputEvent::Type::kMouseDown,
        blink::WebInputEvent::kNoModifiers,
        blink::WebInputEvent::GetStaticTimeStampForTests());
    click_event.button = blink::WebMouseEvent::Button::kLeft;
    click_event.click_count = 1;
    click_event.SetPositionInWidget(point.x(), point.y());
    web_contents()
        ->GetPrimaryMainFrame()
        ->GetRenderViewHost()
        ->GetWidget()
        ->ForwardMouseEvent(click_event);
    click_event.SetType(blink::WebInputEvent::Type::kMouseUp);
    web_contents()
        ->GetPrimaryMainFrame()
        ->GetRenderViewHost()
        ->GetWidget()
        ->ForwardMouseEvent(click_event);
  }

  void WaitForFrameReady() {
    // We should wait for the main frame's hit-test data to be ready before
    // sending the click event below to avoid flakiness.
    content::WaitForHitTestData(web_contents()->GetPrimaryMainFrame());
    // Ensure the compositor thread is aware of the mouse events.
    content::MainThreadFrameObserver frame_observer(GetRenderWidgetHost());
    frame_observer.Wait();
  }

  void SimulateUserInteraction(
      page_load_metrics::PageLoadMetricsTestWaiter* waiter,
      int expected_num_interactions) {
    waiter->AddNumInteractionsExpectation(expected_num_interactions);

    EXPECT_TRUE(ExecJs(web_contents(), "registerEventListeners(); "));

    WaitForFrameReady();

    // Simulate a click on button which has default browser-driven presentation.
    content::SimulateMouseClickOrTapElementWithId(web_contents(), "div");

    EXPECT_TRUE(ExecJs(web_contents(), "waitForClick();"));

    waiter->Wait();
  }

  void TriggerSoftNavigation(
      page_load_metrics::PageLoadMetricsTestWaiter* waiter,
      int expected_soft_nav_count) {
    waiter->AddSoftNavigationCountExpectation(expected_soft_nav_count);
    waiter->AddSoftNavigationImageLCPExpectation(expected_soft_nav_count);

    content::SimulateMouseClickOrTapElementWithId(web_contents(), "link");

    waiter->Wait();
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
        ukm_recorder(),
        ukm::builders::PageLoad::kInteractiveTiming_NumInteractionsName,
        &INP_numOfInteraction_value);
    bool extract_worst_interaction = ExtractUKMPageLoadMetric(
        ukm_recorder(),
        ukm::builders::PageLoad::
            kInteractiveTiming_WorstUserInteractionLatency_MaxEventDurationName,
        &INP_worst_value);
    bool extract_98th_interaction = ExtractUKMPageLoadMetric(
        ukm_recorder(),
        ukm::builders::PageLoad::
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
        ukm_recorder(),
        ukm::builders::SoftNavigation::kInteractiveTiming_NumInteractionsName);
    EXPECT_EQ(soft_nav_source_id_to_num_of_interactions.size(), 2u);
    EXPECT_EQ(soft_nav_source_id_to_num_of_interactions.begin()->second, 2);
    EXPECT_EQ(
        std::next(soft_nav_source_id_to_num_of_interactions.begin())->second,
        1);

    // Verify there are 2 soft nav offsets; the first could be 1 or two; the
    // second should be one.
    auto soft_nav_source_id_to_offsets = GetSoftNavigationMetrics(
        ukm_recorder(),
        ukm::builders::SoftNavigation::kInteractiveTiming_INPOffsetName);
    EXPECT_EQ(soft_nav_source_id_to_offsets.size(), 2u);
    EXPECT_GT(soft_nav_source_id_to_offsets.begin()->second, 0);
    EXPECT_LT(soft_nav_source_id_to_offsets.begin()->second, 3);
    EXPECT_EQ(std::next(soft_nav_source_id_to_offsets.begin())->second, 1);

    // Verify there are 2 soft nav times.
    auto soft_nav_source_id_to_times = GetSoftNavigationMetrics(
        ukm_recorder(),
        ukm::builders::SoftNavigation::kInteractiveTiming_INPOffsetName);
    EXPECT_EQ(soft_nav_source_id_to_times.size(), 2u);

    // Verify that num_of_interactions before soft nav is 2.
    int64_t INP_numOfInteraction_value_before_soft_nav;
    bool extract_num_of_interaction_before_soft_nav = ExtractUKMPageLoadMetric(
        ukm_recorder(),
        ukm::builders::PageLoad::
            kInteractiveTimingBeforeSoftNavigation_NumInteractionsName,
        &INP_numOfInteraction_value_before_soft_nav);
    EXPECT_TRUE(extract_num_of_interaction_before_soft_nav);

    EXPECT_EQ(INP_numOfInteraction_value_before_soft_nav, 2);

    // Verify that INP before soft nav exists.
    int64_t INP_before_soft_nav;
    bool extract_INP_before_soft_nav = ExtractUKMPageLoadMetric(
        ukm_recorder(),
        ukm::builders::PageLoad::
            kInteractiveTimingBeforeSoftNavigation_UserInteractionLatency_HighPercentile2_MaxEventDurationName,
        &INP_before_soft_nav);
    EXPECT_TRUE(extract_INP_before_soft_nav);

    // Verify that 2 soft nav INP exist.
    auto soft_nav_source_id_to_INP = GetSoftNavigationMetrics(
        ukm_recorder(),
        ukm::builders::SoftNavigation::
            kInteractiveTiming_UserInteractionLatency_HighPercentile2_MaxEventDurationName);
    EXPECT_EQ(soft_nav_source_id_to_INP.size(), 2u);

    return true;
  }

  bool ExtractUKMPageLoadMetric(const ukm::TestUkmRecorder& ukm_recorder,
                                std::string_view metric_name,
                                int64_t* extracted_value) {
    std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
        ukm_recorder.GetMergedEntriesByName(
            ukm::builders::PageLoad::kEntryName);
    const auto& kv = merged_entries.begin();
    auto* metric_value =
        ukm::TestUkmRecorder::GetEntryMetric(kv->second.get(), metric_name);
    if (!metric_value) {
      return false;
    }
    *extracted_value = *metric_value;
    return true;
  }

  int ExtractMaxInteractionDurationFromTrace(
      trace_analyzer::TraceEventVector events) {
    int max_duration = 0;
    int sizeOfEvents = (int)events.size();
    for (int i = 0; i < sizeOfEvents; i++) {
      auto* traceEvent = events[i];

      // If the traceEvent doesn't contain args data, it is not
      // one of pointerdown, pointerup and click.
      if (traceEvent->HasDictArg("data")) {
        base::Value::Dict data = traceEvent->GetKnownArgAsDict("data");

        // INP only consider the events with interactionID greater than 0.
        std::string* event_name = data.FindString("type");
        if ((*event_name == "pointerdown" || *event_name == "pointerup" ||
             *event_name == "click") &&
            data.FindInt("interactionId").value_or(-1) > 0) {
          int duration = (int)*(data.FindDouble("duration"));

          // Ensure the max_duration carries the largest duration out of
          // pointerdown, pointerup and click.
          max_duration = fmax(max_duration, duration);
        }
      }
    }
    return max_duration;
  }

  double GetCLSFromList(base::Value::List& entry_records_list,
                        bool ignore_has_recent_input = false) {
    // cls is the normalized cls value.
    double cls = 0;

    // sessionCls is the cls score for current session window.
    double sessionCls = 0;

    // sessionDeadline is the maximum duration for current session window.
    double sessionDeadline = 0;

    // sessionGaptime is the time after one second gap of current shift.
    double sessionGaptime = 0;

    size_t entry_records_list_size = entry_records_list.size();
    for (size_t i = 0; i < entry_records_list_size; i++) {
      std::optional<double> record_startTime =
          entry_records_list[i].GetDict().FindDouble("startTime");
      std::optional<double> record_score =
          entry_records_list[i].GetDict().FindDouble("score");
      std::optional<double> record_hadRecentInput =
          entry_records_list[i].GetDict().FindBool("hadRecentInput");

      // Verify that the optional<double> has value.
      EXPECT_TRUE(record_startTime.has_value());
      EXPECT_TRUE(record_score.has_value());
      EXPECT_TRUE(record_hadRecentInput.has_value());

      if (!ignore_has_recent_input && *record_hadRecentInput) {
        continue;
      }

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

    return cls;
  }

  void VerifySoftNavigationCount(int64_t expected_count) {
    int64_t soft_navigation_count;
    bool extract_soft_navigation_count = ExtractUKMPageLoadMetric(
        ukm_recorder(), ukm::builders::PageLoad::kSoftNavigationCountName,
        &soft_navigation_count);
    EXPECT_TRUE(extract_soft_navigation_count);
    EXPECT_EQ(soft_navigation_count, expected_count);
  }

  std::string JsSnippetGetSoftLcpStartTimes() {
    return R"(
      (() => {
        const observer = new PerformanceObserver(() => {});
        observer.observe({type: 'interaction-contentful-paint', buffered: true,
                           includeSoftNavigationObservations: true});
        const lcpCandidates = observer.takeRecords();
        // For each soft navigation, report the last LCP candidate's start time.
        const startTimeByNavigationId = new Map();
        for (c of lcpCandidates) {
          startTimeByNavigationId.set(c.navigationId, c.startTime);
        }
        return Array.from(startTimeByNavigationId.values());
      })();
    )";
  }

  void VerifySoftNavIdsAndSoftLcpStartTimes(
      const base::Value::List& soft_nav_lcp_list,
      uint32_t expected_soft_nav_count) {
    bool soft_nav_heuristics_enabled = GetParam();

    // Soft navigation start times.
    auto source_id_to_start_time = GetSoftNavigationMetrics(
        ukm_recorder(), ukm::builders::SoftNavigation::kStartTimeName);
    EXPECT_EQ(source_id_to_start_time.size(), expected_soft_nav_count);

    // Soft navigation ids.
    auto source_id_to_navigation_id = GetSoftNavigationMetrics(
        ukm_recorder(), ukm::builders::SoftNavigation::kNavigationIdName);
    EXPECT_EQ(source_id_to_navigation_id.size(), expected_soft_nav_count);

    // Soft navigation LCP.
    auto source_id_to_lcp = GetSoftNavigationMetrics(
        ukm_recorder(),
        ukm::builders::SoftNavigation::kPaintTiming_LargestContentfulPaintName);
    EXPECT_EQ(source_id_to_lcp.size(), expected_soft_nav_count);

    std::vector<ukm::SourceId> source_ids;
    std::vector<double> soft_nav_start_times;
    for (const auto& [source_id, start_time] : source_id_to_start_time) {
      source_ids.push_back(source_id);
      soft_nav_start_times.push_back(start_time);
    }

    // Verify that the soft navigation start times are sorted and unique.
    EXPECT_EQ(source_ids.size(), expected_soft_nav_count);

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
      EXPECT_EQ(soft_nav_lcp_list.size(), expected_soft_nav_count);
      for (uint32_t i = 0; i < soft_nav_lcp_list.size(); ++i) {
        SCOPED_TRACE(base::StringPrintf("soft_nav_lcp_list[%d]", i));
        double expected_lcp = soft_nav_lcp[i] + soft_nav_start_times[i];
        EXPECT_NEAR(soft_nav_lcp_list[i].GetDouble(), expected_lcp, 6);
      }
    }

    // Also check the hard navigation LCP, and the LCP before the first soft
    // navigation.
    int64_t lcp;
    bool extract_lcp = ExtractUKMPageLoadMetric(
        ukm_recorder(),
        ukm::builders::PageLoad::
            kPaintTiming_NavigationToLargestContentfulPaint2Name,
        &lcp);
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
        ukm_recorder(),
        ukm::builders::PageLoad::
            kPaintTimingBeforeSoftNavigation_NavigationToLargestContentfulPaint2Name,  // NOLINT
        &lcp_before_first_soft_nav);
    EXPECT_TRUE(extract_lcp_before_first_soft_nav);
    EXPECT_EQ(lcp_before_first_soft_nav, lcp);

    histogram_tester_->ExpectUniqueSample(
        "PageLoad.BeforeSoftNavigation.LargestContentfulPaint2",
        lcp_before_first_soft_nav, 1);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

// This test focuses on measuring the image LCP of a soft navigation in UKM.
IN_PROC_BROWSER_TEST_P(SoftNavigationTest, ImageLargestContentfulPaint) {
  // Start the test, load soft_navigation_basics.html and wait for
  // load, fcp, and lcp to be observed.
  Start();
  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  waiter->AddPageExpectation(TimingField::kLargestContentfulPaint);
  Load("/soft_navigation_basics.html#image");
  waiter->Wait();

  // 1st soft navigation: click on the next page button and wait for soft
  // navigation count and image lcp.
  waiter->AddSoftNavigationCountExpectation(1);
  waiter->AddSoftNavigationImageLCPExpectation(1);
  WaitForFrameReady();
  content::SimulateMouseClickOrTapElementWithId(web_contents(), "next-page");
  waiter->Wait();

  // 2nd soft navigation: click on the next page button and wait for soft
  // navigation count and image lcp.
  waiter->AddSoftNavigationCountExpectation(2);
  waiter->AddSoftNavigationImageLCPExpectation(2);
  WaitForFrameReady();
  content::SimulateMouseClickOrTapElementWithId(web_contents(), "next-page");
  waiter->Wait();

  base::Value::List soft_nav_lcp_list;
  if (GetParam()) {
    soft_nav_lcp_list = EvalJs(web_contents()->GetPrimaryMainFrame(),
                               JsSnippetGetSoftLcpStartTimes())
                            .TakeValue()
                            .TakeList();
    EXPECT_EQ(soft_nav_lcp_list.size(), 2ul);
  }

  // Navigate to about:blank (untracked) to ensure all UKM are recorded.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  VerifySoftNavigationCount(/*expected_count=*/2);

  VerifySoftNavIdsAndSoftLcpStartTimes(
      soft_nav_lcp_list, /*expected_soft_nav_count=*/2);

  // Verify 2 LCP discovery time timings are reported.
  auto source_id_to_lcp_discovery_time = GetSoftNavigationMetrics(
      ukm_recorder(),
      ukm::builders::SoftNavigation::
          kPaintTiming_LargestContentfulPaintImageDiscoveryTimeName);

  EXPECT_EQ(source_id_to_lcp_discovery_time.size(), 2u);

  // Verify 2 LCP load start timings are reported.
  auto source_id_to_lcp_image_load_start = GetSoftNavigationMetrics(
      ukm_recorder(),
      ukm::builders::SoftNavigation::
          kPaintTiming_LargestContentfulPaintImageLoadStartName);

  EXPECT_EQ(source_id_to_lcp_image_load_start.size(), 2u);

  // Verify 2 LCP load end timings are reported.
  auto source_id_to_lcp_image_load_end = GetSoftNavigationMetrics(
      ukm_recorder(), ukm::builders::SoftNavigation::
                          kPaintTiming_LargestContentfulPaintImageLoadEndName);

  EXPECT_EQ(source_id_to_lcp_image_load_end.size(), 2u);

  // Verify LCP types.
  auto source_id_to_lcp_type = GetSoftNavigationMetrics(
      ukm_recorder(), ukm::builders::SoftNavigation::
                          kPaintTiming_LargestContentfulPaintTypeName);

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
      ukm_recorder(), ukm::builders::SoftNavigation::
                          kPaintTiming_LargestContentfulPaintBPPName);
  // Bpp value is fixed for a given image.
  EXPECT_EQ(source_id_to_lcp_bpp.cbegin()->second, 23u);

  EXPECT_EQ(std::next(source_id_to_lcp_bpp.cbegin())->second, 23u);

  // Verify LCP priority.
  auto source_id_to_lcp_request_priority = GetSoftNavigationMetrics(
      ukm_recorder(),
      ukm::builders::SoftNavigation::
          kPaintTiming_LargestContentfulPaintRequestPriorityName);

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
  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  waiter->AddPageExpectation(TimingField::kLargestContentfulPaint);
  Load("/soft_navigation_basics.html#text");
  waiter->Wait();

  // 1st soft navigation: click on the next page button and wait for soft
  // navigation count and text lcp.
  waiter->AddSoftNavigationCountExpectation(1);
  waiter->AddSoftNavigationTextLCPExpectation(1);
  WaitForFrameReady();
  content::SimulateMouseClickOrTapElementWithId(web_contents(), "next-page");
  waiter->Wait();

  // 2nd soft navigation: click on the next page button and wait for soft
  // navigation count and text lcp.
  waiter->AddSoftNavigationCountExpectation(2);
  waiter->AddSoftNavigationTextLCPExpectation(2);
  WaitForFrameReady();
  content::SimulateMouseClickOrTapElementWithId(web_contents(), "next-page");
  waiter->Wait();

  base::Value::List soft_nav_lcp_list;
  if (GetParam()) {
    soft_nav_lcp_list = EvalJs(web_contents()->GetPrimaryMainFrame(),
                               JsSnippetGetSoftLcpStartTimes())
                            .TakeValue()
                            .TakeList();
    EXPECT_EQ(soft_nav_lcp_list.size(), 2ul);
  }

  // Navigate to about:blank (untracked) to ensure all UKM are recorded.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  VerifySoftNavigationCount(/*expected_count=*/2);

  VerifySoftNavIdsAndSoftLcpStartTimes(
      soft_nav_lcp_list, /*expected_soft_nav_count=*/2);

  // Verify LCP types.
  auto source_id_to_lcp_type = GetSoftNavigationMetrics(
      ukm_recorder(), ukm::builders::SoftNavigation::
                          kPaintTiming_LargestContentfulPaintTypeName);

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
  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kLargestContentfulPaint);
  Load("/soft_navigation_basics.html#text");
  waiter->Wait();

  // 1st soft navigation: click on the next page button.
  waiter->AddSoftNavigationCountExpectation(1);
  waiter->AddSoftNavigationTextLCPExpectation(1);
  WaitForFrameReady();
  content::SimulateMouseClickOrTapElementWithId(web_contents(), "next-page");
  waiter->Wait();

  // 2nd soft navigation: click on the next page button.
  waiter->AddSoftNavigationCountExpectation(2);
  waiter->AddSoftNavigationTextLCPExpectation(2);
  WaitForFrameReady();
  content::SimulateMouseClickOrTapElementWithId(web_contents(), "next-page");
  waiter->Wait();

  // Now we simulate the back button. However, a regular back-button click or
  // content::HistoryGoBack would trigger this intervention:
  // https://chromium.googlesource.com/chromium/src/+/main/docs/history_manipulation_intervention.md
  // Therefore we use -1 history offsets, which are like long-press back-button
  // menu clicks. This is simpler (and perhaps slightly more robust) than
  // avoiding the intervention by adding a user activation to the page.

  // 3rd soft navigation: going backwards in history.
  waiter->AddSoftNavigationCountExpectation(3);
  waiter->AddSoftNavigationTextLCPExpectation(3);
  WaitForFrameReady();
  ASSERT_TRUE(content::HistoryGoToOffset(web_contents(), -1));
  waiter->Wait();

  // 4th soft navigation: going backwards in history.
  waiter->AddSoftNavigationCountExpectation(4);
  waiter->AddSoftNavigationTextLCPExpectation(4);
  WaitForFrameReady();
  ASSERT_TRUE(content::HistoryGoToOffset(web_contents(), -1));
  waiter->Wait();

  base::Value::List soft_nav_lcp_list;
  if (GetParam()) {
    soft_nav_lcp_list = EvalJs(web_contents()->GetPrimaryMainFrame(),
                               JsSnippetGetSoftLcpStartTimes())
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
  auto source_id_to_start_time = GetSoftNavigationMetrics(
      ukm_recorder(), ukm::builders::SoftNavigation::kStartTimeName);
  EXPECT_EQ(source_id_to_start_time.size(), 4);

  std::vector<std::string> urls;
  int port = 0;
  for (const auto& [source_id, start_time] : source_id_to_start_time) {
    const ukm::UkmSource* s = ukm_recorder().GetSourceForSourceId(source_id);
    port = s->url().IntPort();
    urls.push_back(s->url().spec());
  }
  EXPECT_THAT(
      urls,
      testing::ElementsAre(
          absl::StrFormat("http://example.com:%d/page.html?id=1#text", port),
          absl::StrFormat("http://example.com:%d/page.html?id=2#text", port),
          absl::StrFormat("http://example.com:%d/page.html?id=1#text", port),
          absl::StrFormat("http://example.com:%d/page.html?id=0#text", port)));
}

// TODO(crbug.com/334416161): Re-enable this test.
#if BUILDFLAG(IS_WIN)
#define MAYBE_NoSoftNavigation DISABLED_NoSoftNavigation
#else
#define MAYBE_NoSoftNavigation NoSoftNavigation
#endif
IN_PROC_BROWSER_TEST_P(SoftNavigationTest, MAYBE_NoSoftNavigation) {
  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      web_contents());

  waiter->AddMinimumLargestContentfulPaintImageExpectation(1);

  Start();
  Load("/soft_navigation.html");

  waiter->Wait();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // Verify that no soft navigation metric is recorded.
  ExpectUkmEventNotRecorded(ukm::builders::SoftNavigation::kEntryName);
}

INSTANTIATE_TEST_SUITE_P(All,
                         SoftNavigationTest,
                         ::testing::Values(false, true));

// TODO(crbug.com/338061920, crbug.com/333963663): Flaky on Win.
#if BUILDFLAG(IS_WIN)
#define MAYBE_INP_ClickWithPresentation DISABLED_INP_ClickWithPresentation
#else
#define MAYBE_INP_ClickWithPresentation INP_ClickWithPresentation
#endif  //  BUILDFLAG(IS_WIN)

IN_PROC_BROWSER_TEST_P(SoftNavigationTest, DISABLED_INP_ClickWithPresentation) {
  // Add waiter to wait for the interaction is arrived in browser.
  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      web_contents());

  // Start tracing to record tracing data.
  StartTracing({"devtools.timeline"});
  Start();
  Load("/soft_navigation.html");

  // Set up for soft navigation.
  EXPECT_TRUE(EvalJs(web_contents()->GetPrimaryMainFrame(), "setEventAndWait()")
                  .is_ok());

  // Add event listener to change color on click.
  EXPECT_TRUE(ExecJs(web_contents(), "addChangeColorEventListener();"));

  WaitForFrameReady();

  SimulateUserInteraction(waiter.get(), 1);

  // Trigger 1st soft nav.
  TriggerSoftNavigation(waiter.get(), 1);

  // Trigger a user interaction.
  SimulateUserInteraction(waiter.get(), 3);

  // Trigger 2nd soft nav.
  TriggerSoftNavigation(waiter.get(), 2);

  // Trigger a user interaction.
  SimulateUserInteraction(waiter.get(), 5);

  // Navigate to blank page to ensure the data gets flushed from renderer to
  // browser.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  auto analyzer = StopTracingAndAnalyze();

  ASSERT_TRUE(VerifyInpUkmAndTraceData(*analyzer));
}

// TODO(crbug.com/338061920): Flaky on win-asan.
#if BUILDFLAG(IS_WIN) && defined(ADDRESS_SANITIZER)
#define MAYBE_LayoutShift DISABLED_LayoutShift
#else
#define MAYBE_LayoutShift LayoutShift
#endif  //  BUILDFLAG(IS_WIN) && defined(ADDRESS_SANITIZER)

IN_PROC_BROWSER_TEST_P(SoftNavigationTest, DISABLED_LayoutShift) {
  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      web_contents());

  waiter->AddPageLayoutShiftExpectation(
      page_load_metrics::PageLoadMetricsTestWaiter::ShiftFrame::
          LayoutShiftOnlyInMainFrame,
      /*num_layout_shifts=*/1);
  Start();

  // Start tracking with layout_shift related information.
  StartTracing({"loading", TRACE_DISABLED_BY_DEFAULT("layout_shift.debug")});
  Load("/soft_navigation.html");

  // Retrieve web exposed values of the layout shift that happens before any
  // soft navigation happens.
  base::Value::List entry_records_list =
      EvalJs(web_contents(), "GetLayoutShift()").TakeValue().TakeList();

  // Verify that the entry_records_list has 1 or 2 records. There could be 2
  // layout shift entries emitted for the initial triggerLayoutShift() call.
  EXPECT_LE(entry_records_list.size(), 2u);

  double cls_before_soft_nav = GetCLSFromList(entry_records_list);

  waiter->Wait();

  // Set up for soft navigation.
  EXPECT_TRUE(EvalJs(web_contents()->GetPrimaryMainFrame(), "setEventAndWait()")
                  .is_ok());

  // Trigger 1st soft navigation.
  TriggerSoftNavigation(waiter.get(), 1);

  // Trigger a layout shift.
  waiter->AddPageLayoutShiftExpectation(
      page_load_metrics::PageLoadMetricsTestWaiter::ShiftFrame::
          LayoutShiftOnlyInMainFrame,
      /*num_layout_shifts=*/1);

  // TODO: handle return value.
  std::ignore = EvalJs(web_contents(), "triggerLayoutShift()");

  waiter->Wait();

  // Retrieve web exposed layout shift entries if the runtime flag for soft nav
  // is on.
  double soft_nav_1_cls;
  if (GetParam()) {
    auto soft_nav_1_entry_records_list =
        EvalJs(web_contents(), "GetLayoutShift(1)").TakeValue().TakeList();

    // Verify that there is 1 layout shift entry after soft nav 1.
    EXPECT_EQ(soft_nav_1_entry_records_list.size(), 1u);

    // The first part of the expect_score contains total CLS, and the second
    // part of the expect_score contains normalized CLS.
    soft_nav_1_cls = GetCLSFromList(soft_nav_1_entry_records_list, true);
  }

  // Trigger 2nd soft navigation.
  TriggerSoftNavigation(waiter.get(), 2);

  // Trigger a layout shift.
  waiter->AddPageLayoutShiftExpectation(
      page_load_metrics::PageLoadMetricsTestWaiter::ShiftFrame::
          LayoutShiftOnlyInMainFrame,
      /*num_layout_shifts=*/1);

  // TODO: handle return value.
  std::ignore = EvalJs(web_contents(),
                       "triggerLayoutShift(" + base::NumberToString(1.5) + ")");

  waiter->Wait();

  // Retrieve web exposed layout shift entries if the runtime flag for soft nav
  // is on.
  double soft_nav_2_cls;
  if (GetParam()) {
    auto soft_nav_2_entry_records_list =
        EvalJs(web_contents(), "GetLayoutShift(2)").TakeValue().TakeList();

    // Verify that there is 1 layout shift entry after soft nav 1.
    EXPECT_EQ(soft_nav_2_entry_records_list.size(), 1u);

    // The first part of the expect_score contains total CLS, and the second
    // part of the expect_score contains normalized CLS.
    soft_nav_2_cls = GetCLSFromList(soft_nav_2_entry_records_list, true);
  }

  // Finish session.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // Check UKM with CLS Normalization value, and it should be the same as the
  // layout shift score.
  ExpectUKMPageLoadMetricNear(
      ukm::builders::PageLoad::
          kLayoutInstabilityBeforeSoftNavigation_MaxCumulativeShiftScore_MainFrame_SessionWindow_Gap1000ms_Max5000msName,
      page_load_metrics::LayoutShiftUkmValue(cls_before_soft_nav), 1);

  // Verify soft nav CLS records exist.
  auto source_id_to_soft_nav_cls = GetSoftNavigationMetrics(
      ukm_recorder(),
      ukm::builders::SoftNavigation::
          kLayoutInstability_MaxCumulativeShiftScore_SessionWindow_Gap1000ms_Max5000msName);

  EXPECT_EQ(source_id_to_soft_nav_cls.size(), 2u);

  // Verify soft navigation layout shift values against web-exposed values
  if (GetParam()) {
    EXPECT_NEAR(source_id_to_soft_nav_cls.begin()->second,
                page_load_metrics::LayoutShiftUkmValue(soft_nav_1_cls), 1);

    EXPECT_NEAR(std::next(source_id_to_soft_nav_cls.begin())->second,
                page_load_metrics::LayoutShiftUkmValue(soft_nav_2_cls), 1);
  }
}
}  // namespace
