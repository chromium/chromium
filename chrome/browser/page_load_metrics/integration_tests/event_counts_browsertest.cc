// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/page_load_metrics/integration_tests/metric_integration_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

IN_PROC_BROWSER_TEST_F(MetricIntegrationTest, EventCountsForMouseClick) {
  LoadHTML(R"HTML(
    <p>Sample website</p>
    <script type="text/javascript">
    var eventCounts = {mouseup: 0, pointerup: 0, click: 0};

    const eventPromise = new Promise(resolve => {
      for (var evt in eventCounts) {
        document.addEventListener(evt, function(e) {
          eventCounts[e.type]++;
          if (eventCounts.click == 2 && eventCounts.pointerup == 2 &&
                eventCounts.mouseup == 2) {
            resolve(true);
          }
        });
      }
    });

    runtest = async () => {
      return await eventPromise;
    };
    document.title='ready';
    </script>
  )HTML");

  WaitUntilHTMLLoadedAndTitleChanged();

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
