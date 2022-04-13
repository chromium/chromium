// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/page_load_metrics/integration_tests/metric_integration_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"

IN_PROC_BROWSER_TEST_F(MetricIntegrationTest, EventCountsForMouseClick) {
  LoadHTML(R"HTML(
    <p>Sample website</p>

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
  content::WaitForHitTestData(web_contents()->GetMainFrame());
  // Ensure the compositor thread is aware of the wheel listener.
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
