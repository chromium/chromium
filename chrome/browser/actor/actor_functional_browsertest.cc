// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/actor/actor_metrics.h"
#include "chrome/browser/actor/actor_proto_conversion.h"
#include "chrome/browser/glic/actor/glic_actor_functional_browsertest.h"
#include "chrome/browser/page_content_annotations/multi_source_page_context_fetcher.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"

namespace actor {
namespace {

using ::base::test::ValueIs;
using ::glic::actor::HasResultCode;
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
