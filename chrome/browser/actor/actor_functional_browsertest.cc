// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_metrics.h"
#include "chrome/browser/actor/actor_proto_conversion.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/glic/actor/glic_actor_functional_browsertest.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/page_content_annotations/multi_source_page_context_fetcher.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace actor {
namespace {

using ::base::test::TestFuture;
using ::base::test::ValueIs;
using ::glic::actor::AsyncActionWaiter;
using ::glic::actor::HasResultCode;
using ::glic::actor::MakeNavigateForTaskId;
using ::glic::actor::MakeWaitForTaskId;
using ::optimization_guide::proto::Actions;
using ::optimization_guide::proto::ActionsResult;
using ::optimization_guide::proto::ClickAction;
using ::optimization_guide::proto::TabObservation;
using ::page_content_annotations::FetchPageContextResult;


// Helper to mock the result returned on a TabObservation built using
// actor::BuildActionsResultWithObservations. While live, use the provided
// function to set TabObservationResults. Unset on destruction.
class ScopedMockTabObservationResult {
 public:
  explicit ScopedMockTabObservationResult(
      base::RepeatingCallback<void(TabObservation*,
                                   const FetchPageContextResult&)> callback) {
    SetTabObservationResultOverrideForTesting(callback);
  }
  ~ScopedMockTabObservationResult() {
    SetTabObservationResultOverrideForTesting(
        base::RepeatingCallback<void(TabObservation*,
                                     const FetchPageContextResult&)>());
  }
};

class ActorFunctionalBrowserTest
    : public glic::actor::GlicActorFunctionalBrowserTestBase {
 public:
  ActorFunctionalBrowserTest() = default;
  ~ActorFunctionalBrowserTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    glic::test::GlicFunctionalBrowserTestBase::SetUpOnMainThread();
    RunTestSequence(OpenGlic());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(crbug.com/465188408): Move all test cases to dedicated files grouped by
// the functionality being tested.
IN_PROC_BROWSER_TEST_F(ActorFunctionalBrowserTest, PerformNavigateAction) {
  ASSERT_OK_AND_ASSIGN(TaskId task_id, CreateTask());
  EXPECT_NE(task_id, TaskId());

  TestFuture<ActorTask::State> task_completion_state;
  base::CallbackListSubscription subscription =
      CreateTaskCompletionSubscription(task_id, task_completion_state);

  // Construct the Actions proto.
  const GURL target_url =
      embedded_test_server()->GetURL("/actor/blank.html?target");
  Actions action = MakeNavigateForTaskId(active_tab()->GetHandle(),
                                         target_url.spec(), task_id);

  EXPECT_THAT(PerformActions(action),
              ValueIs(HasResultCode(mojom::ActionResultCode::kOk)));
  EXPECT_EQ(target_url, web_contents()->GetURL());

  StopActorTask(task_id, glic::mojom::ActorTaskStopReason::kTaskComplete);

  EXPECT_EQ(ActorTask::State::kFinished, task_completion_state.Get())
      << "Task " << task_id << " did not reach kFinished state.";
}

IN_PROC_BROWSER_TEST_F(ActorFunctionalBrowserTest, PerformClickAction) {
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
  Actions action =
      MakeClick(*web_contents()->GetPrimaryMainFrame(), link_node_id.value(),
                ClickAction::LEFT, ClickAction::SINGLE);
  action.set_task_id(task_id.value());

  EXPECT_THAT(PerformActions(action),
              ValueIs(HasResultCode(mojom::ActionResultCode::kOk)));
  EXPECT_EQ(target_url, web_contents()->GetURL());

  StopActorTask(task_id, glic::mojom::ActorTaskStopReason::kTaskComplete);
  EXPECT_EQ(ActorTask::State::kFinished, task_completion_state.Get())
      << "Task " << task_id << " did not reach kFinished state.";
}

IN_PROC_BROWSER_TEST_F(ActorFunctionalBrowserTest,
                       PerformConcurrentAsyncWaitActions) {
  // Manually create tasks via ActorKeyedService.
  TaskId task_id_1 =
      actor_keyed_service()->CreateTask(NoEnterprisePolicyChecker());
  TaskId task_id_2 =
      actor_keyed_service()->CreateTask(NoEnterprisePolicyChecker());

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
  Actions action_1 = MakeWaitForTaskId(kShortWaitTime * 2, tab_1, task_id_1);
  Actions action_2 = MakeWaitForTaskId(kShortWaitTime, tab_2, task_id_2);

  std::unique_ptr<AsyncActionWaiter> waiter_1 = PerformActionsAsync(action_1);
  std::unique_ptr<AsyncActionWaiter> waiter_2 = PerformActionsAsync(action_2);

  // We should still be able to wait for result_2 after result_1 despite
  // action_2 resolving first.
  ASSERT_OK_AND_ASSIGN(ActionsResult result_1, waiter_1->Wait());
  ASSERT_OK_AND_ASSIGN(ActionsResult result_2, waiter_2->Wait());

  // Verify a tab observation was included in the results.
  EXPECT_THAT(result_1, HasResultCode(mojom::ActionResultCode::kOk));
  EXPECT_THAT(result_1.tabs(), testing::SizeIs(1));
  EXPECT_THAT(result_1.tabs().at(0).result(),
              TabObservation::TAB_OBSERVATION_OK);

  EXPECT_THAT(result_2, HasResultCode(mojom::ActionResultCode::kOk));
  EXPECT_THAT(result_2.tabs(), testing::SizeIs(1));
  EXPECT_THAT(result_2.tabs().at(0).result(),
              TabObservation::TAB_OBSERVATION_OK);
}

IN_PROC_BROWSER_TEST_F(ActorFunctionalBrowserTest, CloseTabWhileActing) {
  base::HistogramTester histogram_tester;

  ASSERT_OK_AND_ASSIGN(TaskId task_id, CreateTask());
  EXPECT_NE(task_id, TaskId());

  TestFuture<ActorTask::State> task_completion_state;
  base::CallbackListSubscription subscription =
      CreateTaskCompletionSubscription(task_id, task_completion_state);

  optimization_guide::proto::Actions wait_action =
      MakeWaitForTaskId(kLongWaitTime, active_tab()->GetHandle(), task_id);
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
  histogram_tester.ExpectUniqueSample("Actor.ExecutionEngine.Action.ResultCode",
                                      mojom::ActionResultCode::kTaskWentAway,
                                      1);
}

IN_PROC_BROWSER_TEST_F(ActorFunctionalBrowserTest,
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
  Actions action = MakeClick(active_tab()->GetHandle(), gfx::Point(1, 1),
                             ::optimization_guide::proto::ClickAction::LEFT,
                             ::optimization_guide::proto::ClickAction::SINGLE);
  action.set_task_id(task_id.value());

  content::TestNavigationManager reload_observer(web_contents(), initial_url);
  EXPECT_THAT(
      PerformActions(action),
      ValueIs(HasResultCode(mojom::ActionResultCode::kRendererCrashed)));
  EXPECT_TRUE(reload_observer.WaitForNavigationFinished());
  EXPECT_FALSE(web_contents()->IsCrashed());
}

IN_PROC_BROWSER_TEST_F(ActorFunctionalBrowserTest,
                       RetryFailedContextFetchAfterPerformActions) {
  ASSERT_OK_AND_ASSIGN(TaskId task_id, CreateTask());
  ASSERT_NE(task_id, TaskId());

  // Perform a click action.
  ::optimization_guide::proto::Actions action =
      MakeClick(active_tab()->GetHandle(), gfx::Point(1, 1),
                ::optimization_guide::proto::ClickAction::LEFT,
                ::optimization_guide::proto::ClickAction::SINGLE);
  action.set_task_id(task_id.value());

  // Mock the context fetch so that the first time the TabObservationResult is a
  // failure. This should result in a retry which then succeeds.
  int num_calls = 0;
  ScopedMockTabObservationResult mock_result(base::BindLambdaForTesting(
      [&](TabObservation* observation, const FetchPageContextResult&) {
        ++num_calls;
        if (num_calls == 1) {
          observation->set_result(
              TabObservation::TAB_OBSERVATION_PAGE_CONTEXT_NOT_ELIGIBLE);
        } else {
          observation->set_result(TabObservation::TAB_OBSERVATION_OK);
          observation->set_annotated_page_content_result(
              TabObservation::ANNOTATED_PAGE_CONTENT_OK);
          observation->set_screenshot_result(TabObservation::SCREENSHOT_OK);
        }
      }));

  EXPECT_THAT(PerformActions(action),
              ValueIs(HasResultCode(mojom::ActionResultCode::kOk)));
  EXPECT_EQ(num_calls, 2);
}

IN_PROC_BROWSER_TEST_F(ActorFunctionalBrowserTest,
                       FailedContextFetchOnlyRetriesOnce) {
  ASSERT_OK_AND_ASSIGN(TaskId task_id, CreateTask());
  ASSERT_NE(task_id, TaskId());

  // Perform a click action.
  ::optimization_guide::proto::Actions action =
      MakeClick(active_tab()->GetHandle(), gfx::Point(1, 1),
                ::optimization_guide::proto::ClickAction::LEFT,
                ::optimization_guide::proto::ClickAction::SINGLE);
  action.set_task_id(task_id.value());

  int num_calls = 0;
  ScopedMockTabObservationResult mock_result(base::BindLambdaForTesting(
      [&](TabObservation* observation, const FetchPageContextResult&) {
        ++num_calls;
        observation->set_result(
            TabObservation::TAB_OBSERVATION_PAGE_CONTEXT_NOT_ELIGIBLE);
      }));

  optimization_guide::proto::ActionsResult result =
      PerformActions(action).value();
  EXPECT_THAT(result, HasResultCode(mojom::ActionResultCode::kOk));
  ASSERT_EQ(result.tabs_size(), 1);
  ASSERT_TRUE(result.tabs().at(0).has_result());
  EXPECT_EQ(result.tabs().at(0).result(),
            TabObservation::TAB_OBSERVATION_PAGE_CONTEXT_NOT_ELIGIBLE);

  EXPECT_EQ(num_calls, 2);
}

IN_PROC_BROWSER_TEST_F(ActorFunctionalBrowserTest, CancelActions) {
  // Makes sure we are on about:blank so the browser won't open a new tab to
  // navigate.
  ASSERT_TRUE(
      content::NavigateToURL(web_contents(), GURL(url::kAboutBlankURL)));

  ASSERT_OK_AND_ASSIGN(TaskId task_id, CreateTask());
  ASSERT_NE(task_id, TaskId());
  const GURL target_url = embedded_test_server()->GetURL("/title1.html");
  content::TestNavigationManager navigation_manager(web_contents(), target_url);

  optimization_guide::proto::Actions action = MakeNavigateForTaskId(
      active_tab()->GetHandle(), target_url.spec(), task_id);
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
  EXPECT_THAT(result.value(),
              HasResultCode(mojom::ActionResultCode::kActionsCancelled));
}

IN_PROC_BROWSER_TEST_F(ActorFunctionalBrowserTest,
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

IN_PROC_BROWSER_TEST_F(ActorFunctionalBrowserTest,
                       LogsActorTaskCreatedOnCreateTask) {
  base::HistogramTester histogram_tester;

  ASSERT_OK_AND_ASSIGN(TaskId task_id, CreateTask());
  EXPECT_NE(task_id, TaskId());

  constexpr std::string_view kActorTaskCreatedHistogram = "Actor.Task.Created";
  histogram_tester.ExpectUniqueSample(kActorTaskCreatedHistogram, true, 1);
}

class ActorFunctionalBrowserTestWithoutPolicyExemption
    : public ActorFunctionalBrowserTest {
 public:
  ActorFunctionalBrowserTestWithoutPolicyExemption() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{{features::kGlicActor,
                               {{features::kGlicActorPolicyControlExemption
                                     .name,
                                 "false"}}}},
        /*disabled_features=*/{});
  }
  ~ActorFunctionalBrowserTestWithoutPolicyExemption() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ActorFunctionalBrowserTestWithoutPolicyExemption,
                       LogsActorTaskFailedOnCreateTask) {
  base::HistogramTester histogram_tester;

  base::expected<TaskId, std::string> result = CreateTask();
  EXPECT_FALSE(result.has_value());

  constexpr std::string_view kActorTaskCreatedHistogram = "Actor.Task.Created";
  histogram_tester.ExpectUniqueSample(kActorTaskCreatedHistogram, false, 1);
}

class ActorPageContextMetricsTest : public ActorFunctionalBrowserTest {
 public:
  using ResultCallback =
      base::RepeatingCallback<void(size_t /*fetch_num*/,
                                   TabObservation*,
                                   const FetchPageContextResult&)>;
  void RunTestWithPageContextResult(ResultCallback result_callback) {
    ASSERT_OK_AND_ASSIGN(TaskId task_id, CreateTask());
    ASSERT_NE(task_id, TaskId());

    // Perform an arbitrary action.
    ::optimization_guide::proto::Actions action =
        MakeClick(active_tab()->GetHandle(), gfx::Point(1, 1),
                  ::optimization_guide::proto::ClickAction::LEFT,
                  ::optimization_guide::proto::ClickAction::SINGLE);
    action.set_task_id(task_id.value());

    // Each test case provides its own faked/mocked result for the
    // TabObservation.
    ScopedMockTabObservationResult mock_result(base::BindLambdaForTesting(
        [&, this](TabObservation* observation,
                  const FetchPageContextResult& fetch_result) {
          ++num_fetches_;
          result_callback.Run(num_fetches_, observation, fetch_result);
        }));

    auto result = PerformActions(action);

    ASSERT_TRUE(result.has_value());
  }

  void SuccessfulObservation(TabObservation* observation) {
    observation->set_result(TabObservation::TAB_OBSERVATION_OK);
    observation->set_annotated_page_content_result(
        TabObservation::ANNOTATED_PAGE_CONTENT_OK);
    observation->set_screenshot_result(TabObservation::SCREENSHOT_OK);
  }

  size_t num_fetches() const { return num_fetches_; }

 private:
  size_t num_fetches_ = 0;
};

IN_PROC_BROWSER_TEST_F(ActorPageContextMetricsTest,
                       ObservationOutcomeMetrics_Success) {
  base::HistogramTester histogram_tester;

  RunTestWithPageContextResult(base::BindLambdaForTesting(
      [&](size_t fetch_num, TabObservation* observation,
          const FetchPageContextResult&) {
        SuccessfulObservation(observation);
      }));

  ASSERT_EQ(num_fetches(), 1ul);

  histogram_tester.ExpectUniqueSample(kActorPageContextObservationOutcome,
                                      ActorObservationOutcome::kSuccess, 1);
}

IN_PROC_BROWSER_TEST_F(ActorPageContextMetricsTest,
                       ObservationOutcomeMetrics_SuccessAfterRetry) {
  base::HistogramTester histogram_tester;

  RunTestWithPageContextResult(base::BindLambdaForTesting(
      [&](size_t fetch_num, TabObservation* observation,
          const FetchPageContextResult&) {
        if (fetch_num == 1) {
          observation->set_result(
              TabObservation::TAB_OBSERVATION_PAGE_CONTEXT_NOT_ELIGIBLE);
        } else {
          SuccessfulObservation(observation);
        }
      }));

  ASSERT_EQ(num_fetches(), 2ul);

  histogram_tester.ExpectUniqueSample(
      kActorPageContextObservationOutcome,
      ActorObservationOutcome::kSuccessAfterRetry, 1);
}

IN_PROC_BROWSER_TEST_F(ActorPageContextMetricsTest,
                       ObservationOutcomeMetrics_Failure) {
  base::HistogramTester histogram_tester;

  RunTestWithPageContextResult(base::BindLambdaForTesting(
      [&](size_t fetch_num, TabObservation* observation,
          const FetchPageContextResult&) {
        observation->set_result(
            TabObservation::TAB_OBSERVATION_PAGE_CONTEXT_NOT_ELIGIBLE);
      }));

  ASSERT_EQ(num_fetches(), 2ul);

  histogram_tester.ExpectUniqueSample(
      kActorPageContextObservationOutcome,
      ActorObservationOutcome::kFailureAfterRetry, 1);
}

IN_PROC_BROWSER_TEST_F(ActorPageContextMetricsTest,
                       TabObservationResult_Success) {
  base::HistogramTester histogram_tester;

  RunTestWithPageContextResult(base::BindLambdaForTesting(
      [&](size_t fetch_num, TabObservation* observation,
          const FetchPageContextResult&) {
        SuccessfulObservation(observation);
      }));

  ASSERT_EQ(num_fetches(), 1ul);

  histogram_tester.ExpectUniqueSample(kActorPageContextTabObservationResult,
                                      ActorTabObservationResult::kSuccess, 1);
}

IN_PROC_BROWSER_TEST_F(ActorPageContextMetricsTest,
                       TabObservationResult_APCFailure) {
  base::HistogramTester histogram_tester;

  RunTestWithPageContextResult(base::BindLambdaForTesting(
      [&](size_t fetch_num, TabObservation* observation,
          const FetchPageContextResult&) {
        if (fetch_num == 1) {
          observation->set_result(TabObservation::TAB_OBSERVATION_FETCH_ERROR);
          observation->set_annotated_page_content_result(
              TabObservation::ANNOTATED_PAGE_CONTENT_ERROR);
          observation->set_screenshot_result(TabObservation::SCREENSHOT_OK);
        } else {
          SuccessfulObservation(observation);
        }
      }));

  ASSERT_EQ(num_fetches(), 2ul);

  // Ensure we record a failure in APC (for initial failure) and a success (for
  // retry).
  histogram_tester.ExpectTotalCount(kActorPageContextTabObservationResult, 2);
  histogram_tester.ExpectBucketCount(kActorPageContextTabObservationResult,
                                     ActorTabObservationResult::kApcError, 1);
  histogram_tester.ExpectBucketCount(kActorPageContextTabObservationResult,
                                     ActorTabObservationResult::kSuccess, 1);
}

IN_PROC_BROWSER_TEST_F(ActorPageContextMetricsTest,
                       TabObservationResult_RepeatedAPCFailure) {
  base::HistogramTester histogram_tester;

  RunTestWithPageContextResult(base::BindLambdaForTesting(
      [&](size_t fetch_num, TabObservation* observation,
          const FetchPageContextResult&) {
        observation->set_result(TabObservation::TAB_OBSERVATION_FETCH_ERROR);
        observation->set_annotated_page_content_result(
            TabObservation::ANNOTATED_PAGE_CONTENT_ERROR);
        observation->set_screenshot_result(TabObservation::SCREENSHOT_OK);
      }));

  ASSERT_EQ(num_fetches(), 2ul);

  // Ensure we record two failures in APC since the retry fails as well.
  histogram_tester.ExpectUniqueSample(kActorPageContextTabObservationResult,
                                      ActorTabObservationResult::kApcError, 2);
}

IN_PROC_BROWSER_TEST_F(ActorPageContextMetricsTest,
                       TabObservationResult_APCAndScreenshotFailure) {
  base::HistogramTester histogram_tester;

  RunTestWithPageContextResult(base::BindLambdaForTesting(
      [&](size_t fetch_num, TabObservation* observation,
          const FetchPageContextResult&) {
        observation->set_result(TabObservation::TAB_OBSERVATION_FETCH_ERROR);
        observation->set_annotated_page_content_result(
            TabObservation::ANNOTATED_PAGE_CONTENT_ERROR);
        observation->set_screenshot_result(TabObservation::SCREENSHOT_ERROR);
      }));

  ASSERT_EQ(num_fetches(), 2ul);

  // Since both APC and screenshot had failures ensure the combined bucket is
  // used.
  histogram_tester.ExpectUniqueSample(
      kActorPageContextTabObservationResult,
      ActorTabObservationResult::kApcAndScreenshotNotOk, 2);
}

IN_PROC_BROWSER_TEST_F(ActorPageContextMetricsTest,
                       TabObservationResult_MultipleFailures) {
  base::HistogramTester histogram_tester;

  RunTestWithPageContextResult(base::BindLambdaForTesting(
      [&](size_t fetch_num, TabObservation* observation,
          const FetchPageContextResult&) {
        if (fetch_num == 1) {
          observation->set_result(TabObservation::TAB_OBSERVATION_FETCH_ERROR);
          observation->set_annotated_page_content_result(
              TabObservation::ANNOTATED_PAGE_CONTENT_TIMEOUT);
          observation->set_screenshot_result(TabObservation::SCREENSHOT_OK);
        } else {
          observation->set_result(
              TabObservation::TAB_OBSERVATION_WEB_CONTENTS_CHANGED);
        }
      }));

  ASSERT_EQ(num_fetches(), 2ul);

  histogram_tester.ExpectTotalCount(kActorPageContextTabObservationResult, 2);
  histogram_tester.ExpectBucketCount(kActorPageContextTabObservationResult,
                                     ActorTabObservationResult::kApcTimeout, 1);
  histogram_tester.ExpectBucketCount(
      kActorPageContextTabObservationResult,
      ActorTabObservationResult::kWebContentsChanged, 1);
}

class ActorFunctionalBrowserTestCreateActorTab
    : public ActorFunctionalBrowserTest,
      public ::testing::WithParamInterface<GURL> {
 public:
  ActorFunctionalBrowserTestCreateActorTab() = default;
  ~ActorFunctionalBrowserTestCreateActorTab() override = default;

  GURL GetInitiatorTabUrl() { return GetParam(); }
};

IN_PROC_BROWSER_TEST_P(ActorFunctionalBrowserTestCreateActorTab,
                       CreateActorTab) {
  // Navigate the current tab to the initiator URL.
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetInitiatorTabUrl()));
  ASSERT_EQ(browser()->tab_strip_model()->count(), 1u);
  SessionID initiator_window_id = browser()->session_id();
  tabs::TabHandle initiator_tab = active_tab()->GetHandle();

  base::expected<TaskId, std::string> task_id = CreateTask();
  ASSERT_TRUE(task_id.has_value()) << task_id.error();

  // Create a new tab for the task.
  base::expected<tabs::TabHandle, std::string> new_tab_handler =
      CreateActorTab(task_id.value(), /*open_in_background=*/false,
                     base::ToString(initiator_tab.raw_value()),
                     base::ToString(initiator_window_id.id()));
  ASSERT_TRUE(new_tab_handler.has_value()) << new_tab_handler.error();

  // Verify it is bound to the task.
  EXPECT_TRUE(actor_keyed_service()
                  ->GetTask(task_id.value())
                  ->GetTabs()
                  .contains(new_tab_handler.value()));
}

IN_PROC_BROWSER_TEST_P(ActorFunctionalBrowserTestCreateActorTab,
                       CreateActorTabWithInvalidTask) {
  // Navigate the current tab to the initiator URL.
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetInitiatorTabUrl()));
  ASSERT_EQ(browser()->tab_strip_model()->count(), 1u);
  SessionID initiator_window_id = browser()->session_id();
  tabs::TabHandle initiator_tab = active_tab()->GetHandle();

  base::expected<TaskId, std::string> task_id = CreateTask();
  ASSERT_TRUE(task_id.has_value()) << task_id.error();

  TaskId invalid_task_id = actor::TaskId(task_id.value().value() + 100);

  // Create a new tab with an invalid task id.
  base::expected<tabs::TabHandle, std::string> new_tab_handler =
      CreateActorTab(invalid_task_id, /*open_in_background=*/false,
                     base::ToString(initiator_tab.raw_value()),
                     base::ToString(initiator_window_id.id()));

  // CreateActorTab should have returned an error;
  EXPECT_FALSE(new_tab_handler.has_value());

  // Verify it is bound to the task.
  EXPECT_TRUE(
      actor_keyed_service()->GetTask(task_id.value())->GetTabs().empty());
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    ActorFunctionalBrowserTestCreateActorTab,
    ::testing::Values(GURL(chrome::kChromeUINewTabURL),
                      GURL(url::kAboutBlankURL)));

}  // namespace
}  // namespace actor
