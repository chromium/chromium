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
#include "base/time/time.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::welcome_tour_prefs {
namespace {

// Aliases ---------------------------------------------------------------------

using welcome_tour_metrics::kAllExperimentalArmsSet;
using welcome_tour_metrics::kAllInteractionsSet;
using welcome_tour_metrics::kAllPreventedReasonsSet;

using welcome_tour_metrics::ExperimentalArm;
using welcome_tour_metrics::Interaction;
using welcome_tour_metrics::PreventedReason;

// Constants -------------------------------------------------------------------

static constexpr char kFirstExperimentalArm[] =
    "ash.welcome_tour.v2.experimental_arm.first";
static constexpr char kTimeOfFirstTourAborted[] =
    "ash.welcome_tour.v2.aborted.first_time";
static constexpr char kTimeOfFirstTourCompletion[] =
    "ash.welcome_tour.v2.completed.first_time";
static constexpr char kTimeOfFirstTourPrevention[] =
    "ash.welcome_tour.v2.prevented.first_time";
static constexpr char kReasonForFirstTourPrevention[] =
    "ash.welcome_tour.v2.prevented.first_reason";

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

// Expects that setting and fetching the first experimental arm will
// work for all valid values, and handle invalid ones properly.
TEST_F(WelcomeTourPrefsTest, ExperimentalArms) {
  // The arm should by default be absent.
  EXPECT_EQ(GetFirstExperimentalArm(pref_service()), std::nullopt);

  // Expect that all valid values for `ExperimentalArm` can be set and fetched.
  for (auto arm : kAllExperimentalArmsSet) {
    EXPECT_TRUE(MarkFirstExperimentalArm(pref_service(), arm));
    EXPECT_EQ(GetFirstExperimentalArm(pref_service()), arm);

    // Clear the first experimental arm pref for the next loop.
    // `MarkFirstExperimentalArm()` would exit early otherwise.
    pref_service()->ClearPref(kFirstExperimentalArm);
  }

  // For any values that are out of bounds of the enum, it should default to
  // `std::nullopt`.
  pref_service()->SetUserPref(
      kFirstExperimentalArm,
      base::Value(static_cast<int>(ExperimentalArm::kMaxValue) + 1));
  EXPECT_EQ(GetFirstExperimentalArm(pref_service()), std::nullopt);
}

// Expects the first experimental arm pref can be set exactly once.
TEST_F(WelcomeTourPrefsTest, FirstExperimentalArm) {
  // Should be unset by default.
  EXPECT_EQ(GetFirstExperimentalArm(pref_service()), std::nullopt);

  // The first time the mark method is called, it should succeed and mark the
  // experimental arm with the given value.
  EXPECT_TRUE(
      MarkFirstExperimentalArm(pref_service(), ExperimentalArm::kHoldback));
  EXPECT_EQ(GetFirstExperimentalArm(pref_service()),
            ExperimentalArm::kHoldback);

  // For any call beyond the first, the function should return false and the
  // marked first experimental arm should not change.
  EXPECT_FALSE(MarkFirstExperimentalArm(pref_service(), ExperimentalArm::kV2));
  EXPECT_EQ(GetFirstExperimentalArm(pref_service()),
            ExperimentalArm::kHoldback);
}

// Expects the first interaction time prefs can be set exactly once.
TEST_F(WelcomeTourPrefsTest, FirstInteraction) {
  MarkTimeOfFirstTourCompletion(pref_service());

  for (auto interaction : kAllInteractionsSet) {
    // Should be unset by default.
    EXPECT_EQ(GetTimeOfFirstInteraction(pref_service(), interaction),
              std::nullopt);

    // The first time the mark method is called, it should succeed and mark the
    // time as now.
    auto before = base::Time::Now();
    EXPECT_TRUE(MarkTimeOfFirstInteraction(pref_service(), interaction));
    auto after = base::Time::Now();

    auto interaction_time =
        GetTimeOfFirstInteraction(pref_service(), interaction);
    ASSERT_TRUE(interaction_time);
    EXPECT_GE(interaction_time, before);
    EXPECT_LE(interaction_time, after);

    // For any call beyond the first, the function should return false and the
    // marked time should not change.
    EXPECT_FALSE(MarkTimeOfFirstInteraction(pref_service(), interaction));
    EXPECT_EQ(GetTimeOfFirstInteraction(pref_service(), interaction),
              interaction_time);
  }
}

// Expects the first tour aborted time pref can be set exactly once.
TEST_F(WelcomeTourPrefsTest, FirstTourAborted) {
  // Should be unset by default.
  EXPECT_EQ(GetTimeOfFirstTourAborted(pref_service()), std::nullopt);

  // The first time the mark method is called, it should succeed and mark the
  // time as now.
  auto before = base::Time::Now();
  EXPECT_TRUE(MarkTimeOfFirstTourAborted(pref_service()));
  auto after = base::Time::Now();

  auto aborted_time = GetTimeOfFirstTourAborted(pref_service());
  ASSERT_TRUE(aborted_time);
  EXPECT_GE(aborted_time, before);
  EXPECT_LE(aborted_time, after);

  // For any call beyond the first, the function should return false and the
  // marked time should not change.
  EXPECT_FALSE(MarkTimeOfFirstTourAborted(pref_service()));
  EXPECT_EQ(GetTimeOfFirstTourAborted(pref_service()), aborted_time);
}

// Expects the first tour attempt time is minimum time among all attempts.
TEST_F(WelcomeTourPrefsTest, FirstTourAttemptAmongAllAttempts) {
  // Should be unset by default.
  EXPECT_EQ(GetTimeOfFirstTourAttempt(pref_service()), std::nullopt);

  // The first time the mark method is called, it should succeed and mark the
  // time as now and the given reason as the reason.
  auto before = base::Time::Now();
  EXPECT_TRUE(MarkFirstTourPrevention(pref_service(),
                                      PreventedReason::kHoldbackExperimentArm));
  auto after = base::Time::Now();

  EXPECT_EQ(GetReasonForFirstTourPrevention(pref_service()),
            PreventedReason::kHoldbackExperimentArm);

  auto prevention_time = GetTimeOfFirstTourPrevention(pref_service());
  ASSERT_TRUE(prevention_time);
  EXPECT_GE(prevention_time, before);
  EXPECT_LE(prevention_time, after);

  // Prevention reason of holdback will return the prevention time as the
  // attempt time.
  auto attempt_time = GetTimeOfFirstTourAttempt(pref_service());
  ASSERT_TRUE(attempt_time);
  EXPECT_GE(attempt_time, before);
  EXPECT_LE(attempt_time, after);
  EXPECT_EQ(attempt_time, prevention_time);

  // Set an aborted time.
  before = base::Time::Now();
  EXPECT_TRUE(MarkTimeOfFirstTourAborted(pref_service()));
  after = base::Time::Now();

  auto aborted_time = GetTimeOfFirstTourAborted(pref_service());
  ASSERT_TRUE(aborted_time);
  EXPECT_GE(aborted_time, before);
  EXPECT_LE(aborted_time, after);

  attempt_time = GetTimeOfFirstTourAttempt(pref_service());
  EXPECT_NE(attempt_time, aborted_time);
  EXPECT_EQ(attempt_time, prevention_time);

  // Set a completion time.
  before = base::Time::Now();
  EXPECT_TRUE(MarkTimeOfFirstTourCompletion(pref_service()));
  after = base::Time::Now();

  auto completion_time = GetTimeOfFirstTourCompletion(pref_service());
  ASSERT_TRUE(completion_time);
  EXPECT_GE(completion_time, before);
  EXPECT_LE(completion_time, after);

  attempt_time = GetTimeOfFirstTourAttempt(pref_service());
  EXPECT_NE(attempt_time, aborted_time);
  EXPECT_NE(attempt_time, completion_time);
  EXPECT_EQ(attempt_time, prevention_time);

  // Clear the prevention time and reason prefs.
  pref_service()->ClearPref(kReasonForFirstTourPrevention);
  pref_service()->ClearPref(kTimeOfFirstTourPrevention);

  attempt_time = GetTimeOfFirstTourAttempt(pref_service());
  EXPECT_EQ(attempt_time, aborted_time);
  EXPECT_NE(attempt_time, completion_time);
  EXPECT_NE(attempt_time, prevention_time);

  // Clear the aborted time.
  pref_service()->ClearPref(kTimeOfFirstTourAborted);

  attempt_time = GetTimeOfFirstTourAttempt(pref_service());
  EXPECT_NE(attempt_time, aborted_time);
  EXPECT_EQ(attempt_time, completion_time);
  EXPECT_NE(attempt_time, prevention_time);

  // Mark the aborted time again.
  EXPECT_TRUE(MarkTimeOfFirstTourAborted(pref_service()));
  aborted_time = GetTimeOfFirstTourAborted(pref_service());
  ASSERT_TRUE(aborted_time);

  attempt_time = GetTimeOfFirstTourAttempt(pref_service());
  EXPECT_NE(attempt_time, aborted_time);
  EXPECT_EQ(attempt_time, completion_time);
  EXPECT_NE(attempt_time, prevention_time);

  // Clear the completion time.
  pref_service()->ClearPref(kTimeOfFirstTourCompletion);

  attempt_time = GetTimeOfFirstTourAttempt(pref_service());
  EXPECT_EQ(attempt_time, aborted_time);
  EXPECT_NE(attempt_time, completion_time);
  EXPECT_NE(attempt_time, prevention_time);
}

// Expects the first tour attempt time is the aborted time.
TEST_F(WelcomeTourPrefsTest, FirstTourAttemptWhenAborted) {
  // Should be unset by default.
  EXPECT_EQ(GetTimeOfFirstTourAttempt(pref_service()), std::nullopt);

  // The first time the mark method is called, it should succeed and mark the
  // time as now.
  auto before = base::Time::Now();
  EXPECT_TRUE(MarkTimeOfFirstTourAborted(pref_service()));
  auto after = base::Time::Now();

  auto aborted_time = GetTimeOfFirstTourAborted(pref_service());
  ASSERT_TRUE(aborted_time);
  EXPECT_GE(aborted_time, before);
  EXPECT_LE(aborted_time, after);

  // Prevention reason of holdback will return the prevention time as the
  // attempt time.
  auto attempt_time = GetTimeOfFirstTourAttempt(pref_service());
  ASSERT_TRUE(attempt_time);
  EXPECT_GE(attempt_time, before);
  EXPECT_LE(attempt_time, after);

  EXPECT_EQ(attempt_time, aborted_time);
}

// Expects the first tour attempt time is the completion time.
TEST_F(WelcomeTourPrefsTest, FirstTourAttemptWhenCompleted) {
  // Should be unset by default.
  EXPECT_EQ(GetTimeOfFirstTourAttempt(pref_service()), std::nullopt);

  // The first time the mark method is called, it should succeed and mark the
  // time as now.
  auto before = base::Time::Now();
  EXPECT_TRUE(MarkTimeOfFirstTourCompletion(pref_service()));
  auto after = base::Time::Now();

  auto completion_time = GetTimeOfFirstTourCompletion(pref_service());
  ASSERT_TRUE(completion_time);
  EXPECT_GE(completion_time, before);
  EXPECT_LE(completion_time, after);

  // Prevention reason of holdback will return the prevention time as the
  // attempt time.
  auto attempt_time = GetTimeOfFirstTourAttempt(pref_service());
  ASSERT_TRUE(attempt_time);
  EXPECT_GE(attempt_time, before);
  EXPECT_LE(attempt_time, after);

  EXPECT_EQ(attempt_time, completion_time);
}

// Expects the first tour attempt time is nullopt if the prevention reason is
// not holdback.
TEST_F(WelcomeTourPrefsTest, FirstTourAttemptWhenPreventionIsNotHoldback) {
  // Should be unset by default.
  EXPECT_EQ(GetTimeOfFirstTourAttempt(pref_service()), std::nullopt);

  // Should be unset by default.
  EXPECT_EQ(GetReasonForFirstTourPrevention(pref_service()), std::nullopt);
  EXPECT_EQ(GetTimeOfFirstTourPrevention(pref_service()), std::nullopt);

  // The first time the mark method is called, it should succeed and mark the
  // time as now and the given reason as the reason.
  EXPECT_TRUE(MarkFirstTourPrevention(pref_service(),
                                      PreventedReason::kUserTypeNotRegular));
  EXPECT_EQ(GetReasonForFirstTourPrevention(pref_service()),
            PreventedReason::kUserTypeNotRegular);

  auto attempt_time = GetTimeOfFirstTourAttempt(pref_service());

  // Only prevention reason of holdback will return the prevention time as the
  // attempt time.
  ASSERT_FALSE(attempt_time);
}

// Expects the first tour attempt time is the prevention time if the prevention
// reason is holdback.
TEST_F(WelcomeTourPrefsTest, FirstTourAttemptWhenPreventionIsHoldback) {
  // Should be unset by default.
  EXPECT_EQ(GetTimeOfFirstTourAttempt(pref_service()), std::nullopt);

  // Should be unset by default.
  EXPECT_EQ(GetReasonForFirstTourPrevention(pref_service()), std::nullopt);
  EXPECT_EQ(GetTimeOfFirstTourPrevention(pref_service()), std::nullopt);

  // The first time the mark method is called, it should succeed and mark the
  // time as now and the given reason as the reason.
  auto before = base::Time::Now();
  EXPECT_TRUE(MarkFirstTourPrevention(pref_service(),
                                      PreventedReason::kHoldbackExperimentArm));
  auto after = base::Time::Now();

  EXPECT_EQ(GetReasonForFirstTourPrevention(pref_service()),
            PreventedReason::kHoldbackExperimentArm);

  // Prevention reason of holdback will return the prevention time as the
  // attempt time.
  auto attempt_time = GetTimeOfFirstTourAttempt(pref_service());
  ASSERT_TRUE(attempt_time);
  EXPECT_GE(attempt_time, before);
  EXPECT_LE(attempt_time, after);
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

}  // namespace ash::welcome_tour_prefs
