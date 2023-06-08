// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <vector>
#include "cc/base/switches.h"
#include "chrome/browser/page_load_metrics/integration_tests/metric_integration_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/switches.h"
#include "ui/gfx/geometry/point_conversions.h"

namespace {
class SoftNavigationTest : public MetricIntegrationTest {
 public:
  void SetUpOnMainThread() override {
    MetricIntegrationTest::SetUpOnMainThread();
  }
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(cc::switches::kEnableGpuBenchmarking);
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
    feature_list_.InitWithFeatures({blink::features::kSoftNavigationHeuristics,
                                    blink::features::kNavigationId} /*enabled*/,
                                   {} /*disabled*/);
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
      base::StringPiece metric_name) {
    std::map<int64_t, double> source_id_to_metric_name;
    for (auto* entry : ukm_recorder.GetEntriesByName(
             ukm::builders::SoftNavigation::kEntryName)) {
      if (auto* rs = ukm_recorder.GetEntryMetric(entry, metric_name)) {
        source_id_to_metric_name[entry->source_id] = *rs;
      }
    }
    return source_id_to_metric_name;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SoftNavigationTest, StartTimeAndNavigationId) {
  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      web_contents());

  // Expect 2 soft navigation updates.
  waiter->AddSoftNavigationCountExpectation(1);

  Start();
  Load("/soft_navigation.html");

  EXPECT_EQ(
      EvalJs(web_contents()->GetPrimaryMainFrame(), "setEventAndWait()").error,
      "");

  SimulateMouseDownElementWithId("link");
  EXPECT_EQ(EvalJs(web_contents()->GetPrimaryMainFrame(),
                   "waitForSoftNavigationEntry()")
                .error,
            "");

  waiter->Wait();
  waiter->AddSoftNavigationCountExpectation(2);

  SimulateMouseDownElementWithId("link");
  EXPECT_EQ(EvalJs(web_contents()->GetPrimaryMainFrame(),
                   "waitForSoftNavigationEntry2()")
                .error,
            "");
  waiter->Wait();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // Verify start time.
  auto source_id_to_start_time = GetSoftNavigationMetrics(
      ukm_recorder(), ukm::builders::SoftNavigation::kStartTimeName);

  // Assert there are 2 soft navigation start times.
  EXPECT_EQ(source_id_to_start_time.size(), 2ul);
  // Each soft navigation have different source ids;
  EXPECT_NE(std::next(source_id_to_start_time.cbegin())->first,
            source_id_to_start_time.cbegin()->first);

  // Assert second soft navigation start time is larger than first one.
  EXPECT_GT(std::next(source_id_to_start_time.cbegin())->second,
            source_id_to_start_time.cbegin()->second);

  // Verify navigation id.
  auto source_id_to_navigation_id = GetSoftNavigationMetrics(
      ukm_recorder(), ukm::builders::SoftNavigation::kNavigationIdName);

  // Assert there are 2 soft navigation start times.
  EXPECT_EQ(source_id_to_navigation_id.size(), 2ul);
  // Each soft navigation have different source ids;
  EXPECT_NE(std::next(source_id_to_navigation_id.cbegin())->first,
            source_id_to_navigation_id.cbegin()->first);
}
}  // namespace
