// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/user_education_controller.h"

#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

// UserEducationControllerTest -------------------------------------------------

// Base class for tests of the `UserEducationController` parameterized by
// whether user education features are enabled.
class UserEducationControllerTest
    : public AshTestBase,
      public testing::WithParamInterface<
          std::tuple</*capture_mode_tour_enabled=*/bool,
                     /*holding_space_tour_enabled=*/bool,
                     /*welcome_tour_enabled=*/bool>> {
 public:
  UserEducationControllerTest() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    (IsCaptureModeTourEnabled() ? enabled_features : disabled_features)
        .emplace_back(features::kCaptureModeTour);
    (IsHoldingSpaceTourEnabled() ? enabled_features : disabled_features)
        .emplace_back(features::kHoldingSpaceTour);
    (IsWelcomeTourEnabled() ? enabled_features : disabled_features)
        .emplace_back(features::kWelcomeTour);
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  // Returns whether the Capture Mode Tour is enabled given test
  // parameterization.
  bool IsCaptureModeTourEnabled() const { return std::get<0>(GetParam()); }

  // Returns whether the Holding Space Tour is enabled given test
  // parameterization.
  bool IsHoldingSpaceTourEnabled() const { return std::get<1>(GetParam()); }

  // Returns whether the Welcome Tour is enabled given test parameterization.
  bool IsWelcomeTourEnabled() const { return std::get<2>(GetParam()); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    UserEducationControllerTest,
    testing::Combine(/*capture_mode_tour_enabled=*/testing::Bool(),
                     /*holding_space_tour_enabled=*/testing::Bool(),
                     /*welcome_tour_enabled=*/testing::Bool()));

// Tests -----------------------------------------------------------------------

// Verifies that the controller exists iff user education features are enabled.
TEST_P(UserEducationControllerTest, Exists) {
  EXPECT_EQ(!!UserEducationController::Get(), IsCaptureModeTourEnabled() ||
                                                  IsHoldingSpaceTourEnabled() ||
                                                  IsWelcomeTourEnabled());
}

}  // namespace ash
