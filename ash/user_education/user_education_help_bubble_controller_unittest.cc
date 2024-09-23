// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/user_education_help_bubble_controller.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/user_education/mock_user_education_delegate.h"
#include "ash/user_education/user_education_ash_test_base.h"
#include "ash/user_education/user_education_types.h"
#include "ash/user_education/user_education_util.h"
#include "ash/user_education/views/help_bubble_factory_views_ash.h"
#include "ash/user_education/views/help_bubble_view_ash.h"
#include "base/callback_list.h"
#include "base/test/mock_callback.h"
#include "base/test/repeating_test_future.h"
#include "base/test/scoped_feature_list.h"
#include "components/user_education/common/help_bubble.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/user_education/views/help_bubble_views_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// Aliases.
using ::testing::A;
using ::testing::AllOf;
using ::testing::ByMove;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::Matches;
using ::testing::Mock;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::Return;
using ::testing::WithArgs;
using ::user_education::HelpBubble;
using ::user_education::HelpBubbleParams;

// Element identifiers.
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kElementId);

// Helpers ---------------------------------------------------------------------

gfx::Rect GetBoundsInScreen(const views::Widget* widget) {
  return widget->GetWindowBoundsInScreen();
}

HelpBubbleViewAsh* GetHelpBubbleView(HelpBubble* help_bubble) {
  return help_bubble->IsA<HelpBubbleViewsAsh>()
             ? help_bubble->AsA<HelpBubbleViewsAsh>()->bubble_view()
             : nullptr;
}

const aura::Window* GetRootWindow(const views::Widget* widget) {
  return widget->GetNativeWindow()->GetRootWindow();
}

// Actions ---------------------------------------------------------------------

// NOTE: This action intentionally does *not* use `ACTION_P` macros, as actions
// generated in that way struggle to support move-only types.
template <typename ClassPtr, typename MethodPtr, typename ResultPtr>
auto InvokeAndCopyResultAddressTo(ClassPtr class_ptr,
                                  MethodPtr method_ptr,
                                  ResultPtr output_ptr) {
  return [class_ptr, method_ptr, output_ptr](auto&&... args) {
    auto result = (class_ptr->*method_ptr)(std::move(args)...);
    *output_ptr = result.get();
    return result;
  };
}

// Matchers --------------------------------------------------------------------

MATCHER_P(AnchorBoundsInScreen, matcher, "") {
  return Matches(matcher)(arg.anchor_bounds_in_screen);
}

MATCHER_P(AnchorRootWindow, matcher, "") {
  return Matches(matcher)(arg.anchor_root_window);
}

MATCHER_P(Key, matcher, "") {
  return Matches(matcher)(arg.key);
}

}  // namespace

// UserEducationHelpBubbleControllerTest ---------------------------------------

// Base class for tests of the `UserEducationHelpBubbleController`.
class UserEducationHelpBubbleControllerTest : public UserEducationAshTestBase {
 public:
  UserEducationHelpBubbleControllerTest() {
    // NOTE: The `UserEducationHelpBubbleController` exists only when a user
    // education feature is enabled. Controller existence is verified in test
    // coverage for the controller's owner.
    scoped_feature_list_.InitAndEnableFeature(features::kWelcomeTour);
  }

  // Creates and returns a help bubble for the specified `help_bubble_params`,
  // anchored to the `help_bubble_anchor_widget()`.
  std::unique_ptr<HelpBubble> CreateHelpBubble(
      HelpBubbleParams help_bubble_params = HelpBubbleParams()) {
    // Set `help_bubble_id` in extended properties.
    help_bubble_params.extended_properties.values().Merge(std::move(
        user_education_util::CreateExtendedProperties(HelpBubbleId::kTest)
            .values()));

    // Create the help bubble.
    return help_bubble_factory()->CreateBubble(
        ui::ElementTracker::GetElementTracker()->GetFirstMatchingElement(
            kElementId, help_bubble_anchor_context()),
        std::move(help_bubble_params));
  }

  // Returns the singleton instance owned by the `UserEducationController`.
  UserEducationHelpBubbleController* controller() {
    return UserEducationHelpBubbleController::Get();
  }

  // Returns the element context to use for help bubble anchors.
  ui::ElementContext help_bubble_anchor_context() const {
    return views::ElementTrackerViews::GetContextForWidget(
        help_bubble_anchor_widget_.get());
  }

  // Returns the widget to use for help bubble anchors.
  views::Widget* help_bubble_anchor_widget() {
    return help_bubble_anchor_widget_.get();
  }

  // Returns the factory to use to create help bubbles.
  HelpBubbleFactoryViewsAsh* help_bubble_factory() {
    return &help_bubble_factory_;
  }

  // Returns the account ID for the primary user profile which is logged in
  // during test `SetUp()`. Note that user education in Ash is currently only
  // supported for the primary user profile.
  const AccountId& primary_user_account_id() const {
    return primary_user_account_id_;
  }

 private:
  // UserEducationAshTestBase:
  void SetUp() override {
    UserEducationAshTestBase::SetUp();

    // User education in Ash is currently only supported for the primary user
    // profile. This is a self-imposed restriction. Log in the primary user.
    primary_user_account_id_ = AccountId::FromUserEmail("primary@test");
    SimulateUserLogin(primary_user_account_id_);

    // Create and show a `help_bubble_anchor_widget_`.
    help_bubble_anchor_widget_ = CreateFramelessTestWidget();
    help_bubble_anchor_widget_->SetContentsView(
        views::Builder<views::View>()
            .SetProperty(views::kElementIdentifierKey, kElementId)
            .Build());
    help_bubble_anchor_widget_->CenterWindow(gfx::Size(50, 50));
    help_bubble_anchor_widget_->ShowInactive();
  }

  // Used to enable user education features which are required for existence of
  // the `controller()` under test.
  base::test::ScopedFeatureList scoped_feature_list_;

  // The widget to use for help bubble anchors.
  views::UniqueWidgetPtr help_bubble_anchor_widget_;

  // Used to mock help bubble creation given that user education services in
  // the browser are non-existent for unit tests in Ash.
  user_education::test::TestHelpBubbleDelegate help_bubble_delegate_;
  HelpBubbleFactoryViewsAsh help_bubble_factory_{&help_bubble_delegate_};

  // The account ID for the primary user profile which is logged in during test
  // `SetUp()`. Note that user education in Ash is currently only supported for
  // the primary user profile.
  AccountId primary_user_account_id_;
};

// Tests -----------------------------------------------------------------------

// Verifies that the `UserEducationHelpBubbleController` tracks/exposes metadata
// for currently showing help bubbles as intended.
TEST_F(UserEducationHelpBubbleControllerTest, Metadata) {
  // Verify that cached help bubble metadata is empty.
  EXPECT_THAT(controller()->help_bubble_metadata_by_key(), IsEmpty());

  // Create a `help_bubble`.
  std::unique_ptr<HelpBubble> help_bubble = CreateHelpBubble();
  ASSERT_TRUE(help_bubble);

  // Verify that a `help_bubble_view` was created.
  HelpBubbleViewAsh* help_bubble_view = GetHelpBubbleView(help_bubble.get());
  ASSERT_TRUE(help_bubble_view);

  // Verify that cached help bubble metadata is populated as expected.
  EXPECT_THAT(controller()->help_bubble_metadata_by_key(),
              ElementsAre(Pair(Eq(help_bubble_view),
                               AllOf(Key(Eq(help_bubble_view)),
                                     AnchorRootWindow(Eq(GetRootWindow(
                                         help_bubble_anchor_widget()))),
                                     AnchorBoundsInScreen(Eq(GetBoundsInScreen(
                                         help_bubble_anchor_widget())))))));

  // Change `help_bubble` anchor bounds.
  help_bubble_anchor_widget()->CenterWindow(gfx::Size(100, 100));

  // Verify that cached help bubble metadata is updated as expected.
  EXPECT_THAT(controller()->help_bubble_metadata_by_key(),
              ElementsAre(Pair(Eq(help_bubble_view),
                               AllOf(Key(Eq(help_bubble_view)),
                                     AnchorRootWindow(Eq(GetRootWindow(
                                         help_bubble_anchor_widget()))),
                                     AnchorBoundsInScreen(Eq(GetBoundsInScreen(
                                         help_bubble_anchor_widget())))))));

  // Destroy `help_bubble`.
  views::test::WidgetDestroyedWaiter waiter(help_bubble_view->GetWidget());
  help_bubble->Close();
  waiter.Wait();
  help_bubble = nullptr;
  help_bubble_view = nullptr;

  // Verify that cached help bubble metadata is empty.
  EXPECT_THAT(controller()->help_bubble_metadata_by_key(), IsEmpty());
}

// Verifies that `UserEducationHelpBubbleController` subscriptions are WAI.
TEST_F(UserEducationHelpBubbleControllerTest, Subscriptions) {
  std::unique_ptr<HelpBubble> help_bubble;

  {
    // Expect that subscribers will be notified of shown events.
    // Note that help bubbles are shown automatically when created.
    base::test::RepeatingTestFuture<void> event_future;
    base::CallbackListSubscription subscription =
        controller()->AddHelpBubbleShownCallback(event_future.GetCallback());

    // Create the `help_bubble`.
    help_bubble = CreateHelpBubble();
    EXPECT_TRUE(help_bubble);

    // Verify expectations.
    EXPECT_TRUE(event_future.Wait());
  }

  {
    // Expect that subscribers will be notified of anchor bounds changed events.
    base::test::RepeatingTestFuture<void> event_future;
    base::CallbackListSubscription subscription =
        controller()->AddHelpBubbleAnchorBoundsChangedCallback(
            event_future.GetCallback());

    // Change `help_bubble` anchor bounds.
    help_bubble_anchor_widget()->CenterWindow(gfx::Size(100, 100));

    // Verify expectations.
    EXPECT_TRUE(event_future.Wait());
  }

  {
    // Expect that subscribers will be notified of closed events.
    base::test::RepeatingTestFuture<void> event_future;
    base::CallbackListSubscription subscription =
        controller()->AddHelpBubbleClosedCallback(event_future.GetCallback());

    // Close the `help_bubble`.
    ASSERT_TRUE(help_bubble);
    help_bubble->Close();
    help_bubble = nullptr;

    // Verify expectations.
    EXPECT_TRUE(event_future.Wait());
  }
}

}  // namespace ash
