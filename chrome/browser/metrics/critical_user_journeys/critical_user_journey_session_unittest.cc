// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey_session.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey.h"
#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey_step.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_test_util.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_sequence.h"

namespace metrics {

namespace {
BASE_FEATURE(kTestJourney, base::FEATURE_ENABLED_BY_DEFAULT);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestElementId1);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestElementId2);
constexpr ui::ElementContext kTestContext =
    ui::ElementContext::CreateFakeContextForTesting(1);
}  // namespace

class CriticalUserJourneySessionTest : public testing::Test {
 public:
  CriticalUserJourneySessionTest() = default;
  ~CriticalUserJourneySessionTest() override = default;

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(CriticalUserJourneySessionTest, SimpleJourneyCompletion) {
  auto journey = CriticalUserJourney::Builder(&kTestJourney)
                     .AddStep(kTestElementId1,
                              ui::InteractionSequence::StepType::kShown, 1)
                     .AddStep(kTestElementId2,
                              ui::InteractionSequence::StepType::kShown, 2)
                     .Build();

  bool done = false;
  auto session = std::make_unique<CriticalUserJourneySession>(journey.get());
  session->set_on_done_callback(base::BindLambdaForTesting(
      [&](CriticalUserJourneySession::JourneyResult) { done = true; }));

  // Step 1: Show element 1
  ui::test::TestElement el1(kTestElementId1, kTestContext);
  el1.Show();

  session->Start(/*first_step_metric_id=*/std::nullopt, &el1);

  // Step 2: Show element 2
  ui::test::TestElement el2(kTestElementId2, kTestContext);
  el2.Show();

  EXPECT_TRUE(base::test::RunUntil([&]() { return done; }));
}

TEST_F(CriticalUserJourneySessionTest, JourneyAborted) {
  base::HistogramTester histogram_tester;
  auto journey = CriticalUserJourney::Builder(&kTestJourney)
                     .AddStep(kTestElementId1,
                              ui::InteractionSequence::StepType::kShown, 1)
                     .AddStep(kTestElementId2,
                              ui::InteractionSequence::StepType::kShown, 2)
                     .Build();

  bool done = false;
  auto session = std::make_unique<CriticalUserJourneySession>(journey.get());
  session->set_on_done_callback(base::BindLambdaForTesting(
      [&](CriticalUserJourneySession::JourneyResult) { done = true; }));

  // Step 1: Show element 1
  ui::test::TestElement el1(kTestElementId1, kTestContext);
  el1.Show();

  session->Start(/*first_step_metric_id=*/std::nullopt, &el1);

  // Abort: Hide element 1 before Step 2 starts
  el1.Hide();

  EXPECT_TRUE(base::test::RunUntil([&]() { return done; }));
  histogram_tester.ExpectUniqueSample(
      "CriticalUserJourney.TestJourney.Result",
      CriticalUserJourneySession::JourneyResult::kAborted, 1);
}

TEST_F(CriticalUserJourneySessionTest, CompletionCallbackTriggered) {
  bool journey_completed = false;
  auto journey = CriticalUserJourney::Builder(&kTestJourney)
                     .AddStep(kTestElementId1,
                              ui::InteractionSequence::StepType::kShown, 1)
                     .AddCustomCompletionCallback(base::BindLambdaForTesting(
                         [&]() { journey_completed = true; }))
                     .Build();

  bool session_done = false;
  auto session = std::make_unique<CriticalUserJourneySession>(journey.get());
  session->set_on_done_callback(base::BindLambdaForTesting(
      [&](CriticalUserJourneySession::JourneyResult) { session_done = true; }));

  ui::test::TestElement el1(kTestElementId1, kTestContext);
  el1.Show();

  session->Start(/*first_step_metric_id=*/std::nullopt, &el1);

  EXPECT_TRUE(base::test::RunUntil([&]() { return session_done; }));
  EXPECT_TRUE(journey_completed);
}

TEST_F(CriticalUserJourneySessionTest, BranchingJourneyCompletion) {
  auto journey =
      CriticalUserJourney::Builder(&kTestJourney)
          .AddStep(kTestElementId1, ui::InteractionSequence::StepType::kShown,
                   1)
          .AddAnyOf({
              Branch(kTestElementId1,
                     ui::InteractionSequence::StepType::kActivated, 2),
              Branch(kTestElementId2, ui::InteractionSequence::StepType::kShown,
                     3),
          })
          .Build();

  bool done = false;
  auto session = std::make_unique<CriticalUserJourneySession>(journey.get());
  session->set_on_done_callback(base::BindLambdaForTesting(
      [&](CriticalUserJourneySession::JourneyResult) { done = true; }));

  // Step 1: Show element 1
  ui::test::TestElement el1(kTestElementId1, kTestContext);
  el1.Show();

  session->Start(/*first_step_metric_id=*/std::nullopt, &el1);

  // Step 2: Choose one branch (e.g., show element 2)
  ui::test::TestElement el2(kTestElementId2, kTestContext);
  el2.Show();

  EXPECT_TRUE(base::test::RunUntil([&]() { return done; }));
}

TEST_F(CriticalUserJourneySessionTest, JourneyTimeout) {
  base::HistogramTester histogram_tester;
  auto journey = CriticalUserJourney::Builder(&kTestJourney)
                     .AddStep(kTestElementId1,
                              ui::InteractionSequence::StepType::kShown, 1)
                     .AddStep(kTestElementId2,
                              ui::InteractionSequence::StepType::kShown, 2)
                     .Build();

  bool done = false;
  auto session = std::make_unique<CriticalUserJourneySession>(journey.get());
  session->set_on_done_callback(base::BindLambdaForTesting(
      [&](CriticalUserJourneySession::JourneyResult) { done = true; }));

  ui::test::TestElement el1(kTestElementId1, kTestContext);
  el1.Show();

  session->Start(/*first_step_metric_id=*/std::nullopt, &el1);

  // Fast forward by 2 minutes to trigger the timeout.
  task_environment_.FastForwardBy(base::Minutes(2));

  EXPECT_TRUE(done);
  histogram_tester.ExpectUniqueSample(
      "CriticalUserJourney.TestJourney.Result",
      CriticalUserJourneySession::JourneyResult::kTimeout, 1);
}

namespace {

// Builds a journey whose final step has a completion branch (el2 activated)
// and an exit branch (el2 hidden) with a 1500 ms minimum dwell.
std::unique_ptr<CriticalUserJourney> BuildExitBranchJourney(
    base::RepeatingClosure completion_callback = base::DoNothing()) {
  return CriticalUserJourney::Builder(&kTestJourney)
      .AddStep(kTestElementId1, ui::InteractionSequence::StepType::kShown, 1)
      .AddAnyOf({
          Branch(kTestElementId2, ui::InteractionSequence::StepType::kActivated,
                 2),
          Branch(kTestElementId2, ui::InteractionSequence::StepType::kHidden, 3)
              .SetExitBranch(base::Milliseconds(1500)),
      })
      .AddCustomCompletionCallback(std::move(completion_callback))
      .Build();
}

}  // namespace

TEST_F(CriticalUserJourneySessionTest, ExitBranchRecordsAbandoned) {
  base::HistogramTester histogram_tester;
  bool completion_callback_called = false;
  auto journey = BuildExitBranchJourney(
      base::BindLambdaForTesting([&]() { completion_callback_called = true; }));

  std::optional<CriticalUserJourneySession::JourneyResult> final_result;
  auto session = std::make_unique<CriticalUserJourneySession>(journey.get());
  session->set_on_done_callback(base::BindLambdaForTesting(
      [&](CriticalUserJourneySession::JourneyResult result) {
        final_result = result;
      }));

  ui::test::TestElement el1(kTestElementId1, kTestContext);
  el1.Show();
  session->Start(/*first_step_metric_id=*/std::nullopt, &el1);

  // The trigger element stays visible. Show el2, wait past the dwell
  // threshold, then hide el2 to take the exit branch.
  ui::test::TestElement el2(kTestElementId2, kTestContext);
  el2.Show();
  task_environment_.FastForwardBy(base::Milliseconds(2000));
  el2.Hide();

  EXPECT_TRUE(base::test::RunUntil([&]() { return final_result.has_value(); }));
  EXPECT_EQ(*final_result,
            CriticalUserJourneySession::JourneyResult::kAbandoned);
  EXPECT_FALSE(completion_callback_called);
  histogram_tester.ExpectUniqueSample(
      "CriticalUserJourney.TestJourney.Result",
      CriticalUserJourneySession::JourneyResult::kAbandoned, 1);
  histogram_tester.ExpectBucketCount(
      "CriticalUserJourney.TestJourney.StepReached", 3, 1);
  histogram_tester.ExpectTotalCount(
      "CriticalUserJourney.TestJourney.OverallDuration", 0);
}

TEST_F(CriticalUserJourneySessionTest, ExitBranchBelowDwellThreshold) {
  base::HistogramTester histogram_tester;
  bool completion_callback_called = false;
  auto journey = BuildExitBranchJourney(
      base::BindLambdaForTesting([&]() { completion_callback_called = true; }));

  std::optional<CriticalUserJourneySession::JourneyResult> final_result;
  auto session = std::make_unique<CriticalUserJourneySession>(journey.get());
  session->set_on_done_callback(base::BindLambdaForTesting(
      [&](CriticalUserJourneySession::JourneyResult result) {
        final_result = result;
      }));

  ui::test::TestElement el1(kTestElementId1, kTestContext);
  el1.Show();
  session->Start(/*first_step_metric_id=*/std::nullopt, &el1);

  // Hide el2 after only 500 ms (< 1500 ms threshold).
  ui::test::TestElement el2(kTestElementId2, kTestContext);
  el2.Show();
  task_environment_.FastForwardBy(base::Milliseconds(500));
  el2.Hide();

  EXPECT_TRUE(base::test::RunUntil([&]() { return final_result.has_value(); }));
  EXPECT_EQ(*final_result,
            CriticalUserJourneySession::JourneyResult::kAbandonedBelowDwell);
  EXPECT_FALSE(completion_callback_called);
  histogram_tester.ExpectUniqueSample(
      "CriticalUserJourney.TestJourney.Result",
      CriticalUserJourneySession::JourneyResult::kAbandonedBelowDwell, 1);
  histogram_tester.ExpectBucketCount(
      "CriticalUserJourney.TestJourney.StepReached", 3, 0);
}

TEST_F(CriticalUserJourneySessionTest, CompletionBranchWithExitBranchSibling) {
  base::HistogramTester histogram_tester;
  bool completion_callback_called = false;
  auto journey = BuildExitBranchJourney(
      base::BindLambdaForTesting([&]() { completion_callback_called = true; }));

  std::optional<CriticalUserJourneySession::JourneyResult> final_result;
  auto session = std::make_unique<CriticalUserJourneySession>(journey.get());
  session->set_on_done_callback(base::BindLambdaForTesting(
      [&](CriticalUserJourneySession::JourneyResult result) {
        final_result = result;
      }));

  ui::test::TestElement el1(kTestElementId1, kTestContext);
  el1.Show();
  session->Start(/*first_step_metric_id=*/std::nullopt, &el1);

  // el2 is not visible when the branches start. The completion branch must
  // still be able to finish once el2 appears and is activated.
  task_environment_.FastForwardBy(base::Milliseconds(100));
  ui::test::TestElement el2(kTestElementId2, kTestContext);
  el2.Show();
  el2.Activate();

  EXPECT_TRUE(base::test::RunUntil([&]() { return final_result.has_value(); }));
  EXPECT_EQ(*final_result,
            CriticalUserJourneySession::JourneyResult::kCompleted);
  EXPECT_TRUE(completion_callback_called);
  histogram_tester.ExpectUniqueSample(
      "CriticalUserJourney.TestJourney.Result",
      CriticalUserJourneySession::JourneyResult::kCompleted, 1);
  histogram_tester.ExpectBucketCount(
      "CriticalUserJourney.TestJourney.StepReached", 2, 1);
}

// Regression test: a plain kActivated step still aborts when its element is
// not visible at the start of the step.
TEST_F(CriticalUserJourneySessionTest,
       ActivatedStepAbortsWhenElementNotVisibleAtStart) {
  base::HistogramTester histogram_tester;
  auto journey = CriticalUserJourney::Builder(&kTestJourney)
                     .AddStep(kTestElementId1,
                              ui::InteractionSequence::StepType::kShown, 1)
                     .AddStep(kTestElementId2,
                              ui::InteractionSequence::StepType::kActivated, 2)
                     .Build();

  std::optional<CriticalUserJourneySession::JourneyResult> final_result;
  auto session = std::make_unique<CriticalUserJourneySession>(journey.get());
  session->set_on_done_callback(base::BindLambdaForTesting(
      [&](CriticalUserJourneySession::JourneyResult result) {
        final_result = result;
      }));

  ui::test::TestElement el1(kTestElementId1, kTestContext);
  el1.Show();
  session->Start(/*first_step_metric_id=*/std::nullopt, &el1);

  EXPECT_TRUE(base::test::RunUntil([&]() { return final_result.has_value(); }));
  EXPECT_EQ(*final_result, CriticalUserJourneySession::JourneyResult::kAborted);
  histogram_tester.ExpectUniqueSample(
      "CriticalUserJourney.TestJourney.Result",
      CriticalUserJourneySession::JourneyResult::kAborted, 1);
}

using CriticalUserJourneySessionDeathTest = CriticalUserJourneySessionTest;

TEST_F(CriticalUserJourneySessionDeathTest, ExitBranchNotInFinalStepCrashes) {
  EXPECT_CHECK_DEATH(
      CriticalUserJourney::Builder(&kTestJourney)
          .AddStep(kTestElementId1, ui::InteractionSequence::StepType::kShown,
                   1)
          .AddAnyOf({
              Branch(kTestElementId2,
                     ui::InteractionSequence::StepType::kActivated, 2),
              Branch(kTestElementId2,
                     ui::InteractionSequence::StepType::kHidden, 3)
                  .SetExitBranch(),
          })
          .AddStep(kTestElementId1, ui::InteractionSequence::StepType::kHidden,
                   4)
          .Build());
}

TEST_F(CriticalUserJourneySessionDeathTest, ExitBranchInFirstStepCrashes) {
  EXPECT_CHECK_DEATH(
      CriticalUserJourney::Builder(&kTestJourney)
          .AddAnyOf({
              Branch(kTestElementId1,
                     ui::InteractionSequence::StepType::kActivated, 1),
              Branch(kTestElementId2,
                     ui::InteractionSequence::StepType::kActivated, 2)
                  .SetExitBranch(),
          })
          .Build());
}

}  // namespace metrics
