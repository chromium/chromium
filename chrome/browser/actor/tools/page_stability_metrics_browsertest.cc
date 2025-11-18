// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
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

}  // namespace

class PageStabilityMetricsTest : public PageStabilityTest {
 public:
  PageStabilityMetricsTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        ::features::kGlicActor,
        {// Do not use min wait.
         {::features::kGlicActorPageStabilityMinWait.name, "0ms"}});
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
}

IN_PROC_BROWSER_TEST_F(PageStabilityMetricsTest, Paint) {
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

  content::SimulateMouseClickOrTapElementWithId(web_contents(), "btnPaint");

  ASSERT_TRUE(result.Wait());
  ASSERT_EQ(GetOutputText(), "PAINT");

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
}

class PageStabilityMetricsMinWaitTest : public PageStabilityTest {
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
  ASSERT_TRUE(result.Wait());
  ASSERT_EQ(GetOutputText(), "NETWORK DONE");

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

  content::SimulateMouseClickOrTapElementWithId(web_contents(), "btnPaint");

  ASSERT_TRUE(result.Wait());
  ASSERT_EQ(GetOutputText(), "PAINT");

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
}

}  // namespace actor
