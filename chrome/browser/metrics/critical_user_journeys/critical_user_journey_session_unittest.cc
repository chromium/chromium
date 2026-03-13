// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey_session.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/bind.h"
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
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestElementId1);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestElementId2);
constexpr ui::ElementContext kTestContext =
    ui::ElementContext::CreateFakeContextForTesting(1);
}  // namespace

class CriticalUserJourneySessionTest : public testing::Test {
 public:
  CriticalUserJourneySessionTest() = default;
  ~CriticalUserJourneySessionTest() override = default;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(CriticalUserJourneySessionTest, SimpleJourneyCompletion) {
  auto journey = CriticalUserJourney::Builder("Test Journey")
                     .AddStep(kTestElementId1,
                              ui::InteractionSequence::StepType::kShown, 1)
                     .AddStep(kTestElementId2,
                              ui::InteractionSequence::StepType::kShown, 2)
                     .Build();

  bool done = false;
  auto session = std::make_unique<CriticalUserJourneySession>(journey.get());
  session->set_on_done_callback(
      base::BindLambdaForTesting([&]() { done = true; }));

  // Step 1: Show element 1
  ui::test::TestElement el1(kTestElementId1, kTestContext);
  el1.Show();

  session->Start(&el1);

  // Step 2: Show element 2
  ui::test::TestElement el2(kTestElementId2, kTestContext);
  el2.Show();

  EXPECT_TRUE(base::test::RunUntil([&]() { return done; }));
}

TEST_F(CriticalUserJourneySessionTest, JourneyAborted) {
  auto journey = CriticalUserJourney::Builder("Test Journey")
                     .AddStep(kTestElementId1,
                              ui::InteractionSequence::StepType::kShown, 1)
                     .AddStep(kTestElementId2,
                              ui::InteractionSequence::StepType::kShown, 2)
                     .Build();

  bool done = false;
  auto session = std::make_unique<CriticalUserJourneySession>(journey.get());
  session->set_on_done_callback(
      base::BindLambdaForTesting([&]() { done = true; }));

  // Step 1: Show element 1
  ui::test::TestElement el1(kTestElementId1, kTestContext);
  el1.Show();

  session->Start(&el1);

  // Abort: Hide element 1 before Step 2 starts
  el1.Hide();

  EXPECT_TRUE(base::test::RunUntil([&]() { return done; }));
}

TEST_F(CriticalUserJourneySessionTest, CompletionCallbackTriggered) {
  bool journey_completed = false;
  auto journey = CriticalUserJourney::Builder("Test Journey")
                     .AddStep(kTestElementId1,
                              ui::InteractionSequence::StepType::kShown, 1)
                     .AddCustomCompletionCallback(base::BindLambdaForTesting(
                         [&]() { journey_completed = true; }))
                     .Build();

  bool session_done = false;
  auto session = std::make_unique<CriticalUserJourneySession>(journey.get());
  session->set_on_done_callback(
      base::BindLambdaForTesting([&]() { session_done = true; }));

  ui::test::TestElement el1(kTestElementId1, kTestContext);
  el1.Show();

  session->Start(&el1);

  EXPECT_TRUE(base::test::RunUntil([&]() { return session_done; }));
  EXPECT_TRUE(journey_completed);
}

TEST_F(CriticalUserJourneySessionTest, BranchingJourneyCompletion) {
  auto journey =
      CriticalUserJourney::Builder("Branching Journey")
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
  session->set_on_done_callback(
      base::BindLambdaForTesting([&]() { done = true; }));

  // Step 1: Show element 1
  ui::test::TestElement el1(kTestElementId1, kTestContext);
  el1.Show();

  session->Start(&el1);

  // Step 2: Choose one branch (e.g., show element 2)
  ui::test::TestElement el2(kTestElementId2, kTestContext);
  el2.Show();

  EXPECT_TRUE(base::test::RunUntil([&]() { return done; }));
}

}  // namespace metrics
