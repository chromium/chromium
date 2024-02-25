// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/page_load_metrics/integration_tests/metric_integration_test.h"
#include "content/public/test/browser_test_utils.h"

#include "build/build_config.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/hit_test_region_observer.h"
#include "services/metrics/public/cpp/ukm_builders.h"

using ukm::builders::PageLoad;

IN_PROC_BROWSER_TEST_F(MetricIntegrationTest, FirstScrollDelay) {
  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      web_contents());
  waiter->AddPageExpectation(page_load_metrics::PageLoadMetricsTestWaiter::
                                 TimingField::kFirstScrollDelay);
  LoadHTML(R"HTML(
    <div id="content">
      <p>This is some text</p>
    </div>
    <script>
      // Stretch the content tall enough to be able to scroll the page.
      content.style.height = window.innerHeight * 2;
    </script>
  )HTML");

  // We should wait for the main frame's hit-test data to be ready before
  // sending the click event below to avoid flakiness.
  content::WaitForHitTestData(web_contents()->GetPrimaryMainFrame());
  // Ensure the compositor thread is ready for wheel events.
  content::MainThreadFrameObserver main_thread(GetRenderWidgetHost());
  main_thread.Wait();

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
  const float device_scale_factor =
      web_contents()->GetRenderWidgetHostView()->GetDeviceScaleFactor();
  frame_observer.WaitForScrollOffset(
      gfx::PointF(0, scroll_distance * device_scale_factor));

  waiter->Wait();

  // Navigate away.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // Check UKM.
  std::vector<raw_ptr<const ukm::mojom::UkmEntry, VectorExperimental>> entries =
      ukm_recorder().GetEntriesByName(PageLoad::kEntryName);
  // There could be other metrics recorded for PageLoad; filter them out.
  std::erase_if(entries, [](const ukm::mojom::UkmEntry* entry) {
    return !ukm::TestUkmRecorder::EntryHasMetric(
        entry, PageLoad::kInteractiveTiming_FirstScrollDelayName);
  });
  EXPECT_EQ(1ul, entries.size());
}
