// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/integration_tests/metric_integration_test.h"

#include "build/build_config.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "services/metrics/public/cpp/ukm_builders.h"

using ukm::builders::PageLoad;

// http://crbug.com/1098148
IN_PROC_BROWSER_TEST_F(MetricIntegrationTest, DISABLED_FirstScrollDelay) {
  LoadHTML(R"HTML(
<div id="content">
  <p>This is some text</p>
</div>
<script>
  // Stretch the content tall enough to be able to scroll the page.
  content.style.height = window.innerHeight * 2;
</script>
  )HTML");

  // With a |RenderFrameSubmissionObsever| we can block until the scroll
  // completes.
  content::RenderFrameSubmissionObserver frame_observer(web_contents());

  // Simulate scroll input.
  const gfx::Point mouse_position(50, 50);
  const float scroll_distance = 20.0;
  content::SimulateMouseEvent(web_contents(), blink::WebInputEvent::Type::kMouseMove,
                              mouse_position);
  content::SimulateMouseWheelEvent(web_contents(), mouse_position,
                                   gfx::Vector2d(0, -scroll_distance),
                                   blink::WebMouseWheelEvent::kPhaseBegan);
  content::SimulateMouseWheelEvent(web_contents(), mouse_position,
                                   gfx::Vector2d(0, 0),
                                   blink::WebMouseWheelEvent::kPhaseEnded);

  // Wait for the scroll to complete and the display to be updated.
  frame_observer.WaitForScrollOffset(gfx::Vector2d(0, scroll_distance));

  // Navigate away.
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));

  // Check UKM.
  std::vector<const ukm::mojom::UkmEntry*> entries =
      ukm_recorder().GetEntriesByName(PageLoad::kEntryName);
  auto name_filter = [](const ukm::mojom::UkmEntry* entry) {
    return !ukm::TestUkmRecorder::EntryHasMetric(
        entry, PageLoad::kInteractiveTiming_FirstScrollDelayName);
  };
  // There could be other metrics recorded for PageLoad; filter them out.
  entries.erase(std::remove_if(entries.begin(), entries.end(), name_filter),
                entries.end());
  EXPECT_EQ(1ul, entries.size());
}
