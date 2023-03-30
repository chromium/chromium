// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/user_education_controller.h"

#include <map>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/test/ash_test_base.h"
#include "ash/test_shell_delegate.h"
#include "ash/user_education/mock_user_education_delegate.h"
#include "ash/user_education/tutorial_controller.h"
#include "ash/user_education/welcome_tour/welcome_tour_controller.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "components/account_id/account_id.h"
#include "components/user_education/common/tutorial_description.h"
#include "components/user_education/common/tutorial_identifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

// Aliases.
using testing::_;
using testing::Eq;
using testing::StrictMock;
using user_education::TutorialDescription;
using user_education::TutorialIdentifier;

}  // namespace

// UserEducationControllerTest -------------------------------------------------

// Base class for tests of the `UserEducationController` parameterized by
// whether user education features are enabled.
class UserEducationControllerTest
    : public NoSessionAshTestBase,
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

  // Returns the mocked delegate which facilitates communication between Ash and
  // user education services in the browser.
  StrictMock<MockUserEducationDelegate>* user_education_delegate() {
    return user_education_delegate_;
  }

 private:
  // NoSessionAshTestBase:
  void SetUp() override {
    // Mock the user education delegate. Note that it is expected that the user
    // education delegate be created once and only once.
    auto shell_delegate = std::make_unique<TestShellDelegate>();
    shell_delegate->SetUserEducationDelegateFactory(base::BindLambdaForTesting(
        [&]() -> std::unique_ptr<UserEducationDelegate> {
          EXPECT_EQ(user_education_delegate_, nullptr);
          auto user_education_delegate =
              std::make_unique<StrictMock<MockUserEducationDelegate>>();
          user_education_delegate_ = user_education_delegate.get();
          return user_education_delegate;
        }));
    NoSessionAshTestBase::SetUp(std::move(shell_delegate));
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  StrictMock<MockUserEducationDelegate>* user_education_delegate_ = nullptr;
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

// Verifies that the Welcome Tour controller exists iff the feature is enabled.
TEST_P(UserEducationControllerTest, WelcomeTourControllerExists) {
  EXPECT_EQ(!!WelcomeTourController::Get(), IsWelcomeTourEnabled());
}

// Verifies that tutorials are registered when the primary user session is
// added. Note that this test is skipped if the controller does not exist.
TEST_P(UserEducationControllerTest, RegistersTutorials) {
  auto* user_education_controller = UserEducationController::Get();
  if (!user_education_controller) {
    GTEST_SKIP();
  }

  // Ensure user delegate exists.
  auto* user_education_delegate = this->user_education_delegate();
  ASSERT_TRUE(user_education_delegate);

  // Create and cache an account ID for the primary user.
  AccountId primary_user_account_id = AccountId::FromUserEmail("primary@test");

  // Expect Welcome Tour tutorials to be registered with user education services
  // in the browser if and only if the Welcome Tour feature is enabled.
  if (IsWelcomeTourEnabled()) {
    auto* welcome_tour_controller = WelcomeTourController::Get();
    ASSERT_TRUE(welcome_tour_controller);
    for (const auto& [tutorial_id, ignore] :
         static_cast<TutorialController*>(welcome_tour_controller)
             ->GetTutorialDescriptions()) {
      EXPECT_CALL(
          *user_education_delegate,
          RegisterTutorial(Eq(primary_user_account_id), Eq(tutorial_id), _));
    }
  }

  // Add the primary user session and verify expectations.
  SimulateUserLogin(primary_user_account_id);
  testing::Mock::VerifyAndClearExpectations(user_education_delegate);

  // Add a secondary user session and verify that *no* tutorials are registered
  // with user education services in the browser.
  EXPECT_CALL(*user_education_delegate, RegisterTutorial).Times(0);
  SimulateUserLogin(AccountId::FromUserEmail("secondary@test"));
}

}  // namespace ash
