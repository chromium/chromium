// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/welcome_tour/welcome_tour_prefs.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ash_prefs.h"
#include "ash/test/ash_test_base.h"
#include "ash/user_education/welcome_tour/welcome_tour_metrics.h"
#include "base/test/scoped_feature_list.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash::welcome_tour_prefs {
namespace {

// Aliases ---------------------------------------------------------------------

using welcome_tour_metrics::kAllPreventedReasonsSet;
using welcome_tour_metrics::PreventedReason;

// Constants -------------------------------------------------------------------

static constexpr char kTimeOfFirstTourPrevention[] =
    "ash.welcome_tour.prevented.first_time";
static constexpr char kReasonForFirstTourPrevention[] =
    "ash.welcome_tour.prevented.first_reason";

}  // namespace

using WelcomeTourPrefsTest = AshTestBase;

// Expects that setting and fetching the reason for first tour prevention will
// work for all valid values, and handle invalid ones properly.
TEST_F(WelcomeTourPrefsTest, ReasonForFirstTourPrevention) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kWelcomeTour);

  auto prefs = std::make_unique<TestingPrefServiceSimple>();
  RegisterUserProfilePrefs(prefs->registry(), /*country=*/"",
                           /*for_test=*/true);

  // The reason should by default be absent.
  EXPECT_EQ(GetReasonForFirstTourPrevention(prefs.get()), absl::nullopt);

  // Expect that all valid values for `PreventedReason` can be set and fetched.
  for (auto reason : kAllPreventedReasonsSet) {
    MarkFirstTourPrevention(prefs.get(), reason);
    EXPECT_EQ(GetReasonForFirstTourPrevention(prefs.get()), reason);

    // Clear the time pref for the next loop. `MarkFirstTourPrevention()` would
    // exit early otherwise.
    prefs->ClearPref(kTimeOfFirstTourPrevention);
  }

  // For any values that are out of bounds of the enum, it should default to
  // `PreventedReason::kUnknown`.
  prefs->SetUserPref(
      kReasonForFirstTourPrevention,
      base::Value(static_cast<int>(PreventedReason::kMaxValue) + 1));
  EXPECT_EQ(GetReasonForFirstTourPrevention(prefs.get()),
            PreventedReason::kUnknown);
}

}  // namespace ash::welcome_tour_prefs
