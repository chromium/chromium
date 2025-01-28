// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace contextual_cueing {

namespace {

class ContextualCueingServiceTest : public testing::Test {
 public:
  ContextualCueingServiceTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{contextual_cueing::kContextualCueing,
          {{"BackoffTime", "24h"},
           {"BackoffMultiplierBase", "2.0"},
           {"NudgeCapTime", "24h"},
           {"NudgeCapCount", "3"}}}},
        /*disabled_features=*/{});
  }

  void SetUp() override {
    service_ = std::make_unique<ContextualCueingService>();
  }

  ContextualCueingService* service() { return service_.get(); }

  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<ContextualCueingService> service_;
};

TEST_F(ContextualCueingServiceTest, AllowsNudge) {
  EXPECT_TRUE(service()->CanShowNudge());
}

TEST_F(ContextualCueingServiceTest, NudgesCappedByBackoffRule) {
  EXPECT_TRUE(service()->CanShowNudge());

  service()->CueingNudgeShown();
  service()->CueingNudgeDismissed();  // Backoff time is 24 hours.
  EXPECT_FALSE(service()->CanShowNudge());
  task_environment.FastForwardBy(base::Hours(13));
  EXPECT_FALSE(service()->CanShowNudge());
  task_environment.FastForwardBy(base::Hours(12));
  EXPECT_TRUE(service()->CanShowNudge());

  service()->CueingNudgeShown();
  service()->CueingNudgeDismissed();  // Backoff time is 48 hours.
  EXPECT_FALSE(service()->CanShowNudge());
  task_environment.FastForwardBy(base::Minutes(48 * 60 + 1));
  EXPECT_TRUE(service()->CanShowNudge());

  service()->CueingNudgeShown();
  service()->CueingNudgeDismissed();  // Backoff time is 96 hours.
  task_environment.FastForwardBy(base::Minutes(96 * 60 - 1));
  EXPECT_FALSE(service()->CanShowNudge());
  task_environment.FastForwardBy(base::Minutes(2));
  EXPECT_TRUE(service()->CanShowNudge());
}

TEST_F(ContextualCueingServiceTest, BackoffCountResetAfterClick) {
  service()->CueingNudgeShown();
  service()->CueingNudgeDismissed();  // Backoff time is 24 hours.
  task_environment.FastForwardBy(base::Hours(25));
  EXPECT_TRUE(service()->CanShowNudge());

  service()->CueingNudgeShown();
  service()->CueingNudgeDismissed();  // Backoff time is 48 hours.
  task_environment.FastForwardBy(base::Hours(49));
  EXPECT_TRUE(service()->CanShowNudge());

  service()->CueingNudgeShown();
  service()->CueingNudgeClicked();  // Backoff count resets.
  service()->CueingNudgeShown();
  service()->CueingNudgeDismissed();  // Backoff time is 24 hours.

  task_environment.FastForwardBy(base::Hours(23));
  EXPECT_FALSE(service()->CanShowNudge());
  task_environment.FastForwardBy(base::Hours(2));
  EXPECT_TRUE(service()->CanShowNudge());
}

TEST_F(ContextualCueingServiceTest, NudgesCappedByFrequency) {
  EXPECT_TRUE(service()->CanShowNudge());
  service()->CueingNudgeShown();
  task_environment.FastForwardBy(base::Hours(1));
  service()->CueingNudgeShown();
  task_environment.FastForwardBy(base::Hours(4));
  service()->CueingNudgeShown();
  EXPECT_FALSE(service()->CanShowNudge());  // only 5 hours since 1st nudge.

  task_environment.FastForwardBy(base::Hours(18));
  EXPECT_FALSE(service()->CanShowNudge());
  task_environment.FastForwardBy(base::Hours(2));
  EXPECT_TRUE(service()->CanShowNudge());

  service()->CueingNudgeShown();
  EXPECT_FALSE(service()->CanShowNudge());
}

}  // namespace

}  // namespace contextual_cueing
