// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <vector>
#include "base/json/json_reader.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "cc/base/switches.h"
#include "chrome/browser/page_load_metrics/integration_tests/metric_integration_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
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
        blink::features::kNavigationId};
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

// TODO(crbug.com/1466868): Investigate timeout issue on linux-lacros-rel and
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

INSTANTIATE_TEST_SUITE_P(All,
                         SoftNavigationTest,
                         ::testing::Values(false, true));
}  // namespace
