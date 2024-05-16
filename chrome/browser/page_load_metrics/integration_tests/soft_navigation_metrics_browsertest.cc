// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <string_view>
#include <vector>

#include "base/json/json_reader.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/trace_event_analyzer.h"
#include "base/values.h"
#include "cc/base/switches.h"
#include "chrome/browser/page_load_metrics/integration_tests/metric_integration_test.h"
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

namespace {
class SoftNavigationTest : public MetricIntegrationTest,
                           public testing::WithParamInterface<bool> {
 public:
  void SetUpOnMainThread() override {
    MetricIntegrationTest::SetUpOnMainThread();
  }
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(cc::switches::kEnableGpuBenchmarking);
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
    std::vector<base::test::FeatureRef> enabled_feature_list = {
        blink::features::kNavigationId,
        blink::features::kSoftNavigationDetection};
    if (GetParam()) {
      enabled_feature_list.push_back(
          blink::features::kSoftNavigationHeuristics);
    }
    feature_list_.InitWithFeatures(enabled_feature_list, {} /*disabled*/);
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

 private:
  base::test::ScopedFeatureList feature_list_;
};

// TODO(crbug.com/40924160): Investigate timeout issue on linux-lacros-rel and
// linux-wayland when retrieving web exposed soft nav lcp entries using the
// EvalJs method.
#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_LINUX)
#define MAYBE_LargestContentfulPaint DISABLED_LargestContentfulPaint
#else
#define MAYBE_LargestContentfulPaint LargestContentfulPaint
#endif

IN_PROC_BROWSER_TEST_P(SoftNavigationTest, MAYBE_LargestContentfulPaint) {
  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      web_contents());

  // Expect 1st soft navigation update.
  waiter->AddSoftNavigationCountExpectation(1);
  waiter->AddSoftNavigationImageLCPExpectation(1);

  Start();
  Load("/soft_navigation.html");

  EXPECT_EQ(
      EvalJs(web_contents()->GetPrimaryMainFrame(), "setEventAndWait()").error,
      "");

  SimulateMouseDownElementWithId("link");

  if (GetParam()) {
    EXPECT_EQ(EvalJs(web_contents()->GetPrimaryMainFrame(),
                     "waitForSoftNavigationEntry()")
                  .error,
              "");
  }

  waiter->Wait();

  // Expect 2nd soft navigation update.
  waiter->AddSoftNavigationCountExpectation(2);
  waiter->AddSoftNavigationImageLCPExpectation(2);

  SimulateMouseDownElementWithId("link");

  if (GetParam()) {
    EXPECT_EQ(EvalJs(web_contents()->GetPrimaryMainFrame(),
                     "waitForSoftNavigationEntry2()")
                  .error,
              "");
  }

  waiter->Wait();

  // If the SoftNavigationHeuristics flag is enabled, we verify exact values
  // in Ukm against the web exposed values. Otherwise, we only verify that
  // there are 2 soft nav lcp reported to Ukm.
  base::Value soft_nav_lcp_list_result;
  if (GetParam()) {
    soft_nav_lcp_list_result = EvalJs(web_contents()->GetPrimaryMainFrame(),
                                      "GetSoftNavigationLCPEntries()")
                                   .ExtractList();
  }

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // Verify start time.
  auto source_id_to_start_time = GetSoftNavigationMetrics(
      ukm_recorder(), ukm::builders::SoftNavigation::kStartTimeName);

  // Assert there are 2 soft navigation start times.
  EXPECT_EQ(source_id_to_start_time.size(), 2ul);

  // Each soft navigation has a different source id;
  EXPECT_NE(std::next(source_id_to_start_time.cbegin())->first,
            source_id_to_start_time.cbegin()->first);

  // Assert second soft navigation start time is larger than first one.
  EXPECT_GT(std::next(source_id_to_start_time.cbegin())->second,
            source_id_to_start_time.cbegin()->second);

  // Verify there are 2 soft navigation navigation ids.
  auto source_id_to_navigation_id = GetSoftNavigationMetrics(
      ukm_recorder(), ukm::builders::SoftNavigation::kNavigationIdName);

  EXPECT_EQ(source_id_to_navigation_id.size(), 2ul);

  // Each soft navigation id has a different source id;
  EXPECT_NE(std::next(source_id_to_navigation_id.cbegin())->first,
            source_id_to_navigation_id.cbegin()->first);

  // Verify 2 soft navigation lcp are reported.
  auto source_id_to_lcp = GetSoftNavigationMetrics(
      ukm_recorder(),
      ukm::builders::SoftNavigation::kPaintTiming_LargestContentfulPaintName);

  EXPECT_EQ(source_id_to_lcp.size(), 2u);

  // If the SoftNavigationHeuristics flag is enabled, we verify exact values
  // in Ukm against the web exposed values.
  if (GetParam()) {
    auto& soft_nav_lcp_list = soft_nav_lcp_list_result.GetList();

    auto json_soft_nav_lcp1 =
        base::JSONReader::Read(soft_nav_lcp_list[0].GetString());

    auto json_soft_nav_lcp2 =
        base::JSONReader::Read(soft_nav_lcp_list[1].GetString());

    const base::Value::Dict& soft_nav_lcp1 = json_soft_nav_lcp1->GetDict();

    const base::Value::Dict& soft_nav_lcp2 = json_soft_nav_lcp2->GetDict();

    double soft_nav_1_web_exposed_lcp =
        soft_nav_lcp1.FindDouble("startTime").value();
    double soft_nav_2_web_exposed_lcp =
        soft_nav_lcp2.FindDouble("startTime").value();

    double soft_nav_1_start_time = source_id_to_start_time.cbegin()->second;
    double soft_nav_2_start_time =
        std::next(source_id_to_start_time.cbegin())->second;

    double soft_nav_1_lcp = source_id_to_lcp.cbegin()->second;
    double soft_nav_2_lcp = std::next(source_id_to_lcp.cbegin())->second;

    EXPECT_NEAR(soft_nav_1_start_time + soft_nav_1_lcp,
                soft_nav_1_web_exposed_lcp, 2);

    EXPECT_NEAR(soft_nav_2_start_time + soft_nav_2_lcp,
                soft_nav_2_web_exposed_lcp, 2);
  }

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

  auto flag_set = blink::LargestContentfulPaintType::kImage |
                  blink::LargestContentfulPaintType::kPNG;

  // Whether the LCP image is after mouseover is flaky.
  EXPECT_TRUE(
      source_id_to_lcp_type.cbegin()->second ==
          static_cast<int64_t>(flag_set) ||
      source_id_to_lcp_type.cbegin()->second ==
          static_cast<int64_t>(
              flag_set | blink::LargestContentfulPaintType::kAfterMouseover));

  EXPECT_TRUE(
      source_id_to_lcp_type.cbegin()->second ==
          static_cast<int64_t>(flag_set) ||
      source_id_to_lcp_type.cbegin()->second ==
          static_cast<int64_t>(
              flag_set | blink::LargestContentfulPaintType::kAfterMouseover));

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

  // 2 is medium priority.
  EXPECT_EQ(source_id_to_lcp_request_priority.cbegin()->second, 2u);

  EXPECT_EQ(std::next(source_id_to_lcp_request_priority.cbegin())->second, 2u);
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
IN_PROC_BROWSER_TEST_P(SoftNavigationTest, MAYBE_INP_ClickWithPresentation) {
  // Add waiter to wait for the interaction is arrived in browser.
  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      web_contents());

  // Start tracing to record tracing data.
  StartTracing({"devtools.timeline"});
  Start();
  Load("/soft_navigation.html");

  // Set up for soft navigation.
  EXPECT_EQ(
      EvalJs(web_contents()->GetPrimaryMainFrame(), "setEventAndWait()").error,
      "");

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
IN_PROC_BROWSER_TEST_P(SoftNavigationTest, MAYBE_LayoutShift) {
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
  base::Value entry_records =
      EvalJs(web_contents(), "GetLayoutShift()").ExtractList();
  auto& entry_records_list = entry_records.GetList();

  // Verify that the entry_records_list has 1 or 2 records. There could be 2
  // layout shift entries emitted for the initial triggerLayoutShift() call.
  EXPECT_LE(entry_records_list.size(), 2u);

  double cls_before_soft_nav = GetCLSFromList(entry_records_list);

  waiter->Wait();

  // Set up for soft navigation.
  EXPECT_EQ(
      EvalJs(web_contents()->GetPrimaryMainFrame(), "setEventAndWait()").error,
      "");

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
    auto soft_nav_entry_records =
        EvalJs(web_contents(), "GetLayoutShift(1)").ExtractList();
    auto& soft_nav_1_entry_records_list = soft_nav_entry_records.GetList();

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
    auto soft_nav_entry_records =
        EvalJs(web_contents(), "GetLayoutShift(2)").ExtractList();
    auto& soft_nav_2_entry_records_list = soft_nav_entry_records.GetList();

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
