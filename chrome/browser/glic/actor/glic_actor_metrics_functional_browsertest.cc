// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_metrics.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/glic/actor/glic_actor_functional_browsertest.h"
#include "chrome/browser/glic/actor/glic_actor_policy_checker.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "components/actor/core/actor_features.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace glic::actor {
namespace {
using ::actor::ActorObservationOutcome;
using ::actor::ActorTabObservationResult;
using ::actor::ScopedMockTabObservationResult;
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

  histogram_tester.ExpectUniqueSample("Actor.Task.Created", true, 1);
  histogram_tester.ExpectUniqueSample("Actor.Task.CreateFailedReason", 0, 1);
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

  histogram_tester.ExpectUniqueSample("Actor.Task.Created", false, 1);
  histogram_tester.ExpectTotalCount("Actor.Task.CreateFailedReason", 1);
}

class GlicActorMetricsFunctionalBrowserTestWithDisabledPolicy
    : public GlicActorMetricsFunctionalBrowserTestWithoutPolicyExemption {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    glic_test_environment().SetForceSigninAndModelExecutionCapability(true);
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
    policy::ManagementServiceFactory::GetForPlatform()
        ->SetManagementAuthoritiesForTesting(
            policy::EnterpriseManagementAuthority::CLOUD);
    GlicActorMetricsFunctionalBrowserTestWithoutPolicyExemption::
        SetUpInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    policy::ManagementServiceFactory::GetForProfile(browser()->profile())
        ->SetManagementAuthoritiesForTesting(
            policy::EnterpriseManagementAuthority::CLOUD);
    GlicActorMetricsFunctionalBrowserTestWithoutPolicyExemption::
        SetUpOnMainThread();
    policy::PolicyMap policies;
    // Set GeminiActOnWebSettings to Disabled (1)
    policies.Set(policy::key::kGeminiActOnWebSettings,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_ENTERPRISE_DEFAULT, base::Value(1),
                 nullptr);
    policy_provider_.UpdateChromePolicy(policies);

    glic_test_service().SetModelExecutionCapability(true);
  }

 private:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

IN_PROC_BROWSER_TEST_F(GlicActorMetricsFunctionalBrowserTestWithDisabledPolicy,
                       LogsActorTaskFailedOnCreateTaskPolicyDisabled) {
  base::HistogramTester histogram_tester;

  base::expected<TaskId, std::string> result = CreateTask();
  EXPECT_FALSE(result.has_value());

  histogram_tester.ExpectUniqueSample("Actor.Task.Created", false, 1);
  histogram_tester.ExpectUniqueSample(
      "Actor.Task.CreateFailedReason",
      GlicActorPolicyChecker::CannotActReason::kDisabledByPolicy, 1);
}

class GlicActorPageContextMetricsFunctionalBrowserTest
    : public GlicActorFunctionalBrowserTestBase,
      public testing::WithParamInterface<bool> {
 public:
  GlicActorPageContextMetricsFunctionalBrowserTest() {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          ::actor::kGlicActorTabObservationController);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          ::actor::kGlicActorTabObservationController);
    }
  }
  ~GlicActorPageContextMetricsFunctionalBrowserTest() override = default;

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
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(GlicActorPageContextMetricsFunctionalBrowserTest,
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

IN_PROC_BROWSER_TEST_P(GlicActorPageContextMetricsFunctionalBrowserTest,
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

IN_PROC_BROWSER_TEST_P(GlicActorPageContextMetricsFunctionalBrowserTest,
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

IN_PROC_BROWSER_TEST_P(GlicActorPageContextMetricsFunctionalBrowserTest,
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

IN_PROC_BROWSER_TEST_P(GlicActorPageContextMetricsFunctionalBrowserTest,
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

  if (base::FeatureList::IsEnabled(
          ::actor::kGlicActorTabObservationController)) {
    // Ensure we record the final outcome (success).
    histogram_tester.ExpectTotalCount(
        ::actor::kActorPageContextTabObservationResult, 1);
    histogram_tester.ExpectBucketCount(
        ::actor::kActorPageContextTabObservationResult,
        ActorTabObservationResult::kSuccess, 1);
  } else {
    // Ensure we record a failure in APC (for initial failure) and a success
    // (for retry).
    histogram_tester.ExpectTotalCount(
        ::actor::kActorPageContextTabObservationResult, 2);
    histogram_tester.ExpectBucketCount(
        ::actor::kActorPageContextTabObservationResult,
        ActorTabObservationResult::kApcError, 1);
    histogram_tester.ExpectBucketCount(
        ::actor::kActorPageContextTabObservationResult,
        ActorTabObservationResult::kSuccess, 1);
  }
}

IN_PROC_BROWSER_TEST_P(GlicActorPageContextMetricsFunctionalBrowserTest,
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

  if (base::FeatureList::IsEnabled(
          ::actor::kGlicActorTabObservationController)) {
    // The final outcome is recorded.
    histogram_tester.ExpectUniqueSample(
        ::actor::kActorPageContextTabObservationResult,
        ActorTabObservationResult::kApcError, 1);
  } else {
    // Ensure we record two failures in APC since the retry fails as well.
    histogram_tester.ExpectUniqueSample(
        ::actor::kActorPageContextTabObservationResult,
        ActorTabObservationResult::kApcError, 2);
  }
}

IN_PROC_BROWSER_TEST_P(GlicActorPageContextMetricsFunctionalBrowserTest,
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
  if (base::FeatureList::IsEnabled(
          ::actor::kGlicActorTabObservationController)) {
    histogram_tester.ExpectUniqueSample(
        ::actor::kActorPageContextTabObservationResult,
        ActorTabObservationResult::kApcAndScreenshotNotOk, 1);
  } else {
    histogram_tester.ExpectUniqueSample(
        ::actor::kActorPageContextTabObservationResult,
        ActorTabObservationResult::kApcAndScreenshotNotOk, 2);
  }
}

IN_PROC_BROWSER_TEST_P(GlicActorPageContextMetricsFunctionalBrowserTest,
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

  if (base::FeatureList::IsEnabled(
          ::actor::kGlicActorTabObservationController)) {
    // Ensure we record the final outcome (failure).
    histogram_tester.ExpectTotalCount(
        ::actor::kActorPageContextTabObservationResult, 1);
    histogram_tester.ExpectBucketCount(
        ::actor::kActorPageContextTabObservationResult,
        ActorTabObservationResult::kWebContentsChanged, 1);
  } else {
    histogram_tester.ExpectTotalCount(
        ::actor::kActorPageContextTabObservationResult, 2);
    histogram_tester.ExpectBucketCount(
        ::actor::kActorPageContextTabObservationResult,
        ActorTabObservationResult::kApcTimeout, 1);
    histogram_tester.ExpectBucketCount(
        ::actor::kActorPageContextTabObservationResult,
        ActorTabObservationResult::kWebContentsChanged, 1);
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         GlicActorPageContextMetricsFunctionalBrowserTest,
                         testing::Bool());

}  // namespace
}  // namespace glic::actor
