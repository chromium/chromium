// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task_metadata.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/actor_util.h"
#include "chrome/browser/actor/tools/wait_tool_request.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/actor/core/actor_features.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace actor {

namespace {

// A tool request that behaves like a Wait action but allows us to control
// RequiresOpeningWebContents.
class TestToolRequest : public WaitToolRequest {
 public:
  explicit TestToolRequest(bool requires_opening_web_contents)
      : WaitToolRequest(base::Hours(1)),
        requires_opening_web_contents_(requires_opening_web_contents) {}

  bool RequiresOpeningWebContents() const override {
    return requires_opening_web_contents_;
  }

 private:
  const bool requires_opening_web_contents_;
};

}  // namespace

class ActorPopupBrowserTest : public InProcessBrowserTest {
 public:
  ActorPopupBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlic, features::kGlicActor},
        /*disabled_features=*/{features::kGlicWarming});
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::RenderFrameHost* main_frame() {
    return web_contents()->GetPrimaryMainFrame();
  }

  ActorKeyedService* actor_keyed_service() {
    return ActorKeyedService::Get(browser()->profile());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that a sandboxed iframe without allow-top-navigation opens a new popup
// window instead of trying to navigate the top-level document (which is blocked
// by sandbox) when Actor popup interception is active.
IN_PROC_BROWSER_TEST_F(ActorPopupBrowserTest,
                       SandboxedIframeOpensPopupInsteadOfSameTabNavigation) {
  TaskId task_id = actor_keyed_service()->CreateTask(
      TestTaskSourceInfo(), NoEnterprisePolicyChecker());
  const GURL initial_url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  ActorTask* task = actor_keyed_service()->GetTask(task_id);
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  actor::AddTabToTask(*tab, *task);

  auto test_request = std::make_unique<TestToolRequest>(
      /*requires_opening_web_contents=*/false);

  PerformActionsFuture result_future;
  actor_keyed_service()->PerformActions(
      task_id,
      ToRequestList<std::unique_ptr<ToolRequest>>(std::move(test_request)),
      ActorTaskMetadata(), result_future.GetCallback());

  base::RunLoop run_loop;
  ExecutionEngineStateWaiter waiter(run_loop.QuitClosure(),
                                    task->GetExecutionEngine(),
                                    ExecutionEngine::State::kToolInvoke);
  run_loop.Run();

  EXPECT_TRUE(actor::HasActorTaskPreventingNewWebContents(main_frame()));

  // Create a sandboxed iframe.
  EXPECT_TRUE(content::ExecJs(main_frame(),
                              "var iframe = document.createElement('iframe');"
                              "iframe.sandbox = 'allow-scripts allow-popups';"
                              "iframe.src = '/title1.html';"
                              "document.body.appendChild(iframe);"));

  // Wait for the iframe to load.
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));

  content::RenderFrameHost* iframe_rfh = content::ChildFrameAt(main_frame(), 0);
  ASSERT_TRUE(iframe_rfh);

  const GURL new_url = embedded_test_server()->GetURL("/title2.html");

  ui_test_utils::BrowserCreatedObserver popup_observer;

  // Try to open a popup from the sandboxed iframe. We expect it to open in a
  // new browser window (popup) rather than navigating the current tab.
  EXPECT_TRUE(content::ExecJs(
      iframe_rfh,
      base::StringPrintf("window.open('%s', '_blank', 'width=100,height=100')",
                         new_url.spec().c_str())));

  // Wait for the new popup window to open.
  Browser* popup_browser = popup_observer.Wait();
  ASSERT_TRUE(popup_browser);

  // Verify that the new window has the correct URL.
  content::WebContents* popup_contents =
      popup_browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(popup_contents));
  EXPECT_EQ(popup_contents->GetLastCommittedURL(), new_url);

  // Verify that the original main frame URL did NOT change.
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), initial_url);
}

}  // namespace actor
