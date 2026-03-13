// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey_step.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/interaction_sequence.h"

namespace metrics {

DECLARE_ELEMENT_IDENTIFIER_VALUE(kTestElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kTestElementId);

TEST(CriticalUserJourneyTest, SimpleJourneyConstruction) {
  const std::string kJourneyName = "Test Journey";
  const int kMetricId = 101;

  auto journey =
      CriticalUserJourney::Builder(kJourneyName)
          .AddStep(kTestElementId, ui::InteractionSequence::StepType::kShown,
                   kMetricId)
          .Build();

  EXPECT_EQ(journey->name(), kJourneyName);
  ASSERT_EQ(journey->steps().size(), 1u);
  EXPECT_EQ(journey->steps()[0]->id, kTestElementId);
  EXPECT_EQ(journey->steps()[0]->type,
            ui::InteractionSequence::StepType::kShown);
  EXPECT_EQ(journey->steps()[0]->metric_id, kMetricId);
}

TEST(CriticalUserJourneyTest, BranchingLogic) {
  auto journey =
      CriticalUserJourney::Builder("Parent Journey")
          .AddAnyOf({
              Branch(kTestElementId, ui::InteractionSequence::StepType::kShown,
                     1),
              Branch(kTestElementId,
                     ui::InteractionSequence::StepType::kActivated, 2),
          })
          .Build();

  ASSERT_EQ(journey->steps().size(), 1u);
  auto* step = journey->steps()[0].get();
  EXPECT_EQ(step->type, ui::InteractionSequence::StepType::kSubsequence);
  EXPECT_EQ(step->mode, ui::InteractionSequence::SubsequenceMode::kAtLeastOne);
  ASSERT_EQ(step->branches.size(), 2u);
  EXPECT_EQ(step->branches[0]->steps().size(), 1u);
  EXPECT_EQ(step->branches[0]->steps()[0]->metric_id, 1);
  EXPECT_EQ(step->branches[1]->steps()[0]->metric_id, 2);
}

TEST(CriticalUserJourneyTest, CompletionCallback) {
  bool called = false;
  auto callback = base::BindLambdaForTesting([&]() { called = true; });

  auto journey = CriticalUserJourney::Builder("Test Journey")
                     .AddCustomCompletionCallback(callback)
                     .Build();

  ASSERT_FALSE(journey->completion_callback().is_null());
  journey->completion_callback().Run();
  EXPECT_TRUE(called);
}

}  // namespace metrics
