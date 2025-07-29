// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/execution_engine.h"

#include <optional>
#include <string_view>

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/click_tool_request.h"
#include "chrome/browser/actor/tools/tab_management_tool_request.h"
#include "chrome/browser/actor/ui/event_dispatcher.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/optimization_guide/browser_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"

using ::base::test::TestFuture;
using ::optimization_guide::proto::BrowserAction;
using ::optimization_guide::proto::ClickAction;

namespace actor {

namespace {

class ExecutionEngineBrowserTest : public InProcessBrowserTest {
 public:
  ExecutionEngineBrowserTest()
      : prerender_helper_(
            base::BindRepeating(&ExecutionEngineBrowserTest::web_contents,
                                base::Unretained(this))) {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlic, features::kTabstripComboButton,
                              features::kGlicActor},
        /*disabled_features=*/{features::kGlicWarming});
  }
  ExecutionEngineBrowserTest(const ExecutionEngineBrowserTest&) = delete;
  ExecutionEngineBrowserTest& operator=(const ExecutionEngineBrowserTest&) =
      delete;

  ~ExecutionEngineBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    SetUpBlocklist(command_line, "blocked.example.com");
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(embedded_https_test_server().Start());

    auto execution_engine =
        std::make_unique<ExecutionEngine>(browser()->profile());
    ExecutionEngine* raw_execution_engine = execution_engine.get();
    auto event_dispatcher = ui::NewUiEventDispatcher(
        actor_keyed_service()->GetActorUiStateManager());
    auto task = std::make_unique<ActorTask>(
        GetProfile(), std::move(execution_engine), std::move(event_dispatcher));
    raw_execution_engine->SetOwner(task.get());
    task_id_ = actor_keyed_service()->AddActiveTask(std::move(task));

    // Optimization guide uses this histogram to signal initialization in tests.
    optimization_guide::RetryForHistogramUntilCountReached(
        &histogram_tester_for_init_,
        "OptimizationGuide.HintsManager.HintCacheInitialized", 1);
  }

 protected:
  tabs::TabInterface* active_tab() {
    return browser()->tab_strip_model()->GetActiveTab();
  }

  content::WebContents* web_contents() { return active_tab()->GetContents(); }

  content::RenderFrameHost* main_frame() {
    return web_contents()->GetPrimaryMainFrame();
  }

  ActorKeyedService* actor_keyed_service() {
    return ActorKeyedService::Get(browser()->profile());
  }

  ActorTask& actor_task() { return *actor_keyed_service()->GetTask(task_id_); }

  void ClickTarget(
      std::string_view query_selector,
      mojom::ActionResultCode expected_code = mojom::ActionResultCode::kOk) {
    std::optional<int> dom_node_id =
        content::GetDOMNodeId(*main_frame(), query_selector);
    ASSERT_TRUE(dom_node_id);
    std::unique_ptr<ToolRequest> click =
        MakeClickRequest(*main_frame(), dom_node_id.value());
    TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result;
    actor_task().Act(ToRequestList(click), result.GetCallback());
    if (expected_code == mojom::ActionResultCode::kOk) {
      ExpectOkResult(result);
    } else {
      ExpectErrorResult(result, expected_code);
    }
  }

  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_helper_;
  }

 private:
  TaskId task_id_;
  content::test::PrerenderTestHelper prerender_helper_;
  base::HistogramTester histogram_tester_for_init_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// The coordinator does not yet handle multi-tab cases. For now,
// while acting on a tab, we override attempts by the page to create new
// tabs, and instead navigate the existing tab.
IN_PROC_BROWSER_TEST_F(ExecutionEngineBrowserTest, ForceSameTabNavigation) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/target_blank_links.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Check specifically that it's the existing frame that navigates.
  content::TestFrameNavigationObserver frame_nav_observer(main_frame());
  ClickTarget("#anchorTarget");
  frame_nav_observer.Wait();
}

IN_PROC_BROWSER_TEST_F(ExecutionEngineBrowserTest,
                       ForceSameTabNavigationByScript) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/target_blank_links.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Check specifically that it's the existing frame that navigates.
  content::TestFrameNavigationObserver frame_nav_observer(main_frame());
  ClickTarget("#scriptOpen");
  frame_nav_observer.Wait();
}

IN_PROC_BROWSER_TEST_F(ExecutionEngineBrowserTest, TwoClicks) {
  const GURL url = embedded_test_server()->GetURL("/actor/two_clicks.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Check initial background color is red
  EXPECT_EQ("red", EvalJs(web_contents(), "document.body.bgColor"));

  // Create a single BrowserAction with two click actions
  std::optional<int> button1_id =
      content::GetDOMNodeId(*main_frame(), "#button1");
  std::optional<int> button2_id =
      content::GetDOMNodeId(*main_frame(), "#button2");
  ASSERT_TRUE(button1_id);
  ASSERT_TRUE(button2_id);

  std::unique_ptr<ToolRequest> click1 =
      MakeClickRequest(*main_frame(), button1_id.value());
  std::unique_ptr<ToolRequest> click2 =
      MakeClickRequest(*main_frame(), button2_id.value());

  // Execute the action
  TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result;
  actor_task().Act(ToRequestList(click1, click2), result.GetCallback());
  ExpectOkResult(result);

  // Check background color changed to green
  EXPECT_EQ("green", EvalJs(web_contents(), "document.body.bgColor"));
}

IN_PROC_BROWSER_TEST_F(ExecutionEngineBrowserTest, TwoClicksInBackgroundTab) {
  const GURL url = embedded_test_server()->GetURL("/actor/two_clicks.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Check initial background color is red
  EXPECT_EQ("red", EvalJs(web_contents(), "document.body.bgColor"));

  // Store a pointer to the first tab.
  content::WebContents* first_tab_contents = web_contents();
  auto* tab = browser()->GetActiveTabInterface();

  // Create a second tab, which will be in the foreground.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // The first tab should now be in the background.
  ASSERT_TRUE(!tab->IsVisible());

  // Create a single Actions proto with two click actions on the background tab.
  std::optional<int> button1_id = content::GetDOMNodeId(
      *first_tab_contents->GetPrimaryMainFrame(), "#button1");
  std::optional<int> button2_id = content::GetDOMNodeId(
      *first_tab_contents->GetPrimaryMainFrame(), "#button2");
  ASSERT_TRUE(button1_id);
  ASSERT_TRUE(button2_id);

  std::unique_ptr<ToolRequest> click1 = MakeClickRequest(
      *first_tab_contents->GetPrimaryMainFrame(), button1_id.value());
  std::unique_ptr<ToolRequest> click2 = MakeClickRequest(
      *first_tab_contents->GetPrimaryMainFrame(), button2_id.value());

  // Execute the actions.
  TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result;
  actor_task().Act(ToRequestList(click1, click2), result.GetCallback());

  // Check that the action succeeded.
  ExpectOkResult(*result.Get<0>());

  // Check background color changed to green in the background tab.
  EXPECT_EQ("green", EvalJs(tab->GetContents(), "document.body.bgColor"));
}

IN_PROC_BROWSER_TEST_F(ExecutionEngineBrowserTest, ClickLinkToBlockedSite) {
  const GURL start_url = embedded_https_test_server().GetURL(
      "example.com", "/actor/blocked_links.html");
  const GURL blocked_url = embedded_https_test_server().GetURL(
      "blocked.example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  EXPECT_TRUE(content::ExecJs(
      web_contents(), content::JsReplace("setBlockedSite($1);", blocked_url)));
  ClickTarget("#directToBlocked",
              mojom::ActionResultCode::kTriggeredNavigationBlocked);
}

IN_PROC_BROWSER_TEST_F(ExecutionEngineBrowserTest,
                       ClickLinkToBlockedSiteWithRedirect) {
  const GURL start_url = embedded_https_test_server().GetURL(
      "example.com", "/actor/blocked_links.html");
  const GURL blocked_url = embedded_https_test_server().GetURL(
      "blocked.example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  EXPECT_TRUE(content::ExecJs(
      web_contents(), content::JsReplace("setBlockedSite($1);", blocked_url)));
  ClickTarget("#redirectToBlocked",
              mojom::ActionResultCode::kTriggeredNavigationBlocked);
}

IN_PROC_BROWSER_TEST_F(ExecutionEngineBrowserTest, PrerenderBlockedSite) {
  const GURL start_url = embedded_https_test_server().GetURL(
      "example.com", "/actor/blocked_links.html");
  const GURL blocked_url = embedded_https_test_server().GetURL(
      "blocked.example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  EXPECT_TRUE(content::ExecJs(
      web_contents(), content::JsReplace("setBlockedSite($1);", blocked_url)));

  base::RunLoop loop;
  actor_task().AddTab(
      active_tab()->GetHandle(),
      base::BindLambdaForTesting([&](mojom::ActionResultPtr result) {
        EXPECT_TRUE(IsOk(*result));
        loop.Quit();
      }));
  loop.Run();

  // While we have an active task, cancel any prerenders which would be to a
  // blocked site.
  content::test::PrerenderHostObserver prerender_observer(*web_contents(),
                                                          blocked_url);
  prerender_helper().AddPrerenderAsync(blocked_url);
  prerender_observer.WaitForDestroyed();

  ClickTarget("#directToBlocked",
              mojom::ActionResultCode::kTriggeredNavigationBlocked);
}

}  // namespace

}  // namespace actor
