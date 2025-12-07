// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/observation_delay_test_util.h"

#include <memory>
#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace actor {

namespace {

using State = ::actor::ObservationDelayController::State;

std::string_view ToString(State state) {
  return ObservationDelayController::StateToString(state);
}

}  // namespace

NavigateToLoadDeferredPage::NavigateToLoadDeferredPage(
    content::WebContents* web_contents,
    net::test_server::EmbeddedTestServer* server)
    : url_main_frame_(server->GetURL("/actor/simple_iframe.html")),
      url_subframe_(server->GetURL("/actor/blank.html")),
      web_contents_(web_contents) {
  subframe_manager_ = std::make_unique<content::TestNavigationManager>(
      web_contents, url_subframe_);
  main_manager_ = std::make_unique<content::TestNavigationManager>(
      web_contents, url_main_frame_);
}

NavigateToLoadDeferredPage::~NavigateToLoadDeferredPage() = default;

bool NavigateToLoadDeferredPage::RunToDOMContentLoadedEvent() {
  // Now start a navigation to a new document that has an iframe. Block the
  // iframe's navigation to control the load event timing.
  bool begin_navigate =
      content::BeginNavigateToURLFromRenderer(web_contents_, url_main_frame_);
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

bool NavigateToLoadDeferredPage::RunToLoadEvent() {
  return subframe_manager_->WaitForNavigationFinished();
}

TestObservationDelayController::TestObservationDelayController(
    content::RenderFrameHost& target_frame,
    TaskId task_id,
    AggregatedJournal& journal,
    PageStabilityConfig page_stability_config)
    : ObservationDelayController(target_frame,
                                 task_id,
                                 journal,
                                 page_stability_config) {
  // Ensure the monitor is created in the renderer before returning. This
  // ensures the PageStabilityMonitor captures the initial state at the
  // current point in the test.
  page_stability_monitor_remote_.FlushForTesting();
}

TestObservationDelayController::~TestObservationDelayController() = default;

bool TestObservationDelayController::WaitForState(State state) {
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

void TestObservationDelayController::SetState(State state) {
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

ObservationDelayTest::ObservationDelayTest() = default;

ObservationDelayTest::~ObservationDelayTest() = default;

tabs::TabInterface* ObservationDelayTest::active_tab() {
  return chrome_test_utils::GetActiveTab(this);
}

ObservationDelayController::PageStabilityConfig
ObservationDelayTest::PageStabilityConfig() const {
  // Use default values.
  return ObservationDelayController::PageStabilityConfig();
}

bool ObservationDelayTest::InitiateFetchRequest() {
  // Perform a same-document navigation. The page has a navigation handler
  // that will initiate a fetch from this event.  This works via the
  // navigation handler on the harness' test page.
  CHECK_EQ(web_contents()->GetURL(), GetPageStabilityTestURL());
  CHECK_EQ(GetOutputText(), "INITIAL");

  const GURL hash_navigation_to_initiate_fetch =
      embedded_test_server()->GetURL("/actor/page_stability.html#fetch");

  bool navigate_result =
      content::NavigateToURL(web_contents(), hash_navigation_to_initiate_fetch);
  EXPECT_TRUE(navigate_result);
  if (!navigate_result) {
    return false;
  }

  fetch_response().WaitForRequest();
  // The page should not receive a response until `Respond` is called.
  EXPECT_EQ(GetOutputText(), "INITIAL");
  return true;
}

// Sleep long enough to verify that a state we're in is steady. This is, of
// course, non difinitive but in practice should shake out any cases where the
// state isn't steady. Scales the tiny timeout for more certainty.
void ObservationDelayTest::SteadyStateSleep() {
  base::TimeDelta timeout = TestTimeouts::tiny_timeout() * 3;
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), timeout);
  run_loop.Run();
}

bool ObservationDelayTest::DoesReachSteadyState(
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

}  // namespace actor
