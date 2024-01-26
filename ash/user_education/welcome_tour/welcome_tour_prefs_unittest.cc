// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/welcome_tour/welcome_tour_prefs.h"

#include <optional>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ash_prefs.h"
#include "ash/user_education/user_education_types.h"
#include "ash/user_education/welcome_tour/welcome_tour_metrics.h"
#include "base/json/values_util.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::welcome_tour_prefs {
namespace {

// Aliases ---------------------------------------------------------------------

using welcome_tour_metrics::kAllInteractionsSet;
using welcome_tour_metrics::kAllPreventedReasonsSet;

using welcome_tour_metrics::Interaction;
using welcome_tour_metrics::PreventedReason;

// Constants -------------------------------------------------------------------

static constexpr char kTimeOfFirstInteractionPrefPrefix[] =
    "ash.welcome_tour.interaction_time.";
static constexpr char kTimeOfFirstTourCompletion[] =
    "ash.welcome_tour.completed.first_time";
static constexpr char kTimeOfFirstTourPrevention[] =
    "ash.welcome_tour.prevented.first_time";
static constexpr char kReasonForFirstTourPrevention[] =
    "ash.welcome_tour.prevented.first_reason";

}  // namespace

// WelcomeTourPrefsTest --------------------------------------------------------

// Base class for tests that verify the behavior of Welcome Tour prefs.
class WelcomeTourPrefsTest : public testing::Test {
 public:
  WelcomeTourPrefsTest() {
    feature_list_.InitAndEnableFeature(features::kWelcomeTour);
    RegisterUserProfilePrefs(pref_service_.registry(), /*country=*/"",
                             /*for_test=*/true);
  }

 protected:
  TestingPrefServiceSimple* pref_service() { return &pref_service_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  TestingPrefServiceSimple pref_service_;
};

// Tests -----------------------------------------------------------------------

// Expects the first interaction time prefs can be set exactly once.
TEST_F(WelcomeTourPrefsTest, FirstInteraction) {
  MarkTimeOfFirstTourCompletion(pref_service());

  for (auto interaction : kAllInteractionsSet) {
    // Should be unset by default.
    EXPECT_EQ(GetTimeOfFirstInteraction(pref_service(), interaction),
              std::nullopt);

    // The first time the mark method is called, it should succeed and mark the
    // time as now, and the time bucket as `kOneMinute`, since that is the
    // smallest increment.
    auto before = base::Time::Now();
    EXPECT_TRUE(MarkTimeOfFirstInteraction(pref_service(), interaction));
    auto after = base::Time::Now();

    auto interaction_time =
        GetTimeOfFirstInteraction(pref_service(), interaction);
    ASSERT_TRUE(interaction_time);
    EXPECT_GE(interaction_time, before);
    EXPECT_LE(interaction_time, after);

    auto interaction_time_bucket =
        GetTimeBucketOfFirstInteraction(pref_service(), interaction);
    ASSERT_TRUE(interaction_time_bucket);
    EXPECT_EQ(interaction_time_bucket, TimeBucket::kOneMinute);

    // For any call beyond the first, the function should return false and the
    // marked time should not change.
    EXPECT_FALSE(MarkTimeOfFirstInteraction(pref_service(), interaction));
    EXPECT_EQ(GetTimeOfFirstInteraction(pref_service(), interaction),
              interaction_time);
    EXPECT_EQ(GetTimeBucketOfFirstInteraction(pref_service(), interaction),
              interaction_time_bucket);
  }
}

// Expects the first tour completion time pref can be set exactly once.
TEST_F(WelcomeTourPrefsTest, FirstTourCompletion) {
  // Should be unset by default.
  EXPECT_EQ(GetTimeOfFirstTourCompletion(pref_service()), std::nullopt);

  // The first time the mark method is called, it should succeed and mark the
  // time as now.
  auto before = base::Time::Now();
  EXPECT_TRUE(MarkTimeOfFirstTourCompletion(pref_service()));
  auto after = base::Time::Now();

  auto completion_time = GetTimeOfFirstTourCompletion(pref_service());
  ASSERT_TRUE(completion_time);
  EXPECT_GE(completion_time, before);
  EXPECT_LE(completion_time, after);

  // For any call beyond the first, the function should return false and the
  // marked time should not change.
  EXPECT_FALSE(MarkTimeOfFirstTourCompletion(pref_service()));
  EXPECT_EQ(GetTimeOfFirstTourCompletion(pref_service()), completion_time);
}

// Expects the first tour prevention time pref and reason pref can be set
// exactly once.
TEST_F(WelcomeTourPrefsTest, FirstTourPrevention) {
  // Should be unset by default.
  EXPECT_EQ(GetReasonForFirstTourPrevention(pref_service()), std::nullopt);
  EXPECT_EQ(GetTimeOfFirstTourPrevention(pref_service()), std::nullopt);

  // The first time the mark method is called, it should succeed and mark the
  // time as now and the given reason as the reason.
  auto before = base::Time::Now();
  EXPECT_TRUE(
      MarkFirstTourPrevention(pref_service(), PreventedReason::kMaxValue));
  auto after = base::Time::Now();

  EXPECT_EQ(GetReasonForFirstTourPrevention(pref_service()),
            PreventedReason::kMaxValue);

  auto prevention_time = GetTimeOfFirstTourPrevention(pref_service());
  ASSERT_TRUE(prevention_time);
  EXPECT_GE(prevention_time, before);
  EXPECT_LE(prevention_time, after);

  // For any call beyond the first, the function should return false and the
  // marked time and reason should not change.
  EXPECT_FALSE(MarkFirstTourPrevention(pref_service(),
                                       PreventedReason::kChromeVoxEnabled));
  EXPECT_EQ(GetReasonForFirstTourPrevention(pref_service()),
            PreventedReason::kMaxValue);
  EXPECT_EQ(GetTimeOfFirstTourPrevention(pref_service()), prevention_time);
}

// Expects that setting and fetching the reason for first tour prevention will
// work for all valid values, and handle invalid ones properly.
TEST_F(WelcomeTourPrefsTest, ReasonForFirstTourPrevention) {
  // The reason should by default be absent.
  EXPECT_EQ(GetReasonForFirstTourPrevention(pref_service()), std::nullopt);

  // Expect that all valid values for `PreventedReason` can be set and fetched.
  for (auto reason : kAllPreventedReasonsSet) {
    MarkFirstTourPrevention(pref_service(), reason);
    EXPECT_EQ(GetReasonForFirstTourPrevention(pref_service()), reason);

    // Clear the time pref for the next loop. `MarkFirstTourPrevention()` would
    // exit early otherwise.
    pref_service()->ClearPref(kTimeOfFirstTourPrevention);
  }

  // For any values that are out of bounds of the enum, it should default to
  // `PreventedReason::kUnknown`.
  pref_service()->SetUserPref(
      kReasonForFirstTourPrevention,
      base::Value(static_cast<int>(PreventedReason::kMaxValue) + 1));
  EXPECT_EQ(GetReasonForFirstTourPrevention(pref_service()),
            PreventedReason::kUnknown);
}

// Expects that `SyncInteractionPrefs()` will sync the continuous and quantized
// interaction prefs synced properly when the bucket is not set.
TEST_F(WelcomeTourPrefsTest, BackfillInteractionTimeBucketPref) {
  MarkTimeOfFirstTourCompletion(pref_service());

  for (auto interaction : kAllInteractionsSet) {
    // Should be unset by default.
    EXPECT_EQ(GetTimeBucketOfFirstInteraction(pref_service(), interaction),
              std::nullopt);
    EXPECT_EQ(GetTimeOfFirstInteraction(pref_service(), interaction),
              std::nullopt);

    // Manually set the time pref, as though it was set before the time bucket
    // pref existed.
    auto time_pref_name = base::StrCat(
        {kTimeOfFirstInteractionPrefPrefix,
         welcome_tour_metrics::ToString(interaction), ".first_time"});
    pref_service()->SetTime(time_pref_name, base::Time::Now());

    SyncInteractionPrefs(pref_service());
    EXPECT_EQ(GetTimeBucketOfFirstInteraction(pref_service(), interaction),
              TimeBucket::kOneMinute);
  }
}

// Expects that `SyncInteractionPrefs()` will mark the quantized interaction
// prefs with the max value once the max period is exceeded.
TEST_F(WelcomeTourPrefsTest, InteractionTimeBucketPrefMaxPassed) {
  // Manually set tour to have been completed over two weeks ago so that
  // backfill will be triggered.
  pref_service()->SetTime(kTimeOfFirstTourCompletion,
                          base::Time::Now() - base::Days(15));

  for (auto interaction : kAllInteractionsSet) {
    // Should be unset by default.
    EXPECT_EQ(GetTimeBucketOfFirstInteraction(pref_service(), interaction),
              std::nullopt);
    EXPECT_EQ(GetTimeOfFirstInteraction(pref_service(), interaction),
              std::nullopt);
  }

  SyncInteractionPrefs(pref_service());

  for (auto interaction : kAllInteractionsSet) {
    // Expect that syncing fills in the time bucket prefs with the appropriate
    // value while leaving the continuous prefs untouched.
    EXPECT_EQ(GetTimeBucketOfFirstInteraction(pref_service(), interaction),
              TimeBucket::kOverTwoWeeks);
    EXPECT_EQ(GetTimeOfFirstInteraction(pref_service(), interaction),
              std::nullopt);
  }
}
}  // namespace ash::welcome_tour_prefs
