// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/execution_engine.h"

#include <optional>
#include <string_view>

#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/click_tool_request.h"
#include "chrome/browser/actor/tools/tab_management_tool_request.h"
#include "chrome/browser/actor/ui/event_dispatcher.h"
#include "chrome/browser/chrome_content_browser_client.h"
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
#include "components/optimization_guide/core/filters/optimization_hints_component_update_listener.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"

using ::base::test::TestFuture;
using ::optimization_guide::proto::ClickAction;

namespace actor {

namespace {
class FakeChromeContentBrowserClient : public ChromeContentBrowserClient {
 public:
  bool HandleExternalProtocol(
      const GURL& url,
      content::WebContents::Getter web_contents_getter,
      content::FrameTreeNodeId frame_tree_node_id,
      content::NavigationUIData* navigation_data,
      bool is_primary_main_frame,
      bool is_in_fenced_frame_tree,
      network::mojom::WebSandboxFlags sandbox_flags,
      ::ui::PageTransition page_transition,
      bool has_user_gesture,
      const std::optional<url::Origin>& initiating_origin,
      content::RenderFrameHost* initiator_document,
      const net::IsolationInfo& isolation_info,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>* out_factory)
      override {
    external_protocol_result_ =
        ChromeContentBrowserClient::HandleExternalProtocol(
            url, web_contents_getter, frame_tree_node_id, navigation_data,
            is_primary_main_frame, is_in_fenced_frame_tree, sandbox_flags,
            page_transition, has_user_gesture, initiating_origin,
            initiator_document, isolation_info, out_factory);

    return external_protocol_result_.value();
  }

  std::optional<bool> external_protocol_result() {
    return external_protocol_result_;
  }

 private:
  std::optional<bool> external_protocol_result_;
};

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

    StartNewTask();

    // Optimization guide uses this histogram to signal initialization in tests.
    optimization_guide::RetryForHistogramUntilCountReached(
        &histogram_tester_for_init_,
        "OptimizationGuide.HintsManager.HintCacheInitialized", 1);

    // Simulate the component loading, as the implementation checks it, but the
    // actual list is set via the command line.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    optimization_guide::OptimizationHintsComponentUpdateListener::GetInstance()
        ->MaybeUpdateHintsComponent(
            {base::Version("123"),
             temp_dir_.GetPath().Append(FILE_PATH_LITERAL("dont_care"))});

    content::SetBrowserClientForTesting(&mock_browser_client_);
  }

 protected:
  void StartNewTask() {
    auto execution_engine =
        std::make_unique<ExecutionEngine>(browser()->profile());
    ExecutionEngine* raw_execution_engine = execution_engine.get();
    auto event_dispatcher = ui::NewUiEventDispatcher(
        actor_keyed_service()->GetActorUiStateManager());
    auto task = std::make_unique<ActorTask>(
        GetProfile(), std::move(execution_engine), std::move(event_dispatcher));
    raw_execution_engine->SetOwner(task.get());
    task_id_ = actor_keyed_service()->AddActiveTask(std::move(task));
  }

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
    ActResultFuture result;
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

  FakeChromeContentBrowserClient& browser_client() {
    return mock_browser_client_;
  }

 private:
  TaskId task_id_;
  content::test::PrerenderTestHelper prerender_helper_;
  base::HistogramTester histogram_tester_for_init_;
  base::test::ScopedFeatureList scoped_feature_list_;
  FakeChromeContentBrowserClient mock_browser_client_;
  base::ScopedTempDir temp_dir_;
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
  ActResultFuture result;
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
  ActResultFuture result;
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

// Ensure that the block list is only active while the actor task is in
// progress.
IN_PROC_BROWSER_TEST_F(ExecutionEngineBrowserTest, AllowBlockedSiteWhenPaused) {
  const GURL start_url = embedded_https_test_server().GetURL(
      "example.com", "/actor/blocked_links.html");
  const GURL blocked_url = embedded_https_test_server().GetURL(
      "blocked.example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));

  // Arbitrary click to add the tab to the ActorTask.
  ClickTarget("h1");

  EXPECT_TRUE(content::ExecJs(
      web_contents(), content::JsReplace("setBlockedSite($1);", blocked_url)));

  // Pause the task as if the user took over. Blocked links should now be
  // allowed.
  actor_task().Pause(true);

  content::TestNavigationManager main_manager(web_contents(), blocked_url);

  EXPECT_TRUE(content::ExecJs(
      web_contents(), "document.getElementById('directToBlocked').click()"));

  ASSERT_TRUE(main_manager.WaitForNavigationFinished());
  EXPECT_TRUE(main_manager.was_committed());
  EXPECT_TRUE(main_manager.was_successful());
  EXPECT_EQ(web_contents()->GetURL(), blocked_url);
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

IN_PROC_BROWSER_TEST_F(ExecutionEngineBrowserTest, FirstActionOnBlockedSite) {
  const GURL start_url = embedded_https_test_server().GetURL(
      "blocked.example.com", "/actor/link.html");
  const GURL second_url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  EXPECT_TRUE(content::ExecJs(web_contents(),
                              content::JsReplace("setLink($1);", second_url)));

  ClickTarget("#link", mojom::ActionResultCode::kUrlBlocked);

  // Even though the first action failed, the tab should still be associated
  // with the task.
  EXPECT_TRUE(
      actor_task().GetLastActedTabs().contains(active_tab()->GetHandle()));
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

IN_PROC_BROWSER_TEST_F(ExecutionEngineBrowserTest,
                       ExternalProtocolLinkBlocked) {
  const GURL start_url = embedded_https_test_server().GetURL(
      "example.com", "/actor/external_protocol_links.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));

  ClickTarget("#mailto", mojom::ActionResultCode::kTriggeredNavigationBlocked);
}

// We need to follow a link which then spawns the external protocol request in
// an iframe to test this. If we launch click the external protocol link
// directly, its caught by the network throttler as seen in the test above. If
// we click a button that creates the iframe request directly, the actor will
// finish the task before ChromeContentBrowserClient has a chance to check for
// the actor task.
IN_PROC_BROWSER_TEST_F(ExecutionEngineBrowserTest,
                       BackgroundExternalProtocolBlocked) {
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  const GURL second_url = embedded_https_test_server().GetURL(
      "example.com", "/actor/external_protocol.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  EXPECT_TRUE(content::ExecJs(web_contents(),
                              content::JsReplace("setLink($1);", second_url)));

  ClickTarget("#link", mojom::ActionResultCode::kOk);

  EXPECT_FALSE(browser_client().external_protocol_result().value());
}

IN_PROC_BROWSER_TEST_F(ExecutionEngineBrowserTest, PromptToConfirmDownload) {
  ActorKeyedService* actor_service = actor_keyed_service();
  int32_t download_id = 123;

  // Mocking IPC for browsertest.
  // We will run it with a test response from the web client in a UI test.
  base::CallbackListSubscription user_confirmation_dialog_subscription =
      actor_service->AddRequestToShowUserConfirmationDialogSubscriberCallback(
          base::BindLambdaForTesting(
              [&](const std::optional<url::Origin>& got_navigation_origin,
                  const std::optional<int32_t> got_download_id,
                  ActorKeyedService::UserConfirmationDialogCallback callback) {
                // Verify the request is what the IPC expects.
                EXPECT_FALSE(got_navigation_origin);
                EXPECT_TRUE(got_download_id);
                EXPECT_EQ(got_download_id, download_id);
                // Send a mock IPC response.
                std::move(callback).Run(
                    webui::mojom::UserConfirmationDialogResponse::New(
                        webui::mojom::UserConfirmationDialogResult::
                            NewPermissionGranted(true)));
              }));

  base::test::TestFuture<webui::mojom::UserConfirmationDialogResponsePtr>
      future;
  actor_task().GetExecutionEngine()->PromptToConfirmDownload(
      download_id, future.GetCallback());

  // Verify response was forwarded to the callback correctly.
  auto response = future.Take();
  EXPECT_FALSE(response->result->is_error_reason());
  EXPECT_TRUE(response->result->is_permission_granted());
  EXPECT_TRUE(response->result->get_permission_granted());
}

class ExecutionEngineDangerousContentBrowserTest
    : public ExecutionEngineBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  ExecutionEngineDangerousContentBrowserTest() {
    scoped_feature_list_.InitWithFeatureState(
        kGlicBlockNavigationToDangerousContentTypes,
        should_block_dangerous_navigations());
  }

  bool should_block_dangerous_navigations() { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(ExecutionEngineDangerousContentBrowserTest,
                       BlockNavigationToJson) {
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  const GURL json_url =
      embedded_https_test_server().GetURL("example.com", "/actor/test.json");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  EXPECT_TRUE(content::ExecJs(web_contents(),
                              content::JsReplace("setLink($1);", json_url)));

  ClickTarget("#link",
              should_block_dangerous_navigations()
                  ? mojom::ActionResultCode::kTriggeredNavigationBlocked
                  : mojom::ActionResultCode::kOk);
  ASSERT_EQ(web_contents()->GetLastCommittedURL(),
            should_block_dangerous_navigations() ? start_url : json_url);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ExecutionEngineDangerousContentBrowserTest,
                         testing::Bool());
class ExecutionEngineOriginGatingBrowserTest
    : public ExecutionEngineBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  ExecutionEngineOriginGatingBrowserTest() {
    scoped_feature_list_.InitWithFeatureState(kGlicCrossOriginNavigationGating,
                                              origin_gating_enabled());
  }

  bool origin_gating_enabled() { return GetParam(); }

  void CreateMockPromptIPCResponse(
      std::optional<url::Origin> expected_navigation_origin,
      bool permission_granted) {
    user_confirmation_dialog_subscription_ =
        actor_keyed_service()
            ->AddRequestToShowUserConfirmationDialogSubscriberCallback(
                base::BindLambdaForTesting(
                    [expected_navigation_origin, permission_granted](
                        const std::optional<url::Origin>& got_navigation_origin,
                        const std::optional<int32_t> got_download_id,
                        ActorKeyedService::UserConfirmationDialogCallback
                            callback) {
                      EXPECT_EQ(got_navigation_origin,
                                expected_navigation_origin);
                      EXPECT_FALSE(got_download_id);
                      // Send a mock IPC response.
                      std::move(callback).Run(
                          webui::mojom::UserConfirmationDialogResponse::New(
                              webui::mojom::UserConfirmationDialogResult::
                                  NewPermissionGranted(permission_granted)));
                    }));
  }

 protected:
  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::CallbackListSubscription user_confirmation_dialog_subscription_;
};

IN_PROC_BROWSER_TEST_P(ExecutionEngineOriginGatingBrowserTest,
                       GateCrossOriginNavigations_Denied) {
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  const GURL second_url =
      embedded_https_test_server().GetURL("foo.com", "/actor/blank.html");

  CreateMockPromptIPCResponse(url::Origin::Create(second_url),
                              /*permission_granted=*/false);

  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  EXPECT_TRUE(content::ExecJs(web_contents(),
                              content::JsReplace("setLink($1);", start_url)));

  ClickTarget("#link", mojom::ActionResultCode::kOk);

  EXPECT_TRUE(content::ExecJs(web_contents(),
                              content::JsReplace("setLink($1);", second_url)));

  ClickTarget("#link",
              origin_gating_enabled()
                  ? mojom::ActionResultCode::kTriggeredNavigationBlocked
                  : mojom::ActionResultCode::kOk);

  // The first navigation should log that gating was not applied. The second
  // should log that gating was applied.
  histogram_tester_.ExpectBucketCount("Actor.NavigationGating.AppliedGate",
                                      false, origin_gating_enabled() ? 1 : 0);
  histogram_tester_.ExpectBucketCount("Actor.NavigationGating.AppliedGate",
                                      true, origin_gating_enabled() ? 1 : 0);
  // Should log that there was one same-site navigation and one cross-site
  // navigation.
  histogram_tester_.ExpectBucketCount("Actor.NavigationGating.CrossOrigin",
                                      false, origin_gating_enabled() ? 1 : 0);
  histogram_tester_.ExpectBucketCount("Actor.NavigationGating.CrossOrigin",
                                      true, origin_gating_enabled() ? 1 : 0);
  histogram_tester_.ExpectBucketCount("Actor.NavigationGating.CrossSite", false,
                                      origin_gating_enabled() ? 1 : 0);
  histogram_tester_.ExpectBucketCount("Actor.NavigationGating.CrossSite", true,
                                      origin_gating_enabled() ? 1 : 0);
  // Should log that permission was *denied* once.
  histogram_tester_.ExpectBucketCount(
      "Actor.NavigationGating.PermissionGranted", false,
      origin_gating_enabled() ? 1 : 0);
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineOriginGatingBrowserTest,
                       GateCrossOriginNavigations_Granted) {
  const GURL start_url = embedded_https_test_server().GetURL(
      "www.example.com", "/actor/link.html");
  const GURL second_url = embedded_https_test_server().GetURL(
      "foo.example.com", "/actor/blank.html");

  CreateMockPromptIPCResponse(url::Origin::Create(second_url),
                              /*permission_granted=*/true);

  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  EXPECT_TRUE(content::ExecJs(web_contents(),
                              content::JsReplace("setLink($1);", start_url)));

  ClickTarget("#link", mojom::ActionResultCode::kOk);

  EXPECT_TRUE(content::ExecJs(web_contents(),
                              content::JsReplace("setLink($1);", second_url)));

  ClickTarget("#link", mojom::ActionResultCode::kOk);

  // The first navigation should log that gating was not applied. The second
  // should log that gating was applied.
  histogram_tester_.ExpectBucketCount("Actor.NavigationGating.AppliedGate",
                                      false, origin_gating_enabled() ? 1 : 0);
  histogram_tester_.ExpectBucketCount("Actor.NavigationGating.AppliedGate",
                                      true, origin_gating_enabled() ? 1 : 0);
  // Should log that there was only a cross-origin navigation and not a
  // cross-site navigation.
  histogram_tester_.ExpectBucketCount("Actor.NavigationGating.CrossOrigin",
                                      false, origin_gating_enabled() ? 1 : 0);
  histogram_tester_.ExpectBucketCount("Actor.NavigationGating.CrossOrigin",
                                      true, origin_gating_enabled() ? 1 : 0);
  histogram_tester_.ExpectBucketCount("Actor.NavigationGating.CrossSite", false,
                                      origin_gating_enabled() ? 2 : 0);
  // Should log that permission was *granted* once.
  histogram_tester_.ExpectBucketCount(
      "Actor.NavigationGating.PermissionGranted", true,
      origin_gating_enabled() ? 1 : 0);
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineOriginGatingBrowserTest,
                       AddWritableMainframeOrigins) {
  // This test is not meaningful if origin gating is disabled.
  if (!origin_gating_enabled()) {
    return;
  }

  const GURL cross_origin_url =
      embedded_https_test_server().GetURL("bar.com", "/actor/blank.html");
  const GURL link_page_url = embedded_https_test_server().GetURL(
      "foo.com", base::StrCat({"/actor/link_full_page.html?href=",
                               EncodeURI(cross_origin_url.spec())}));

  // Mock IPC response waill always reject navigation.
  CreateMockPromptIPCResponse(url::Origin::Create(cross_origin_url),
                              /*permission_granted=*/false);

  // Start on link page on foo.com.
  ASSERT_TRUE(content::NavigateToURL(web_contents(), link_page_url));
  // Click on full-page link to bar.com only.
  std::unique_ptr<ToolRequest> click_link =
      MakeClickRequest(*active_tab(), gfx::Point(1, 1));
  ActResultFuture result1;
  actor_task().Act(ToRequestList(click_link), result1.GetCallback());
  // Expect the navigation to be blocked by origin gating.
  ExpectErrorResult(result1,
                    mojom::ActionResultCode::kTriggeredNavigationBlocked);

  // Add bar.com's origin to writable mainframe origins.
  actor_task().GetExecutionEngine()->AddWritableMainframeOrigins(
      {url::Origin::Create(cross_origin_url)});

  // Click on full-page link to bar.com only.
  std::unique_ptr<ToolRequest> click_link_again =
      MakeClickRequest(*active_tab(), gfx::Point(1, 1));
  ActResultFuture result2;
  actor_task().Act(ToRequestList(click_link_again), result2.GetCallback());
  // Now the navigation should not be blocked.
  ExpectOkResult(result2);
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineOriginGatingBrowserTest,
                       OriginGatingNavigateAction) {
  // This test is not meaningful if origin gating is disabled.
  if (!origin_gating_enabled()) {
    return;
  }

  const GURL start_url =
      embedded_https_test_server().GetURL("foo.com", "/actor/blank.html");
  const GURL cross_origin_url =
      embedded_https_test_server().GetURL("bar.com", "/actor/blank.html");
  const GURL link_page_url = embedded_https_test_server().GetURL(
      "foo.com", base::StrCat({"/actor/link_full_page.html?href=",
                               EncodeURI(cross_origin_url.spec())}));

  // Mock IPC response waill always reject navigation.
  CreateMockPromptIPCResponse(url::Origin::Create(cross_origin_url),
                              /*permission_granted=*/false);

  // Start on foo.com.
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  // Navigate to bar.com.
  std::unique_ptr<ToolRequest> navigate_x_origin =
      MakeNavigateRequest(*active_tab(), cross_origin_url.spec());
  // Navigate to foo.com page with a link to bar.com.
  std::unique_ptr<ToolRequest> navigate_to_link_page =
      MakeNavigateRequest(*active_tab(), link_page_url.spec());
  // Clicks on full-page link to bar.com.
  std::unique_ptr<ToolRequest> click_link =
      MakeClickRequest(*active_tab(), gfx::Point(1, 1));

  ActResultFuture result1;
  actor_task().Act(
      ToRequestList(navigate_x_origin, navigate_to_link_page, click_link),
      result1.GetCallback());
  ExpectOkResult(result1);

  // Test that navigation allowlist is not persisted across separate tasks.
  auto previous_id = actor_task().id();
  actor_keyed_service()->ResetForTesting();
  StartNewTask();
  ASSERT_NE(previous_id, actor_task().id());

  // Start on link page on foo.com.
  ASSERT_TRUE(content::NavigateToURL(web_contents(), link_page_url));
  // Click on full-page link to bar.com only.
  std::unique_ptr<ToolRequest> click_link_only =
      MakeClickRequest(*active_tab(), gfx::Point(1, 1));

  ActResultFuture result2;
  actor_task().Act(ToRequestList(click_link_only), result2.GetCallback());
  // Expect the navigation to be blocked by origin gating.
  ExpectErrorResult(result2,
                    mojom::ActionResultCode::kTriggeredNavigationBlocked);

  // All but the last navigation should not have gating applied.
  histogram_tester_.ExpectBucketCount("Actor.NavigationGating.AppliedGate",
                                      false, 3);
  histogram_tester_.ExpectBucketCount("Actor.NavigationGating.AppliedGate",
                                      true, 1);
  // Should log that permission was denied the one time it was prompted.
  histogram_tester_.ExpectBucketCount(
      "Actor.NavigationGating.PermissionGranted", false, 1);
  // Should log the allow-list has 2 entries at the end of the first task.
  histogram_tester_.ExpectBucketCount("Actor.NavigationGating.AllowListSize", 2,
                                      1);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ExecutionEngineOriginGatingBrowserTest,
                         testing::Bool());

}  // namespace

}  // namespace actor
