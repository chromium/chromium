// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/user_education_help_bubble_controller.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/user_education/mock_user_education_delegate.h"
#include "ash/user_education/user_education_ash_test_base.h"
#include "ash/user_education/user_education_types.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "components/user_education/common/help_bubble.h"
#include "components/user_education/common/help_bubble_params.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/framework_specific_implementation.h"
#include "ui/views/view.h"

namespace ash {
namespace {

// Aliases.
using ::testing::A;
using ::testing::ByMove;
using ::testing::Eq;
using ::testing::InvokeWithoutArgs;
using ::testing::Mock;
using ::testing::Return;
using ::user_education::HelpBubble;
using ::user_education::HelpBubbleParams;

// Element identifiers.
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kElementId);

// MockHelpBubble --------------------------------------------------------------

class MockHelpBubble : public HelpBubble {
 public:
  DECLARE_FRAMEWORK_SPECIFIC_METADATA()

  // HelpBubble:
  MOCK_METHOD(void, CloseBubbleImpl, (), (override));
  MOCK_METHOD(ui::ElementContext, GetContext, (), (const, override));
  MOCK_METHOD(bool, ToggleFocusForAccessibility, (), (override));
};

DEFINE_FRAMEWORK_SPECIFIC_METADATA(MockHelpBubble)

}  // namespace

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
  // User education in Ash is currently only supported for the primary user
  // profile. This is a self-imposed restriction. Log in the primary user.
  AccountId primary_user_account_id = AccountId::FromUserEmail("primary@test");
  SimulateUserLogin(primary_user_account_id);

  // Mock an `element_context`.
  constexpr size_t kContextValue(0u);
  const ui::ElementContext element_context(&kContextValue);

  // Help bubble creation is delegated. The delegate may opt *not* to return a
  // help bubble in certain circumstances, e.g. if there is an ongoing tutorial.
  EXPECT_CALL(*user_education_delegate(),
              CreateHelpBubble(Eq(primary_user_account_id),
                               Eq(HelpBubbleId::kTest), A<HelpBubbleParams>(),
                               Eq(kElementId), Eq(element_context)))
      .WillOnce(Return(ByMove(nullptr)));

  // When the delegate opts *not* to return a help bubble, the `controller()`
  // should indicate to the caller that no help bubble was created.
  EXPECT_FALSE(controller()->CreateHelpBubble(
      HelpBubbleId::kTest, HelpBubbleParams(), kElementId, element_context));
  Mock::VerifyAndClearExpectations(user_education_delegate());

  // When no circumstances exist which would otherwise prevent it from doing
  // so, the delegate will return a `help_bubble` for the `controller()` to own.
  HelpBubble* help_bubble = nullptr;
  EXPECT_CALL(*user_education_delegate(),
              CreateHelpBubble(Eq(primary_user_account_id),
                               Eq(HelpBubbleId::kTest), A<HelpBubbleParams>(),
                               Eq(kElementId), Eq(element_context)))
      .WillOnce(InvokeWithoutArgs([&]() {
        auto result = std::make_unique<MockHelpBubble>();
        help_bubble = result.get();
        return result;
      }));

  // When the delegate returns a `help_bubble`, the `controller()` should
  // indicate to the caller that a `help_bubble` was created.
  EXPECT_TRUE(controller()->CreateHelpBubble(
      HelpBubbleId::kTest, HelpBubbleParams(), kElementId, element_context));
  Mock::VerifyAndClearExpectations(user_education_delegate());

  // While a `help_bubble` is showing, no delegation should occur.
  EXPECT_CALL(*user_education_delegate(), CreateHelpBubble).Times(0);

  // Instead, the `controller()` should indicate to the caller that no help
  // bubble was created since a `help_bubble` is already vying for the user's
  // attention.
  EXPECT_FALSE(controller()->CreateHelpBubble(
      HelpBubbleId::kTest, HelpBubbleParams(), kElementId, element_context));
  Mock::VerifyAndClearExpectations(user_education_delegate());

  // Close the `help_bubble`.
  ASSERT_TRUE(help_bubble);
  help_bubble->Close();
  help_bubble = nullptr;

  // Once the `help_bubble` has been closed, the delegate should again be tasked
  // with subsequent `help_bubble` creation.
  EXPECT_CALL(*user_education_delegate(),
              CreateHelpBubble(Eq(primary_user_account_id),
                               Eq(HelpBubbleId::kTest), A<HelpBubbleParams>(),
                               Eq(kElementId), Eq(element_context)))
      .WillOnce(InvokeWithoutArgs([&]() {
        auto result = std::make_unique<MockHelpBubble>();
        help_bubble = result.get();
        return result;
      }));

  // The `controller()` should indicate to the caller success when attempting to
  // create a new `help_bubble` since the previous `help_bubble` was closed.
  // Note that this time a `close_callback` is provided.
  base::MockOnceClosure close_callback;
  EXPECT_TRUE(controller()->CreateHelpBubble(
      HelpBubbleId::kTest, HelpBubbleParams(), kElementId, element_context,
      close_callback.Get()));
  Mock::VerifyAndClearExpectations(user_education_delegate());

  // Expect that closing the `help_bubble` will invoke `close_callback`.
  EXPECT_CALL(close_callback, Run);

  // Close the `help_bubble`.
  ASSERT_TRUE(help_bubble);
  help_bubble->Close();
  help_bubble = nullptr;
}

}  // namespace ash
