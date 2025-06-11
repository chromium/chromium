// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/synchronization/lock.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"

namespace actor {

namespace {

using base::test::ScopedFeatureList;
using base::test::TestFuture;
using content::EvalJs;
using content::ExecJs;
using ::content::JsReplace;
using ::content::RenderFrameHost;
using ::content::WebContents;
using optimization_guide::proto::BrowserAction;
using optimization_guide::proto::ClickAction;

// Note: this file doesn't actually exist, the response is manually provided by
// tests.
const char* kFetchPath = "/fetchtarget.html";

// Tests for the PageStabilityMonitor's functionality of delaying renderer-tool
// completion until the page is ready for an observation.
class ActorPageStabilityTest : public InProcessBrowserTest {
 public:
  ActorPageStabilityTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlic, features::kTabstripComboButton,
                              features::kGlicActor},
        /*disabled_features=*/{features::kGlicWarming});
  }
  ActorPageStabilityTest(const ActorPageStabilityTest&) = delete;
  ActorPageStabilityTest& operator=(const ActorPageStabilityTest&) = delete;

  ~ActorPageStabilityTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    fetch_response_ =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            embedded_test_server(), kFetchPath);

    ASSERT_TRUE(embedded_test_server()->Start());
    auto execution_engine = std::make_unique<ExecutionEngine>(
        browser()->profile(), browser()->GetActiveTabInterface());
    actor_task_ = std::make_unique<ActorTask>(std::move(execution_engine));
  }

  void TearDownOnMainThread() override {
    // The execution engine has a pointer to the profile, which must be released
    // before the browser is torn down to avoid a dangling pointer.
    actor_task_.reset();
  }

  // Pause execution for 300ms - matching the busy work delay in
  // page_stability.html
  void Sleep300ms() {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(300));
    run_loop.Run();
  }

  WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  RenderFrameHost* main_frame() {
    return web_contents()->GetPrimaryMainFrame();
  }

  std::string GetFetchOutput() {
    return EvalJs(web_contents(), "document.getElementById('output').innerText")
        .ExtractString();
  }

  ExecutionEngine& execution_engine() {
    return *actor_task_->GetExecutionEngine();
  }

  net::test_server::ControllableHttpResponse& fetch_response() {
    return *fetch_response_;
  }

  void Respond(std::string_view text) {
    fetch_response_->Send(net::HTTP_OK, /*content_type=*/"text/html",
                          /*content=*/"",
                          /*cookies=*/{}, /*extra_headers=*/{});
    fetch_response_->Send(std::string(text));
    fetch_response_->Done();
  }

 private:
  std::unique_ptr<net::test_server::ControllableHttpResponse> fetch_response_;
  ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<ActorTask> actor_task_;
};

// Ensure the page isn't considered stable until after a network fetch is
// resolved.
IN_PROC_BROWSER_TEST_F(ActorPageStabilityTest, WaitOnNetworkFetch) {
  const GURL url = embedded_test_server()->GetURL("/actor/page_stability.html");
  const GURL url_fetch = embedded_test_server()->GetURL(kFetchPath);
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  ASSERT_EQ(GetFetchOutput(), "INITIAL");

  std::optional<int> button_id = GetDOMNodeId(*main_frame(), "#btnFetch");
  ASSERT_TRUE(button_id);
  BrowserAction action = MakeClick(*main_frame(), button_id.value());
  TestFuture<mojom::ActionResultPtr> result;
  execution_engine().Act(action, result.GetCallback());

  fetch_response().WaitForRequest();

  Sleep300ms();

  // The fetch hasn't resolved yet, the tool use shouldn't have returned yet
  // either.
  ASSERT_EQ(GetFetchOutput(), "INITIAL");
  EXPECT_FALSE(result.IsReady());

  Respond("NETWORK DONE");

  ExpectOkResult(result);
  ASSERT_EQ(GetFetchOutput(), "NETWORK DONE");
}

// Simulate a network fetch followed by heavy main thread activity. Ensure the
// page isn't considered stable until after the main thread work finishes.
IN_PROC_BROWSER_TEST_F(ActorPageStabilityTest, WaitOnFetchAndWork) {
  const GURL url = embedded_test_server()->GetURL("/actor/page_stability.html");
  const GURL url_fetch = embedded_test_server()->GetURL(kFetchPath);
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  ASSERT_EQ(GetFetchOutput(), "INITIAL");

  std::optional<int> button_id =
      GetDOMNodeId(*main_frame(), "#btnFetchAndWork");
  ASSERT_TRUE(button_id);
  BrowserAction action = MakeClick(*main_frame(), button_id.value());
  TestFuture<mojom::ActionResultPtr> result;
  execution_engine().Act(action, result.GetCallback());
  fetch_response().WaitForRequest();

  Sleep300ms();

  EXPECT_FALSE(result.IsReady());
  ASSERT_EQ(GetFetchOutput(), "INITIAL");

  // Respond to the fetch, this will start 3 tasks of 300ms each on the main
  // thead.
  Respond("NETWORK DONE");
  Sleep300ms();

  // The fetch should have resolved but the main thread is busy so the
  // page isn't yet stable.
  ASSERT_EQ(GetFetchOutput(), "NETWORK DONE");
  EXPECT_FALSE(result.IsReady());

  Sleep300ms();

  EXPECT_FALSE(result.IsReady());

  // Wait and the main thread will eventually finish.
  ExpectOkResult(result);
  ASSERT_EQ(GetFetchOutput(), "WORK DONE");
}

// Shorten timeouts to test they work.
// LocalTimeout is the timeout delay used when waiting on non-network actions
// like an idle main thread and display compositor frame presentation.
// GlobalTimeout is the timeout delay used end-to-end in the
template <int LocalTimeout, int GlobalTimeout>
class ActorPageStabilityTimeoutTest : public ActorPageStabilityTest {
 public:
  ActorPageStabilityTimeoutTest() {
    std::string local_timeout = absl::StrFormat("%dms", LocalTimeout);
    std::string global_timeout = absl::StrFormat("%dms", GlobalTimeout);
    timeout_scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{{features::kGlic, {}},
                              {features::kTabstripComboButton, {}},
                              {features::kGlicActor,
                               {{"glic-actor-observation-delay", local_timeout},
                                {"glic-actor-page-stability-timeout",
                                 global_timeout}}}},
        /*disabled_features=*/{features::kGlicWarming});
  }
  ActorPageStabilityTimeoutTest(const ActorPageStabilityTimeoutTest&) = delete;
  ActorPageStabilityTimeoutTest& operator=(
      const ActorPageStabilityTimeoutTest&) = delete;

  ~ActorPageStabilityTimeoutTest() override = default;

 private:
  ScopedFeatureList timeout_scoped_feature_list_;
};

// Shorten the timeout under test and make the other timeout very long to avoid
// tripping it.
using ActorPageStabilityLocalTimeoutTest =
    ActorPageStabilityTimeoutTest<100, 100000>;
using ActorPageStabilityGlobalTimeoutTest =
    ActorPageStabilityTimeoutTest<100000, 100>;

// Ensure that if a network request runs long, the stability monitor will
// eventually timeout.
IN_PROC_BROWSER_TEST_F(ActorPageStabilityGlobalTimeoutTest, NetworkTimeout) {
  const GURL url = embedded_test_server()->GetURL("/actor/page_stability.html");
  const GURL url_fetch = embedded_test_server()->GetURL(kFetchPath);
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  ASSERT_EQ(GetFetchOutput(), "INITIAL");

  std::optional<int> button_id =
      GetDOMNodeId(*main_frame(), "#btnFetchAndWork");
  ASSERT_TRUE(button_id);
  BrowserAction action = MakeClick(*main_frame(), button_id.value());
  TestFuture<mojom::ActionResultPtr> result;
  execution_engine().Act(action, result.GetCallback());

  // Never respond to the request
  fetch_response().WaitForRequest();

  // Ensure the stability monitor eventually allows completion.
  ExpectOkResult(result);
  ASSERT_EQ(GetFetchOutput(), "INITIAL");
}

// Ensure that if the main thread never becomes idle the stability monitor will
// eventually timeout.
IN_PROC_BROWSER_TEST_F(ActorPageStabilityGlobalTimeoutTest, BusyMainThread) {
  const GURL url = embedded_test_server()->GetURL("/actor/page_stability.html");
  const GURL url_fetch = embedded_test_server()->GetURL(kFetchPath);
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  std::optional<int> button_id = GetDOMNodeId(*main_frame(), "#btnWorkForever");
  ASSERT_TRUE(button_id);
  BrowserAction action = MakeClick(*main_frame(), button_id.value());
  TestFuture<mojom::ActionResultPtr> result;
  execution_engine().Act(action, result.GetCallback());

  // Ensure the stability monitor eventually allows completion.
  ExpectOkResult(result);
}

// Ensure that if the main thread never becomes idle the stability monitor will
// eventually timeout on the local timeout.
IN_PROC_BROWSER_TEST_F(ActorPageStabilityLocalTimeoutTest, BusyMainThread) {
  const GURL url = embedded_test_server()->GetURL("/actor/page_stability.html");
  const GURL url_fetch = embedded_test_server()->GetURL(kFetchPath);
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  std::optional<int> button_id = GetDOMNodeId(*main_frame(), "#btnWorkForever");
  ASSERT_TRUE(button_id);
  BrowserAction action = MakeClick(*main_frame(), button_id.value());
  TestFuture<mojom::ActionResultPtr> result;
  execution_engine().Act(action, result.GetCallback());

  // Ensure the stability monitor eventually allows completion.
  ExpectOkResult(result);
}

}  // namespace

}  // namespace actor
