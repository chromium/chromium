// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/actor/glic_actor_functional_browsertest.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"

namespace glic::actor {
namespace {

using ::actor::ActorTask;
using ::actor::TaskId;
using ::base::test::TestFuture;
using ::base::test::ValueIs;
using ::optimization_guide::proto::Actions;
using ::optimization_guide::proto::ActionsResult;
using ::optimization_guide::proto::ClickAction;
using ::optimization_guide::proto::TabObservation;

class GlicActorActionExecutionFunctionalBrowserTest
    : public GlicActorFunctionalBrowserTestBase {
 public:
  GlicActorActionExecutionFunctionalBrowserTest() = default;
  ~GlicActorActionExecutionFunctionalBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_F(GlicActorActionExecutionFunctionalBrowserTest,
                       PerformNavigateAction) {
  ASSERT_OK_AND_ASSIGN(TaskId task_id, CreateTask());
  EXPECT_NE(task_id, TaskId());

  TestFuture<ActorTask::State> task_completion_state;
  base::CallbackListSubscription subscription =
      CreateTaskCompletionSubscription(task_id, task_completion_state);

  // Construct the Actions proto.
  const GURL target_url =
      embedded_test_server()->GetURL("/actor/blank.html?target");
  Actions action = ::actor::MakeNavigate(active_tab()->GetHandle(),
                                         target_url.spec(), task_id);

  EXPECT_THAT(PerformActions(action),
              ValueIs(HasResultCode(::actor::mojom::ActionResultCode::kOk)));
  EXPECT_EQ(target_url, web_contents()->GetURL());

  StopActorTask(task_id, glic::mojom::ActorTaskStopReason::kTaskComplete);

  EXPECT_EQ(ActorTask::State::kFinished, task_completion_state.Get())
      << "Task " << task_id << " did not reach kFinished state.";
}

IN_PROC_BROWSER_TEST_F(GlicActorActionExecutionFunctionalBrowserTest,
                       PerformClickAction) {
  // Set up the initial page with a link to the target page.
  const GURL initial_url = embedded_test_server()->GetURL("/actor/link.html");
  const GURL target_url = embedded_test_server()->GetURL("/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), initial_url));
  EXPECT_TRUE(content::ExecJs(web_contents(),
                              content::JsReplace("setLink($1);", target_url)));

  ASSERT_OK_AND_ASSIGN(TaskId task_id, CreateTask());
  EXPECT_NE(task_id, TaskId());

  TestFuture<ActorTask::State> task_completion_state;
  base::CallbackListSubscription subscription =
      CreateTaskCompletionSubscription(task_id, task_completion_state);

  // Click link to navigate to target page.
  std::optional<int> link_node_id =
      content::GetDOMNodeId(*web_contents()->GetPrimaryMainFrame(), "#link");
  Actions action = ::actor::MakeClick(*web_contents()->GetPrimaryMainFrame(),
                                      link_node_id.value(), ClickAction::LEFT,
                                      ClickAction::SINGLE, task_id);

  EXPECT_THAT(PerformActions(action),
              ValueIs(HasResultCode(::actor::mojom::ActionResultCode::kOk)));
  EXPECT_EQ(target_url, web_contents()->GetURL());

  StopActorTask(task_id, glic::mojom::ActorTaskStopReason::kTaskComplete);
  EXPECT_EQ(ActorTask::State::kFinished, task_completion_state.Get())
      << "Task " << task_id << " did not reach kFinished state.";
}

IN_PROC_BROWSER_TEST_F(GlicActorActionExecutionFunctionalBrowserTest,
                       PerformConcurrentAsyncWaitActions) {
  // Manually create tasks via ActorKeyedService.
  TaskId task_id_1 =
      actor_keyed_service()->CreateTask(::actor::NoEnterprisePolicyChecker());
  TaskId task_id_2 =
      actor_keyed_service()->CreateTask(::actor::NoEnterprisePolicyChecker());

  // Create tabs for each task using CreateActorTab API to ensure a
  // TabObservation is included in its result.
  ASSERT_OK_AND_ASSIGN(
      tabs::TabHandle tab_1,
      CreateActorTab(task_id_1, /*open_in_background=*/false,
                     base::ToString(active_tab()->GetHandle().raw_value()),
                     base::ToString(browser()->session_id().id())));
  ASSERT_OK_AND_ASSIGN(
      tabs::TabHandle tab_2,
      CreateActorTab(task_id_2, /*open_in_background=*/false,
                     base::ToString(active_tab()->GetHandle().raw_value()),
                     base::ToString(browser()->session_id().id())));

  // Perform two WaitActions where the first resolves after the second
  Actions action_1 = ::actor::MakeWait(kShortWaitTime * 2, tab_1, task_id_1);
  Actions action_2 = ::actor::MakeWait(kShortWaitTime, tab_2, task_id_2);

  std::unique_ptr<AsyncActionWaiter> waiter_1 = PerformActionsAsync(action_1);
  std::unique_ptr<AsyncActionWaiter> waiter_2 = PerformActionsAsync(action_2);

  // We should still be able to wait for result_2 after result_1 despite
  // action_2 resolving first.
  ASSERT_OK_AND_ASSIGN(ActionsResult result_1, waiter_1->Wait());
  ASSERT_OK_AND_ASSIGN(ActionsResult result_2, waiter_2->Wait());

  // Verify a tab observation was included in the results.
  EXPECT_THAT(result_1, HasResultCode(::actor::mojom::ActionResultCode::kOk));
  EXPECT_THAT(result_1.tabs(), testing::SizeIs(1));
  EXPECT_THAT(result_1.tabs().at(0).result(),
              TabObservation::TAB_OBSERVATION_OK);

  EXPECT_THAT(result_2, HasResultCode(::actor::mojom::ActionResultCode::kOk));
  EXPECT_THAT(result_2.tabs(), testing::SizeIs(1));
  EXPECT_THAT(result_2.tabs().at(0).result(),
              TabObservation::TAB_OBSERVATION_OK);
}

IN_PROC_BROWSER_TEST_F(GlicActorActionExecutionFunctionalBrowserTest,
                       CloseTabWhileActing) {
  base::HistogramTester histogram_tester;

  ASSERT_OK_AND_ASSIGN(TaskId task_id, CreateTask());
  EXPECT_NE(task_id, TaskId());

  TestFuture<ActorTask::State> task_completion_state;
  base::CallbackListSubscription subscription =
      CreateTaskCompletionSubscription(task_id, task_completion_state);

  Actions wait_action =
      ::actor::MakeWait(kLongWaitTime, active_tab()->GetHandle(), task_id);
  std::unique_ptr<AsyncActionWaiter> action_waiter =
      PerformActionsAsync(wait_action);

  // Wait for the task to start acting before closing the tab.
  WaitForTaskState(task_id, ActorTask::State::kActing);

  // Add a new background tab to prevent the browser from closing.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL),
      WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Close the active web contents.
  browser()->tab_strip_model()->CloseWebContentsAt(
      browser()->tab_strip_model()->GetIndexOfWebContents(web_contents()),
      TabCloseTypes::CLOSE_NONE);

  // After an acting tab is closed, the task should be cancelled and the
  // corresponding action have a result code of kTaskWentAway.
  // NOTE: We cannot use `action_waiter->Wait()` to check the result code
  // because the test client is destroyed when all task tabs are closed.
  EXPECT_EQ(ActorTask::State::kCancelled, task_completion_state.Get());
  histogram_tester.ExpectUniqueSample(
      "Actor.ExecutionEngine.Action.ResultCode",
      ::actor::mojom::ActionResultCode::kTaskWentAway, 1);
}

IN_PROC_BROWSER_TEST_F(GlicActorActionExecutionFunctionalBrowserTest,
                       PerformActionsOnCrashedTabReloadsTab) {
  const GURL& initial_url = web_contents()->GetLastCommittedURL();
  ASSERT_OK_AND_ASSIGN(TaskId task_id, CreateTask());
  ASSERT_NE(task_id, TaskId());

  TestFuture<ActorTask::State> task_completion_state;
  base::CallbackListSubscription subscription =
      CreateTaskCompletionSubscription(task_id, task_completion_state);

  // Crash the tab.
  content::CrashTab(web_contents());

  // Perform a click action on the crashed tab.
  Actions action =
      ::actor::MakeClick(active_tab()->GetHandle(), gfx::Point(1, 1),
                         ClickAction::LEFT, ClickAction::SINGLE, task_id);

  content::TestNavigationManager reload_observer(web_contents(), initial_url);
  EXPECT_THAT(PerformActions(action),
              ValueIs(HasResultCode(
                  ::actor::mojom::ActionResultCode::kRendererCrashed)));
  EXPECT_TRUE(reload_observer.WaitForNavigationFinished());
  EXPECT_FALSE(web_contents()->IsCrashed());
}

IN_PROC_BROWSER_TEST_F(GlicActorActionExecutionFunctionalBrowserTest,
                       CancelActions) {
  // Makes sure we are on about:blank so the browser won't open a new tab to
  // navigate.
  ASSERT_TRUE(
      content::NavigateToURL(web_contents(), GURL(url::kAboutBlankURL)));

  ASSERT_OK_AND_ASSIGN(TaskId task_id, CreateTask());
  ASSERT_NE(task_id, TaskId());
  const GURL target_url = embedded_test_server()->GetURL("/title1.html");
  content::TestNavigationManager navigation_manager(web_contents(), target_url);

  Actions action = ::actor::MakeNavigate(active_tab()->GetHandle(),
                                         target_url.spec(), task_id);
  std::unique_ptr<AsyncActionWaiter> waiter = PerformActionsAsync(action);

  // WaitForRequestStart() also pauses the navigation.
  EXPECT_TRUE(navigation_manager.WaitForRequestStart());
  EXPECT_EQ(actor_keyed_service()->GetTask(task_id)->GetState(),
            ActorTask::State::kActing);
  EXPECT_THAT(CancelActions(task_id),
              base::test::ValueIs(glic::mojom::CancelActionsResult::kSuccess));
  EXPECT_FALSE(navigation_manager.was_committed());
  EXPECT_EQ(actor_keyed_service()->GetTask(task_id)->GetState(),
            ActorTask::State::kReflecting);
  auto result = waiter->Wait();
  EXPECT_TRUE(result.has_value());
  EXPECT_THAT(
      result.value(),
      HasResultCode(::actor::mojom::ActionResultCode::kActionsCancelled));
}

IN_PROC_BROWSER_TEST_F(GlicActorActionExecutionFunctionalBrowserTest,
                       CancelActionsNoActionsToCancel) {
  ASSERT_OK_AND_ASSIGN(TaskId task_id, CreateTask());
  EXPECT_NE(task_id, TaskId());
  EXPECT_EQ(actor_keyed_service()->GetTask(task_id)->GetState(),
            ActorTask::State::kCreated);
  EXPECT_THAT(CancelActions(task_id),
              base::test::ValueIs(glic::mojom::CancelActionsResult::kSuccess));
  EXPECT_EQ(actor_keyed_service()->GetTask(task_id)->GetState(),
            ActorTask::State::kCreated);
}

}  // namespace
}  // namespace glic::actor
