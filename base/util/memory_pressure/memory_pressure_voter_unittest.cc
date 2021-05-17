// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/util/memory_pressure/memory_pressure_voter.h"

#include "base/bind.h"
#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace util {

namespace {

class TestDelegate : public util::MemoryPressureVoteAggregator::Delegate {
 private:
  void OnMemoryPressureLevelChanged(
      base::MemoryPressureListener::MemoryPressureLevel level) override {}
  void OnNotifyListenersRequested() override {}
};

}  // namespace

TEST(MemoryPressureVoterTest, EvaluateVotes) {
  TestDelegate delegate;
  MemoryPressureVoteAggregator aggregator(&delegate);

  aggregator.SetVotesForTesting(1, 2, 3);
  EXPECT_EQ(aggregator.EvaluateVotesForTesting(),
            base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);

  aggregator.SetVotesForTesting(1, 20, 1);
  EXPECT_EQ(aggregator.EvaluateVotesForTesting(),
            base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);

  aggregator.SetVotesForTesting(0, 0, 0);
  EXPECT_EQ(aggregator.EvaluateVotesForTesting(),
            base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE);

  aggregator.SetVotesForTesting(0, 2, 0);
  EXPECT_EQ(aggregator.EvaluateVotesForTesting(),
            base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);

  // Reset votes so destructor doesn't think there are loose voters.
  aggregator.SetVotesForTesting(0, 0, 0);
}

TEST(MemoryPressureVoterTest, OnVote) {
  TestDelegate delegate;
  MemoryPressureVoteAggregator aggregator(&delegate);

  // vote count = 0,0,0
  EXPECT_EQ(aggregator.EvaluateVotesForTesting(),
            base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE);

  aggregator.OnVoteForTesting(
      base::nullopt, base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE);
  // vote count = 1,0,0
  EXPECT_EQ(aggregator.EvaluateVotesForTesting(),
            base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE);

  aggregator.OnVoteForTesting(
      base::nullopt,
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  // vote count = 1,0,1
  EXPECT_EQ(aggregator.EvaluateVotesForTesting(),
            base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);

  aggregator.OnVoteForTesting(
      base::nullopt,
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
  // vote count = 1,1,1
  EXPECT_EQ(aggregator.EvaluateVotesForTesting(),
            base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);

  aggregator.OnVoteForTesting(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL,
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
  // vote count = 1,2,0
  EXPECT_EQ(aggregator.EvaluateVotesForTesting(),
            base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);

  aggregator.OnVoteForTesting(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
      base::nullopt);
  // vote count = 1,1,0
  EXPECT_EQ(aggregator.EvaluateVotesForTesting(),
            base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);

  // Reset votes so destructor doesn't think there are loose voters.
  aggregator.SetVotesForTesting(0, 0, 0);
}

TEST(MemoryPressureVoterTest, SetVote) {
  TestDelegate delegate;
  MemoryPressureVoteAggregator aggregator(&delegate);
  auto voter_critical = aggregator.CreateVoter();
  auto voter_moderate = aggregator.CreateVoter();

  voter_critical->SetVote(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL, false);
  EXPECT_EQ(aggregator.EvaluateVotesForTesting(),
            base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);

  voter_moderate->SetVote(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE, false);
  EXPECT_EQ(aggregator.EvaluateVotesForTesting(),
            base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);

  voter_critical.reset();
  EXPECT_EQ(aggregator.EvaluateVotesForTesting(),
            base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);

  voter_moderate.reset();
  EXPECT_EQ(aggregator.EvaluateVotesForTesting(),
            base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE);
}

}  // namespace util
