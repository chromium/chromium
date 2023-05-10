// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/user_education_help_bubble_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/user_education/user_education_ash_test_base.h"
#include "ash/user_education/user_education_types.h"
#include "base/test/scoped_feature_list.h"
#include "components/user_education/common/help_bubble_params.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"

namespace ash {

// UserEducationHelpBubbleControllerTest ---------------------------------------

// Base class for tests of the `UserEducationHelpBubbleController`.
class UserEducationHelpBubbleControllerTest : public UserEducationAshTestBase {
 public:
  UserEducationHelpBubbleControllerTest() {
    // NOTE: The `UserEducationHelpBubbleController` exists only when a user
    // education feature is enabled. Controller existence is verified in test
    // coverage for the controller's owner.
    std::vector<base::test::FeatureRef> enabled_features;
    enabled_features.emplace_back(features::kCaptureModeTour);
    enabled_features.emplace_back(features::kHoldingSpaceTour);
    enabled_features.emplace_back(features::kWelcomeTour);
    scoped_feature_list_.InitWithFeatures(enabled_features, {});
  }

  // Returns the singleton instance owned by the `UserEducationController`.
  UserEducationHelpBubbleController* controller() {
    return UserEducationHelpBubbleController::Get();
  }

 private:
  // Used to enable user education features which are required for existence of
  // the `controller()` under test.
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests -----------------------------------------------------------------------

// Verifies that `CreateHelpBubble()` is currently hardcoded to return `false`.
TEST_F(UserEducationHelpBubbleControllerTest, CreateHelpBubble) {
  EXPECT_FALSE(controller()->CreateHelpBubble(
      HelpBubbleId::kTest, user_education::HelpBubbleParams(),
      ui::ElementIdentifier(), ui::ElementContext()));
}

}  // namespace ash
