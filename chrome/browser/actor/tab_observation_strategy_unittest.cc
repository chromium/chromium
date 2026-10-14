// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tab_observation_strategy.h"

#include "chrome/browser/actor/actor_surface_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor {

class TabObservationStrategyTest : public testing::Test {
 protected:
  const ActorSurfaceHandle actor_surface_handle1_ =
      ActorSurfaceHandle::CreateForTesting();
  const ActorSurfaceHandle actor_surface_handle2_ =
      ActorSurfaceHandle::CreateForTesting();
};

TEST_F(TabObservationStrategyTest, DefaultPoliciesWhenNoVotes) {
  TabObservationStrategy strategy;
  strategy.Lock();
  EXPECT_EQ(strategy.GetScreenshotPolicy(actor_surface_handle1_),
            ScreenshotPolicy::kRequested);
  EXPECT_EQ(strategy.GetPageContentExtractionPolicy(actor_surface_handle1_),
            PageContentExtractionPolicy::kRequested);
}

TEST_F(TabObservationStrategyTest, SingleVote) {
  TabObservationStrategy strategy;
  strategy.VoteForScreenshot(actor_surface_handle1_,
                             ScreenshotPolicy::kRequired);
  strategy.VoteForPageContentExtraction(actor_surface_handle1_,
                                        PageContentExtractionPolicy::kSkipped);
  strategy.Lock();

  EXPECT_EQ(strategy.GetScreenshotPolicy(actor_surface_handle1_),
            ScreenshotPolicy::kRequired);
  EXPECT_EQ(strategy.GetPageContentExtractionPolicy(actor_surface_handle1_),
            PageContentExtractionPolicy::kSkipped);
}

TEST_F(TabObservationStrategyTest, CombineVotesRequiredOverridesSkipped) {
  TabObservationStrategy strategy;
  // Tool 1 votes kSkipped
  strategy.VoteForScreenshot(actor_surface_handle1_,
                             ScreenshotPolicy::kSkipped);
  // Tool 2 votes kRequired
  strategy.VoteForScreenshot(actor_surface_handle1_,
                             ScreenshotPolicy::kRequired);
  strategy.Lock();

  EXPECT_EQ(strategy.GetScreenshotPolicy(actor_surface_handle1_),
            ScreenshotPolicy::kRequired);
}

TEST_F(TabObservationStrategyTest, CombineVotesRequestedOverridesSkipped) {
  TabObservationStrategy strategy;
  // Tool 1 votes kSkipped
  strategy.VoteForScreenshot(actor_surface_handle1_,
                             ScreenshotPolicy::kSkipped);
  // Tool 2 votes kRequested
  strategy.VoteForScreenshot(actor_surface_handle1_,
                             ScreenshotPolicy::kRequested);
  strategy.Lock();

  EXPECT_EQ(strategy.GetScreenshotPolicy(actor_surface_handle1_),
            ScreenshotPolicy::kRequested);
}

TEST_F(TabObservationStrategyTest, MultiTabIndependentTracking) {
  TabObservationStrategy strategy;
  strategy.VoteForScreenshot(actor_surface_handle1_,
                             ScreenshotPolicy::kRequired);
  strategy.VoteForScreenshot(actor_surface_handle2_,
                             ScreenshotPolicy::kSkipped);
  strategy.Lock();

  EXPECT_EQ(strategy.GetScreenshotPolicy(actor_surface_handle1_),
            ScreenshotPolicy::kRequired);
  EXPECT_EQ(strategy.GetScreenshotPolicy(actor_surface_handle2_),
            ScreenshotPolicy::kSkipped);
}

TEST_F(TabObservationStrategyTest, LockPreventsVoting) {
  TabObservationStrategy strategy;
  strategy.Lock();

  EXPECT_DEATH_IF_SUPPORTED(
      strategy.VoteForScreenshot(actor_surface_handle1_,
                                 ScreenshotPolicy::kRequired),
      "");
  EXPECT_DEATH_IF_SUPPORTED(
      strategy.VoteForPageContentExtraction(
          actor_surface_handle1_, PageContentExtractionPolicy::kRequired),
      "");
}

TEST_F(TabObservationStrategyTest, QueryBeforeLockFails) {
  TabObservationStrategy strategy;

  EXPECT_DEATH_IF_SUPPORTED(
      strategy.GetScreenshotPolicy(actor_surface_handle1_), "");
  EXPECT_DEATH_IF_SUPPORTED(
      strategy.GetPageContentExtractionPolicy(actor_surface_handle1_), "");
}

TEST_F(TabObservationStrategyTest,
       VoteForBothScreenshotAndDomExtractionOnSameTab) {
  TabObservationStrategy strategy;
  strategy.VoteForScreenshot(actor_surface_handle1_,
                             ScreenshotPolicy::kRequired);
  strategy.VoteForPageContentExtraction(actor_surface_handle1_,
                                        PageContentExtractionPolicy::kRequired);
  strategy.Lock();

  EXPECT_EQ(strategy.GetScreenshotPolicy(actor_surface_handle1_),
            ScreenshotPolicy::kRequired);
  EXPECT_EQ(strategy.GetPageContentExtractionPolicy(actor_surface_handle1_),
            PageContentExtractionPolicy::kRequired);
}

}  // namespace actor
