// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/execution_engine.h"

#include <optional>
#include <string_view>
#include <tuple>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_file_util.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_policy_checker.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/click_tool_request.h"
#include "chrome/browser/actor/tools/tab_management_tool_request.h"
#include "chrome/browser/actor/ui/event_dispatcher.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/download/download_test_file_activity_observer.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/test_support/non_interactive_glic_test.h"
#include "chrome/browser/optimization_guide/browser_test_util.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/file_system_access/file_system_access_test_utils.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/javascript_dialogs/app_modal_dialog_controller.h"
#include "components/javascript_dialogs/app_modal_dialog_queue.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/core/filters/optimization_hints_component_update_listener.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/base/features.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "url/gurl.h"

using ::base::test::TestFuture;
using ::optimization_guide::proto::ClickAction;
using ::testing::_;

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
                              features::kGlicActor,
                              kGlicExternalProtocolActionResultCode},
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
    if (UseCertTestNames()) {
      embedded_https_test_server().SetSSLConfig(
          net::EmbeddedTestServer::CERT_TEST_NAMES);
    }
    ASSERT_TRUE(embedded_https_test_server().Start());

    actor_keyed_service()->GetPolicyChecker().SetActOnWebForTesting(true);

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

  virtual bool UseCertTestNames() const { return false; }

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
      mojom::ActionResultCode expected_code = mojom::ActionResultCode::kOk,
      content::RenderFrameHost* execution_target = nullptr) {
    content::RenderFrameHost& rfh =
        execution_target ? *execution_target : *main_frame();
    std::optional<int> dom_node_id = content::GetDOMNodeId(rfh, query_selector);
    ASSERT_TRUE(dom_node_id);
    std::unique_ptr<ToolRequest> click =
        MakeClickRequest(rfh, dom_node_id.value());
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

  ClickTarget("#mailto",
              mojom::ActionResultCode::kExternalProtocolNavigationBlocked);
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

// TODO(crbug.com/456759397): Add coverage for multi-tab cases in
// foreground/background visibility metric.

// Android uses a different dropdown UI that doesn't respect styling.
#if !BUILDFLAG(IS_ANDROID)
class ExecutionEnginePixelBrowserTest : public ExecutionEngineBrowserTest {
 public:
  ExecutionEnginePixelBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kGlicActorInternalPopups);
  }

  void SetUp() override {
    EnablePixelOutput();
    ExecutionEngineBrowserTest::SetUp();
  }

  // Captures the page with CopyFromSurface() and returns true if any red
  // pixels are found.
  bool HasRedPixels() {
    bool found_red = false;
    base::RunLoop run_loop;
    web_contents()->GetRenderWidgetHostView()->CopyFromSurface(
        gfx::Rect(), gfx::Size(),
        base::BindLambdaForTesting(
            [&](const viz::CopyOutputBitmapWithMetadata& result) {
              const SkBitmap& bitmap = result.bitmap;
              ASSERT_FALSE(bitmap.drawsNothing());
              for (int x = 0; x < bitmap.width() && !found_red; ++x) {
                for (int y = 0; y < bitmap.height() && !found_red; ++y) {
                  if (bitmap.getColor(x, y) == SK_ColorRED) {
                    found_red = true;
                  }
                }
              }
              base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE, run_loop.QuitClosure());
            }));
    run_loop.Run();
    return found_red;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO: b/456801048 - Fix test flakiness on Linux.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_DropdownCapturedWhenActing DISABLED_DropdownCapturedWhenActing
#else
#define MAYBE_DropdownCapturedWhenActing DropdownCapturedWhenActing
#endif  // BUILDFLAG(IS_LINUX)

// Tests that dropdown menus are visible in captures during an actor-controlled
// state.
IN_PROC_BROWSER_TEST_F(ExecutionEnginePixelBrowserTest,
                       MAYBE_DropdownCapturedWhenActing) {
  // Render an HTML <select> element whose second item appears red.
  // The second item should appear when the element is clicked.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/actor/red_dropdown.html")));
  EXPECT_TRUE(WaitForRenderFrameReady(web_contents()->GetPrimaryMainFrame()));
  content::SimulateEndOfPaintHoldingOnPrimaryMainFrame(web_contents());

  base::RunLoop loop;
  actor_task().AddTab(
      active_tab()->GetHandle(),
      base::BindLambdaForTesting([&](mojom::ActionResultPtr result) {
        EXPECT_TRUE(IsOk(*result));
        loop.Quit();
      }));
  loop.Run();

  // Set the actor task to an actor-controlled state.
  actor_task().SetState(ActorTask::State::kActing);

  {
    content::ShowPopupWidgetWaiter dropdown_waiter(web_contents(),
                                                   main_frame());
    ClickTarget("#select");
    dropdown_waiter.Wait();
    ASSERT_FALSE(dropdown_waiter.last_initial_rect().IsEmpty());
  }
  content::WaitForCopyableViewInFrame(main_frame());

  // CopyFromSurface() should have seen red pixels from the dropdown.
  EXPECT_TRUE(HasRedPixels());

  // Dismissing popups only happens on Mac -- it happens to shift back to the
  // external UI.
#if BUILDFLAG(IS_MAC)
  // Move to a non-actor-controlled state, which should dismiss the popup.
  actor_task().SetState(ActorTask::State::kPausedByUser);

  // Capture again, and expect no red pixels.
  EXPECT_FALSE(HasRedPixels());

  // Re-open the popup. Since the actor is not in an actor-controlled state,
  // the popup should be external and not styled.
  {
    content::ShowPopupWidgetWaiter dropdown_waiter(web_contents(),
                                                   main_frame());
    content::SimulateMouseClickAt(
        web_contents(), /*modifiers=*/0, blink::WebMouseEvent::Button::kLeft,
        gfx::ToFlooredPoint(content::GetCenterCoordinatesOfElementWithId(
            web_contents(), "select")));
    dropdown_waiter.Wait();
    ASSERT_FALSE(dropdown_waiter.last_initial_rect().IsEmpty());
    content::WaitForCopyableViewInFrame(main_frame());
  }

  // Capture again, and expect no red pixels.
  EXPECT_FALSE(HasRedPixels());

  // Now, go back to an actor-controlled state again, and re-open the popup. We
  // should get red pixels again.
  actor_task().SetState(ActorTask::State::kReflecting);
  actor_task().SetState(ActorTask::State::kActing);

  {
    content::ShowPopupWidgetWaiter dropdown_waiter(web_contents(),
                                                   main_frame());
    ClickTarget("#select");
    dropdown_waiter.Wait();
    ASSERT_FALSE(dropdown_waiter.last_initial_rect().IsEmpty());
  }
  content::WaitForCopyableViewInFrame(main_frame());

  // CopyFromSurface() should have seen red pixels from the dropdown.
  EXPECT_TRUE(HasRedPixels());
#endif  // BUILDFLAG(IS_MAC)
}

// Only Mac switches between internal and external popups, so this test only
// makes sense on that platform.
#if BUILDFLAG(IS_MAC)

// Determines the ordering of when the the cross-origin iframe is created.
enum class CreateFrameHappens {
  kBeforeStateTransistions,
  kAfterStateTransistions,
};

// Determines if expecting the popup to use internal (when actor controlled) or
// external (when user controlled) popup UI. Only the internal UI is stylable
// with red.
enum class ExpectedPopupType {
  kInternal,
  kExternal,
};

class ExecutionEngineDropdownCaptureOopifBrowserTest
    : public ExecutionEnginePixelBrowserTest,
      public ::testing::WithParamInterface<
          std::tuple<CreateFrameHappens, ExpectedPopupType>> {
 public:
  bool UseCertTestNames() const override { return true; }

  CreateFrameHappens GetCreateFrameHappens() const {
    return std::get<0>(GetParam());
  }

  ExpectedPopupType GetExpectedPopupType() const {
    return std::get<1>(GetParam());
  }

  // Helper function that clicks and opens a popup. This function works both in
  // actor-controlled and non-actor-controlled states.
  void ClickSelect(std::string_view select_id,
                   content::RenderFrameHost* execution_target = nullptr) {
    content::RenderFrameHost& rfh =
        execution_target ? *execution_target : *main_frame();
    content::ShowPopupWidgetWaiter dropdown_waiter(web_contents(), &rfh);
    if (actor_task().IsUnderActorControl()) {
      ClickTarget(base::StrCat({"#", select_id}),
                  /*expected_code=*/mojom::ActionResultCode::kOk,
                  /*execution_target=*/&rfh);
    } else {
      blink::WebMouseEvent mouse_event(
          blink::WebInputEvent::Type::kMouseDown,
          blink::WebInputEvent::kNoModifiers,
          blink::WebInputEvent::GetStaticTimeStampForTests());
      mouse_event.button = blink::WebPointerProperties::Button::kLeft;
      mouse_event.SetPositionInWidget(
          content::GetCenterCoordinatesOfElementWithId(&rfh, select_id));
      rfh.GetRenderWidgetHost()->ForwardMouseEvent(mouse_event);
    }
    dropdown_waiter.Wait();
    ASSERT_FALSE(dropdown_waiter.last_initial_rect().IsEmpty());
  }

  void DoStateTransitions() {
    base::RunLoop loop;
    actor_task().AddTab(
        active_tab()->GetHandle(),
        base::BindLambdaForTesting([&](mojom::ActionResultPtr result) {
          EXPECT_TRUE(IsOk(*result));
          loop.Quit();
        }));
    loop.Run();

    // Set the actor task to an actor-controlled state.
    actor_task().SetState(ActorTask::State::kActing);

    if (GetExpectedPopupType() == ExpectedPopupType::kExternal) {
      // Now, transition back to a user-controlled state.
      actor_task().SetState(ActorTask::State::kPausedByUser);
    }
  }
};

// Ensure that internal / external popup mode correctly propagates to
// newly-created out-of-process (cross-origin) iframes, even those created after
// moving to an actor-controlled state.
//
// Transition to an actor-controlled state, then create a new out-of-process
// iframe that contains a <select> tag.
//
// When the actor opens the select dropdown, it uses the internal UI (styled
// with red) and is visible in the screenshot.
IN_PROC_BROWSER_TEST_P(ExecutionEngineDropdownCaptureOopifBrowserTest,
                       OutOfProcIframeDropdowns) {
  // The main frame is a.test, the iframe with the popup (created dynamically)
  // is in b.test.
  const url::Origin origin_b =
      url::Origin::Create(embedded_https_test_server().GetURL("b.test", "/"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_https_test_server().GetURL(
                     "a.test", base::StrCat({"/actor/oopif_red_dropdown.html?",
                                             origin_b.Serialize()}))));
  EXPECT_TRUE(WaitForRenderFrameReady(web_contents()->GetPrimaryMainFrame()));
  content::SimulateEndOfPaintHoldingOnPrimaryMainFrame(web_contents());

  switch (GetCreateFrameHappens()) {
    case CreateFrameHappens::kBeforeStateTransistions:
      EXPECT_TRUE(content::ExecJs(web_contents(), "createIframe();"));
      DoStateTransitions();
      break;
    case CreateFrameHappens::kAfterStateTransistions:
      DoStateTransitions();
      EXPECT_TRUE(content::ExecJs(web_contents(), "createIframe();"));
      break;
  }

  // Get a handle to the out of process iframe.
  content::RenderFrameHost* iframe =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  ASSERT_NE(iframe, nullptr);

  // Now click on the <select> in the out of process iframe, and then look for
  // red pixels.
  ClickSelect("select", /*execution_target=*/iframe);
  content::WaitForCopyableViewInFrame(iframe);

  // CopyFromSurface() should have seen red pixels from the dropdown.
  switch (GetExpectedPopupType()) {
    case ExpectedPopupType::kInternal:
      EXPECT_TRUE(HasRedPixels());
      break;
    case ExpectedPopupType::kExternal:
      EXPECT_FALSE(HasRedPixels());
      break;
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    ExecutionEngineDropdownCaptureOopifBrowserTest,
    ::testing::Combine(
        ::testing::Values(CreateFrameHappens::kBeforeStateTransistions,
                          CreateFrameHappens::kAfterStateTransistions),
        ::testing::Values(ExpectedPopupType::kInternal,
                          ExpectedPopupType::kExternal)),
    [](const testing::TestParamInfo<
        std::tuple<CreateFrameHappens, ExpectedPopupType>>& info) {
      return base::StrCat(
          {"CreateFrameHappens_",
           std::get<0>(info.param) ==
                   CreateFrameHappens::kBeforeStateTransistions
               ? "BeforeStateTransistions"
               : "AfterStateTransitions",
           "__ExpectedPopupType_",
           std::get<1>(info.param) == ExpectedPopupType::kInternal
               ? "Internal"
               : "External"});
    });

#endif  // BUILDFLAG(IS_MAC)

#endif  // !BUILDFLAG(IS_ANDROID)

class ExecutionEngineFileSystemAccessApiBrowserTest
    : public ExecutionEngineBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  ExecutionEngineFileSystemAccessApiBrowserTest() {
    scoped_feature_list_.InitWithFeatureState(
        kGlicBlockFileSystemAccessApiFilePicker, should_block_file_picker());
  }

  bool should_block_file_picker() { return GetParam(); }

  void SetUp() override {
    ASSERT_TRUE(
        temp_dir_.CreateUniqueTempDirUnderPath(base::GetTempDirForTesting()));
    InProcessBrowserTest::SetUp();
  }

  base::FilePath CreateTestFile(const std::string& contents) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath result;
    EXPECT_TRUE(base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &result));
    EXPECT_TRUE(base::WriteFile(result, contents));
    return result;
  }

  bool IsUsageIndicatorVisible(Browser* browser) {
    auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
    auto* icon_view =
        browser_view->toolbar_button_provider()->GetPageActionView(
            kActionShowFileSystemAccess);
    return icon_view && icon_view->GetVisible();
  }

 private:
  base::ScopedTempDir temp_dir_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(ExecutionEngineFileSystemAccessApiBrowserTest,
                       FilePickerForFileSystemAccessApiBlocked) {
  const base::FilePath test_file = CreateTestFile("");
  const std::string file_contents = "file contents to write";

  ::ui::SelectFileDialog::SetFactory(
      std::make_unique<SelectPredeterminedFileDialogFactory>(
          std::vector<base::FilePath>{test_file}));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/actor/file_system_access.html")));

  EXPECT_FALSE(IsUsageIndicatorVisible(browser()));

  ClickTarget("#save");

  EXPECT_NE(IsUsageIndicatorVisible(browser()), should_block_file_picker());

  // Now check that we can get access to file when not using actor
  actor_keyed_service()->ResetForTesting();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_EQ(test_file.BaseName().AsUTF8Unsafe(),
            content::EvalJs(web_contents, "saveFile()"));

  EXPECT_TRUE(IsUsageIndicatorVisible(browser()))
      << "A save file dialog implicitly grants write access, so usage "
         "indicator should be visible.";
}

INSTANTIATE_TEST_SUITE_P(All,
                         ExecutionEngineFileSystemAccessApiBrowserTest,
                         testing::Bool());
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

class ExecutionEngineSkipBeforeUnloadBrowserTest
    : public ExecutionEngineBrowserTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  ExecutionEngineSkipBeforeUnloadBrowserTest() {
    if (IsSkipFeatureEnabled()) {
      scoped_feature_list_.InitAndEnableFeature(
          kGlicSkipBeforeUnloadDialogAndNavigate);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          kGlicSkipBeforeUnloadDialogAndNavigate);
    }
  }

  bool IsActorActive() const { return std::get<0>(GetParam()); }
  bool IsSkipFeatureEnabled() const { return std::get<1>(GetParam()); }

  void WaitForAppModalDialogToClose() {
    ASSERT_TRUE(base::test::RunUntil([] {
      return !javascript_dialogs::AppModalDialogQueue::GetInstance()
                  ->HasActiveDialog();
    }));
  }

  void CancelActiveAppModalDialog() {
    auto* dialog_queue = javascript_dialogs::AppModalDialogQueue::GetInstance();
    ASSERT_TRUE(javascript_dialogs::AppModalDialogQueue::GetInstance()
                    ->HasActiveDialog());
    javascript_dialogs::AppModalDialogController* dialog =
        dialog_queue->active_dialog();
    dialog->OnCancel(true);
    WaitForAppModalDialogToClose();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// This test is to ensure that the beforeunload dialog is skipped when the
//  kGlicSkipBeforeUnloadDialogAndNavigate feature is enabled and the actor is
//  active on the renderer.
IN_PROC_BROWSER_TEST_P(ExecutionEngineSkipBeforeUnloadBrowserTest,
                       SkipBeforeUnloadDialogAndNavigate) {
  if (IsActorActive()) {
    base::test::TestFuture<mojom::ActionResultPtr> future;
    actor_task().AddTab(active_tab()->GetHandle(), future.GetCallback());
    mojom::ActionResultPtr result = future.Take();
    ASSERT_TRUE(IsOk(*result));
  } else {
    actor_keyed_service()->ResetForTesting();
  }

  const GURL beforeunload_url =
      embedded_test_server()->GetURL("/actor/beforeunload.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), beforeunload_url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::PrepContentsForBeforeUnloadTest(web_contents);
  ASSERT_EQ(beforeunload_url, web_contents->GetLastCommittedURL());

  const GURL target_url = embedded_test_server()->GetURL("/title1.html");

  bool should_skip_dialog = IsActorActive() && IsSkipFeatureEnabled();

  if (should_skip_dialog) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), target_url));
    content::WaitForLoadStop(web_contents);

    EXPECT_EQ(target_url, web_contents->GetLastCommittedURL());
    EXPECT_FALSE(web_contents->NeedToFireBeforeUnloadOrUnloadEvents());

  } else {
    // Expect navigation to be blocked by beforeunload dialogs and no navigation
    // to occur.
    web_contents->GetController().LoadURL(target_url, content::Referrer(),
                                          ::ui::PAGE_TRANSITION_TYPED,
                                          std::string());

    ui_test_utils::WaitForAppModalDialog();
    EXPECT_EQ(beforeunload_url, web_contents->GetLastCommittedURL());
    CancelActiveAppModalDialog();
  }
}

struct SkipBeforeUnloadTestNameGenerator {
  template <class ParamType>
  std::string operator()(const testing::TestParamInfo<ParamType>& info) const {
    return base::StringPrintf(
        "%s_%s", std::get<0>(info.param) ? "ActorActive" : "ActorInactive",
        std::get<1>(info.param) ? "SkipFeatureEnabled" : "SkipFeatureDisabled");
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ExecutionEngineSkipBeforeUnloadBrowserTest,
    testing::Combine(testing::Bool(),   // IsActorActive
                     testing::Bool()),  // IsSkipFeatureEnabled
    SkipBeforeUnloadTestNameGenerator());

class ExecutionEngineDownloadBrowserTest : public ExecutionEngineBrowserTest {
 public:
  void SetUpOnMainThread() override {
    browser()->profile()->GetPrefs()->SetBoolean(prefs::kPromptForDownload,
                                                 true);
    file_activity_observer_ =
        std::make_unique<DownloadTestFileActivityObserver>(
            browser()->profile());
    file_activity_observer_->EnableFileChooser(false);
    ExecutionEngineBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    ExecutionEngineBrowserTest::TearDownOnMainThread();
    // Needs to be torn down on the main thread. file_activity_observer_ holds a
    // reference to the ChromeDownloadManagerDelegate which should be destroyed
    // on the UI thread.
    file_activity_observer_.reset();
  }

 protected:
  base::HistogramTester histogram_tester_;

 private:
  std::unique_ptr<DownloadTestFileActivityObserver> file_activity_observer_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ExecutionEngineDownloadBrowserTest,
                       OnlyActorDownloadsAreRecorded) {
  const GURL start_url = embedded_https_test_server().GetURL(
      "example.com", "/actor/download.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  ASSERT_TRUE(content::ExecJs(web_contents(),
                              "document.getElementById('download').click()"));

  content::DownloadTestObserverTerminal download_observer(
      browser()->profile()->GetDownloadManager(), 2,
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  ClickTarget("#download", mojom::ActionResultCode::kFilePickerTriggered);

  // Execution Engine normal holds onto file picker callback until next resume,
  // resetting forces the callback to get triggered.
  actor_keyed_service()->ResetForTesting();
  download_observer.WaitForFinished();

  histogram_tester_.ExpectUniqueSample("Actor.Download.DirectDownloadTriggered",
                                       true, 1);
  histogram_tester_.ExpectUniqueSample("Actor.Download.SaveAsDialogTriggered",
                                       true, 1);
}

}  // namespace

}  // namespace actor
