// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/holding_space_wallpaper_nudge/holding_space_wallpaper_nudge_prefs.h"

#include <optional>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ash_prefs.h"
#include "ash/user_education/holding_space_wallpaper_nudge/holding_space_wallpaper_nudge_metrics.h"
#include "base/json/values_util.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::holding_space_wallpaper_nudge_prefs {
namespace {

// Aliases ---------------------------------------------------------------------

using holding_space_wallpaper_nudge_metrics::Interaction;
using holding_space_wallpaper_nudge_metrics::kAllInteractionsSet;
using testing::AllOf;
using testing::Ge;
using testing::Le;
using testing::Ne;

// Constants -------------------------------------------------------------------

constexpr char kTimeOfFirstInteractionPrefPrefix[] =
    "ash.holding_space.wallpaper_nudge.interaction_time.";

// Helpers ---------------------------------------------------------------------

// Fetches the time pref associated with the first time of a given interaction.
std::optional<base::Time> GetTimeOfFirstInteraction(PrefService* prefs,
                                                    Interaction interaction) {
  auto pref_name = base::StrCat(
      {kTimeOfFirstInteractionPrefPrefix,
       holding_space_wallpaper_nudge_metrics::ToString(interaction),
       ".first_time"});
  auto* pref = prefs->FindPreference(pref_name);
  return pref->IsDefaultValue() ? std::nullopt
                                : base::ValueToTime(pref->GetValue());
}

}  // namespace

// HoldingSpaceWallpaperNudgePrefsTest -----------------------------------------

// Base class for tests that verify the behavior of Holding Space wallpaper
// nudge prefs.
class HoldingSpaceWallpaperNudgePrefsTest : public testing::Test {
 public:
  HoldingSpaceWallpaperNudgePrefsTest() {
    feature_list_.InitAndEnableFeature(features::kHoldingSpaceWallpaperNudge);
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

// Verifies that the first interaction metrics are recorded when an interaction
// first happens once the session is determined to be eligible.
TEST_F(HoldingSpaceWallpaperNudgePrefsTest, FirstInteraction) {
  // Case: First session not set.
  for (auto interaction : kAllInteractionsSet) {
    // Should be unset by default.
    EXPECT_EQ(GetTimeOfFirstInteraction(pref_service(), interaction),
              std::nullopt);

    // Should remain unset because the user's first session is not set.
    EXPECT_FALSE(MarkTimeOfFirstInteraction(pref_service(), interaction));
    EXPECT_EQ(GetTimeOfFirstInteraction(pref_service(), interaction),
              std::nullopt);
  }

  MarkTimeOfFirstEligibleSession(pref_service());

  // Case: First session set.
  for (auto interaction : kAllInteractionsSet) {
    // The first time the mark method is called, it should succeed and mark the
    // time as now.
    auto before = base::Time::Now();
    EXPECT_TRUE(MarkTimeOfFirstInteraction(pref_service(), interaction));
    auto after = base::Time::Now();

    auto interaction_time =
        GetTimeOfFirstInteraction(pref_service(), interaction);
    EXPECT_THAT(interaction_time,
                AllOf(Ne(std::nullopt), Ge(before), Le(after)));

    // For any call beyond the first, the function should return false and the
    // marked time should not change.
    EXPECT_FALSE(MarkTimeOfFirstInteraction(pref_service(), interaction));
    EXPECT_EQ(GetTimeOfFirstInteraction(pref_service(), interaction),
              interaction_time);
  }
}

// Verifies that the nudge shown count and last shown time are updated when
// `MarkNudgeShown()` is called.
TEST_F(HoldingSpaceWallpaperNudgePrefsTest, MarkNudgeShown) {
  // Case: Initialized to default values
  EXPECT_EQ(GetLastTimeNudgeWasShown(pref_service()), std::nullopt);
  EXPECT_EQ(GetNudgeShownCount(pref_service()), 0u);

  // Case: Called the first time.
  auto before = base::Time::Now();
  MarkNudgeShown(pref_service());
  auto after = base::Time::Now();

  EXPECT_THAT(GetLastTimeNudgeWasShown(pref_service()),
              AllOf(Ne(std::nullopt), Ge(before), Le(after)));
  EXPECT_EQ(GetNudgeShownCount(pref_service()), 1u);

  // Case: Called again.
  before = base::Time::Now();
  MarkNudgeShown(pref_service());
  after = base::Time::Now();

  EXPECT_THAT(GetLastTimeNudgeWasShown(pref_service()),
              AllOf(Ne(std::nullopt), Ge(before), Le(after)));
  EXPECT_EQ(GetNudgeShownCount(pref_service()), 2u);
}

TEST_F(HoldingSpaceWallpaperNudgePrefsTest, CounterfactualPrefIsSeparate) {
  base::Time before, after;

  // Case: Enabled counterfactually, which should have separate prefs.
  {
    base::test::ScopedFeatureList counterfactual_feature_list;
    counterfactual_feature_list.InitAndEnableFeatureWithParameters(
        features::kHoldingSpaceWallpaperNudge, {{"is-counterfactual", "true"}});

    EXPECT_EQ(GetLastTimeNudgeWasShown(pref_service()), std::nullopt);
    EXPECT_EQ(GetNudgeShownCount(pref_service()), 0u);

    before = base::Time::Now();
    MarkNudgeShown(pref_service());
    after = base::Time::Now();

    EXPECT_THAT(GetLastTimeNudgeWasShown(pref_service()),
                AllOf(Ne(std::nullopt), Ge(before), Le(after)));
    EXPECT_EQ(GetNudgeShownCount(pref_service()), 1u);
  }

  // Case: Disabled, counterfactual prefs should have no effect.
  {
    base::test::ScopedFeatureList disabled_feature_list;
    disabled_feature_list.InitAndDisableFeature(
        features::kHoldingSpaceWallpaperNudge);
    EXPECT_EQ(GetLastTimeNudgeWasShown(pref_service()), std::nullopt);
    EXPECT_EQ(GetNudgeShownCount(pref_service()), 0u);
  }

  // Case: Enabled non-counterfactually.
  {
    base::test::ScopedFeatureList noncounterfactual_feature_list;
    noncounterfactual_feature_list.InitAndEnableFeatureWithParameters(
        features::kHoldingSpaceWallpaperNudge,
        {{"is-counterfactual", "false"}});

    EXPECT_EQ(GetLastTimeNudgeWasShown(pref_service()), std::nullopt);
    EXPECT_EQ(GetNudgeShownCount(pref_service()), 0u);

    before = base::Time::Now();
    MarkNudgeShown(pref_service());
    after = base::Time::Now();

    EXPECT_THAT(GetLastTimeNudgeWasShown(pref_service()),
                AllOf(Ne(std::nullopt), Ge(before), Le(after)));
    EXPECT_EQ(GetNudgeShownCount(pref_service()), 1u);
  }

  // Case: Enabled normally, which should be the same as non-counterfactually.
  {
    base::test::ScopedFeatureList feature_list(
        features::kHoldingSpaceWallpaperNudge);

    EXPECT_NE(GetLastTimeNudgeWasShown(pref_service()), std::nullopt);
    EXPECT_EQ(GetNudgeShownCount(pref_service()), 1u);

    before = base::Time::Now();
    MarkNudgeShown(pref_service());
    after = base::Time::Now();

    EXPECT_THAT(GetLastTimeNudgeWasShown(pref_service()),
                AllOf(Ne(std::nullopt), Ge(before), Le(after)));
    EXPECT_EQ(GetNudgeShownCount(pref_service()), 2u);
  }

  // Case: Disabled, normal prefs should be retrievable.
  {
    base::test::ScopedFeatureList disabled_feature_list;
    disabled_feature_list.InitAndDisableFeature(
        features::kHoldingSpaceWallpaperNudge);
    EXPECT_THAT(GetLastTimeNudgeWasShown(pref_service()),
                AllOf(Ne(std::nullopt), Ge(before), Le(after)));
    EXPECT_EQ(GetNudgeShownCount(pref_service()), 2u);
  }
}

TEST_F(HoldingSpaceWallpaperNudgePrefsTest, UserEligibility) {
  // Case: Initialized to default values.
  EXPECT_EQ(GetUserEligibility(pref_service()), std::nullopt);

  // Case: Set the first time.
  EXPECT_TRUE(SetUserEligibility(pref_service(), false));
  EXPECT_EQ(GetUserEligibility(pref_service()), false);

  // Case: Set again, which should fail.
  EXPECT_FALSE(SetUserEligibility(pref_service(), true));
  EXPECT_EQ(GetUserEligibility(pref_service()), false);
}

}  // namespace ash::holding_space_wallpaper_nudge_prefs
