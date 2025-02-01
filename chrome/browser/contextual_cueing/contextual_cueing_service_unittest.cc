// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace contextual_cueing {

namespace {

constexpr char kFooURL[] = "https://foo.com";

class ContextualCueingServiceTest : public testing::Test {
 public:
  virtual void InitializeFeatureList() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{contextual_cueing::kContextualCueing,
          {{"BackoffTime", "24h"},
           {"BackoffMultiplierBase", "2.0"},
           {"NudgeCapTime", "24h"},
           {"NudgeCapCount", "3"},
           {"MinPageCountBetweenNudges", "0"}}}},
        /*disabled_features=*/{});
  }

  void SetUp() override {
    InitializeFeatureList();
    service_ = std::make_unique<ContextualCueingService>();
  }

  ContextualCueingService* service() { return service_.get(); }

  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<ContextualCueingService> service_;
};

class ContextualCueingServiceTestCapCountAndMinPageCount
    : public ContextualCueingServiceTest {
  void InitializeFeatureList() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{contextual_cueing::kContextualCueing,
          {{"NudgeCapCount", "3"}, {"MinPageCountBetweenNudges", "3"}}}},
        /*disabled_features=*/{});
  }
};

// Tests the maximum nudge cap per 24 hours, and the minimum page counts needed
// to show the next nudge. Does not test the backoff logic.
TEST_F(ContextualCueingServiceTestCapCountAndMinPageCount,
       AllowsNudgeCapCountAndMinPageCountBetweenNudges) {
  service()->ReportPageLoad(GURL(kFooURL));
  EXPECT_EQ(service()->CanShowNudge(), NudgeDecision::kSuccess);
  service()->CueingNudgeShown();
  task_environment.FastForwardBy(base::Minutes(1));

  // 2 quiet page loads after the cue.
  for (size_t i = 0; i < 2; i++) {
    service()->ReportPageLoad(GURL(kFooURL));
    EXPECT_EQ(service()->CanShowNudge(),
              NudgeDecision::kNotEnoughPageLoadsSinceLastNudge);
    task_environment.FastForwardBy(base::Minutes(1));
  }

  service()->ReportPageLoad(GURL(kFooURL));
  EXPECT_EQ(service()->CanShowNudge(), NudgeDecision::kSuccess);
  service()->CueingNudgeShown();
  task_environment.FastForwardBy(base::Minutes(1));

  // 2 quiet page loads after the cue.
  for (size_t i = 0; i < 2; i++) {
    service()->ReportPageLoad(GURL(kFooURL));
    EXPECT_EQ(service()->CanShowNudge(),
              NudgeDecision::kNotEnoughPageLoadsSinceLastNudge);
    task_environment.FastForwardBy(base::Minutes(1));
  }

  service()->ReportPageLoad(GURL(kFooURL));
  EXPECT_EQ(service()->CanShowNudge(), NudgeDecision::kSuccess);
  service()->CueingNudgeShown();
  task_environment.FastForwardBy(base::Minutes(1));

  // 2 quiet page loads after the cue.
  for (size_t i = 0; i < 2; i++) {
    service()->ReportPageLoad(GURL(kFooURL));
    EXPECT_EQ(service()->CanShowNudge(),
              NudgeDecision::kNotEnoughPageLoadsSinceLastNudge);
    task_environment.FastForwardBy(base::Minutes(1));
  }

  // 3 cues allowed within 24 hours.
  service()->ReportPageLoad(GURL(kFooURL));
  EXPECT_EQ(service()->CanShowNudge(),
            NudgeDecision::kTooManyNudgesShownToTheUser);

  task_environment.FastForwardBy(base::Hours(25));
  service()->ReportPageLoad(GURL(kFooURL));
  EXPECT_EQ(service()->CanShowNudge(), NudgeDecision::kSuccess);
}

TEST_F(ContextualCueingServiceTest, AllowsNudge) {
  EXPECT_EQ(service()->CanShowNudge(), NudgeDecision::kSuccess);
}

TEST_F(ContextualCueingServiceTest, NudgesCappedByBackoffRule) {
  EXPECT_EQ(service()->CanShowNudge(), NudgeDecision::kSuccess);

  service()->CueingNudgeShown();
  service()->CueingNudgeDismissed();  // Backoff time is 24 hours.
  EXPECT_EQ(service()->CanShowNudge(),
            NudgeDecision::kNotEnoughTimeHasElapsedSinceLastNudge);
  task_environment.FastForwardBy(base::Hours(13));
  EXPECT_EQ(service()->CanShowNudge(),
            NudgeDecision::kNotEnoughTimeHasElapsedSinceLastNudge);
  task_environment.FastForwardBy(base::Hours(12));
  EXPECT_EQ(service()->CanShowNudge(), NudgeDecision::kSuccess);

  service()->CueingNudgeShown();
  service()->CueingNudgeDismissed();  // Backoff time is 48 hours.
  EXPECT_EQ(service()->CanShowNudge(),
            NudgeDecision::kNotEnoughTimeHasElapsedSinceLastNudge);
  task_environment.FastForwardBy(base::Minutes(48 * 60 + 1));
  EXPECT_EQ(service()->CanShowNudge(), NudgeDecision::kSuccess);

  service()->CueingNudgeShown();
  service()->CueingNudgeDismissed();  // Backoff time is 96 hours.
  task_environment.FastForwardBy(base::Minutes(96 * 60 - 1));
  EXPECT_EQ(service()->CanShowNudge(),
            NudgeDecision::kNotEnoughTimeHasElapsedSinceLastNudge);
  task_environment.FastForwardBy(base::Minutes(2));
  EXPECT_EQ(service()->CanShowNudge(), NudgeDecision::kSuccess);
}

TEST_F(ContextualCueingServiceTest, BackoffCountResetAfterClick) {
  service()->CueingNudgeShown();
  service()->CueingNudgeDismissed();  // Backoff time is 24 hours.
  task_environment.FastForwardBy(base::Hours(25));
  EXPECT_EQ(service()->CanShowNudge(), NudgeDecision::kSuccess);

  service()->CueingNudgeShown();
  service()->CueingNudgeDismissed();  // Backoff time is 48 hours.
  task_environment.FastForwardBy(base::Hours(49));
  EXPECT_EQ(service()->CanShowNudge(), NudgeDecision::kSuccess);

  service()->CueingNudgeShown();
  service()->CueingNudgeClicked();  // Backoff count resets.
  service()->CueingNudgeShown();
  service()->CueingNudgeDismissed();  // Backoff time is 24 hours.

  task_environment.FastForwardBy(base::Hours(23));
  EXPECT_EQ(service()->CanShowNudge(),
            NudgeDecision::kNotEnoughTimeHasElapsedSinceLastNudge);
  task_environment.FastForwardBy(base::Hours(2));
  EXPECT_EQ(service()->CanShowNudge(), NudgeDecision::kSuccess);
}

TEST_F(ContextualCueingServiceTest, NudgesCappedByFrequency) {
  EXPECT_EQ(service()->CanShowNudge(), NudgeDecision::kSuccess);
  service()->CueingNudgeShown();
  task_environment.FastForwardBy(base::Hours(1));
  service()->CueingNudgeShown();
  task_environment.FastForwardBy(base::Hours(4));
  service()->CueingNudgeShown();
  EXPECT_EQ(service()->CanShowNudge(),
            NudgeDecision::kTooManyNudgesShownToTheUser);

  task_environment.FastForwardBy(base::Hours(18));
  EXPECT_EQ(service()->CanShowNudge(),
            NudgeDecision::kTooManyNudgesShownToTheUser);
  task_environment.FastForwardBy(base::Hours(2));
  EXPECT_EQ(service()->CanShowNudge(), NudgeDecision::kSuccess);

  service()->CueingNudgeShown();
  EXPECT_EQ(service()->CanShowNudge(),
            NudgeDecision::kTooManyNudgesShownToTheUser);
}

class ContextualCueingServiceTestMinPageCountBetweenNudges
    : public ContextualCueingServiceTest {
  void InitializeFeatureList() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{contextual_cueing::kContextualCueing,
          {{"BackoffTime", "0h"},
           {"BackoffMultiplierBase", "0"},
           {"NudgeCapTime", "0h"},
           {"MinPageCountBetweenNudges", "3"}}}},
        /*disabled_features=*/{});
  }
};

TEST_F(ContextualCueingServiceTestMinPageCountBetweenNudges,
       MinPageCountBetweenNudges) {
  service()->ReportPageLoad(GURL(kFooURL));
  EXPECT_EQ(service()->CanShowNudge(), NudgeDecision::kSuccess);
  service()->CueingNudgeShown();
  task_environment.FastForwardBy(base::Minutes(1));

  // 2 quiet page loads after the cue.
  for (size_t i = 0; i < 2; i++) {
    service()->ReportPageLoad(GURL(kFooURL));
    EXPECT_EQ(service()->CanShowNudge(),
              NudgeDecision::kNotEnoughPageLoadsSinceLastNudge);
    task_environment.FastForwardBy(base::Minutes(1));
  }

  service()->ReportPageLoad(GURL(kFooURL));
  EXPECT_EQ(service()->CanShowNudge(), NudgeDecision::kSuccess);
  service()->CueingNudgeShown();
  task_environment.FastForwardBy(base::Minutes(1));
}

}  // namespace

}  // namespace contextual_cueing
