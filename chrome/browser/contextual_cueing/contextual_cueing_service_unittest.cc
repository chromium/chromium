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
constexpr char kBarURL[] = "https://bar.com";
constexpr char kBazURL[] = "https://baz.com";
constexpr char kQuxURL[] = "https://qux.com";

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
    service_ = std::make_unique<ContextualCueingService>(
        &page_content_extraction_service_);
  }

  ContextualCueingService* service() { return service_.get(); }

  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  page_content_annotations::PageContentExtractionService
      page_content_extraction_service_;
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
  service()->ReportPageLoad();
  EXPECT_EQ(service()->CanShowNudge(GURL(kFooURL)), NudgeDecision::kSuccess);
  service()->CueingNudgeShown(GURL(kFooURL));
  task_environment.FastForwardBy(base::Minutes(1));

  // 3 quiet page loads after the cue.
  for (size_t i = 0; i < 3; i++) {
    service()->ReportPageLoad();
    EXPECT_EQ(service()->CanShowNudge(GURL(kBarURL)),
              NudgeDecision::kNotEnoughPageLoadsSinceLastNudge);
    task_environment.FastForwardBy(base::Minutes(1));
  }

  service()->ReportPageLoad();
  EXPECT_EQ(service()->CanShowNudge(GURL(kBarURL)), NudgeDecision::kSuccess);
  service()->CueingNudgeShown(GURL(kBarURL));
  task_environment.FastForwardBy(base::Minutes(1));

  // 3 quiet page loads after the cue.
  for (size_t i = 0; i < 3; i++) {
    service()->ReportPageLoad();
    EXPECT_EQ(service()->CanShowNudge(GURL(kBazURL)),
              NudgeDecision::kNotEnoughPageLoadsSinceLastNudge);
    task_environment.FastForwardBy(base::Minutes(1));
  }

  service()->ReportPageLoad();
  EXPECT_EQ(service()->CanShowNudge(GURL(kBazURL)), NudgeDecision::kSuccess);
  service()->CueingNudgeShown(GURL(kBazURL));
  task_environment.FastForwardBy(base::Minutes(1));

  // 3 quiet page loads after the cue.
  for (size_t i = 0; i < 3; i++) {
    service()->ReportPageLoad();
    EXPECT_EQ(service()->CanShowNudge(GURL(kQuxURL)),
              NudgeDecision::kNotEnoughPageLoadsSinceLastNudge);
    task_environment.FastForwardBy(base::Minutes(1));
  }

  // 3 cues allowed within 24 hours.
  service()->ReportPageLoad();
  EXPECT_EQ(service()->CanShowNudge(GURL(kQuxURL)),
            NudgeDecision::kTooManyNudgesShownToTheUser);

  task_environment.FastForwardBy(base::Hours(25));
  service()->ReportPageLoad();
  EXPECT_EQ(service()->CanShowNudge(GURL(kQuxURL)), NudgeDecision::kSuccess);
}

TEST_F(ContextualCueingServiceTest, AllowsNudge) {
  EXPECT_EQ(service()->CanShowNudge(GURL(kFooURL)), NudgeDecision::kSuccess);
}

TEST_F(ContextualCueingServiceTest, NudgesCappedByBackoffRule) {
  EXPECT_EQ(service()->CanShowNudge(GURL(kFooURL)), NudgeDecision::kSuccess);

  service()->CueingNudgeShown(GURL(kFooURL));
  service()->CueingNudgeDismissed();  // Backoff time is 24 hours.
  EXPECT_EQ(service()->CanShowNudge(GURL(kBarURL)),
            NudgeDecision::kNotEnoughTimeHasElapsedSinceLastNudge);
  task_environment.FastForwardBy(base::Hours(13));
  EXPECT_EQ(service()->CanShowNudge(GURL(kBarURL)),
            NudgeDecision::kNotEnoughTimeHasElapsedSinceLastNudge);
  task_environment.FastForwardBy(base::Hours(12));
  EXPECT_EQ(service()->CanShowNudge(GURL(kBarURL)), NudgeDecision::kSuccess);

  service()->CueingNudgeShown(GURL(kBarURL));
  service()->CueingNudgeDismissed();  // Backoff time is 48 hours.
  EXPECT_EQ(service()->CanShowNudge(GURL(kBazURL)),
            NudgeDecision::kNotEnoughTimeHasElapsedSinceLastNudge);
  task_environment.FastForwardBy(base::Minutes(48 * 60 + 1));
  EXPECT_EQ(service()->CanShowNudge(GURL(kBazURL)), NudgeDecision::kSuccess);

  service()->CueingNudgeShown(GURL(kBazURL));
  service()->CueingNudgeDismissed();  // Backoff time is 96 hours.
  task_environment.FastForwardBy(base::Minutes(96 * 60 - 1));
  EXPECT_EQ(service()->CanShowNudge(GURL(kFooURL)),
            NudgeDecision::kNotEnoughTimeHasElapsedSinceLastNudge);
  task_environment.FastForwardBy(base::Minutes(2));
  EXPECT_EQ(service()->CanShowNudge(GURL(kFooURL)), NudgeDecision::kSuccess);
}

TEST_F(ContextualCueingServiceTest, BackoffCountResetAfterClick) {
  service()->CueingNudgeShown(GURL(kFooURL));
  service()->CueingNudgeDismissed();  // Backoff time is 24 hours.
  task_environment.FastForwardBy(base::Hours(25));
  EXPECT_EQ(service()->CanShowNudge(GURL(kFooURL)), NudgeDecision::kSuccess);

  service()->CueingNudgeShown(GURL(kBarURL));
  service()->CueingNudgeDismissed();  // Backoff time is 48 hours.
  task_environment.FastForwardBy(base::Hours(49));
  EXPECT_EQ(service()->CanShowNudge(GURL(kBarURL)), NudgeDecision::kSuccess);

  service()->CueingNudgeShown(GURL(kBazURL));
  service()->CueingNudgeClicked();  // Backoff count resets.
  service()->CueingNudgeShown(GURL(kFooURL));
  service()->CueingNudgeDismissed();  // Backoff time is 24 hours.

  task_environment.FastForwardBy(base::Hours(23));
  EXPECT_EQ(service()->CanShowNudge(GURL(kBarURL)),
            NudgeDecision::kNotEnoughTimeHasElapsedSinceLastNudge);
  task_environment.FastForwardBy(base::Hours(2));
  EXPECT_EQ(service()->CanShowNudge(GURL(kBarURL)), NudgeDecision::kSuccess);
}

TEST_F(ContextualCueingServiceTest, NudgesCappedByFrequency) {
  EXPECT_EQ(service()->CanShowNudge(GURL(kFooURL)), NudgeDecision::kSuccess);
  service()->CueingNudgeShown(GURL(kFooURL));
  task_environment.FastForwardBy(base::Hours(1));
  service()->CueingNudgeShown(GURL(kBarURL));
  task_environment.FastForwardBy(base::Hours(4));
  service()->CueingNudgeShown(GURL(kBazURL));
  EXPECT_EQ(service()->CanShowNudge(GURL(kQuxURL)),
            NudgeDecision::kTooManyNudgesShownToTheUser);

  task_environment.FastForwardBy(base::Hours(18));
  EXPECT_EQ(service()->CanShowNudge(GURL(kQuxURL)),
            NudgeDecision::kTooManyNudgesShownToTheUser);
  task_environment.FastForwardBy(base::Hours(2));
  EXPECT_EQ(service()->CanShowNudge(GURL(kQuxURL)), NudgeDecision::kSuccess);

  service()->CueingNudgeShown(GURL(kFooURL));
  EXPECT_EQ(service()->CanShowNudge(GURL(kFooURL)),
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
  service()->ReportPageLoad();
  EXPECT_EQ(service()->CanShowNudge(GURL(kFooURL)), NudgeDecision::kSuccess);
  service()->CueingNudgeShown(GURL(kFooURL));
  task_environment.FastForwardBy(base::Minutes(1));

  // 3 quiet page loads after the cue.
  for (size_t i = 0; i < 3; i++) {
    service()->ReportPageLoad();
    EXPECT_EQ(service()->CanShowNudge(GURL(kBarURL)),
              NudgeDecision::kNotEnoughPageLoadsSinceLastNudge);
    task_environment.FastForwardBy(base::Minutes(1));
  }

  service()->ReportPageLoad();
  EXPECT_EQ(service()->CanShowNudge(GURL(kBarURL)), NudgeDecision::kSuccess);
  service()->CueingNudgeShown(GURL(kBarURL));
  task_environment.FastForwardBy(base::Minutes(1));
}

// Tests the per domain nudge limits, ie., x nudges per y hours for each domain.
class ContextualCueingServiceTestPerDomainLimits
    : public ContextualCueingServiceTest {
  void InitializeFeatureList() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{contextual_cueing::kContextualCueing,
          {{"BackoffTime", "0h"},
           {"BackoffMultiplierBase", "0"},
           {"NudgeCapTime", "100h"},
           {"NudgeCapCount", "100"},
           {"MinPageCountBetweenNudges", "0"},
           {"NudgeCapTimePerDomain", "24h"},
           {"NudgeCapCountPerDomain", "1"}}}},
        /*disabled_features=*/{});
  }
};

TEST_F(ContextualCueingServiceTestPerDomainLimits, PerDomainLimits) {
  service()->ReportPageLoad();
  EXPECT_EQ(service()->CanShowNudge(GURL(kFooURL)), NudgeDecision::kSuccess);
  service()->CueingNudgeShown(GURL(kFooURL));
  task_environment.FastForwardBy(base::Minutes(1));

  EXPECT_EQ(service()->CanShowNudge(GURL(kFooURL)),
            NudgeDecision::kTooManyNudgesShownToTheUserForDomain);
  EXPECT_EQ(service()->CanShowNudge(GURL(kBarURL)), NudgeDecision::kSuccess);

  service()->ReportPageLoad();
  EXPECT_EQ(service()->CanShowNudge(GURL(kBarURL)), NudgeDecision::kSuccess);
  service()->CueingNudgeShown(GURL(kBarURL));
  task_environment.FastForwardBy(base::Minutes(1));

  EXPECT_EQ(service()->CanShowNudge(GURL(kFooURL)),
            NudgeDecision::kTooManyNudgesShownToTheUserForDomain);
  EXPECT_EQ(service()->CanShowNudge(GURL(kBarURL)),
            NudgeDecision::kTooManyNudgesShownToTheUserForDomain);
  EXPECT_EQ(service()->CanShowNudge(GURL(kBazURL)), NudgeDecision::kSuccess);

  task_environment.FastForwardBy(base::Hours(24));
  EXPECT_EQ(service()->CanShowNudge(GURL(kFooURL)), NudgeDecision::kSuccess);
  EXPECT_EQ(service()->CanShowNudge(GURL(kBarURL)), NudgeDecision::kSuccess);
  EXPECT_EQ(service()->CanShowNudge(GURL(kBazURL)), NudgeDecision::kSuccess);
}

}  // namespace

}  // namespace contextual_cueing
