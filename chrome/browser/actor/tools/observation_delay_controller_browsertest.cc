// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/observation_delay_controller.h"

#include <string>

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/with_feature_override.h"
#include "base/time/time.h"
#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/actor/tools/observation_delay_test_util.h"
#include "chrome/common/actor/task_id.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "url/gurl.h"

namespace actor {

namespace {

using ::base::test::ScopedFeatureList;
using ::base::test::TestFuture;
using ::content::BeginNavigateToURLFromRenderer;
using ::content::RenderFrameHost;
using ::content::TestNavigationManager;
using ::content::WebContents;
using ::tabs::TabInterface;

using State = ::actor::ObservationDelayController::State;

class ObservationDelayControllerTest : public ObservationDelayTest {
 public:
  ObservationDelayControllerTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kGlicActor,
        {// Effectively disable the timeout to prevent flakes.
         {features::kGlicActorPageStabilityTimeout.name, "30000ms"},
         // Use small LCP delay.
         {features::kActorObservationDelayLcp.name, "100ms"}});
  }
  ~ObservationDelayControllerTest() override = default;

 private:
  ScopedFeatureList scoped_feature_list_;
};

class ObservationDelayControllerNavigateTest
    : public ObservationDelayControllerTest,
      public base::test::WithFeatureOverride {
 public:
  ObservationDelayControllerNavigateTest()
      : base::test::WithFeatureOverride(
            kActorRestartObservationDelayControllerOnNavigate) {}
};

IN_PROC_BROWSER_TEST_P(ObservationDelayControllerNavigateTest,
                       NavigateDuringPageStabilization) {
  ASSERT_TRUE(
      content::NavigateToURL(web_contents(), GetPageStabilityTestURL()));

  TestObservationDelayController controller(*main_frame(), actor::TaskId(),
                                            journal(), PageStabilityConfig());

  // Initiate a fetch to block page stability.
  ASSERT_TRUE(InitiateFetchRequest());

  // Start waiting on the controller. It should be blocked in page stability.
  TestFuture<ObservationDelayController::Result> result;
  controller.Wait(*active_tab(), result.GetCallback());
  ASSERT_TRUE(DoesReachSteadyState(controller, State::kWaitForPageStability));

  const GURL url = embedded_test_server()->GetURL("/actor/blank.html");
  TestNavigationManager manager(web_contents(), url);
  ASSERT_TRUE(BeginNavigateToURLFromRenderer(web_contents(), url));

  if (IsParamFeatureEnabled()) {
    ASSERT_TRUE(controller.WaitForState(State::kDone));
    ASSERT_EQ(result.Get(), ObservationDelayController::Result::kPageNavigated);
  } else {
    // Stop before committing the navigation. The observer should remain waiting
    // on page stability.
    ASSERT_TRUE(manager.WaitForResponse());
    ASSERT_TRUE(DoesReachSteadyState(controller, State::kWaitForPageStability));

    // Complete the navigation. The controller should wait for load, then a
    // visual update, then complete.
    ASSERT_TRUE(manager.WaitForNavigationFinished());
    ASSERT_TRUE(controller.WaitForState(State::kWaitForLoadCompletion));
    ASSERT_TRUE(controller.WaitForState(State::kWaitForVisualStateUpdate));
    ASSERT_TRUE(controller.WaitForState(State::kMaybeDelayForLcp));
    ASSERT_TRUE(controller.WaitForState(State::kDone));
    ASSERT_EQ(result.Get(), ObservationDelayController::Result::kOk);
  }
}

IN_PROC_BROWSER_TEST_P(ObservationDelayControllerNavigateTest,
                       NavigateWithTooManyRestarts) {
  ASSERT_TRUE(
      content::NavigateToURL(web_contents(), GetPageStabilityTestURL()));

  TestObservationDelayController controller(*main_frame(), actor::TaskId(),
                                            journal(), PageStabilityConfig());
  // Force the navigation count to be very large.
  controller.SetNavigationCount(1000);

  // Initiate a fetch to block page stability.
  ASSERT_TRUE(InitiateFetchRequest());

  // Start waiting on the controller. It should be blocked in page stability.
  TestFuture<ObservationDelayController::Result> result;
  controller.Wait(*active_tab(), result.GetCallback());
  ASSERT_TRUE(DoesReachSteadyState(controller, State::kWaitForPageStability));

  const GURL url = embedded_test_server()->GetURL("/actor/blank.html");
  TestNavigationManager manager(web_contents(), url);
  ASSERT_TRUE(BeginNavigateToURLFromRenderer(web_contents(), url));

  // Stop before committing the navigation. The observer should remain waiting
  // on page stability.
  ASSERT_TRUE(manager.WaitForResponse());
  ASSERT_TRUE(DoesReachSteadyState(controller, State::kWaitForPageStability));

  // Complete the navigation. The controller should wait for load, then a
  // visual update, then complete.
  ASSERT_TRUE(manager.WaitForNavigationFinished());
  ASSERT_TRUE(controller.WaitForState(State::kWaitForLoadCompletion));
  ASSERT_TRUE(controller.WaitForState(State::kWaitForVisualStateUpdate));
  ASSERT_TRUE(controller.WaitForState(State::kMaybeDelayForLcp));
  ASSERT_TRUE(controller.WaitForState(State::kDone));
  ASSERT_EQ(result.Get(), ObservationDelayController::Result::kOk);
}

IN_PROC_BROWSER_TEST_F(ObservationDelayControllerTest,
                       UsePageStabilityForSameDocumentNavigation) {
  ASSERT_TRUE(
      content::NavigateToURL(web_contents(), GetPageStabilityTestURL()));

  TestObservationDelayController controller(*main_frame(), actor::TaskId(),
                                            journal(), PageStabilityConfig());

  // Perform a same-document navigation. The page has a navigation handler
  // that will initiate a fetch from this event.
  ASSERT_TRUE(InitiateFetchRequest());

  // Start waiting on the controller. It should be blocked in page stability.
  TestFuture<ObservationDelayController::Result> result;
  controller.Wait(*active_tab(), result.GetCallback());

  ASSERT_TRUE(DoesReachSteadyState(controller, State::kWaitForPageStability));
  EXPECT_FALSE(result.IsReady());

  Respond("TEST COMPLETE");

  ASSERT_TRUE(controller.WaitForState(State::kWaitForLoadCompletion));
  ASSERT_TRUE(controller.WaitForState(State::kWaitForVisualStateUpdate));
  ASSERT_TRUE(controller.WaitForState(State::kMaybeDelayForLcp));
  ASSERT_TRUE(result.Wait());
  ASSERT_EQ(GetOutputText(), "TEST COMPLETE");
}

// Test waiting on a new document load after waiting for the page to stabilize.
IN_PROC_BROWSER_TEST_P(ObservationDelayControllerNavigateTest,
                       LoadAfterStability) {
  ASSERT_TRUE(
      content::NavigateToURL(web_contents(), GetPageStabilityTestURL()));

  TestObservationDelayController controller(*main_frame(), actor::TaskId(),
                                            journal(), PageStabilityConfig());

  ASSERT_TRUE(InitiateFetchRequest());

  // Start waiting, since a fetch is in progress we should be waiting for page
  // stability.
  TestFuture<ObservationDelayController::Result> result;
  controller.Wait(*active_tab(), result.GetCallback());

  ASSERT_TRUE(DoesReachSteadyState(controller, State::kWaitForPageStability));
  EXPECT_FALSE(result.IsReady());

  // Start a navigation to a page that finishes navigating but is deferred on
  // the load event.
  NavigateToLoadDeferredPage deferred_navigation(web_contents(),
                                                 embedded_test_server());
  ASSERT_TRUE(deferred_navigation.RunToDOMContentLoadedEvent());

  if (IsParamFeatureEnabled()) {
    ASSERT_TRUE(controller.WaitForState(State::kDone));
    ASSERT_EQ(result.Get(), ObservationDelayController::Result::kPageNavigated);
  } else {
    // The controller should reach the loading state and stay there.
    ASSERT_TRUE(
        DoesReachSteadyState(controller, State::kWaitForLoadCompletion));
    EXPECT_FALSE(result.IsReady());

    // Unblock the subframe, the controller should now proceed through the
    // remaining states.
    ASSERT_TRUE(deferred_navigation.RunToLoadEvent());

    ASSERT_TRUE(controller.WaitForState(State::kWaitForVisualStateUpdate));
    ASSERT_TRUE(controller.WaitForState(State::kMaybeDelayForLcp));
    ASSERT_TRUE(controller.WaitForState(State::kDone));
    ASSERT_EQ(result.Get(), ObservationDelayController::Result::kOk);
  }
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(ObservationDelayControllerNavigateTest);

// Ensure that putting a tab into the background while its waiting to stabilize
// doesn't affect the PageStabilityMonitor.
// TODO(b/448641423): This test better belongs in PageStabilityMonitor browser
// tests but is much clearer to write here. Move once the tests are sharing
// infrastructure.
IN_PROC_BROWSER_TEST_F(ObservationDelayControllerTest,
                       BackgroundTabWhileWaitingForStability) {
  ASSERT_TRUE(
      content::NavigateToURL(web_contents(), GetPageStabilityTestURL()));

  TestObservationDelayController controller(*main_frame(), actor::TaskId(),
                                            journal(), PageStabilityConfig());

  ASSERT_TRUE(InitiateFetchRequest());

  // Start waiting, since a fetch is in progress we should be waiting for page
  // stability.
  TestFuture<ObservationDelayController::Result> result;
  controller.Wait(*active_tab(), result.GetCallback());
  ASSERT_TRUE(DoesReachSteadyState(controller, State::kWaitForPageStability));
  EXPECT_FALSE(result.IsReady());

  // Ensure the tab can still produce frames while backgrounded.
  auto scoped_decrement_closure =
      web_contents()->IncrementCapturerCount(gfx::Size(),
                                             /*stay_hidden=*/false,
                                             /*stay_awake=*/true,
                                             /*is_activity=*/true);

  TabInterface* observed_tab = active_tab();
  ASSERT_TRUE(observed_tab->IsActivated());

  // Now open a new tab, putting the tab waiting on page stability in the
  // background.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ASSERT_FALSE(observed_tab->IsActivated());
  ASSERT_NE(active_tab(), observed_tab);

  // Ensure the controller doesn't break out of waiting for page stability.
  EXPECT_TRUE(DoesReachSteadyState(controller, State::kWaitForPageStability));
}

class ObservationDelayControllerLcpTest : public ObservationDelayTest {
 public:
  static constexpr int kLcpDelayInMs = 3000;
  ObservationDelayControllerLcpTest() {
    std::string lcp_delay = absl::StrFormat("%dms", kLcpDelayInMs);
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kGlicActor,
        {// Effectively disable the timeout to prevent flakes.
         {features::kGlicActorPageStabilityTimeout.name, "30000ms"},
         // Do not use min wait
         {features::kGlicActorPageStabilityMinWait.name, "0ms"},
         {features::kActorObservationDelayLcp.name, lcp_delay}});
  }
  ~ObservationDelayControllerLcpTest() override = default;

 private:
  ScopedFeatureList scoped_feature_list_;
};

// Tests that no delay is applied when LCP is already available.
IN_PROC_BROWSER_TEST_F(ObservationDelayControllerLcpTest, NoDelayWhenLcpReady) {
  const GURL url = embedded_test_server()->GetURL("/title1.html");

  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      web_contents());
  waiter->AddPageExpectation(page_load_metrics::PageLoadMetricsTestWaiter::
                                 TimingField::kLargestContentfulPaint);

  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Wait for the LCP metric to be fully reported to the browser process.
  waiter->Wait();

  TestObservationDelayController controller(*main_frame(), actor::TaskId(),
                                            journal(), PageStabilityConfig());

  base::ElapsedTimer timer;
  TestFuture<ObservationDelayController::Result> result;
  controller.Wait(*active_tab(), result.GetCallback());

  ASSERT_TRUE(controller.WaitForState(State::kMaybeDelayForLcp));
  ASSERT_TRUE(result.Wait());

  // Since the page had a paint, LCP is considered valid, and we should not
  // have applied the delay.
  EXPECT_LT(timer.Elapsed(), base::Milliseconds(kLcpDelayInMs));
}

// Tests that the LCP delay is correctly applied when a standard page is loaded
// that has no content to paint (and thus no LCP).
IN_PROC_BROWSER_TEST_F(ObservationDelayControllerLcpTest,
                       DelayIsAppliedForPageWithNoContent) {
  // Navigate to an empty html page. This is a standard navigation, so the
  // PageLoadMetrics system will run, but no LCP will ever be recorded
  // because there is no content.
  const GURL url = embedded_test_server()->GetURL("/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  TestObservationDelayController controller(*main_frame(), actor::TaskId(),
                                            journal(), PageStabilityConfig());

  base::ElapsedTimer timer;
  TestFuture<ObservationDelayController::Result> result;
  controller.Wait(*active_tab(), result.GetCallback());

  ASSERT_TRUE(controller.WaitForState(State::kMaybeDelayForLcp));
  ASSERT_TRUE(controller.WaitForState(State::kDelayForLcp));
  ASSERT_TRUE(result.Wait());

  // The total time should be at least the LCP delay, because the empty page
  // is tracked but has no contentful paint.
  EXPECT_GE(timer.Elapsed(), base::Milliseconds(kLcpDelayInMs));
}

class ObservationDelayControllerExcludeAdRequestsTest
    : public ObservationDelayControllerTest,
      public base::test::WithFeatureOverride {
 public:
  ObservationDelayControllerExcludeAdRequestsTest()
      : base::test::WithFeatureOverride(
            features::kGlicActorObservationDelayExcludeAdFrameLoading) {}
  ~ObservationDelayControllerExcludeAdRequestsTest() override = default;
};

IN_PROC_BROWSER_TEST_P(ObservationDelayControllerExcludeAdRequestsTest,
                       ExcludeAdIframeLoad) {
  const GURL url = embedded_test_server()->GetURL("/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Append an iframe.
  EXPECT_TRUE(content::ExecJs(main_frame(), R"(
      const frame = document.createElement('iframe');
      frame.id = 'child'
      document.body.appendChild(frame);
    )"));
  RenderFrameHost* iframe_rfh = content::ChildFrameAt(main_frame(), 0);
  ASSERT_TRUE(iframe_rfh);

  // Mark the iframe as an ad frame.
  iframe_rfh->UpdateIsAdFrame(/*is_ad_frame=*/true);

  const GURL iframe_url = embedded_test_server()->GetURL("/actor/simple.html");
  TestNavigationManager iframe_manager(web_contents(), iframe_url);

  TestObservationDelayController controller(*main_frame(), actor::TaskId(),
                                            journal(), PageStabilityConfig());

  // Initiate iframe navigation.
  ASSERT_TRUE(BeginNavigateIframeToURL(web_contents(), /*iframe_id=*/"child",
                                       iframe_url));
  ASSERT_TRUE(iframe_manager.WaitForRequestStart());

  // The frame tree is loading because the iframe is loading. However, it is
  // considered as not loading when excluding ad frames.
  ASSERT_TRUE(web_contents()->IsLoading());
  ASSERT_FALSE(web_contents()->IsLoadingExcludingAdSubframes());

  TestFuture<ObservationDelayController::Result> result;
  controller.Wait(*active_tab(), result.GetCallback());

  // Regardless of the feature status, the controller should move to wait for
  // load completion state after the wait starts.
  ASSERT_TRUE(controller.WaitForState(State::kWaitForLoadCompletion));

  if (IsParamFeatureEnabled()) {
    // The controller immediately advances to the next state as it excludes ad
    // frames when waiting for load completion.
    ASSERT_TRUE(controller.WaitForState(State::kWaitForVisualStateUpdate));
    ASSERT_TRUE(controller.WaitForState(State::kMaybeDelayForLcp));
    ASSERT_TRUE(controller.WaitForState(State::kDone));
  } else {
    // The controller should stay in the waiting for load completion state until
    // the iframe navigation finishes.
    ASSERT_TRUE(
        DoesReachSteadyState(controller, State::kWaitForLoadCompletion));

    // Complete the navigation. The controller should wait for visual update,
    // then complete.
    ASSERT_TRUE(iframe_manager.WaitForNavigationFinished());
    ASSERT_TRUE(controller.WaitForState(State::kWaitForVisualStateUpdate));
    ASSERT_TRUE(controller.WaitForState(State::kMaybeDelayForLcp));
    ASSERT_TRUE(controller.WaitForState(State::kDone));
  }

  ASSERT_TRUE(result.Wait());
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(
    ObservationDelayControllerExcludeAdRequestsTest);

}  // namespace

}  // namespace actor
