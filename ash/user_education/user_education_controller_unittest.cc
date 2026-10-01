// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/user_education_controller.h"

#include <string>
#include <vector>

#include "ash/test_shell_delegate.h"
#include "ash/user_education/mock_user_education_delegate.h"
#include "ash/user_education/user_education_ash_test_base.h"
#include "ash/user_education/user_education_feature_controller.h"
#include "ash/user_education/user_education_help_bubble_controller.h"
#include "ash/user_education/user_education_tutorial_controller.h"
#include "ash/user_education/welcome_tour/welcome_tour_controller.h"
#include "base/test/bind.h"
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

// UserEducationControllerTest -------------------------------------------------

// Base class for tests of the `UserEducationController`.
using UserEducationControllerTest = UserEducationAshTestBase;

// Tests -----------------------------------------------------------------------

// Verifies that the controller exists.
TEST_F(UserEducationControllerTest, Exists) {
  EXPECT_TRUE(UserEducationController::Get());
}

// Verifies that the user education help bubble controller exists.
TEST_F(UserEducationControllerTest, UserEducationHelpBubbleControllerExists) {
  EXPECT_TRUE(UserEducationHelpBubbleController::Get());
}

// Verifies that the user education tutorial controller exists.
TEST_F(UserEducationControllerTest, UserEducationTutorialControllerExists) {
  EXPECT_TRUE(UserEducationTutorialController::Get());
}

// Verifies that the Welcome Tour controller exists.
TEST_F(UserEducationControllerTest, WelcomeTourControllerExists) {
  EXPECT_TRUE(WelcomeTourController::Get());
}

// Verifies that `GetElementIdentifierForAppId()` delegates as expected.
TEST_F(UserEducationControllerTest, GetElementIdentifierForAppId) {
  auto* controller = UserEducationController::Get();
  ASSERT_TRUE(controller);

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
