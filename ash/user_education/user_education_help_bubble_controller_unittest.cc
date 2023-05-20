// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/user_education_help_bubble_controller.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/user_education/mock_user_education_delegate.h"
#include "ash/user_education/user_education_ash_test_base.h"
#include "ash/user_education/user_education_types.h"
#include "ash/user_education/user_education_util.h"
#include "ash/user_education/views/help_bubble_factory_views_ash.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "components/user_education/common/help_bubble.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/user_education/views/help_bubble_views_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// Aliases.
using ::testing::A;
using ::testing::ByMove;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::Optional;
using ::testing::Return;
using ::testing::WithArgs;
using ::user_education::HelpBubble;
using ::user_education::HelpBubbleParams;

// Element identifiers.
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kElementId);

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

  // Returns the factory to use to create help bubbles.
  HelpBubbleFactoryViewsAsh* help_bubble_factory() {
    return &help_bubble_factory_;
  }

 private:
  // Used to enable user education features which are required for existence of
  // the `controller()` under test.
  base::test::ScopedFeatureList scoped_feature_list_;

  // Used to mock help bubble creation given that user education services in
  // the browser are non-existent for unit tests in Ash.
  user_education::test::TestHelpBubbleDelegate help_bubble_delegate_;
  HelpBubbleFactoryViewsAsh help_bubble_factory_{&help_bubble_delegate_};
};

// Tests -----------------------------------------------------------------------

// Verifies that `CreateHelpBubble()` can be used to create a help bubble for a
// tracked element, and that `GetHelpBubbleId()` can be used to retrieve the ID
// of the currently showing help bubble for a tracked element.
TEST_F(UserEducationHelpBubbleControllerTest, CreateHelpBubble) {
  // User education in Ash is currently only supported for the primary user
  // profile. This is a self-imposed restriction. Log in the primary user.
  AccountId primary_user_account_id = AccountId::FromUserEmail("primary@test");
  SimulateUserLogin(primary_user_account_id);

  // Create and show a `widget` to serve as help bubble anchor.
  views::UniqueWidgetPtr widget = CreateFramelessTestWidget();
  widget->SetContentsView(
      views::Builder<views::View>()
          .SetProperty(views::kElementIdentifierKey, kElementId)
          .Build());
  widget->CenterWindow(gfx::Size(50, 50));
  widget->ShowInactive();

  // Cache `element_context`.
  const ui::ElementContext element_context =
      views::ElementTrackerViews::GetContextForWidget(widget.get());

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

  // The `controller()` should not return a help bubble ID for `kElementId` and
  // `element_context` since no help bubble was created; neither should it
  // return a help bubble ID for any other context.
  EXPECT_FALSE(controller()->GetHelpBubbleId(kElementId, element_context));
  EXPECT_FALSE(controller()->GetHelpBubbleId(kElementId, ui::ElementContext()));

  // When no circumstances exist which would otherwise prevent it from doing
  // so, the delegate will return a `help_bubble` for the `controller()` to own.
  HelpBubble* help_bubble = nullptr;
  EXPECT_CALL(*user_education_delegate(),
              CreateHelpBubble(Eq(primary_user_account_id),
                               Eq(HelpBubbleId::kTest), A<HelpBubbleParams>(),
                               Eq(kElementId), Eq(element_context)))
      .WillOnce(WithArgs<2>(Invoke([&](HelpBubbleParams help_bubble_params) {
        // Set `help_bubble_id` in extended properties.
        help_bubble_params.extended_properties.values().Merge(std::move(
            user_education_util::CreateExtendedProperties(HelpBubbleId::kTest)
                .values()));

        // Create and cache the `help_bubble`.
        auto result = help_bubble_factory()->CreateBubble(
            ui::ElementTracker::GetElementTracker()->GetFirstMatchingElement(
                kElementId, element_context),
            std::move(help_bubble_params));
        help_bubble = result.get();
        return result;
      })));

  // When the delegate returns a `help_bubble`, the `controller()` should
  // indicate to the caller that a `help_bubble` was created.
  EXPECT_TRUE(controller()->CreateHelpBubble(
      HelpBubbleId::kTest, HelpBubbleParams(), kElementId, element_context));
  Mock::VerifyAndClearExpectations(user_education_delegate());

  // The `controller()` should return the expected help bubble ID for
  // `kElementId` and `element_context` since a `help_bubble` was created; it
  // should not return a help bubble ID for any other context.
  EXPECT_THAT(controller()->GetHelpBubbleId(kElementId, element_context),
              Optional(HelpBubbleId::kTest));
  EXPECT_FALSE(controller()->GetHelpBubbleId(kElementId, ui::ElementContext()));

  // While a `help_bubble` is showing, no delegation should occur.
  EXPECT_CALL(*user_education_delegate(), CreateHelpBubble).Times(0);

  // Instead, the `controller()` should indicate to the caller that no help
  // bubble was created since a `help_bubble` is already vying for the user's
  // attention.
  EXPECT_FALSE(controller()->CreateHelpBubble(
      HelpBubbleId::kTest, HelpBubbleParams(), kElementId, element_context));
  Mock::VerifyAndClearExpectations(user_education_delegate());

  // Despite the fact that no new help bubble was created, the `controller()`
  // should continue to return the expected help bubble ID for `kElementId` and
  // `element_context` since a `help_bubble` already exists; it should not
  // return a help bubble ID for any other context.
  EXPECT_THAT(controller()->GetHelpBubbleId(kElementId, element_context),
              Optional(HelpBubbleId::kTest));
  EXPECT_FALSE(controller()->GetHelpBubbleId(kElementId, ui::ElementContext()));

  // Close the `help_bubble`.
  ASSERT_TRUE(help_bubble);
  help_bubble->Close();
  help_bubble = nullptr;

  // The `controller()` should not return a help bubble ID for `kElementId` and
  // `element_context` since the `help_bubble` was closed; neither should it
  //  return a help bubble ID for any other context.
  EXPECT_FALSE(controller()->GetHelpBubbleId(kElementId, element_context));
  EXPECT_FALSE(controller()->GetHelpBubbleId(kElementId, ui::ElementContext()));

  // Once the `help_bubble` has been closed, the delegate should again be tasked
  // with subsequent `help_bubble` creation.
  EXPECT_CALL(*user_education_delegate(),
              CreateHelpBubble(Eq(primary_user_account_id),
                               Eq(HelpBubbleId::kTest), A<HelpBubbleParams>(),
                               Eq(kElementId), Eq(element_context)))
      .WillOnce(WithArgs<2>(Invoke([&](HelpBubbleParams help_bubble_params) {
        // Set `help_bubble_id` in extended properties.
        help_bubble_params.extended_properties.values().Merge(std::move(
            user_education_util::CreateExtendedProperties(HelpBubbleId::kTest)
                .values()));

        // Create and cache the `help_bubble`.
        auto result = help_bubble_factory()->CreateBubble(
            ui::ElementTracker::GetElementTracker()->GetFirstMatchingElement(
                kElementId, element_context),
            std::move(help_bubble_params));
        help_bubble = result.get();
        return result;
      })));

  // The `controller()` should indicate to the caller success when attempting to
  // create a new `help_bubble` since the previous `help_bubble` was closed.
  // Note that this time a `close_callback` is provided.
  base::MockOnceClosure close_callback;
  EXPECT_TRUE(controller()->CreateHelpBubble(
      HelpBubbleId::kTest, HelpBubbleParams(), kElementId, element_context,
      close_callback.Get()));
  Mock::VerifyAndClearExpectations(user_education_delegate());

  // The `controller()` should return the expected help bubble ID for
  // `kElementId` and `element_context` since a `help_bubble` was created; it
  // should not return a help bubble ID for any other context.
  EXPECT_THAT(controller()->GetHelpBubbleId(kElementId, element_context),
              Optional(HelpBubbleId::kTest));
  EXPECT_FALSE(controller()->GetHelpBubbleId(kElementId, ui::ElementContext()));

  // Expect that closing the `help_bubble` will invoke `close_callback`.
  EXPECT_CALL(close_callback, Run);

  // Close the `help_bubble`.
  ASSERT_TRUE(help_bubble);
  help_bubble->Close();
  help_bubble = nullptr;

  // The `controller()` should not return a help bubble ID for `kElementId` and
  // `element_context` since the `help_bubble` was closed; neither should it
  //  return a help bubble ID for any other context.
  EXPECT_FALSE(controller()->GetHelpBubbleId(kElementId, element_context));
  EXPECT_FALSE(controller()->GetHelpBubbleId(kElementId, ui::ElementContext()));
}

}  // namespace ash
