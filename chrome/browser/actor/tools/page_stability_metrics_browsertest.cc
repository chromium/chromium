// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <string_view>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/actor/tools/page_stability_test_util.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/page_stability_metrics_common.h"
#include "chrome/common/chrome_features.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace actor {

namespace {

using ::base::test::TestFuture;
using ::content::ExecJs;

// Waits until all histograms in the provided list are recorded, or until a
// timeout is reached. Returns the number of unique histograms from the input
// list that were successfully found before the timeout.
[[nodiscard]] size_t WaitForHistograms(
    base::HistogramTester& histogram_tester,
    const std::vector<std::string_view>& histogram_names) {
  base::RunLoop run_loop;

  base::flat_set<std::string_view> found_histograms;

  // This timer will fire if we wait too long. It will quit the RunLoop.
  base::OneShotTimer timeout_timer;
  timeout_timer.Start(FROM_HERE, TestTimeouts::action_timeout(),
                      run_loop.QuitClosure());

  // This timer will poll repeatedly, looking for the histograms.
  base::RepeatingTimer poll_timer;
  poll_timer.Start(
      FROM_HERE, TestTimeouts::tiny_timeout(),
      base::BindLambdaForTesting([&]() {
        metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

        for (const auto& name : histogram_names) {
          // If we haven't found this histogram yet, check for it.
          if (!found_histograms.contains(name) &&
              !histogram_tester.GetAllSamples(name).empty()) {
            found_histograms.insert(name);
          }
        }

        if (found_histograms.size() == histogram_names.size()) {
          timeout_timer.Stop();
          poll_timer.Stop();
          run_loop.Quit();
        }
      }));

  run_loop.Run();

  return found_histograms.size();
}

[[nodiscard]] bool EnsureHistogramsRecorded(
    base::HistogramTester& histogram_tester,
    const std::vector<std::string_view>& histogram_names) {
  return WaitForHistograms(histogram_tester, histogram_names) ==
         histogram_names.size();
}

[[nodiscard]] bool EnsureHistogramsNotRecorded(
    base::HistogramTester& histogram_tester,
    const std::vector<std::string_view>& histogram_names) {
  return WaitForHistograms(histogram_tester, histogram_names) == 0u;
}

}  // namespace

class PageStabilityMetricsTestBase : public PageStabilityTest {
 public:
  PageStabilityMetricsTestBase() = default;

  PageStabilityMetricsTestBase(const PageStabilityMetricsTestBase&) = delete;
  PageStabilityMetricsTestBase& operator=(const PageStabilityMetricsTestBase&) =
      delete;

  ~PageStabilityMetricsTestBase() override = default;

  void WaitForFrameReady() {
    content::MainThreadFrameObserver frame_observer(
        web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost());
    frame_observer.Wait();
  }

  void ClickAndWaitForFrameReady(std::string_view element_id) {
    content::SimulateMouseClickOrTapElementWithId(web_contents(), element_id);
    WaitForFrameReady();
  }
};

class PageStabilityMetricsTest : public PageStabilityMetricsTestBase {
 public:
  PageStabilityMetricsTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        ::features::kGlicActor,
        {// Do not use min wait.
         {::features::kGlicActorPageStabilityMinWait.name, "0ms"},
         {::features::kActorPaintStabilitySubsequentPaintTimeout.name,
          "100ms"}});
  }

  PageStabilityMetricsTest(const PageStabilityMetricsTest&) = delete;
  PageStabilityMetricsTest& operator=(const PageStabilityMetricsTest&) = delete;

  ~PageStabilityMetricsTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PageStabilityMetricsTest, NetworkAndMainThreadIdle) {
  base::HistogramTester histogram_tester;

  ASSERT_TRUE(
      content::NavigateToURL(web_contents(), GetPageStabilityTestURL()));

  mojo::Remote<mojom::PageStabilityMonitor> monitor =
      CreatePageStabilityMonitor();

  ASSERT_EQ(GetOutputText(), "INITIAL");
  InitiateNetworkRequest();

  TestFuture<void> result;
  monitor->NotifyWhenStable(/*observation_delay=*/base::TimeDelta(),
                            result.GetCallback());

  // The fetch hasn't resolved yet, the monitor should still be waiting on
  // network fetches to resolve.
  ASSERT_EQ(GetOutputText(), "INITIAL");
  EXPECT_FALSE(result.IsReady());

  // Complete the fetch, ensure the monitor completes.
  Respond("NETWORK DONE");
  ASSERT_TRUE(result.Wait());
  ASSERT_EQ(GetOutputText(), "NETWORK DONE");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  histogram_tester.ExpectUniqueSample(
      kActorRendererPageStabilityOutcomeMetricName,
      PageStabilityOutcome::kNetworkAndMainThread,
      /*expected_bucket_count=*/1);

  histogram_tester.ExpectTotalCount(
      kActorRendererPageStabilityTotalTimeToStableMetricName, 1);
  histogram_tester.ExpectTotalCount(
      kActorRendererPageStabilityTimeFromMonitoringToStableMetricName, 1);
  histogram_tester.ExpectTotalCount(
      kActorRendererPageStabilityTotalTimeToRenderFrameGoingAwayMetricName, 0);
  histogram_tester.ExpectTotalCount(
      kActorRendererPageStabilityTimeFromMonitoringToNetworkAndMainThreadIdleMetricName,
      1);

  // Now verify that paint stability metrics were still recorded when paint
  // stability was reached after callback invocation.
  histogram_tester.ExpectTotalCount(
      kActorRendererPageStabilityTimeFromMonitoringToPaintStabilityMetricName,
      0);

  ClickAndWaitForFrameReady("btnPaint");

  ASSERT_TRUE(EnsureHistogramsRecorded(
      histogram_tester,
      {kActorRendererPageStabilityTimeFromMonitoringToPaintStabilityMetricName,
       kActorRendererPaintStabilityTimeToFirstInteractionContentfulPaintMetricName}));
  histogram_tester.ExpectTotalCount(
      kActorRendererPageStabilityTimeFromMonitoringToPaintStabilityMetricName,
      1);
  histogram_tester.ExpectTotalCount(
      kActorRendererPaintStabilityTimeToFirstInteractionContentfulPaintMetricName,
      1);
}

IN_PROC_BROWSER_TEST_F(PageStabilityMetricsTest, Paint) {
  base::HistogramTester histogram_tester;

  ASSERT_TRUE(
      content::NavigateToURL(web_contents(), GetPageStabilityTestURL()));
  content::SimulateEndOfPaintHoldingOnPrimaryMainFrame(web_contents());

  mojo::Remote<mojom::PageStabilityMonitor> monitor =
      CreatePageStabilityMonitor();

  ASSERT_EQ(GetOutputText(), "INITIAL");
  InitiateNetworkRequest();

  TestFuture<void> result;
  monitor->NotifyWhenStable(/*observation_delay=*/base::TimeDelta(),
                            result.GetCallback());

  // The fetch hasn't resolved yet, the monitor should still be waiting on
  // network fetches to resolve.
  ASSERT_EQ(GetOutputText(), "INITIAL");
  EXPECT_FALSE(result.IsReady());

  WaitForFrameReady();

  ClickAndWaitForFrameReady("btnPaint");

  ASSERT_TRUE(result.Wait());
  ASSERT_EQ(GetOutputText(), "PAINT 1");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  histogram_tester.ExpectUniqueSample(
      kActorRendererPageStabilityOutcomeMetricName,
      PageStabilityOutcome::kPaint,
      /*expected_bucket_count=*/1);

  histogram_tester.ExpectTotalCount(
      kActorRendererPageStabilityTotalTimeToStableMetricName, 1);
  histogram_tester.ExpectTotalCount(
      kActorRendererPageStabilityTimeFromMonitoringToStableMetricName, 1);
  histogram_tester.ExpectTotalCount(
      kActorRendererPageStabilityTotalTimeToRenderFrameGoingAwayMetricName, 0);
  histogram_tester.ExpectTotalCount(
      kActorRendererPageStabilityTimeFromMonitoringToPaintStabilityMetricName,
      1);

  // Now verify that network/main thread metric was still recorded when the
  // network/main thread became idle after callback invocation.
  histogram_tester.ExpectTotalCount(
      kActorRendererPageStabilityTimeFromMonitoringToNetworkAndMainThreadIdleMetricName,
      0);
  histogram_tester.ExpectTotalCount(
      kActorRendererPaintStabilityTimeToFirstInteractionContentfulPaintMetricName,
      1);
  histogram_tester.ExpectTotalCount(
      kActorRendererPaintStabilityTimeBetweenInteractionContentfulPaintsMetricName,
      0);

  Respond("NETWORK DONE");
  ASSERT_TRUE(EnsureHistogramsRecorded(
      histogram_tester,
      {kActorRendererPageStabilityTimeFromMonitoringToNetworkAndMainThreadIdleMetricName}));
  histogram_tester.ExpectTotalCount(
      kActorRendererPageStabilityTimeFromMonitoringToNetworkAndMainThreadIdleMetricName,
      1);

  // Verify that the metrics for subsequent interaction contentful paints were
  // still recorded after paint stability.
  ClickAndWaitForFrameReady("btnPaint");
  ASSERT_EQ(GetOutputText(), "PAINT 2");

  ClickAndWaitForFrameReady("btnPaint");
  ASSERT_EQ(GetOutputText(), "PAINT 3");

  // The metrics will be flushed on timeout.

  ASSERT_TRUE(EnsureHistogramsRecorded(
      histogram_tester,
      {kActorRendererPaintStabilityTimeBetweenInteractionContentfulPaintsMetricName,
       kActorRendererPaintStabilitySubsequentInteractionContentfulPaintCountMetricName}));
  histogram_tester.ExpectTotalCount(
      kActorRendererPaintStabilityTimeBetweenInteractionContentfulPaintsMetricName,
      1);
  histogram_tester.ExpectUniqueSample(
      kActorRendererPaintStabilitySubsequentInteractionContentfulPaintCountMetricName,
      /*sample=*/2, /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(PageStabilityMetricsTest, Timeout) {
  base::HistogramTester histogram_tester;

  ASSERT_TRUE(
      content::NavigateToURL(web_contents(), GetPageStabilityTestURL()));

  mojo::Remote<mojom::PageStabilityMonitor> monitor =
      CreatePageStabilityMonitor();

  ASSERT_EQ(GetOutputText(), "INITIAL");
  InitiateNetworkRequest();

  TestFuture<void> result;
  monitor->NotifyWhenStable(/*observation_delay=*/base::TimeDelta(),
                            result.GetCallback());

  ASSERT_TRUE(result.Wait());
  ASSERT_EQ(GetOutputText(), "INITIAL");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  histogram_tester.ExpectUniqueSample(
      kActorRendererPageStabilityOutcomeMetricName,
      PageStabilityOutcome::kTimeout,
      /*expected_bucket_count=*/1);

  histogram_tester.ExpectTotalCount(
      kActorRendererPageStabilityTotalTimeToStableMetricName, 0);
  histogram_tester.ExpectTotalCount(
      kActorRendererPageStabilityTimeFromMonitoringToStableMetricName, 0);
  histogram_tester.ExpectTotalCount(
      kActorRendererPageStabilityTotalTimeToRenderFrameGoingAwayMetricName, 0);
  histogram_tester.ExpectTotalCount(
      kActorRendererPageStabilityTimeFromMonitoringToPaintStabilityMetricName,
      0);
  histogram_tester.ExpectTotalCount(
      kActorRendererPageStabilityTimeFromMonitoringToNetworkAndMainThreadIdleMetricName,
      0);

  // Verify that paint stability and network/main thread metrics were not
  // recorded when the stabilicy check completed after callback invocation due
  // to timeout.
  ClickAndWaitForFrameReady("btnPaint");
  Respond("NETWORK DONE");

  ASSERT_TRUE(EnsureHistogramsNotRecorded(
      histogram_tester,
      {kActorRendererPageStabilityTimeFromMonitoringToPaintStabilityMetricName,
       kActorRendererPageStabilityTimeFromMonitoringToNetworkAndMainThreadIdleMetricName,
       kActorRendererPaintStabilityTimeToFirstInteractionContentfulPaintMetricName}));
  histogram_tester.ExpectTotalCount(
      kActorRendererPageStabilityTimeFromMonitoringToPaintStabilityMetricName,
      0);
  histogram_tester.ExpectTotalCount(
      kActorRendererPageStabilityTimeFromMonitoringToNetworkAndMainThreadIdleMetricName,
      0);
  histogram_tester.ExpectTotalCount(
      kActorRendererPaintStabilityTimeToFirstInteractionContentfulPaintMetricName,
      0);
}

IN_PROC_BROWSER_TEST_F(PageStabilityMetricsTest, RenderFrameGoingAway) {
  base::HistogramTester histogram_tester;

  ASSERT_TRUE(
      content::NavigateToURL(web_contents(), GetPageStabilityTestURL()));

  mojo::Remote<mojom::PageStabilityMonitor> monitor =
      CreatePageStabilityMonitor();

  ASSERT_EQ(GetOutputText(), "INITIAL");
  InitiateNetworkRequest();

  TestFuture<void> result;
  monitor->NotifyWhenStable(/*observation_delay=*/base::TimeDelta(),
                            result.GetCallback());

  // The fetch hasn't resolved yet, the monitor should still be waiting on
  // network fetches to resolve.
  ASSERT_EQ(GetOutputText(), "INITIAL");
  EXPECT_FALSE(result.IsReady());

  // Navigate away and finish the navigation.
  const GURL url = embedded_test_server()->GetURL("/actor/blank.html");
  content::TestNavigationManager manager(web_contents(), url);
  ASSERT_TRUE(
      ExecJs(web_contents(), content::JsReplace("window.location = $1", url)));
  ASSERT_TRUE(manager.WaitForNavigationFinished());

  ASSERT_TRUE(result.Wait());

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  histogram_tester.ExpectUniqueSample(
      kActorRendererPageStabilityOutcomeMetricName,
      PageStabilityOutcome::kRenderFrameGoingAway,
      /*expected_bucket_count=*/1);

  histogram_tester.ExpectTotalCount(
      kActorRendererPageStabilityTotalTimeToStableMetricName, 0);
  histogram_tester.ExpectTotalCount(
      kActorRendererPageStabilityTimeFromMonitoringToStableMetricName, 0);
  histogram_tester.ExpectTotalCount(
      kActorRendererPageStabilityTotalTimeToRenderFrameGoingAwayMetricName, 1);
  histogram_tester.ExpectTotalCount(
      kActorRendererPageStabilityTimeFromMonitoringToPaintStabilityMetricName,
      0);
  histogram_tester.ExpectTotalCount(
      kActorRendererPageStabilityTimeFromMonitoringToNetworkAndMainThreadIdleMetricName,
      0);
  histogram_tester.ExpectTotalCount(
      kActorRendererPaintStabilityTimeToFirstInteractionContentfulPaintMetricName,
      0);
  histogram_tester.ExpectTotalCount(
      kActorRendererPaintStabilityTimeBetweenInteractionContentfulPaintsMetricName,
      0);
}

IN_PROC_BROWSER_TEST_F(PageStabilityMetricsTest, MojoDisconnected) {
  base::HistogramTester histogram_tester;

  ASSERT_TRUE(
      content::NavigateToURL(web_contents(), GetPageStabilityTestURL()));

  mojo::Remote<mojom::PageStabilityMonitor> monitor =
      CreatePageStabilityMonitor();

  ASSERT_EQ(GetOutputText(), "INITIAL");
  InitiateNetworkRequest();

  TestFuture<void> result;
  monitor->NotifyWhenStable(/*observation_delay=*/base::TimeDelta(),
                            result.GetCallback());

  // The fetch hasn't resolved yet, the monitor should still be waiting on
  // network fetches to resolve.
  ASSERT_EQ(GetOutputText(), "INITIAL");
  EXPECT_FALSE(result.IsReady());

  monitor.reset();
  Sleep(base::Milliseconds(100));

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  histogram_tester.ExpectUniqueSample(
      kActorRendererPageStabilityOutcomeMetricName,
      PageStabilityOutcome::kMojoDisconnected,
      /*expected_bucket_count=*/1);

  histogram_tester.ExpectTotalCount(
      kActorRendererPageStabilityTotalTimeToStableMetricName, 0);
  histogram_tester.ExpectTotalCount(
      kActorRendererPageStabilityTimeFromMonitoringToStableMetricName, 0);
  histogram_tester.ExpectTotalCount(
      kActorRendererPageStabilityTotalTimeToRenderFrameGoingAwayMetricName, 0);

  // Verify that paint stability and network/main thread metrics were still
  // recorded when the stabilicy check completed after mojo disconnection.
  ClickAndWaitForFrameReady("btnPaint");
  Respond("NETWORK DONE");

  ASSERT_TRUE(EnsureHistogramsRecorded(
      histogram_tester,
      {kActorRendererPageStabilityTimeFromMonitoringToPaintStabilityMetricName,
       kActorRendererPageStabilityTimeFromMonitoringToNetworkAndMainThreadIdleMetricName,
       kActorRendererPaintStabilityTimeToFirstInteractionContentfulPaintMetricName}));
  histogram_tester.ExpectTotalCount(
      kActorRendererPageStabilityTimeFromMonitoringToPaintStabilityMetricName,
      1);
  histogram_tester.ExpectTotalCount(
      kActorRendererPageStabilityTimeFromMonitoringToNetworkAndMainThreadIdleMetricName,
      1);
  histogram_tester.ExpectTotalCount(
      kActorRendererPaintStabilityTimeToFirstInteractionContentfulPaintMetricName,
      1);
}

IN_PROC_BROWSER_TEST_F(PageStabilityMetricsTest, MojoDisconnectedAndTimeout) {
  base::HistogramTester histogram_tester;

  ASSERT_TRUE(
      content::NavigateToURL(web_contents(), GetPageStabilityTestURL()));

  mojo::Remote<mojom::PageStabilityMonitor> monitor =
      CreatePageStabilityMonitor();

  ASSERT_EQ(GetOutputText(), "INITIAL");
  InitiateNetworkRequest();

  TestFuture<void> result;
  monitor->NotifyWhenStable(/*observation_delay=*/base::TimeDelta(),
                            result.GetCallback());

  // The fetch hasn't resolved yet, the monitor should still be waiting on
  // network fetches to resolve.
  ASSERT_EQ(GetOutputText(), "INITIAL");
  EXPECT_FALSE(result.IsReady());

  monitor.reset();
  Sleep(base::Milliseconds(100));

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  histogram_tester.ExpectUniqueSample(
      kActorRendererPageStabilityOutcomeMetricName,
      PageStabilityOutcome::kMojoDisconnected,
      /*expected_bucket_count=*/1);

  histogram_tester.ExpectTotalCount(
      kActorRendererPageStabilityTotalTimeToStableMetricName, 0);
  histogram_tester.ExpectTotalCount(
      kActorRendererPageStabilityTimeFromMonitoringToStableMetricName, 0);
  histogram_tester.ExpectTotalCount(
      kActorRendererPageStabilityTotalTimeToRenderFrameGoingAwayMetricName, 0);

  // Wait until timeout.
  Sleep(features::kGlicActorPageStabilityTimeout.Get());

  // Verify that paint stability and network/main thread metrics were not
  // recorded after timeout.
  ClickAndWaitForFrameReady("btnPaint");
  Respond("NETWORK DONE");

  ASSERT_TRUE(EnsureHistogramsNotRecorded(
      histogram_tester,
      {kActorRendererPageStabilityTimeFromMonitoringToPaintStabilityMetricName,
       kActorRendererPageStabilityTimeFromMonitoringToNetworkAndMainThreadIdleMetricName,
       kActorRendererPaintStabilityTimeToFirstInteractionContentfulPaintMetricName}));
  histogram_tester.ExpectTotalCount(
      kActorRendererPageStabilityTimeFromMonitoringToPaintStabilityMetricName,
      0);
  histogram_tester.ExpectTotalCount(
      kActorRendererPageStabilityTimeFromMonitoringToNetworkAndMainThreadIdleMetricName,
      0);
  histogram_tester.ExpectTotalCount(
      kActorRendererPaintStabilityTimeToFirstInteractionContentfulPaintMetricName,
      0);
}

class PageStabilityMetricsMinWaitTest : public PageStabilityMetricsTestBase {
 public:
  PageStabilityMetricsMinWaitTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        ::features::kGlicActor,
        {{::features::kGlicActorPageStabilityMinWait.name, "3s"}});
  }

  PageStabilityMetricsMinWaitTest(const PageStabilityMetricsMinWaitTest&) =
      delete;
  PageStabilityMetricsMinWaitTest& operator=(
      const PageStabilityMetricsMinWaitTest&) = delete;

  ~PageStabilityMetricsMinWaitTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PageStabilityMetricsMinWaitTest,
                       NetworkAndMainThreadIdleDelayed) {
  base::HistogramTester histogram_tester;

  ASSERT_TRUE(
      content::NavigateToURL(web_contents(), GetPageStabilityTestURL()));

  mojo::Remote<mojom::PageStabilityMonitor> monitor =
      CreatePageStabilityMonitor();

  ASSERT_EQ(GetOutputText(), "INITIAL");
  InitiateNetworkRequest();

  TestFuture<void> result;
  monitor->NotifyWhenStable(/*observation_delay=*/base::TimeDelta(),
                            result.GetCallback());

  // The fetch hasn't resolved yet, the monitor should still be waiting on
  // network fetches to resolve.
  ASSERT_EQ(GetOutputText(), "INITIAL");
  EXPECT_FALSE(result.IsReady());

  Respond("NETWORK DONE");
  ASSERT_TRUE(EnsureHistogramsRecorded(
      histogram_tester,
      {kActorRendererPageStabilityTimeFromMonitoringToNetworkAndMainThreadIdleMetricName}));
  histogram_tester.ExpectTotalCount(
      kActorRendererPageStabilityTimeFromMonitoringToNetworkAndMainThreadIdleMetricName,
      1);
  ASSERT_FALSE(result.IsReady());

  // Verify that paint stability metric was still recorded when paint stability
  // was reached while waiting for minimum wait.
  ClickAndWaitForFrameReady("btnPaint");

  ASSERT_TRUE(result.Wait());

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  histogram_tester.ExpectUniqueSample(
      kActorRendererPageStabilityOutcomeMetricName,
      PageStabilityOutcome::kNetworkAndMainThreadDelayed,
      /*expected_bucket_count=*/1);

  histogram_tester.ExpectTotalCount(
      kActorRendererPageStabilityTotalTimeToStableMetricName, 1);
  histogram_tester.ExpectTotalCount(
      kActorRendererPageStabilityTimeFromMonitoringToStableMetricName, 1);
  histogram_tester.ExpectTotalCount(
      kActorRendererPageStabilityTotalTimeToRenderFrameGoingAwayMetricName, 0);
  histogram_tester.ExpectTotalCount(
      kActorRendererPageStabilityTimeFromMonitoringToPaintStabilityMetricName,
      1);
}

IN_PROC_BROWSER_TEST_F(PageStabilityMetricsMinWaitTest, PaintDelayed) {
  base::HistogramTester histogram_tester;

  ASSERT_TRUE(
      content::NavigateToURL(web_contents(), GetPageStabilityTestURL()));

  mojo::Remote<mojom::PageStabilityMonitor> monitor =
      CreatePageStabilityMonitor();

  ASSERT_EQ(GetOutputText(), "INITIAL");
  InitiateNetworkRequest();

  TestFuture<void> result;
  monitor->NotifyWhenStable(/*observation_delay=*/base::TimeDelta(),
                            result.GetCallback());

  // The fetch hasn't resolved yet, the monitor should still be waiting on
  // network fetches to resolve.
  ASSERT_EQ(GetOutputText(), "INITIAL");
  EXPECT_FALSE(result.IsReady());

  WaitForFrameReady();

  ClickAndWaitForFrameReady("btnPaint");

  ASSERT_TRUE(EnsureHistogramsRecorded(
      histogram_tester,
      {kActorRendererPageStabilityTimeFromMonitoringToPaintStabilityMetricName}));
  histogram_tester.ExpectTotalCount(
      kActorRendererPageStabilityTimeFromMonitoringToPaintStabilityMetricName,
      1);
  ASSERT_FALSE(result.IsReady());

  // Verify that the network/main thread metric was still recorded when the
  // network/main thread became idle while waiting for minimum wait.
  Respond("NETWORK DONE");

  ASSERT_TRUE(result.Wait());

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  histogram_tester.ExpectUniqueSample(
      kActorRendererPageStabilityOutcomeMetricName,
      PageStabilityOutcome::kPaintDelayed,
      /*expected_bucket_count=*/1);

  histogram_tester.ExpectTotalCount(
      kActorRendererPageStabilityTotalTimeToStableMetricName, 1);
  histogram_tester.ExpectTotalCount(
      kActorRendererPageStabilityTimeFromMonitoringToStableMetricName, 1);
  histogram_tester.ExpectTotalCount(
      kActorRendererPageStabilityTotalTimeToRenderFrameGoingAwayMetricName, 0);
  histogram_tester.ExpectTotalCount(
      kActorRendererPageStabilityTimeFromMonitoringToNetworkAndMainThreadIdleMetricName,
      1);
}

}  // namespace actor
