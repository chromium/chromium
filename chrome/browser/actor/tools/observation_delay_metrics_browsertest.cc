// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/observation_delay_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/browser/actor/tools/observation_delay_test_util.h"
#include "chrome/common/actor/task_id.h"
#include "chrome/common/chrome_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "url/gurl.h"

namespace actor {
namespace {

using ::base::test::TestFuture;

using State = ::actor::ObservationDelayController::State;

class ObservationDelayMetricsTest : public ObservationDelayTest {
 public:
  ObservationDelayMetricsTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kGlicActor,
        {// Effectively disable the timeout to prevent flakes.
         {features::kGlicActorPageStabilityTimeout.name, "30s"},
         // Disable LCP delay
         {features::kActorObservationDelayLcp.name, "0ms"}});
  }
  ObservationDelayMetricsTest(const ObservationDelayMetricsTest&) = delete;
  ObservationDelayMetricsTest& operator=(const ObservationDelayMetricsTest&) =
      delete;
  ~ObservationDelayMetricsTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ObservationDelayMetricsTest, CompleteWithoutLoading) {
  base::HistogramTester histogram_tester;

  const GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  TestObservationDelayController controller(*main_frame(), actor::TaskId(),
                                            journal(), PageStabilityConfig());

  TestFuture<void> result;
  controller.Wait(*active_tab(), result.GetCallback());

  ASSERT_TRUE(result.Wait());

  histogram_tester.ExpectTotalCount(
      kActorObservationDelayTotalWaitDurationMetricName, 1);
  histogram_tester.ExpectTotalCount(
      kActorObservationDelayStateDurationWaitForPageStabilityMetricName, 1);
  histogram_tester.ExpectTotalCount(
      kActorObservationDelayStateDurationWaitForLoadCompletionMetricName, 0);
  histogram_tester.ExpectTotalCount(
      kActorObservationDelayStateDurationWaitForVisualStateUpdateMetricName, 1);
  histogram_tester.ExpectUniqueSample(
      kActorObservationDelayDidTimeoutMetricName,
      /*sample=*/false, 1);
  histogram_tester.ExpectUniqueSample(
      kActorObservationDelayLcpDelayNeededMetricName,
      /*sample=*/false, 1);
}

IN_PROC_BROWSER_TEST_F(ObservationDelayMetricsTest, CompleteWithLoading) {
  base::HistogramTester histogram_tester;

  ASSERT_TRUE(
      content::NavigateToURL(web_contents(), GetPageStabilityTestURL()));

  TestObservationDelayController controller(*main_frame(), actor::TaskId(),
                                            journal(), PageStabilityConfig());
  ASSERT_TRUE(InitiateFetchRequest());

  TestFuture<void> result;
  controller.Wait(*active_tab(), result.GetCallback());
  ASSERT_TRUE(DoesReachSteadyState(controller, State::kWaitForPageStability));

  const GURL url = embedded_test_server()->GetURL("/actor/blank.html");
  content::TestNavigationManager manager(web_contents(), url);

  ASSERT_TRUE(content::BeginNavigateToURLFromRenderer(web_contents(), url));

  // Stop before committing the navigation. The observer should remain waiting
  // on page stability.
  ASSERT_TRUE(manager.WaitForResponse());
  ASSERT_TRUE(DoesReachSteadyState(controller, State::kWaitForPageStability));

  // Complete the navigation. The controller should wait for load.
  ASSERT_TRUE(manager.WaitForNavigationFinished());

  ASSERT_TRUE(result.Wait());

  histogram_tester.ExpectTotalCount(
      kActorObservationDelayTotalWaitDurationMetricName, 1);
  histogram_tester.ExpectTotalCount(
      kActorObservationDelayStateDurationWaitForPageStabilityMetricName, 1);
  histogram_tester.ExpectTotalCount(
      kActorObservationDelayStateDurationWaitForLoadCompletionMetricName, 1);
  histogram_tester.ExpectTotalCount(
      kActorObservationDelayStateDurationWaitForVisualStateUpdateMetricName, 1);
  histogram_tester.ExpectUniqueSample(
      kActorObservationDelayDidTimeoutMetricName,
      /*sample=*/false, 1);
  histogram_tester.ExpectUniqueSample(
      kActorObservationDelayLcpDelayNeededMetricName,
      /*sample=*/false, 1);
}

IN_PROC_BROWSER_TEST_F(ObservationDelayMetricsTest, TimeoutOnPageStability) {
  base::HistogramTester histogram_tester;

  ASSERT_TRUE(
      content::NavigateToURL(web_contents(), GetPageStabilityTestURL()));

  TestObservationDelayController controller(*main_frame(), actor::TaskId(),
                                            journal(), PageStabilityConfig());
  ASSERT_TRUE(InitiateFetchRequest());

  TestFuture<void> result;
  controller.Wait(*active_tab(), result.GetCallback());

  ASSERT_TRUE(result.Wait());

  histogram_tester.ExpectTotalCount(
      kActorObservationDelayTotalWaitDurationMetricName, 0);
  histogram_tester.ExpectTotalCount(
      kActorObservationDelayStateDurationWaitForPageStabilityMetricName, 0);
  histogram_tester.ExpectTotalCount(
      kActorObservationDelayStateDurationWaitForLoadCompletionMetricName, 0);
  histogram_tester.ExpectTotalCount(
      kActorObservationDelayStateDurationWaitForVisualStateUpdateMetricName, 0);
  histogram_tester.ExpectUniqueSample(
      kActorObservationDelayDidTimeoutMetricName,
      /*sample=*/true, 1);
  histogram_tester.ExpectTotalCount(
      kActorObservationDelayLcpDelayNeededMetricName, 0);
}

IN_PROC_BROWSER_TEST_F(ObservationDelayMetricsTest, TimeoutOnLoadCompletion) {
  base::HistogramTester histogram_tester;

  ASSERT_TRUE(
      content::NavigateToURL(web_contents(), GetPageStabilityTestURL()));

  TestObservationDelayController controller(*main_frame(), actor::TaskId(),
                                            journal(), PageStabilityConfig());

  ASSERT_TRUE(InitiateFetchRequest());

  // Start waiting, since a fetch is in progress we should be waiting for page
  // stability.
  TestFuture<void> result;
  controller.Wait(*active_tab(), result.GetCallback());

  ASSERT_TRUE(DoesReachSteadyState(controller, State::kWaitForPageStability));
  EXPECT_FALSE(result.IsReady());

  // Start a navigation to a page that finishes navigating but is deferred on
  // the load event.
  NavigateToLoadDeferredPage deferred_navigation(web_contents(),
                                                 embedded_test_server());
  ASSERT_TRUE(deferred_navigation.RunToDOMContentLoadedEvent());

  // The controller should reach the loading state and stay there.
  ASSERT_TRUE(DoesReachSteadyState(controller, State::kWaitForLoadCompletion));
  EXPECT_FALSE(result.IsReady());

  ASSERT_TRUE(result.Wait());

  histogram_tester.ExpectTotalCount(
      kActorObservationDelayTotalWaitDurationMetricName, 0);
  histogram_tester.ExpectTotalCount(
      kActorObservationDelayStateDurationWaitForPageStabilityMetricName, 0);
  histogram_tester.ExpectTotalCount(
      kActorObservationDelayStateDurationWaitForLoadCompletionMetricName, 0);
  histogram_tester.ExpectTotalCount(
      kActorObservationDelayStateDurationWaitForVisualStateUpdateMetricName, 0);
  histogram_tester.ExpectUniqueSample(
      kActorObservationDelayDidTimeoutMetricName,
      /*sample=*/true, 1);
  histogram_tester.ExpectTotalCount(
      kActorObservationDelayLcpDelayNeededMetricName, 0);
}

class ObservationDelayMetricsLcpDelayTest : public ObservationDelayTest {
 public:
  ObservationDelayMetricsLcpDelayTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kGlicActor,
        {// Effectively disable the timeout to prevent flakes.
         {features::kGlicActorPageStabilityTimeout.name, "30s"},
         {features::kActorObservationDelayLcp.name, "100ms"}});
  }
  ObservationDelayMetricsLcpDelayTest(
      const ObservationDelayMetricsLcpDelayTest&) = delete;
  ObservationDelayMetricsLcpDelayTest& operator=(
      const ObservationDelayMetricsLcpDelayTest&) = delete;
  ~ObservationDelayMetricsLcpDelayTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ObservationDelayMetricsLcpDelayTest, LcpDelayNeeded) {
  base::HistogramTester histogram_tester;

  // Navigate to an empty html page. This is a standard navigation, so the
  // PageLoadMetrics system will run, but no LCP will ever be recorded
  // because there is no content.
  const GURL url = embedded_test_server()->GetURL("/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  TestObservationDelayController controller(*main_frame(), actor::TaskId(),
                                            journal(), PageStabilityConfig());

  TestFuture<void> result;
  controller.Wait(*active_tab(), result.GetCallback());

  ASSERT_TRUE(controller.WaitForState(State::kDelayForLcp));
  ASSERT_TRUE(result.Wait());

  histogram_tester.ExpectUniqueSample(
      kActorObservationDelayLcpDelayNeededMetricName,
      /*sample=*/true, 1);
}

}  // namespace
}  // namespace actor
