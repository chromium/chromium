// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_enums.h"
#include "chrome/browser/contextual_cueing/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace contextual_cueing {
namespace {

class ContextualCueingServiceV2Test : public testing::Test {
 public:
  ContextualCueingServiceV2Test() = default;
  ~ContextualCueingServiceV2Test() override = default;

  void SetUp() override {
    service_ = std::make_unique<ContextualCueingService>();
  }

  ContextualCueingService* service() { return service_.get(); }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<ContextualCueingService> service_;
};

TEST_F(ContextualCueingServiceV2Test, CanShowCueSuccess) {
  GURL url("https://example.com");
  EXPECT_EQ(service()->CanShowCue(url), ContextualCueingDecision::kSuccess);
}

TEST_F(ContextualCueingServiceV2Test, NotEnoughTimeSinceLastCue) {
  GURL url("https://example.com");

  // Seeds state.
  service()->OnCueShown(url);

  // Simulate enough page loads elapsing.
  for (int i = 0; i < kMinPageCountBetweenNudges.Get() + 1; ++i) {
    service()->ReportPageLoad();
  }

  // Enough page loads have been reported, but not enough time has passed since
  // the last cue on this specific origin.
  EXPECT_EQ(service()->CanShowCue(url),
            ContextualCueingDecision::kNotEnoughTimeSinceLastCue);
}

TEST_F(ContextualCueingServiceV2Test, TooManyCuesForOriginOverTime) {
  GURL url("https://example.com");

  for (int i = 0; i < kCueCapCountPerOrigin.Get(); ++i) {
    EXPECT_EQ(service()->CanShowCue(url), ContextualCueingDecision::kSuccess);
    service()->OnCueShown(url);
    for (int j = 0; j < kMinPageCountBetweenNudges.Get() + 1; ++j) {
      service()->ReportPageLoad();
    }
    task_environment_.FastForwardBy(kMinTimeBetweenNudges.Get() +
                                    base::Minutes(1));
  }

  // The last cue is blocked because of the per-origin cap.
  EXPECT_EQ(service()->CanShowCue(url),
            ContextualCueingDecision::kTooManyCuesShownToTheUserForOrigin);
}

TEST_F(ContextualCueingServiceV2Test, TooManyCuesForUserOverTime) {
  std::vector<GURL> urls = {GURL("https://foo.com"), GURL("https://bar.com"),
                            GURL("https://baz.com")};

  // Show cues to the user until the per-user cap is reached.
  for (int i = 0; i < kCueCapCount.Get(); ++i) {
    GURL url = urls[i % urls.size()];
    EXPECT_EQ(service()->CanShowCue(url), ContextualCueingDecision::kSuccess);
    service()->OnCueShown(url);
    for (int j = 0; j < kMinPageCountBetweenNudges.Get() + 1; ++j) {
      service()->ReportPageLoad();
    }
    task_environment_.FastForwardBy(kMinTimeBetweenNudges.Get() +
                                    base::Minutes(1));
  }

  // The last cue is blocked because of the per-user cap - origin does not
  // matter.
  EXPECT_EQ(service()->CanShowCue(GURL("https://qux.com")),
            ContextualCueingDecision::kTooManyCuesShownToTheUser);
}

class ContextualCueingServiceDisableBackoffTest
    : public ContextualCueingServiceV2Test {
 public:
  ContextualCueingServiceDisableBackoffTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {
            {kContextualCueingV2,
             {{"ContextualCueingV2DisableCueBackoff", "true"}}},
        },
        {});
  }
  ~ContextualCueingServiceDisableBackoffTest() override = default;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ContextualCueingServiceDisableBackoffTest, BackoffDisabled) {
  GURL url("https://example.com");

  // Simulate a cue being shown.
  service()->OnCueShown(url);

  // Should not be blocked by backoff.
  EXPECT_EQ(service()->CanShowCue(url), ContextualCueingDecision::kSuccess);
}

}  // namespace
}  // namespace contextual_cueing
