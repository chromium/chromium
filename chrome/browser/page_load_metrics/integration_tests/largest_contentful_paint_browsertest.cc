// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/page_load_metrics/integration_tests/metric_integration_test.h"

#include "base/feature_list.h"
#include "base/json/json_string_value_serializer.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/trace_event_analyzer.h"
#include "build/build_config.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/test/browser_test.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/performance/largest_contentful_paint_type.h"
#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif
#include "ui/events/test/event_generator.h"

using trace_analyzer::Query;
using trace_analyzer::TraceAnalyzer;
using trace_analyzer::TraceEvent;
using trace_analyzer::TraceEventVector;
using ukm::builders::PageLoad;

namespace {

void ValidateCandidate(int expected_size, const TraceEvent& event) {
  ASSERT_TRUE(event.HasDictArg("data"));
  base::Value::Dict data = event.GetKnownArgAsDict("data");

  const absl::optional<int> traced_size = data.FindInt("size");
  ASSERT_TRUE(traced_size.has_value());
  EXPECT_EQ(traced_size.value(), expected_size);

  const absl::optional<bool> traced_main_frame_flag =
      data.FindBool("isMainFrame");
  ASSERT_TRUE(traced_main_frame_flag.has_value());
  EXPECT_TRUE(traced_main_frame_flag.value());
}

int GetCandidateIndex(const TraceEvent& event) {
  base::Value::Dict data = event.GetKnownArgAsDict("data");
  absl::optional<int> candidate_idx = data.FindInt("candidateIndex");
  DCHECK(candidate_idx.has_value()) << "couldn't find 'candidateIndex'";

  return candidate_idx.value();
}

bool compare_candidate_index(const TraceEvent* lhs, const TraceEvent* rhs) {
  return GetCandidateIndex(*lhs) < GetCandidateIndex(*rhs);
}

void ValidateTraceEvents(std::unique_ptr<TraceAnalyzer> analyzer) {
  TraceEventVector events;
  analyzer->FindEvents(Query::EventNameIs("largestContentfulPaint::Candidate"),
                       &events);
  EXPECT_EQ(3ul, events.size());
  std::sort(events.begin(), events.end(), compare_candidate_index);

  // LCP_0 uses green-16x16.png, of size 16 x 16.
  ValidateCandidate(16 * 16, *events[0]);
  // LCP_1 uses blue96x96.png, of size 96 x 96.
  ValidateCandidate(96 * 96, *events[1]);
  // LCP_2 uses green-256x256.png, of size 16 x 16.
  ValidateCandidate(256 * 256, *events[2]);
}

}  // namespace

// TODO(crbug.com/1223602): Flaky on Linux, ChromeOS and Mac.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
#define MAYBE_LargestContentfulPaint DISABLED_LargestContentfulPaint
#else
#define MAYBE_LargestContentfulPaint LargestContentfulPaint
#endif
IN_PROC_BROWSER_TEST_F(MetricIntegrationTest, MAYBE_LargestContentfulPaint) {
  Start();
  StartTracing({"loading"});
  Load("/largest_contentful_paint.html");

  // The test harness serves files from something like http://example.com:34777
  // but the port number can vary. Extract the 'window.origin' property so we
  // can compare encountered URLs to expected values.
  const std::string window_origin =
      EvalJs(web_contents(), "window.origin").ExtractString();
  const std::string image_1_url_expected =
      base::StrCat({window_origin, "/images/green-16x16.png"});
  const std::string image_2_url_expected =
      base::StrCat({window_origin, "/images/blue96x96.png"});
  const std::string image_3_url_expected =
      base::StrCat({window_origin, "/images/green-256x256.png"});

  content::EvalJsResult result = EvalJs(web_contents(), "run_test()");
  EXPECT_EQ("", result.error);

  // Verify that the JS API yielded three LCP reports. Note that, as we resolve
  // https://github.com/WICG/largest-contentful-paint/issues/41, this test may
  // need to be updated to reflect new semantics.
  const auto& list = result.value.GetListDeprecated();
  const std::string expected_url[3] = {
      image_1_url_expected, image_2_url_expected, image_3_url_expected};
  absl::optional<double> lcp_timestamps[3];
  for (size_t i = 0; i < 3; i++) {
    const std::string* url = list[i].FindStringPath("url");
    EXPECT_TRUE(url);
    EXPECT_EQ(*url, expected_url[i]);
    lcp_timestamps[i] = list[i].FindDoublePath("time");
    EXPECT_TRUE(lcp_timestamps[i].has_value());
  }
  EXPECT_LT(lcp_timestamps[0], lcp_timestamps[1])
      << "The first LCP report should be before the second";
  EXPECT_LT(lcp_timestamps[1], lcp_timestamps[2])
      << "The second LCP report should be before the third";

  // Need to navigate away from the test html page to force metrics to get
  // flushed/synced.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // Check Trace Events.
  ValidateTraceEvents(StopTracingAndAnalyze());

  // Check UKM.
  // Since UKM rounds to an integer while the JS API returns a coarsened double,
  // we'll assert that the UKM and JS values are within 1.2 of each other.
  // Comparing with strict equality could round incorrectly and introduce
  // flakiness into the test.
  ExpectUKMPageLoadMetricNear(
      PageLoad::kPaintTiming_NavigationToLargestContentfulPaint2Name,
      lcp_timestamps[2].value(), 1.2);
  ExpectUKMPageLoadMetricNear(
      PageLoad::kPaintTiming_NavigationToLargestContentfulPaint2_MainFrameName,
      lcp_timestamps[2].value(), 1.2);

  // Check UMA.
  // Similar to UKM, rounding could introduce flakiness, so use helper to
  // compare near.
  ExpectUniqueUMAPageLoadMetricNear(
      "PageLoad.PaintTiming.NavigationToLargestContentfulPaint2",
      lcp_timestamps[2].value());
  ExpectUniqueUMAPageLoadMetricNear(
      "PageLoad.PaintTiming.NavigationToLargestContentfulPaint2.MainFrame",
      lcp_timestamps[2].value());
}

IN_PROC_BROWSER_TEST_F(MetricIntegrationTest,
                       LargestContentfulPaint_SubframeInput) {
  Start();
  Load("/lcp_subframe_input.html");
  auto* sub = ChildFrameAt(web_contents()->GetMainFrame(), 0);
  EXPECT_EQ(EvalJs(sub, "test_step_1()").value.GetString(), "green-16x16.png");

  content::SimulateMouseClickAt(web_contents(), 0,
                                blink::WebMouseEvent::Button::kLeft,
                                gfx::Point(100, 100));

  EXPECT_EQ(EvalJs(sub, "test_step_2()").value.GetString(), "green-16x16.png");
}

class PageViewportInLCPTest : public MetricIntegrationTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    feature_list_.InitWithFeatures(
        {blink::features::kUsePageViewportInLCP} /*enabled*/, {} /*disabled*/);
  }

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PageViewportInLCPTest, DISABLED_FullSizeImageInIframe) {
  Start();
  StartTracing({"loading"});
  Load("/full_size_image.html");
  double lcpTime = EvalJs(web_contents(), "waitForLCP()").ExtractDouble();

  // Navigate away to force metrics recording.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // |lcpTime| is computed from 3 different JS timestamps, so use an epsilon of
  // 2 to account for coarsening and UKM integer rounding.
  ExpectUKMPageLoadMetricNear(
      PageLoad::kPaintTiming_NavigationToLargestContentfulPaint2Name, lcpTime,
      2.0);
  ExpectUniqueUMAPageLoadMetricNear(
      "PageLoad.PaintTiming.NavigationToLargestContentfulPaint2", lcpTime);
}

class IsAnimatedLCPTest : public MetricIntegrationTest {
 public:
  void test_is_animated(const char* html_name,
                        uint32_t flag_set,
                        bool expected,
                        unsigned entries = 1) {
    auto waiter =
        std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
            web_contents());
    waiter->AddPageExpectation(page_load_metrics::PageLoadMetricsTestWaiter::
                                   TimingField::kLargestContentfulPaint);
    waiter->AddMinimumCompleteResourcesExpectation(entries);
    Start();
    Load(html_name);
    EXPECT_EQ(EvalJs(web_contents()->GetMainFrame(), "run_test()").error, "");

    // Need to navigate away from the test html page to force metrics to get
    // flushed/synced.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
    waiter->Wait();
    ExpectUKMPageLoadMetricFlagSet(
        PageLoad::kPaintTiming_LargestContentfulPaintTypeName, flag_set,
        expected);
  }
};

IN_PROC_BROWSER_TEST_F(IsAnimatedLCPTest, LargestContentfulPaint_IsAnimated) {
  test_is_animated("/is_animated.html",
                   blink::LargestContentfulPaintType::kLCPTypeAnimatedImage,
                   /*expected=*/true);
}

IN_PROC_BROWSER_TEST_F(IsAnimatedLCPTest,
                       LargestContentfulPaint_IsNotAnimated) {
  test_is_animated("/non_animated.html",
                   blink::LargestContentfulPaintType::kLCPTypeAnimatedImage,
                   /*expected=*/false);
}

IN_PROC_BROWSER_TEST_F(
    IsAnimatedLCPTest,
    LargestContentfulPaint_AnimatedImageWithLargerTextFirst) {
  test_is_animated("/animated_image_with_larger_text_first.html",
                   blink::LargestContentfulPaintType::kLCPTypeAnimatedImage,
                   /*expected=*/false);
}

// On MacOS, the functionality required for testing mouse moves is not
// implemented:
// https://chromium-review.googlesource.com/c/chromium/src/+/2971065
// Hence, we're only testing this in Aura capable platforms.
// FWIW, the test is passing on MacOS when the mouse is manually moved.
#if defined(USE_AURA)
class MouseoverLCPTest : public MetricIntegrationTest {
 public:
  void test_mouseover(const char* html_name,
                      uint32_t flag_set,
                      std::string entries,
                      std::string entries2,
                      int x1,
                      int y1,
                      int x2,
                      int y2,
                      bool expected) {
    auto waiter =
        std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
            web_contents());
    waiter->AddPageExpectation(page_load_metrics::PageLoadMetricsTestWaiter::
                                   TimingField::kLargestContentfulPaint);
    waiter->AddMinimumCompleteResourcesExpectation(2);
    Start();
    Load(html_name);
    EXPECT_EQ(EvalJs(web_contents()->GetMainFrame(), "run_test(1)").error, "");

    gfx::NativeView view = web_contents()->GetNativeView();
    ui::test::EventGenerator event_generator(view->GetRootWindow());
    gfx::Rect offset = web_contents()->GetContainerBounds();
    gfx::Point point(x1 + offset.x(), y1 + offset.y());
    event_generator.MoveMouseTo(point);
    RunUntilInputProcessed(
        web_contents()->GetRenderWidgetHostView()->GetRenderWidgetHost());

    // Wait for a second image to load and for LCP entry to be there.
    EXPECT_EQ(EvalJs(web_contents()->GetMainFrame(),
                     "run_test(/*entries_expected= */" + entries + ")")
                  .error,
              "");

    if (x1 != x2 || y1 != y2) {
      // Wait for 600ms before the second mouse move, as our heuristics wait for
      // 500ms after a mousemove event on an LCP image.
      constexpr auto kWaitTime = base::Milliseconds(600);
      base::PlatformThread::Sleep(kWaitTime);
      // TODO(1289726): Here we should call MoveMouseTo() a second time, but
      // currently a second mouse move call is not dispatching the event as it
      // should. So instead, we dispatch the event directly.
      EXPECT_EQ(
          EvalJs(web_contents()->GetMainFrame(), "dispatch_mouseover()").error,
          "");

      // Wait for a third image (potentially) to load and for LCP entry to be
      // there.
      EXPECT_EQ(EvalJs(web_contents()->GetMainFrame(),
                       "run_test(/*entries_expected= */" + entries2 + ")")
                    .error,
                "");
    }

    // Need to navigate away from the test html page to force metrics to get
    // flushed/synced.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
    waiter->Wait();
    ExpectUKMPageLoadMetricFlagSet(
        PageLoad::kPaintTiming_LargestContentfulPaintTypeName, flag_set,
        expected);
  }
};

IN_PROC_BROWSER_TEST_F(MouseoverLCPTest,
                       DISABLED_LargestContentfulPaint_MouseoverOverLCPImage) {
  test_mouseover("/mouseover.html",
                 blink::LargestContentfulPaintType::kLCPTypeAfterMouseover,
                 /*entries=*/"2",
                 /*entries2=*/"2",
                 /*x1=*/10, /*y1=*/10,
                 /*x2=*/10, /*y2=*/10,
                 /*expected=*/true);
}

IN_PROC_BROWSER_TEST_F(
    MouseoverLCPTest,
    DISABLED_LargestContentfulPaint_MouseoverOverLCPImageReplace) {
  test_mouseover("/mouseover.html?replace",
                 blink::LargestContentfulPaintType::kLCPTypeAfterMouseover,
                 /*entries=*/"2",
                 /*entries2=*/"2",
                 /*x1=*/10, /*y1=*/10,
                 /*x2=*/10, /*y2=*/10,
                 /*expected=*/true);
}

IN_PROC_BROWSER_TEST_F(MouseoverLCPTest,
                       LargestContentfulPaint_MouseoverOverBody) {
  test_mouseover("/mouseover.html",
                 blink::LargestContentfulPaintType::kLCPTypeAfterMouseover,
                 /*entries=*/"2",
                 /*entries2=*/"2",
                 /*x1=*/30, /*y1=*/10,
                 /*x2=*/30, /*y2=*/10,
                 /*expected=*/false);
}

IN_PROC_BROWSER_TEST_F(
    MouseoverLCPTest,
    DISABLED_LargestContentfulPaint_MouseoverOverLCPImageThenBody) {
  test_mouseover("/mouseover.html?dispatch",
                 blink::LargestContentfulPaintType::kLCPTypeAfterMouseover,
                 /*entries=*/"2",
                 /*entries2=*/"3",
                 /*x1=*/10, /*y1=*/10,
                 /*x2=*/30, /*y2=*/10,
                 /*expected=*/false);
}
#endif
