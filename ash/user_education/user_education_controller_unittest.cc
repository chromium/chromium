// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/user_education_controller.h"

#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/test_shell_delegate.h"
#include "ash/user_education/holding_space_wallpaper_nudge/holding_space_wallpaper_nudge_controller.h"
#include "ash/user_education/mock_user_education_delegate.h"
#include "ash/user_education/user_education_ash_test_base.h"
#include "ash/user_education/user_education_feature_controller.h"
#include "ash/user_education/user_education_help_bubble_controller.h"
#include "ash/user_education/user_education_ping_controller.h"
#include "ash/user_education/user_education_tutorial_controller.h"
#include "ash/user_education/welcome_tour/welcome_tour_controller.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "components/account_id/account_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

// Aliases.
using ::testing::_;
using ::testing::Eq;
using ::testing::Return;

}  // namespace

// UserEducationControllerTestBase ---------------------------------------------

// Base class for tests of the `UserEducationController`.
class UserEducationControllerTestBase : public UserEducationAshTestBase {
 public:
  UserEducationControllerTestBase(bool holding_space_wallpaper_nudge_enabled,
                                  bool welcome_tour_enabled)
      : holding_space_wallpaper_nudge_enabled_(
            holding_space_wallpaper_nudge_enabled),
        welcome_tour_enabled_(welcome_tour_enabled) {
    scoped_feature_list_.InitWithFeatureStates(
        {{features::kHoldingSpaceWallpaperNudge,
          IsHoldingSpaceWallpaperNudgeEnabled()},
         {features::kWelcomeTour, IsWelcomeTourEnabled()}});
  }

  // Returns whether the Holding Space wallpaper nudge is enabled.
  bool IsHoldingSpaceWallpaperNudgeEnabled() const {
    return holding_space_wallpaper_nudge_enabled_;
  }

  // Returns whether the Welcome Tour is enabled.
  bool IsWelcomeTourEnabled() const { return welcome_tour_enabled_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  const bool holding_space_wallpaper_nudge_enabled_;
  const bool welcome_tour_enabled_;
};

// UserEducationControllerTest -------------------------------------------------

// Base class for tests of the `UserEducationController` parameterized by
// whether user education features are enabled.
class UserEducationControllerTest
    : public UserEducationControllerTestBase,
      public testing::WithParamInterface<
          std::tuple</*holding_space_wallpaper_nudge_enabled=*/bool,
                     /*welcome_tour_enabled=*/bool>> {
 public:
  UserEducationControllerTest()
      : UserEducationControllerTestBase(
            /*holding_space_wallpaper_nudge_enabled=*/std::get<0>(GetParam()),
            /*welcome_tour_enabled=*/std::get<1>(GetParam())) {}
};

INSTANTIATE_TEST_SUITE_P(
    All,
    UserEducationControllerTest,
    testing::Combine(
        /*holding_space_wallpaper_nudge_enabled=*/testing::Bool(),
        /*welcome_tour_enabled=*/testing::Bool()));

// Tests -----------------------------------------------------------------------

// Verifies that the controller exists iff user education features are enabled.
TEST_P(UserEducationControllerTest, Exists) {
  EXPECT_EQ(!!UserEducationController::Get(),
                IsHoldingSpaceWallpaperNudgeEnabled() ||
                IsWelcomeTourEnabled());
}

// Verifies that the Holding Space wallpaper nudge controller exists iff the
// feature is enabled.
TEST_P(UserEducationControllerTest,
       HoldingSpaceWallpaperNudgeControllerExists) {
  EXPECT_EQ(!!HoldingSpaceWallpaperNudgeController::Get(),
            IsHoldingSpaceWallpaperNudgeEnabled());
}

// Verifies that the user education help bubble controller exists iff user
// education features are enabled.
TEST_P(UserEducationControllerTest, UserEducationHelpBubbleControllerExists) {
  EXPECT_EQ(!!UserEducationHelpBubbleController::Get(),
            !!UserEducationController::Get());
}

// Verifies that the user education ping controller exists iff user education
// features are enabled.
TEST_P(UserEducationControllerTest, UserEducationPingControllerExists) {
  EXPECT_EQ(!!UserEducationPingController::Get(),
            !!UserEducationController::Get());
}

// Verifies that the user education tutorial controller exists iff user
// education features are enabled.
TEST_P(UserEducationControllerTest, UserEducationTutorialControllerExists) {
  EXPECT_EQ(!!UserEducationTutorialController::Get(),
            !!UserEducationController::Get());
}

// Verifies that the Welcome Tour controller exists iff the feature is enabled.
TEST_P(UserEducationControllerTest, WelcomeTourControllerExists) {
  EXPECT_EQ(!!WelcomeTourController::Get(), IsWelcomeTourEnabled());
}

// Verifies that `GetElementIdentifierForAppId()` delegates as expected. Note
// that this test is skipped if the controller does not exist.
TEST_P(UserEducationControllerTest, GetElementIdentifierForAppId) {
  auto* controller = UserEducationController::Get();
  if (!controller) {
    GTEST_SKIP();
  }

  // Ensure `delegate` exists.
  auto* delegate = user_education_delegate();
  ASSERT_TRUE(delegate);

  // Create an app ID and associated element identifier.
  constexpr char kAppId[] = "app_id";
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kElementId);

  // Expect that calls to `GetElementIdentifierForAppId()` are delegated.
  EXPECT_CALL(*delegate, GetElementIdentifierForAppId(Eq(kAppId)))
      .WillOnce(Return(kElementId));

  // Invoke `GetElementIdentifierForAppId()` and verify expectations.
  EXPECT_EQ(controller->GetElementIdentifierForAppId(kAppId), kElementId);
  testing::Mock::VerifyAndClearExpectations(delegate);
}

}  // namespace ash
