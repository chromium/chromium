// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/observation_delay_controller.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/actor/task_id.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace actor {

namespace {

using ::base::test::ScopedFeatureList;
using ::base::test::TestFuture;
using ::content::BeginNavigateToURLFromRenderer;
using ::content::EvalJs;
using ::content::RenderFrameHost;
using ::content::TestNavigationManager;
using ::content::WebContents;
using ::tabs::TabInterface;

using State = ::actor::ObservationDelayController::State;

std::string_view ToString(State state) {
  return ObservationDelayController::StateToString(state);
}

const char* kFetchPath = "/fetchtarget.html";
const char* kTestPage = "/actor/observation_delay.html";

// Helper to start a navigation in the main frame to a page that reaches
// DOMContentLoaded in the main frame but doesn't reach the load event until
// `RunToLoadEvent` is called. It does this by deferring a subframe navigation.
class NavigateToLoadDeferredPage {
 public:
  NavigateToLoadDeferredPage(WebContents* web_contents,
                             net::EmbeddedTestServer* server)
      : url_main_frame_(server->GetURL("/actor/simple_iframe.html")),
        url_subframe_(server->GetURL("/actor/blank.html")),
        web_contents_(web_contents) {
    subframe_manager_ =
        std::make_unique<TestNavigationManager>(web_contents, url_subframe_);
    main_manager_ =
        std::make_unique<TestNavigationManager>(web_contents, url_main_frame_);
  }

  [[nodiscard]] bool RunToDOMContentLoadedEvent() {
    // Now start a navigation to a new document that has an iframe. Block the
    // iframe's navigation to control the load event timing.
    bool begin_navigate =
        BeginNavigateToURLFromRenderer(web_contents_, url_main_frame_);
    EXPECT_TRUE(begin_navigate);
    if (!begin_navigate) {
      return false;
    }

    // Wait for the main frame navigation to finish and for the main document to
    // reach DOMContentLoaded and for a frame to be presented but prevent the
    // subframe from completing.
    bool wait_for_main_finished = main_manager_->WaitForNavigationFinished();
    EXPECT_TRUE(wait_for_main_finished);
    if (!wait_for_main_finished) {
      return false;
    }

    bool wait_for_dom_content_loaded =
        WaitForDOMContentLoaded(web_contents_->GetPrimaryMainFrame());
    EXPECT_TRUE(wait_for_dom_content_loaded);
    if (!wait_for_dom_content_loaded) {
      return false;
    }

    WaitForCopyableViewInWebContents(web_contents_);
    bool wait_for_subframe_response = subframe_manager_->WaitForResponse();
    EXPECT_TRUE(wait_for_subframe_response);
    return wait_for_subframe_response;
  }

  [[nodiscard]] bool RunToLoadEvent() {
    return subframe_manager_->WaitForNavigationFinished();
  }

 private:
  const GURL url_main_frame_;
  const GURL url_subframe_;
  WebContents* web_contents_;

  std::unique_ptr<TestNavigationManager> subframe_manager_;
  std::unique_ptr<TestNavigationManager> main_manager_;
};

class TestObservationDelayController : public ObservationDelayController {
 public:
  TestObservationDelayController(
      RenderFrameHost& target_frame,
      TaskId task_id,
      AggregatedJournal& journal,
      std::optional<PageStabilityConfig> page_stability_config)
      : ObservationDelayController(target_frame,
                                   task_id,
                                   journal,
                                   page_stability_config) {
    // Ensure the monitor is created in the renderer before returning. This
    // ensures the PageStabilityMonitor captures the initial state at the
    // current point in the test.
    page_stability_monitor_remote_.FlushForTesting();
  }
  ~TestObservationDelayController() override = default;

  [[nodiscard]] bool WaitForState(State state) {
    if (state_ == state) {
      return true;
    }

    base::RunLoop run_loop;
    waiting_state_ = state;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
    waiting_state_.reset();
    return state_ == state;
  }
  State GetState() const { return state_; }

 protected:
  void SetState(State state) override {
    ObservationDelayController::SetState(state);
    if (!waiting_state_) {
      return;
    }

    if (*waiting_state_ == state) {
      std::move(quit_closure_).Run();
    } else if (state == State::kDone) {
      ADD_FAILURE()
          << "ObservationDelayController completed without reaching waited on "
             "value: "
          << ToString(*waiting_state_);
      std::move(quit_closure_).Run();
    }
  }

  std::optional<State> waiting_state_;
  base::OnceClosure quit_closure_;
};

// TODO(bokan) - Factor out into a common test harness with
// page_stability_browsertest.cc
class ObservationDelayControllerTest : public InProcessBrowserTest {
 public:
  ObservationDelayControllerTest() {
    // GlicActor is actually unneeded but enabled solely to set params.
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{features::kGlicActor,
          {{features::kActorGeneralPageStabilityMode.name,
            features::kActorGeneralPageStabilityMode.GetName(
                features::ActorGeneralPageStabilityMode::kAllEnabled)},
           // Effectively disable the timeouts to prevent flakes.
           {"glic-actor-page-stability-local-timeout", "30000ms"},
           {"glic-actor-page-stability-timeout", "30000ms"},
           // Do not use an invoke delay
           {"glic-actor-page-stability-invoke-callback-delay", "0ms"}}},
         {features::kGlic, {}},
         {features::kTabstripComboButton, {}}},
        /*disabled_features=*/{features::kGlicWarming});
  }
  ObservationDelayControllerTest(const ObservationDelayControllerTest&) =
      delete;
  ObservationDelayControllerTest& operator=(
      const ObservationDelayControllerTest&) = delete;

  ~ObservationDelayControllerTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    fetch_response_ =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            embedded_test_server(), kFetchPath);

    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

  // Pause execution for the specified amount of time.
  void Sleep(base::TimeDelta delta) {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), delta);
    run_loop.Run();
  }

  // Sleep long enough to verify that a state we're in is steady. This is, of
  // course, non difinitive but in practice should shake out any cases where the
  // state isn't steady. Scales the tiny timeout for more certainty.
  void SteadyStateSleep() {
    base::TimeDelta timeout = TestTimeouts::tiny_timeout() * 3;
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), timeout);
    run_loop.Run();
  }

  WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  RenderFrameHost* main_frame() {
    return web_contents()->GetPrimaryMainFrame();
  }

  TabInterface* active_tab() { return chrome_test_utils::GetActiveTab(this); }

  std::string GetOutputText() {
    return EvalJs(web_contents(), "document.getElementById('output').innerText")
        .ExtractString();
  }

  net::test_server::ControllableHttpResponse& fetch_response() {
    return *fetch_response_;
  }

  actor::AggregatedJournal& journal() { return journal_; }

  void RespondToFetchRequest(std::string_view text) {
    fetch_response_->Send(net::HTTP_OK, /*content_type=*/"text/html",
                          /*content=*/"",
                          /*cookies=*/{}, /*extra_headers=*/{});
    fetch_response_->Send(std::string(text));
    fetch_response_->Done();
  }

  ObservationDelayController::PageStabilityConfig PageStabilityConfig() const {
    // Use default values.
    return ObservationDelayController::PageStabilityConfig();
  }

  [[nodiscard]] bool InitiateFetchRequest() {
    // Perform a same-document navigation. The page has a navigation handler
    // that will initiate a fetch from this event.  This works via the
    // navigation handler on the harness' test page.
    CHECK_EQ(web_contents()->GetURL(),
             embedded_test_server()->GetURL(kTestPage));
    CHECK_EQ(GetOutputText(), "INITIAL");

    const GURL hash_navigation_to_initiate_fetch =
        embedded_test_server()->GetURL("/actor/observation_delay.html#fetch");

    bool navigate_result = content::NavigateToURL(
        web_contents(), hash_navigation_to_initiate_fetch);
    EXPECT_TRUE(navigate_result);
    if (!navigate_result) {
      return false;
    }

    fetch_response().WaitForRequest();
    // The page should not receive a response until `Respond` is called.
    EXPECT_EQ(GetOutputText(), "INITIAL");
    return true;
  }

  [[nodiscard]] bool DoesReachSteadyState(
      TestObservationDelayController& controller,
      State state) {
    if (!controller.WaitForState(state)) {
      return false;
    }

    // Ensure the controller stays there for some time.
    SteadyStateSleep();
    EXPECT_EQ(ToString(controller.GetState()), ToString(state));
    return controller.GetState() == state;
  }

 private:
  actor::AggregatedJournal journal_;
  std::unique_ptr<actor::AggregatedJournal::PendingAsyncEntry> journal_entry_;

  std::unique_ptr<net::test_server::ControllableHttpResponse> fetch_response_;

  std::unique_ptr<ObservationDelayController> controller_;
  ScopedFeatureList scoped_feature_list_;
};

// Ensure that a navigation while the page stability monitor is in-progress
// moves the controller to wait on the load.
IN_PROC_BROWSER_TEST_F(ObservationDelayControllerTest,
                       NavigateDuringPageStabilization) {
  // TODO(b/447664500): Remove when fixed.
  content::DisableBackForwardCacheForTesting(
      web_contents(), content::BackForwardCache::DisableForTestingReason::
                          TEST_REQUIRES_NO_CACHING);

  const GURL url = embedded_test_server()->GetURL(kTestPage);
  const GURL url2 = embedded_test_server()->GetURL("/actor/blank.html");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  TestObservationDelayController controller(*main_frame(), actor::TaskId(),
                                            journal(), PageStabilityConfig());

  // Initiate a fetch to block page stability.
  ASSERT_TRUE(InitiateFetchRequest());

  // Start waiting on the controller. It should be blocked in page stability.
  TestFuture<void> result;
  controller.Wait(*active_tab(), result.GetCallback());
  ASSERT_TRUE(DoesReachSteadyState(controller, State::kWaitForPageStability));

  TestNavigationManager manager(web_contents(), url2);
  ASSERT_TRUE(BeginNavigateToURLFromRenderer(web_contents(), url2));

  // Stop before committing the navigation. The observer should remain waiting
  // on page stability.
  ASSERT_TRUE(manager.WaitForResponse());
  ASSERT_TRUE(DoesReachSteadyState(controller, State::kWaitForPageStability));

  // Complete the navigation. The controller should wait for load, then a visual
  // update, then complete.
  ASSERT_TRUE(manager.WaitForNavigationFinished());
  ASSERT_TRUE(controller.WaitForState(State::kWaitForLoadCompletion));
  ASSERT_TRUE(controller.WaitForState(State::kWaitForVisualStateUpdate));
  ASSERT_TRUE(controller.WaitForState(State::kDone));
  ASSERT_TRUE(result.Wait());
}

IN_PROC_BROWSER_TEST_F(ObservationDelayControllerTest,
                       UsePageStabilityForSameDocumentNavigation) {
  const GURL url = embedded_test_server()->GetURL(kTestPage);
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  TestObservationDelayController controller(*main_frame(), actor::TaskId(),
                                            journal(), PageStabilityConfig());

  // Perform a same-document navigation. The page has a navigation handler
  // that will initiate a fetch from this event.
  ASSERT_TRUE(InitiateFetchRequest());

  // Start waiting on the controller. It should be blocked in page stability.
  TestFuture<void> result;
  controller.Wait(*active_tab(), result.GetCallback());

  ASSERT_TRUE(DoesReachSteadyState(controller, State::kWaitForPageStability));
  EXPECT_FALSE(result.IsReady());

  RespondToFetchRequest("TEST COMPLETE");

  ASSERT_TRUE(controller.WaitForState(State::kWaitForLoadCompletion));
  ASSERT_TRUE(controller.WaitForState(State::kWaitForVisualStateUpdate));
  ASSERT_TRUE(result.Wait());
  ASSERT_EQ(GetOutputText(), "TEST COMPLETE");
}

// Test waiting on a new document load after waiting for the page to stabilize.
IN_PROC_BROWSER_TEST_F(ObservationDelayControllerTest, LoadAfterStability) {
  // TODO(b/447664500): Remove when fixed.
  content::DisableBackForwardCacheForTesting(
      web_contents(), content::BackForwardCache::DisableForTestingReason::
                          TEST_REQUIRES_NO_CACHING);
  const GURL url =
      embedded_test_server()->GetURL("/actor/observation_delay.html");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

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

  // Unblock the subframe, the controller should now proceed through the
  // remaining states.
  ASSERT_TRUE(deferred_navigation.RunToLoadEvent());

  ASSERT_TRUE(controller.WaitForState(State::kWaitForVisualStateUpdate));
  ASSERT_TRUE(controller.WaitForState(State::kDone));
  ASSERT_TRUE(result.Wait());
}

// Ensure that putting a tab into the background while its waiting to stabilize
// doesn't affect the PageStabilityMonitor.
// TODO(b/448641423): This test better belongs in PageStabilityMonitor browser
// tests but is much clearer to write here. Move once the tests are sharing
// infrastructure.
IN_PROC_BROWSER_TEST_F(ObservationDelayControllerTest,
                       BackgroundTabWhileWaitingForStability) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/observation_delay.html");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  TestObservationDelayController controller(*main_frame(), actor::TaskId(),
                                            journal(), PageStabilityConfig());

  ASSERT_TRUE(InitiateFetchRequest());

  // Start waiting, since a fetch is in progress we should be waiting for page
  // stability.
  TestFuture<void> result;
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

}  // namespace

}  // namespace actor
