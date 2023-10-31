// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/holding_space_wallpaper_nudge/holding_space_wallpaper_nudge_prefs.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ash_prefs.h"
#include "base/test/scoped_feature_list.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash::holding_space_wallpaper_nudge_prefs {
namespace {

// Aliases ---------------------------------------------------------------------

using testing::AllOf;
using testing::Ge;
using testing::Le;
using testing::Ne;

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

// Verifies that the nudge shown count and last shown time are updated when
// `MarkNudgeShown()` is called.
TEST_F(HoldingSpaceWallpaperNudgePrefsTest, MarkNudgeShown) {
  // Case: Initialized to default values
  EXPECT_EQ(GetLastTimeNudgeWasShown(pref_service()), absl::nullopt);
  EXPECT_EQ(GetNudgeShownCount(pref_service()), 0u);

  // Case: Called the first time.
  auto before = base::Time::Now();
  MarkNudgeShown(pref_service());
  auto after = base::Time::Now();

  EXPECT_THAT(GetLastTimeNudgeWasShown(pref_service()),
              AllOf(Ne(absl::nullopt), Ge(before), Le(after)));
  EXPECT_EQ(GetNudgeShownCount(pref_service()), 1u);

  // Case: Called again.
  before = base::Time::Now();
  MarkNudgeShown(pref_service());
  after = base::Time::Now();

  EXPECT_THAT(GetLastTimeNudgeWasShown(pref_service()),
              AllOf(Ne(absl::nullopt), Ge(before), Le(after)));
  EXPECT_EQ(GetNudgeShownCount(pref_service()), 2u);
}

}  // namespace ash::holding_space_wallpaper_nudge_prefs
