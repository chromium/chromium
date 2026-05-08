// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_util.h"

#include <memory>
#include <vector>

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task_metadata.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/wait_tool_request.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/actor/core/actor_features.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/flags/android/chrome_feature_list.h"
#endif

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

class ActorUtilBrowserTest : public PlatformBrowserTest {
 public:
  ActorUtilBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlic,
#if BUILDFLAG(IS_ANDROID)
                              chrome::android::kBrowserWindowInterfaceMobile,
#endif
                              features::kGlicActor},
        /*disabled_features=*/{features::kGlicWarming});
  }

  void SetUpOnMainThread() override {
    PlatformBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  tabs::TabInterface* active_tab() {
    return chrome_test_utils::GetActiveTab(this);
  }

  content::WebContents* web_contents() { return active_tab()->GetContents(); }

  content::RenderFrameHost* main_frame() {
    return web_contents()->GetPrimaryMainFrame();
  }

  ActorKeyedService* actor_keyed_service() {
    return ActorKeyedService::Get(GetProfile());
  }

  Profile* GetProfile() { return chrome_test_utils::GetProfile(this); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that HasActorTaskPreventingNewWebContents returns true when a task is
// acting and the current action doesn't allow opening new web contents.
IN_PROC_BROWSER_TEST_F(ActorUtilBrowserTest, PreventsNewWebContents) {
  TaskId task_id = actor_keyed_service()->CreateTask(
      TestTaskSourceInfo(), NoEnterprisePolicyChecker());
  // Navigate to an initial page first.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_test_server()->GetURL("/title1.html")));

  ActorTask* task = actor_keyed_service()->GetTask(task_id);
  actor::AddTabToTask(*active_tab(), *task);

  auto test_request = std::make_unique<TestToolRequest>(
      /*requires_opening_web_contents=*/false);

  PerformActionsFuture result_future;
  actor_keyed_service()->PerformActions(
      task_id,
      ToRequestList<std::unique_ptr<ToolRequest>>(std::move(test_request)),
      ActorTaskMetadata(), result_future.GetCallback());

  // Wait for the engine to reach kToolInvoke state.
  base::RunLoop run_loop;
  ExecutionEngineStateWaiter waiter(run_loop.QuitClosure(),
                                    task->GetExecutionEngine(),
                                    ExecutionEngine::State::kToolInvoke);
  run_loop.Run();

  // Verify that the task prevents opening new web contents.
  EXPECT_TRUE(actor::HasActorTaskPreventingNewWebContents(main_frame()));

  // Verify that window.open is overridden to load in the same tab.
  const GURL new_url = embedded_test_server()->GetURL("/title2.html");
  content::TestNavigationObserver observer(web_contents());

  // Use ExecJs to ensure a user gesture is provided.
  EXPECT_TRUE(content::ExecJs(
      main_frame(),
      base::StringPrintf("window.open('%s')", new_url.spec().c_str())));
  observer.Wait();

  EXPECT_EQ(web_contents()->GetLastCommittedURL(), new_url);
}

// Tests that HasActorTaskPreventingNewWebContents returns false when a task is
// acting but the current action explicitly allows opening new web contents.
IN_PROC_BROWSER_TEST_F(ActorUtilBrowserTest, AllowsNewWebContents) {
  TaskId task_id = actor_keyed_service()->CreateTask(
      TestTaskSourceInfo(), NoEnterprisePolicyChecker());

  ActorTask* task = actor_keyed_service()->GetTask(task_id);
  actor::AddTabToTask(*active_tab(), *task);

  auto test_request = std::make_unique<TestToolRequest>(
      /*requires_opening_web_contents=*/true);

  PerformActionsFuture result_future;
  actor_keyed_service()->PerformActions(
      task_id,
      ToRequestList<std::unique_ptr<ToolRequest>>(std::move(test_request)),
      ActorTaskMetadata(), result_future.GetCallback());

  // Wait for the engine to reach kToolInvoke state.
  base::RunLoop run_loop;
  ExecutionEngineStateWaiter waiter(run_loop.QuitClosure(),
                                    task->GetExecutionEngine(),
                                    ExecutionEngine::State::kToolInvoke);
  run_loop.Run();

  // Verify that the engine is in kToolInvoke.
  EXPECT_EQ(task->GetExecutionEngine().state(),
            ExecutionEngine::State::kToolInvoke);

  // Verify that the task does NOT prevent opening new web contents.
  EXPECT_FALSE(actor::HasActorTaskPreventingNewWebContents(main_frame()));
}

// Tests that HasActorTaskPreventingNewWebContents returns false when no task is
// acting on the tab.
IN_PROC_BROWSER_TEST_F(ActorUtilBrowserTest, NoTaskNoPrevention) {
  EXPECT_FALSE(actor::HasActorTaskPreventingNewWebContents(main_frame()));
}

}  // namespace actor
