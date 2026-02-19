// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_metrics.h"
#include "chrome/browser/glic/actor/glic_actor_functional_browsertest.h"
#include "content/public/test/browser_test.h"

namespace glic::actor {
namespace {

using ::actor::ActorObservationOutcome;
using ::actor::ActorTabObservationResult;
using ::glic::actor::ScopedMockTabObservationResult;
using ::optimization_guide::proto::Actions;
using ::optimization_guide::proto::ClickAction;
using ::optimization_guide::proto::TabObservation;
using ::page_content_annotations::FetchPageContextResult;

class GlicActorMetricsFunctionalBrowserTest
    : public GlicActorFunctionalBrowserTestBase {
 public:
  GlicActorMetricsFunctionalBrowserTest() = default;
  ~GlicActorMetricsFunctionalBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_F(GlicActorMetricsFunctionalBrowserTest,
                       LogsActorTaskCreatedOnCreateTask) {
  base::HistogramTester histogram_tester;

  ASSERT_OK_AND_ASSIGN(TaskId task_id, CreateTask());
  EXPECT_NE(task_id, TaskId());

  constexpr std::string_view kActorTaskCreatedHistogram = "Actor.Task.Created";
  histogram_tester.ExpectUniqueSample(kActorTaskCreatedHistogram, true, 1);
}

class GlicActorMetricsFunctionalBrowserTestWithoutPolicyExemption
    : public GlicActorMetricsFunctionalBrowserTest {
 public:
  GlicActorMetricsFunctionalBrowserTestWithoutPolicyExemption() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{{features::kGlicActor,
                               {{features::kGlicActorPolicyControlExemption
                                     .name,
                                 "false"}}}},
        /*disabled_features=*/{});
  }
  ~GlicActorMetricsFunctionalBrowserTestWithoutPolicyExemption() override =
      default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    GlicActorMetricsFunctionalBrowserTestWithoutPolicyExemption,
    LogsActorTaskFailedOnCreateTask) {
  base::HistogramTester histogram_tester;

  base::expected<TaskId, std::string> result = CreateTask();
  EXPECT_FALSE(result.has_value());

  constexpr std::string_view kActorTaskCreatedHistogram = "Actor.Task.Created";
  histogram_tester.ExpectUniqueSample(kActorTaskCreatedHistogram, false, 1);
}

class GlicActorPageContextMetricsFunctionalBrowserTest
    : public GlicActorFunctionalBrowserTestBase {
 public:
  using ResultCallback =
      base::RepeatingCallback<void(size_t /*fetch_num*/,
                                   TabObservation*,
                                   const FetchPageContextResult&)>;
  void RunTestWithPageContextResult(ResultCallback result_callback) {
    ASSERT_OK_AND_ASSIGN(TaskId task_id, CreateTask());
    ASSERT_NE(task_id, TaskId());

    // Perform an arbitrary action.
    Actions action =
        ::actor::MakeClick(active_tab()->GetHandle(), gfx::Point(1, 1),
                           ClickAction::LEFT, ClickAction::SINGLE, task_id);

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

IN_PROC_BROWSER_TEST_F(GlicActorPageContextMetricsFunctionalBrowserTest,
                       ObservationOutcomeMetrics_Success) {
  base::HistogramTester histogram_tester;

  RunTestWithPageContextResult(base::BindLambdaForTesting(
      [&](size_t fetch_num, TabObservation* observation,
          const FetchPageContextResult&) {
        SuccessfulObservation(observation);
      }));

  ASSERT_EQ(num_fetches(), 1ul);

  histogram_tester.ExpectUniqueSample(
      ::actor::kActorPageContextObservationOutcome,
      ActorObservationOutcome::kSuccess, 1);
}

IN_PROC_BROWSER_TEST_F(GlicActorPageContextMetricsFunctionalBrowserTest,
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
      ::actor::kActorPageContextObservationOutcome,
      ActorObservationOutcome::kSuccessAfterRetry, 1);
}

IN_PROC_BROWSER_TEST_F(GlicActorPageContextMetricsFunctionalBrowserTest,
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
      ::actor::kActorPageContextObservationOutcome,
      ActorObservationOutcome::kFailureAfterRetry, 1);
}

IN_PROC_BROWSER_TEST_F(GlicActorPageContextMetricsFunctionalBrowserTest,
                       TabObservationResult_Success) {
  base::HistogramTester histogram_tester;

  RunTestWithPageContextResult(base::BindLambdaForTesting(
      [&](size_t fetch_num, TabObservation* observation,
          const FetchPageContextResult&) {
        SuccessfulObservation(observation);
      }));

  ASSERT_EQ(num_fetches(), 1ul);

  histogram_tester.ExpectUniqueSample(
      ::actor::kActorPageContextTabObservationResult,
      ActorTabObservationResult::kSuccess, 1);
}

IN_PROC_BROWSER_TEST_F(GlicActorPageContextMetricsFunctionalBrowserTest,
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
  histogram_tester.ExpectTotalCount(
      ::actor::kActorPageContextTabObservationResult, 2);
  histogram_tester.ExpectBucketCount(
      ::actor::kActorPageContextTabObservationResult,
      ActorTabObservationResult::kApcError, 1);
  histogram_tester.ExpectBucketCount(
      ::actor::kActorPageContextTabObservationResult,
      ActorTabObservationResult::kSuccess, 1);
}

IN_PROC_BROWSER_TEST_F(GlicActorPageContextMetricsFunctionalBrowserTest,
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
  histogram_tester.ExpectUniqueSample(
      ::actor::kActorPageContextTabObservationResult,
      ActorTabObservationResult::kApcError, 2);
}

IN_PROC_BROWSER_TEST_F(GlicActorPageContextMetricsFunctionalBrowserTest,
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
      ::actor::kActorPageContextTabObservationResult,
      ActorTabObservationResult::kApcAndScreenshotNotOk, 2);
}

IN_PROC_BROWSER_TEST_F(GlicActorPageContextMetricsFunctionalBrowserTest,
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

  histogram_tester.ExpectTotalCount(
      ::actor::kActorPageContextTabObservationResult, 2);
  histogram_tester.ExpectBucketCount(
      ::actor::kActorPageContextTabObservationResult,
      ActorTabObservationResult::kApcTimeout, 1);
  histogram_tester.ExpectBucketCount(
      ::actor::kActorPageContextTabObservationResult,
      ActorTabObservationResult::kWebContentsChanged, 1);
}

}  // namespace
}  // namespace glic::actor
