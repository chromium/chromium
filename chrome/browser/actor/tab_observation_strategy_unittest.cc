// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tab_observation_strategy.h"

#include "components/tabs/public/tab_interface.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor {

class TabObservationStrategyTest : public testing::Test {
 protected:
  tabs::TabHandle tab1_{1};
  tabs::TabHandle tab2_{2};
};

TEST_F(TabObservationStrategyTest, DefaultPoliciesWhenNoVotes) {
  TabObservationStrategy strategy;
  strategy.Lock();
  EXPECT_EQ(strategy.GetScreenshotPolicy(tab1_), ScreenshotPolicy::kRequested);
  EXPECT_EQ(strategy.GetPageContentExtractionPolicy(tab1_),
            PageContentExtractionPolicy::kRequested);
}

TEST_F(TabObservationStrategyTest, SingleVote) {
  TabObservationStrategy strategy;
  strategy.VoteForScreenshot(tab1_, ScreenshotPolicy::kRequired);
  strategy.VoteForPageContentExtraction(tab1_,
                                        PageContentExtractionPolicy::kSkipped);
  strategy.Lock();

  EXPECT_EQ(strategy.GetScreenshotPolicy(tab1_), ScreenshotPolicy::kRequired);
  EXPECT_EQ(strategy.GetPageContentExtractionPolicy(tab1_),
            PageContentExtractionPolicy::kSkipped);
}

TEST_F(TabObservationStrategyTest, CombineVotesRequiredOverridesSkipped) {
  TabObservationStrategy strategy;
  // Tool 1 votes kSkipped
  strategy.VoteForScreenshot(tab1_, ScreenshotPolicy::kSkipped);
  // Tool 2 votes kRequired
  strategy.VoteForScreenshot(tab1_, ScreenshotPolicy::kRequired);
  strategy.Lock();

  EXPECT_EQ(strategy.GetScreenshotPolicy(tab1_), ScreenshotPolicy::kRequired);
}

TEST_F(TabObservationStrategyTest, CombineVotesRequestedOverridesSkipped) {
  TabObservationStrategy strategy;
  // Tool 1 votes kSkipped
  strategy.VoteForScreenshot(tab1_, ScreenshotPolicy::kSkipped);
  // Tool 2 votes kRequested
  strategy.VoteForScreenshot(tab1_, ScreenshotPolicy::kRequested);
  strategy.Lock();

  EXPECT_EQ(strategy.GetScreenshotPolicy(tab1_), ScreenshotPolicy::kRequested);
}

TEST_F(TabObservationStrategyTest, MultiTabIndependentTracking) {
  TabObservationStrategy strategy;
  strategy.VoteForScreenshot(tab1_, ScreenshotPolicy::kRequired);
  strategy.VoteForScreenshot(tab2_, ScreenshotPolicy::kSkipped);
  strategy.Lock();

  EXPECT_EQ(strategy.GetScreenshotPolicy(tab1_), ScreenshotPolicy::kRequired);
  EXPECT_EQ(strategy.GetScreenshotPolicy(tab2_), ScreenshotPolicy::kSkipped);
}

TEST_F(TabObservationStrategyTest, LockPreventsVoting) {
  TabObservationStrategy strategy;
  strategy.Lock();

  EXPECT_DEATH_IF_SUPPORTED(
      strategy.VoteForScreenshot(tab1_, ScreenshotPolicy::kRequired), "");
  EXPECT_DEATH_IF_SUPPORTED(strategy.VoteForPageContentExtraction(
                                tab1_, PageContentExtractionPolicy::kRequired),
                            "");
}

TEST_F(TabObservationStrategyTest, QueryBeforeLockFails) {
  TabObservationStrategy strategy;

  EXPECT_DEATH_IF_SUPPORTED(strategy.GetScreenshotPolicy(tab1_), "");
  EXPECT_DEATH_IF_SUPPORTED(strategy.GetPageContentExtractionPolicy(tab1_), "");
}

TEST_F(TabObservationStrategyTest,
       VoteForBothScreenshotAndDomExtractionOnSameTab) {
  TabObservationStrategy strategy;
  strategy.VoteForScreenshot(tab1_, ScreenshotPolicy::kRequired);
  strategy.VoteForPageContentExtraction(tab1_,
                                        PageContentExtractionPolicy::kRequired);
  strategy.Lock();

  EXPECT_EQ(strategy.GetScreenshotPolicy(tab1_), ScreenshotPolicy::kRequired);
  EXPECT_EQ(strategy.GetPageContentExtractionPolicy(tab1_),
            PageContentExtractionPolicy::kRequired);
}

}  // namespace actor
