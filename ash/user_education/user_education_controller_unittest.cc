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
// whether the Welcome Tour feature is enabled.
class UserEducationControllerTest
    : public AshTestBase,
      public testing::WithParamInterface</*welcome_tour_enabled=*/bool> {
 public:
  UserEducationControllerTest() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    (IsWelcomeTourEnabled() ? enabled_features : disabled_features)
        .emplace_back(features::kWelcomeTour);
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  // Returns whether the Welcome Tour is enabled given test parameterization.
  bool IsWelcomeTourEnabled() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         UserEducationControllerTest,
                         /*welcome_tour_enabled=*/testing::Bool());

// Tests -----------------------------------------------------------------------

// Verifies that the controller exists if and only if Welcome Tour is enabled.
TEST_P(UserEducationControllerTest, Exists) {
  EXPECT_EQ(!!UserEducationController::Get(), IsWelcomeTourEnabled());
}

}  // namespace ash
