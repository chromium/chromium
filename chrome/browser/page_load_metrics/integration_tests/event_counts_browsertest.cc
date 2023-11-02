// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "cc/base/switches.h"
#include "chrome/browser/page_load_metrics/integration_tests/metric_integration_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"

class EventCountsBrowserTest : public MetricIntegrationTest {
 public:
  ~EventCountsBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    MetricIntegrationTest::SetUpCommandLine(command_line);

    // content::SimulateTouchEventAt can only be used in Aura, however
    // chrome.gpuBenchmarking.tap can be used on all platforms.
    command_line->AppendSwitch(cc::switches::kEnableGpuBenchmarking);
  }
};

IN_PROC_BROWSER_TEST_F(EventCountsBrowserTest, EventCountsForMouseClick) {
  LoadHTML(R"HTML(
    <script type="text/javascript">
    let eventCounts = {mouseup: 0, pointerup: 0, click: 0};
    const loadPromise = new Promise(resolve => {
      window.addEventListener("load", () => {
        resolve(true);
      });
    });

    let eventPromise;
    checkLoad = async () => {
      await loadPromise;
      eventPromise = new Promise(resolve => {
        for (let evt in eventCounts) {
          window.addEventListener(evt, function(e) {
            eventCounts[e.type]++;
            if (eventCounts.click == 2 &&
                eventCounts.pointerup == 2 &&
                eventCounts.mouseup == 2) {
              resolve(true);
            }
          });
        }
      });
      return true;
    };

    runtest = async () => {
      return await eventPromise;
    };
   </script>
  )HTML");

  // Make sure the page is fully loaded.
  ASSERT_TRUE(EvalJs(web_contents(), "checkLoad()").ExtractBool());

  // We should wait for the main frame's hit-test data to be ready before
  // sending the click event below to avoid flakiness.
  content::WaitForHitTestData(web_contents()->GetPrimaryMainFrame());
  // Ensure the compositor thread is aware of the event listener.
  content::MainThreadFrameObserver frame_observer(GetRenderWidgetHost());
  frame_observer.Wait();

  // Simulate clicks.
  content::SimulateMouseClick(web_contents(), 0,
                              blink::WebMouseEvent::Button::kLeft);
  content::SimulateMouseClick(web_contents(), 0,
                              blink::WebMouseEvent::Button::kLeft);

  ASSERT_TRUE(EvalJs(web_contents(), "runtest()").ExtractBool());

  // Check event counts.
  int expected_pointerdown =
      EvalJs(web_contents(),
             "window.performance.eventCounts.get('pointerdown')")
          .ExtractInt();
  int expected_pointerup =
      EvalJs(web_contents(), "window.performance.eventCounts.get('pointerup')")
          .ExtractInt();
  int expected_mousedown =
      EvalJs(web_contents(), "window.performance.eventCounts.get('mousedown')")
          .ExtractInt();
  int expected_mouseup =
      EvalJs(web_contents(), "window.performance.eventCounts.get('mouseup')")
          .ExtractInt();
  int expected_click =
      EvalJs(web_contents(), "window.performance.eventCounts.get('click')")
          .ExtractInt();

  EXPECT_EQ(expected_pointerdown, 2);
  EXPECT_EQ(expected_pointerup, 2);
  EXPECT_EQ(expected_mousedown, 2);
  EXPECT_EQ(expected_mouseup, 2);
  EXPECT_EQ(expected_click, 2);
}

IN_PROC_BROWSER_TEST_F(EventCountsBrowserTest, EventCountsForTouchTap) {
  LoadHTML(R"HTML(
    <script type="text/javascript">
    let eventCounts = {touchend: 0, pointerup: 0, click: 0};
    const loadPromise = new Promise(resolve => {
      window.addEventListener("load", () => {
        resolve(true);
      });
    });

    let eventPromise;
    checkLoad = async () => {
      await loadPromise;
      eventPromise = new Promise(resolve => {
        for (let evt in eventCounts) {
          window.addEventListener(evt, function(e) {
            eventCounts[e.type]++;
            if (eventCounts.click == 1 &&
                eventCounts.pointerup == 1 &&
                eventCounts.touchend == 1) {
              resolve(true);
            }
          });
        }
      });
      return true;
    };

    runtest = async () => {
      return await eventPromise;
    };
   </script>
  )HTML");

  // Make sure the page is fully loaded.
  ASSERT_TRUE(EvalJs(web_contents(), "checkLoad()").ExtractBool());

  // We should wait for the main frame's hit-test data to be ready before
  // sending the touch events below to avoid flakiness.
  content::WaitForHitTestData(web_contents()->GetPrimaryMainFrame());
  // Ensure the compositor thread is aware of the event listener.
  content::MainThreadFrameObserver frame_observer(GetRenderWidgetHost());
  frame_observer.Wait();

  // Simulate touch tap which will generate touch, pointer and mouse events and
  // a click event.
  EXPECT_TRUE(ExecJs(web_contents(),
                     "chrome.gpuBenchmarking.tap(100, 150, ()=>{}, "
                     "50, chrome.gpuBenchmarking.TOUCH_INPUT);"));

  ASSERT_TRUE(EvalJs(web_contents(), "runtest()").ExtractBool());

  // Check event counts.
  int expected_pointerdown =
      EvalJs(web_contents(),
             "window.performance.eventCounts.get('pointerdown')")
          .ExtractInt();
  int expected_pointerup =
      EvalJs(web_contents(), "window.performance.eventCounts.get('pointerup')")
          .ExtractInt();
  int expected_mousedown =
      EvalJs(web_contents(), "window.performance.eventCounts.get('mousedown')")
          .ExtractInt();
  int expected_mouseup =
      EvalJs(web_contents(), "window.performance.eventCounts.get('mouseup')")
          .ExtractInt();
  int expected_click =
      EvalJs(web_contents(), "window.performance.eventCounts.get('click')")
          .ExtractInt();
  int expected_touchstart =
      EvalJs(web_contents(), "window.performance.eventCounts.get('touchstart')")
          .ExtractInt();
  int expected_touchend =
      EvalJs(web_contents(), "window.performance.eventCounts.get('touchend')")
          .ExtractInt();

  EXPECT_EQ(expected_pointerdown, 1);
  EXPECT_EQ(expected_pointerup, 1);
  EXPECT_EQ(expected_mousedown, 1);
  EXPECT_EQ(expected_mouseup, 1);
  EXPECT_EQ(expected_click, 1);
  EXPECT_EQ(expected_touchstart, 1);
  EXPECT_EQ(expected_touchend, 1);
}
