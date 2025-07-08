// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/execution_engine.h"

#include <optional>
#include <string_view>

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/glic/glic_keyed_service.h"
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
#include "content/public/test/test_frame_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"

using ::base::test::TestFuture;
using ::optimization_guide::proto::BrowserAction;
using ::optimization_guide::proto::ClickAction;

namespace actor {

namespace {

class ExecutionEngineBrowserTest : public InProcessBrowserTest {
 public:
  ExecutionEngineBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlic, features::kTabstripComboButton,
                              features::kGlicActor},
        /*disabled_features=*/{features::kGlicWarming});
  }
  ExecutionEngineBrowserTest(const ExecutionEngineBrowserTest&) = delete;
  ExecutionEngineBrowserTest& operator=(const ExecutionEngineBrowserTest&) =
      delete;

  ~ExecutionEngineBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());

    auto execution_engine = InitializeExecutionEngine();
    ExecutionEngine* raw_execution_engine = execution_engine.get();
    auto task = std::make_unique<ActorTask>(std::move(execution_engine));
    raw_execution_engine->SetOwner(task.get());
    task_id_ = ActorKeyedService::Get(browser()->profile())
                   ->AddActiveTask(std::move(task));
  }

 protected:
  virtual std::unique_ptr<ExecutionEngine> InitializeExecutionEngine() {
    return std::make_unique<ExecutionEngine>(
        browser()->profile(), browser()->GetActiveTabInterface());
  }

  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  content::RenderFrameHost* main_frame() {
    return web_contents()->GetPrimaryMainFrame();
  }

  ExecutionEngine& execution_engine() {
    return *actor_task().GetExecutionEngine();
  }

  ActorTask& actor_task() {
    return *ActorKeyedService::Get(browser()->profile())->GetTask(task_id_);
  }

  void ClickTarget(std::string_view query_selector) {
    std::optional<int> dom_node_id =
        content::GetDOMNodeId(*main_frame(), query_selector);
    ASSERT_TRUE(dom_node_id);
    BrowserAction action = MakeClick(*main_frame(), dom_node_id.value());
    action.set_task_id(task_id_.value());
    TestFuture<mojom::ActionResultPtr> result;
    execution_engine().Act(action, result.GetCallback());
    ExpectOkResult(result);
  }

 private:
  TaskId task_id_;
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

  BrowserAction action;
  ClickAction* click1 = action.add_actions()->mutable_click();
  click1->mutable_target()->set_content_node_id(button1_id.value());
  click1->mutable_target()->mutable_document_identifier()->set_serialized_token(
      *optimization_guide::DocumentIdentifierUserData::GetDocumentIdentifier(
          main_frame()->GetGlobalFrameToken()));
  click1->set_click_type(ClickAction::LEFT);
  click1->set_click_count(ClickAction::SINGLE);

  ClickAction* click2 = action.add_actions()->mutable_click();
  click2->mutable_target()->set_content_node_id(button2_id.value());
  click2->mutable_target()->mutable_document_identifier()->set_serialized_token(
      *optimization_guide::DocumentIdentifierUserData::GetDocumentIdentifier(
          main_frame()->GetGlobalFrameToken()));
  click2->set_click_type(ClickAction::LEFT);
  click2->set_click_count(ClickAction::SINGLE);

  action.set_task_id(actor_task().id().value());

  // Execute the action
  TestFuture<mojom::ActionResultPtr> result;
  execution_engine().Act(action, result.GetCallback());
  ExpectOkResult(result);

  // Check background color changed to green
  EXPECT_EQ("green", EvalJs(web_contents(), "document.body.bgColor"));
}

// ActorToolsTest but using the V2 ExecutionEngine API.
// TODO(crbug.com/411462297): All tests should eventually use the V2 API and the
// original test harness should be migrated to the new API. New tests should use
// this harness.
class ExecutionEngineBrowserTestV2 : public ExecutionEngineBrowserTest {
 public:
  ExecutionEngineBrowserTestV2() = default;
  ~ExecutionEngineBrowserTestV2() override = default;
  explicit ExecutionEngineBrowserTestV2(const ExecutionEngineBrowserTestV2&) =
      delete;
  ExecutionEngineBrowserTestV2& operator=(const ExecutionEngineBrowserTestV2&) =
      delete;

  std::unique_ptr<ExecutionEngine> InitializeExecutionEngine() override {
    return std::make_unique<ExecutionEngine>(browser()->profile());
  }
};

IN_PROC_BROWSER_TEST_F(ExecutionEngineBrowserTestV2, TwoClicksInBackgroundTab) {
  const GURL url = embedded_test_server()->GetURL("/actor/two_clicks.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Check initial background color is red
  EXPECT_EQ("red", EvalJs(web_contents(), "document.body.bgColor"));

  // Store a pointer to the first tab.
  content::WebContents* first_tab_contents = web_contents();
  auto* tab = browser()->GetActiveTabInterface();
  auto tab_handle = tab->GetHandle();

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

  optimization_guide::proto::Actions actions;
  actions.set_task_id(actor_task().id().value());

  ClickAction* click1 = actions.add_actions()->mutable_click();
  click1->mutable_target()->set_content_node_id(button1_id.value());
  click1->mutable_target()->mutable_document_identifier()->set_serialized_token(
      *optimization_guide::DocumentIdentifierUserData::GetDocumentIdentifier(
          first_tab_contents->GetPrimaryMainFrame()->GetGlobalFrameToken()));
  click1->set_click_type(ClickAction::LEFT);
  click1->set_click_count(ClickAction::SINGLE);
  click1->set_tab_id(tab_handle.raw_value());

  ClickAction* click2 = actions.add_actions()->mutable_click();
  click2->set_tab_id(tab_handle.raw_value());
  click2->mutable_target()->set_content_node_id(button2_id.value());
  click2->mutable_target()->mutable_document_identifier()->set_serialized_token(
      *optimization_guide::DocumentIdentifierUserData::GetDocumentIdentifier(
          first_tab_contents->GetPrimaryMainFrame()->GetGlobalFrameToken()));
  click2->set_click_type(ClickAction::LEFT);
  click2->set_click_count(ClickAction::SINGLE);
  click2->set_tab_id(tab_handle.raw_value());

  // Execute the actions.
  TestFuture<optimization_guide::proto::ActionsResult> result;
  execution_engine().Act(actions, result.GetCallback());

  // Check that the action succeeded.
  EXPECT_EQ(result.Get().action_result(),
            static_cast<int>(mojom::ActionResultCode::kOk));

  // Check background color changed to green in the background tab.
  EXPECT_EQ("green", EvalJs(tab->GetContents(), "document.body.bgColor"));
}

}  // namespace

}  // namespace actor
