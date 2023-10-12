// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/holding_space_tour/holding_space_tour_prefs.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ash_prefs.h"
#include "base/test/scoped_feature_list.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash::holding_space_tour_prefs {
namespace {

// Aliases ---------------------------------------------------------------------

using testing::AllOf;
using testing::Ge;
using testing::Le;
using testing::Ne;

}  // namespace

// HoldingSpaceTourPrefsTest ---------------------------------------------------

// Base class for tests that verify the behavior of Holding Space tour prefs.
class HoldingSpaceTourPrefsTest : public testing::Test {
 public:
  HoldingSpaceTourPrefsTest() {
    feature_list_.InitAndEnableFeature(features::kHoldingSpaceTour);
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

// Verifies that the tour shown count and last shown time are updated when
// `MarkTourShown()` is called.
TEST_F(HoldingSpaceTourPrefsTest, MarkTourShown) {
  // Case: Initialized to default values
  EXPECT_EQ(GetLastTimeTourWasShown(pref_service()), absl::nullopt);
  EXPECT_EQ(GetTourShownCount(pref_service()), 0u);

  // Case: Called the first time.
  auto before = base::Time::Now();
  MarkTourShown(pref_service());
  auto after = base::Time::Now();

  EXPECT_THAT(GetLastTimeTourWasShown(pref_service()),
              AllOf(Ne(absl::nullopt), Ge(before), Le(after)));
  EXPECT_EQ(GetTourShownCount(pref_service()), 1u);

  // Case: Called again.
  before = base::Time::Now();
  MarkTourShown(pref_service());
  after = base::Time::Now();

  EXPECT_THAT(GetLastTimeTourWasShown(pref_service()),
              AllOf(Ne(absl::nullopt), Ge(before), Le(after)));
  EXPECT_EQ(GetTourShownCount(pref_service()), 2u);
}

}  // namespace ash::holding_space_tour_prefs
