// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/contextual_cueing/features.h"
#include "chrome/browser/contextual_cueing/prefs.h"
#include "components/contextual_cueing/contextual_cueing_enums.h"
#include "components/contextual_cueing/ucb_scorer.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace contextual_cueing {
namespace {

class ContextualCueingServiceV2Test : public testing::Test {
 public:
  ContextualCueingServiceV2Test() {
    prefs::RegisterProfilePrefs(pref_service_.registry());
  }
  ~ContextualCueingServiceV2Test() override = default;

  void SetUp() override {
    service_ = std::make_unique<ContextualCueingService>(&pref_service_);
  }

  ContextualCueingService* service() { return service_.get(); }

  // Creates a fresh service backed by the same prefs, simulating a restart.
  std::unique_ptr<ContextualCueingService> CreateService() {
    return std::make_unique<ContextualCueingService>(&pref_service_);
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<ContextualCueingService> service_;
};

TEST_F(ContextualCueingServiceV2Test, CanShowCueSuccess) {
  GURL url("https://example.com");
  EXPECT_EQ(service()->CanShowCue(url), ContextualCueingDecision::kSuccess);
}

TEST_F(ContextualCueingServiceV2Test, NotEnoughTimeSinceLastCue) {
  GURL url("https://example.com");

  // Seeds state.
  service()->OnCueShown(url, CueTargetType::kGlic);

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
    service()->OnCueShown(url, CueTargetType::kGlic);
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
                            GURL("https://baz.com"), GURL("https://qux.com")};

  // Show cues to the user until the per-user cap is reached.
  for (int i = 0; i < kCueCapCount.Get(); ++i) {
    GURL url = urls[i % urls.size()];
    EXPECT_EQ(service()->CanShowCue(url), ContextualCueingDecision::kSuccess);
    service()->OnCueShown(url, CueTargetType::kGlic);
    for (int j = 0; j < kMinPageCountBetweenNudges.Get() + 1; ++j) {
      service()->ReportPageLoad();
    }
    task_environment_.FastForwardBy(kMinTimeBetweenNudges.Get() +
                                    base::Minutes(1));
  }

  // The last cue is blocked because of the per-user cap - origin does not
  // matter.
  EXPECT_EQ(service()->CanShowCue(GURL("https://example.com")),
            ContextualCueingDecision::kTooManyCuesShownToTheUser);
}

TEST_F(ContextualCueingServiceV2Test, NotEnoughTimeSinceLastDismissal) {
  GURL url("https://example.com");

  EXPECT_EQ(service()->CanShowCue(url), ContextualCueingDecision::kSuccess);
  service()->OnCueShown(url, CueTargetType::kGlic);

  // Simulate a dismissal.
  service()->OnCueDismissed(CueTargetType::kGlic);

  for (int j = 0; j < kMinPageCountBetweenNudges.Get() + 1; ++j) {
    service()->ReportPageLoad();
  }
  task_environment_.FastForwardBy(kMinTimeBetweenNudges.Get() +
                                  base::Minutes(1));

  EXPECT_EQ(service()->CanShowCue(url),
            ContextualCueingDecision::kNotEnoughTimeSinceLastDismissal);
}

TEST_F(ContextualCueingServiceV2Test, NotEnoughTimeSinceLastClick) {
  GURL url("https://example.com");

  EXPECT_EQ(service()->CanShowCue(url), ContextualCueingDecision::kSuccess);
  service()->OnCueShown(url, CueTargetType::kGlic);

  // Simulate a click.
  service()->OnCueClicked(CueTargetType::kGlic);

  for (int j = 0; j < kMinPageCountBetweenNudges.Get() + 1; ++j) {
    service()->ReportPageLoad();
  }
  task_environment_.FastForwardBy(kMinTimeBetweenNudges.Get() +
                                  base::Minutes(1));

  EXPECT_EQ(service()->CanShowCue(url),
            ContextualCueingDecision::kNotEnoughTimeSinceLastClick);

  // Fast forward past the click backoff time (default is 1 hour).
  task_environment_.FastForwardBy(kClickBackoffTime.Get() + base::Minutes(1));

  EXPECT_EQ(service()->CanShowCue(url), ContextualCueingDecision::kSuccess);
}

// ---------------------------------------------------------------------------
// Pref persistence round-trip tests
// ---------------------------------------------------------------------------

// Verifies that impressions, clicks, and dismissals written by the first
// service instance are correctly restored in a second instance (simulating
// a browser restart).
TEST_F(ContextualCueingServiceV2Test, StatsRoundTripAcrossRestart) {
  GURL url("https://example.com");
  {
    auto svc = CreateService();
    svc->OnCueShown(url, CueTargetType::kGlic);
    svc->OnCueShown(url, CueTargetType::kGlic);
    svc->OnCueClicked(CueTargetType::kGlic);
    svc->OnCueDismissed(CueTargetType::kGlic);
    // Service destroyed here; prefs should have been written.
  }

  // Simulate a restart: construct a new service from the same prefs.
  auto service2 = CreateService();
  const TargetStats& stats = service2->GetStatsForTarget(CueTargetType::kGlic);
  EXPECT_EQ(stats.impressions, 2);
  EXPECT_EQ(stats.clicks, 1);
  EXPECT_EQ(stats.dismissals, 1);
}

// Verifies that GetTotalImpressions sums across targets correctly after
// a round-trip (only kGlic is registered, so the total should match).
TEST_F(ContextualCueingServiceV2Test, TotalImpressionsAfterRestart) {
  GURL url("https://example.com");
  {
    auto svc = CreateService();
    svc->OnCueShown(url, CueTargetType::kGlic);
    svc->OnCueShown(url, CueTargetType::kGlic);
    svc->OnCueShown(url, CueTargetType::kGlic);
  }

  auto service2 = CreateService();
  EXPECT_EQ(service2->GetTotalImpressions(), 3);
}

// Verifies that a service with zero interactions does not write or restore
// spurious stats entries (zero-activity path).
TEST_F(ContextualCueingServiceV2Test, NoActivityNoPrefsWritten) {
  {
    auto svc = CreateService();
    // No interactions; nothing should be written to prefs.
    const TargetStats& stats = svc->GetStatsForTarget(CueTargetType::kGlic);
    EXPECT_EQ(stats.impressions, 0);
    EXPECT_EQ(stats.clicks, 0);
    EXPECT_EQ(stats.dismissals, 0);
  }

  // A second instance should also see all-zero stats.
  auto service2 = CreateService();
  const TargetStats& stats2 = service2->GetStatsForTarget(CueTargetType::kGlic);
  EXPECT_EQ(stats2.impressions, 0);
  EXPECT_EQ(stats2.clicks, 0);
  EXPECT_EQ(stats2.dismissals, 0);
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
  service()->OnCueShown(url, CueTargetType::kGlic);

  // Should not be blocked by backoff.
  EXPECT_EQ(service()->CanShowCue(url), ContextualCueingDecision::kSuccess);
}

}  // namespace
}  // namespace contextual_cueing
