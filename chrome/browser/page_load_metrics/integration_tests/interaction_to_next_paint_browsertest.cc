// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/test/trace_event_analyzer.h"
#include "build/build_config.h"
#include "chrome/browser/page_load_metrics/integration_tests/metric_integration_test.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "services/metrics/public/cpp/ukm_builders.h"

using base::Bucket;
using base::Value;
using std::optional;
using trace_analyzer::Query;
using trace_analyzer::TraceAnalyzer;
using trace_analyzer::TraceEventVector;
using ukm::builders::PageLoad;

class InteractionToNextPaintTest : public MetricIntegrationTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "ReportEventTimingAtVisibilityChange");
  }

 protected:
  // This function will extract the target UKM value from ukm_recorder
  // by the given metric_name in PageLoad.
  bool ExtractUKMPageLoadMetric(const ukm::TestUkmRecorder& ukm_recorder,
                                std::string_view metric_name,
                                int64_t* extracted_value);

  // This function extract the maximum duration for EventTiming from
  // trace data.
  int ExtractMaxInteractionDurationFromTrace(TraceEventVector events);

  // This function will extract and compare the INP values in TraceAnalyzer
  // and UKM.
  bool VerifyUKMAndTraceData(TraceAnalyzer& analyzer);

  // Create Performance Observers to observe first input and events in the
  // program. We are leveraging the Performance Observer to ensure we
  // received the input in renderer.
  void InjectWaitJavaScript();

  // Perform hit test and frame waiter to ensure the frame is ready.
  void WaitForFrameReady();
};

bool InteractionToNextPaintTest::ExtractUKMPageLoadMetric(
    const ukm::TestUkmRecorder& ukm_recorder,
    std::string_view metric_name,
    int64_t* extracted_value) {
  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      ukm_recorder.GetMergedEntriesByName(PageLoad::kEntryName);
  const auto& kv = merged_entries.begin();
  auto* metric_value =
      ukm::TestUkmRecorder::GetEntryMetric(kv->second.get(), metric_name);
  if (!metric_value)
    return false;
  *extracted_value = *metric_value;
  return true;
}

int InteractionToNextPaintTest::ExtractMaxInteractionDurationFromTrace(
    TraceEventVector events) {
  int max_duration = 0;
  int sizeOfEvents = (int)events.size();
  for (int i = 0; i < sizeOfEvents; i++) {
    auto* traceEvent = events[i];

    // If the traceEvent doesn't contain args data, it is not
    // one of pointerdown, pointerup and click.
    if (traceEvent->HasDictArg("data")) {
      Value::Dict data = traceEvent->GetKnownArgAsDict("data");

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

bool InteractionToNextPaintTest::VerifyUKMAndTraceData(
    TraceAnalyzer& analyzer) {
  TraceEventVector events;

  // Extract the events by name EventTiming.
  analyzer.FindEvents(Query::EventNameIs("EventTiming"), &events);

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

  // The duration value in trace data is rounded to 8md
  // which means the value before rounding should be in the
  // range of plus and minus 8ms of the rounded value.
  EXPECT_GE(max_duration, INP_98th_value - 8);
  EXPECT_LE(max_duration, INP_98th_value + 8);

  // Ensure that we only have one interaction in UKM.
  EXPECT_EQ(INP_numOfInteraction_value, 1);
  return true;
}

void InteractionToNextPaintTest::InjectWaitJavaScript() {
  EXPECT_TRUE(ExecJs(web_contents(), R"(
   waitForFirstInput = async () => {
      const observePromise = new Promise(resolve => {
        new PerformanceObserver(e => {
          e.getEntries().forEach(entry => {
            resolve(true);
          })
        }).observe({type: 'first-input', buffered: true});
      });
      return await observePromise;
    };

    let eventCounts = {mouseup: 0, pointerup: 0, click: 0};
    let eventPromise;
    registerEventListeners = () => {
      eventPromise = new Promise(resolve => {
        for (let evt in eventCounts) {
          window.addEventListener(evt, function(e) {
            eventCounts[e.type]++;
            if (eventCounts.click == 1 &&
                eventCounts.pointerup == 1 &&
                eventCounts.mouseup == 1) {
              resolve(true);
            }
          });
        }
      });
      return true;
    };
    awaitEventListeners = async () => {
      return await eventPromise;
    };
  )"));
  ASSERT_TRUE(EvalJs(web_contents(), "registerEventListeners()").ExtractBool());
}

void InteractionToNextPaintTest::WaitForFrameReady() {
  // We should wait for the main frame's hit-test data to be ready before
  // sending the click event below to avoid flakiness.
  content::WaitForHitTestData(web_contents()->GetPrimaryMainFrame());
  // Ensure the compositor thread is aware of the mouse events.
  content::MainThreadFrameObserver frame_observer(GetRenderWidgetHost());
  frame_observer.Wait();
}

IN_PROC_BROWSER_TEST_F(InteractionToNextPaintTest, INP_ClickOnPage) {
  // Add waiter to wait for the interaction is arrived in browser.
  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      web_contents());
  waiter->AddNumInteractionsExpectation(1);

  // Start tracing to record tracing data.
  StartTracing({"devtools.timeline"});
  LoadHTML(R"HTML(
  <p>Sample website</p>
  )HTML");

  // Create Performance Observers to observe first input and events in the
  // program.
  InjectWaitJavaScript();
  WaitForFrameReady();

  // Simulate a click on div which has no renderer actions as our interaction.
  content::SimulateMouseClick(web_contents(), 0,
                              blink::WebMouseEvent::Button::kLeft);

  // Start the waitForFirstInput Performance Observer.
  ASSERT_TRUE(EvalJs(web_contents(), "waitForFirstInput()").ExtractBool());
  ASSERT_TRUE(EvalJs(web_contents(), "awaitEventListeners()").ExtractBool());
  waiter->Wait();

  // Navigate to blank page to ensure the data gets flushed from renderer to
  // browser.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  auto analyzer = StopTracingAndAnalyze();
  ASSERT_TRUE(VerifyUKMAndTraceData(*analyzer));
}

IN_PROC_BROWSER_TEST_F(InteractionToNextPaintTest, INP_ClickOnButton) {
  // Add waiter to wait for the interaction is arrived in browser.
  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      web_contents());
  waiter->AddNumInteractionsExpectation(1);

  // Start tracing to record tracing data.
  StartTracing({"devtools.timeline"});
  LoadHTML(R"HTML(
    <button id="button">Click me</button>
  )HTML");

  // Create Performance Observers to observe first input and events in the
  // program.
  InjectWaitJavaScript();
  WaitForFrameReady();

  // Simulate a click on button which has default browser-driven presentation.
  content::SimulateMouseClickOrTapElementWithId(web_contents(), "button");

  // Start the waitForFirstInput Performance Observer.
  ASSERT_TRUE(EvalJs(web_contents(), "waitForFirstInput()").ExtractBool());
  ASSERT_TRUE(EvalJs(web_contents(), "awaitEventListeners()").ExtractBool());
  waiter->Wait();

  // Navigate to blank page to ensure the data gets flushed from renderer to
  // browser.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  auto analyzer = StopTracingAndAnalyze();
  ASSERT_TRUE(VerifyUKMAndTraceData(*analyzer));
}

IN_PROC_BROWSER_TEST_F(InteractionToNextPaintTest, INP_ClickWithPresentation) {
  // Add waiter to wait for the interaction is arrived in browser.
  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      web_contents());
  waiter->AddNumInteractionsExpectation(1);

  // Start tracing to record tracing data.
  StartTracing({"devtools.timeline"});
  LoadHTML(R"HTML(
    <div id="div">content</div>
  )HTML");

  // Create Performance Observers to observe first input and events in the
  // program.
  InjectWaitJavaScript();

  // Inject javascript to add event listener to change color
  // after click. By doing this, we test click with
  // presentation change for INP.
  EXPECT_TRUE(ExecJs(web_contents(), R"(
    const element = document.getElementById('div');
    element.addEventListener("pointerdown", () => {
      element.style="color:red";
    });
  )"));
  WaitForFrameReady();

  // Simulate a click on button which has default browser-driven presentation.
  content::SimulateMouseClickOrTapElementWithId(web_contents(), "div");

  // Start the waitForFirstInput Performance Observer.
  ASSERT_TRUE(EvalJs(web_contents(), "waitForFirstInput()").ExtractBool());
  ASSERT_TRUE(EvalJs(web_contents(), "awaitEventListeners()").ExtractBool());
  waiter->Wait();

  // Navigate to blank page to ensure the data gets flushed from renderer to
  // browser.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  auto analyzer = StopTracingAndAnalyze();
  ASSERT_TRUE(VerifyUKMAndTraceData(*analyzer));
}

// Timeout of the PageLoadMetricsTestWaiter can happen though rarely, due to
// fast shutdown process. For example, the browser side observer could be
// destroyed before the UKM IPC is received.
// TODO(crbug.com/353730407): investigate and re-enable the test.
IN_PROC_BROWSER_TEST_F(InteractionToNextPaintTest,
                       DISABLED_INP_ReportBeforeNavigatingAway) {
  // Add waiter to wait for the interaction is arrived in browser.
  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      web_contents());
  waiter->AddNumInteractionsExpectation(1);

  waiter->AddOnCompleteCalledExpectation();

  // Start tracing to record tracing data.
  StartTracing({"devtools.timeline"});

  Start();

  Load("/inp_report_before_navigating_away.html");

  WaitForFrameReady();

  // Simulate a click on the link that would navigatie away from the document.
  content::SimulateMouseClickOrTapElementWithId(web_contents(), "link");

  waiter->Wait();

  // Verify trace and UKM data are recorded.
  auto analyzer = StopTracingAndAnalyze();
  ASSERT_TRUE(VerifyUKMAndTraceData(*analyzer));
}

// Timeout of the PageLoadMetricsTestWaiter can happen though rarely, due to
// fast shutdown process. For example, the browser side observer could be
// destroyed before the UKM IPC is received.
// TODO(crbug.com/353730407): investigate and re-enable the test.
IN_PROC_BROWSER_TEST_F(InteractionToNextPaintTest,
                       DISABLED_INP_ReportBeforeNavigatingAwayToCrossOrigin) {
  // Add waiter to wait for the interaction is arrived in browser.
  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      web_contents());
  waiter->AddNumInteractionsExpectation(1);

  waiter->AddOnCompleteCalledExpectation();

  // Start tracing to record tracing data.
  StartTracing({"devtools.timeline"});

  Start();

  Load("/inp_report_before_navigating_away.html");

  WaitForFrameReady();
  auto cross_origin_url = "http://www.b.com:" +
                          base::NumberToString(embedded_test_server()->port()) +
                          "/resources/empty.html";

  auto* script =
      R"(
        let link = document.getElementById("link");
        link.setAttribute("href", $1);
        )";

  EXPECT_TRUE(
      ExecJs(web_contents(), content::JsReplace(script, cross_origin_url)));

  // Simulate a click on the link that would navigatie away from the document.
  content::SimulateMouseClickOrTapElementWithId(web_contents(), "link");

  waiter->Wait();

  // Verify trace and UKM data are recorded.
  auto analyzer = StopTracingAndAnalyze();
  ASSERT_TRUE(VerifyUKMAndTraceData(*analyzer));
}

IN_PROC_BROWSER_TEST_F(InteractionToNextPaintTest,
                       INP_ReportBeforeSwitchingTabAndNavigatingAway) {
  // Add waiter to wait for the interaction is arrived in browser.
  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      web_contents());
  waiter->AddNumInteractionsExpectation(1);

  waiter->AddOnCompleteCalledExpectation();

  // Start tracing to record tracing data.
  StartTracing({"devtools.timeline"});
  Start();
  Load("/inp_report_click_button.html");

  WaitForFrameReady();

  auto* initial_web_contents = web_contents();
  // Simulate a click on button which has default browser-driven
  // presentation.
  content::SimulateMouseClickOrTapElementWithId(initial_web_contents, "button");

  // Add a new tab and switch to it.
  std::unique_ptr<content::WebContents> web_contents_to_add =
      content::WebContents::Create(
          content::WebContents::CreateParams(browser()->profile()));

  web_contents_to_add->GetController().LoadURL(
      embedded_test_server()->GetURL("/resources/empty.html"),
      content::Referrer(), ::ui::PAGE_TRANSITION_AUTO_TOPLEVEL, std::string());

  auto* tab_strip_model = browser()->tab_strip_model();

  tab_strip_model->AddWebContents(std::move(web_contents_to_add), -1,
                                  ::ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                  AddTabTypes::ADD_ACTIVE);

  // Verify the initial tab is backgrounded.
  EXPECT_NE(initial_web_contents,
            browser()->tab_strip_model()->GetActiveWebContents());

  // Switch back to the previous tab and navigate away to let the UKM entries be
  // recorded.
  browser()->tab_strip_model()->ActivateTabAt(0);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  waiter->Wait();

  // Verify trace and UKM data are recorded.
  auto analyzer = StopTracingAndAnalyze();
  ASSERT_TRUE(VerifyUKMAndTraceData(*analyzer));
}

IN_PROC_BROWSER_TEST_F(InteractionToNextPaintTest,
                       INP_ReportBeforeSwitchingTabAndCloseBackgroundedTab) {
  // Add waiter to wait for the interaction is arrived in browser.
  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      web_contents());

  waiter->AddNumInteractionsExpectation(1);

  // Start tracing to record tracing data.
  StartTracing({"devtools.timeline"});

  Start();

  Load("/inp_report_click_button.html");

  WaitForFrameReady();

  auto* initial_web_contents = web_contents();

  // Simulate a click on button.
  content::SimulateMouseClickOrTapElementWithId(initial_web_contents, "button");

  // Add a new tab and switch to it.
  std::unique_ptr<content::WebContents> web_contents_to_add =
      content::WebContents::Create(
          content::WebContents::CreateParams(browser()->profile()));

  web_contents_to_add->GetController().LoadURL(
      embedded_test_server()->GetURL("/resources/empty.html"),
      content::Referrer(), ::ui::PAGE_TRANSITION_AUTO_TOPLEVEL, std::string());

  auto* tab_strip_model = browser()->tab_strip_model();

  // Add the tab and foreground it. Effectively this is switching tab.
  tab_strip_model->AddWebContents(std::move(web_contents_to_add), -1,
                                  ::ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                  AddTabTypes::ADD_ACTIVE);

  // Verify the initial tab is backgrounded.
  EXPECT_NE(initial_web_contents,
            browser()->tab_strip_model()->GetActiveWebContents());

  waiter->Wait();

  // Close the initial tab.
  waiter->AddOnCompleteCalledExpectation();

  // Get the tab index of the given WebContents.
  int tab_index = tab_strip_model->GetIndexOfWebContents(initial_web_contents);

  // Expect the tab index of the given WebContents is found.
  EXPECT_NE(tab_index, TabStripModel::kNoTab);

  // Close the tab.
  tab_strip_model->CloseWebContentsAt(tab_index,
                                      TabCloseTypes::CLOSE_USER_GESTURE);

  waiter->Wait();

  // Verify trace and UKM data are recorded.
  auto analyzer = StopTracingAndAnalyze();
  ASSERT_TRUE(VerifyUKMAndTraceData(*analyzer));
}

// Timeout of the PageLoadMetricsTestWaiter can happen though rarely.
// TODO(crbug.com/353730407): investigate and re-enable the test.
IN_PROC_BROWSER_TEST_F(InteractionToNextPaintTest,
                       DISABLED_INP_ReportBeforeEnteringBFCache) {
  if (!content::IsBackForwardCacheEnabled()) {
    return;
  }

  // Add waiter to wait for the interaction is arrived in browser.
  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      web_contents());

  waiter->AddNumInteractionsExpectation(1);

  // Start tracing to record tracing data.
  StartTracing({"devtools.timeline"});

  Start();

  Load("/inp_report_click_button.html");

  WaitForFrameReady();

  // Simulate a click on button.
  content::SimulateMouseClickOrTapElementWithId(web_contents(), "button");

  // Navigate away and enter back/forward cache.
  content::RenderFrameHostImplWrapper rfh(
      web_contents()->GetPrimaryMainFrame());

  auto url = embedded_test_server()->GetURL("/resources/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Verify `rfh` is stored in back/forward cache.
  ASSERT_TRUE(rfh->IsInBackForwardCache());

  waiter->Wait();

  // Verify trace and UKM data are recorded.
  auto analyzer = StopTracingAndAnalyze();
  ASSERT_TRUE(VerifyUKMAndTraceData(*analyzer));
}
