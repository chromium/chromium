// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/integration_tests/metric_integration_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

#if defined(USE_AURA)
IN_PROC_BROWSER_TEST_F(MetricIntegrationTest, DISABLED_EventCounts) {
  LoadHTML(R"HTML(
    <p>Sample website</p>
  )HTML");

  // Simulate tap on screen.
  content::SimulateTouchEventAt(web_contents(), ui::ET_TOUCH_PRESSED,
                                gfx::Point(30, 60));
  content::SimulateTapDownAt(web_contents(), gfx::Point(30, 60));
  content::SimulateTapAt(web_contents(), gfx::Point(30, 60));
  content::SimulateTouchEventAt(web_contents(), ui::ET_TOUCH_RELEASED,
                                gfx::Point(30, 60));

  // Simulate clicks.
  content::SimulateMouseClick(web_contents(), 0,
                              blink::WebMouseEvent::Button::kLeft);
  content::SimulateMouseClick(web_contents(), 0,
                              blink::WebMouseEvent::Button::kLeft);

  base::PlatformThread::Sleep(base::Milliseconds(3000));

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

  EXPECT_EQ(expected_pointerdown, 3);
  EXPECT_EQ(expected_pointerup, 3);
  EXPECT_EQ(expected_mousedown, 3);
  EXPECT_EQ(expected_mouseup, 3);
  EXPECT_EQ(expected_click, 3);
  EXPECT_EQ(expected_touchstart, 1);
  EXPECT_EQ(expected_touchend, 1);
}
#endif
