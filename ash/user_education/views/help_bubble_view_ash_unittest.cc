// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/views/help_bubble_view_ash.h"

#include <optional>
#include <vector>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/user_education/user_education_types.h"
#include "ash/user_education/user_education_util.h"
#include "ash/user_education/views/help_bubble_view_ash_test_base.h"
#include "ash/wm/window_util.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/aura/window_targeter.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_provider.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {
namespace {

// Aliases.
using ::testing::AnyOf;
using ::testing::Conditional;
using ::testing::Eq;
using ::testing::IsNull;
using ::testing::Not;
using ::testing::Property;
using ::user_education::HelpBubbleArrow;

// Matchers --------------------------------------------------------------------

MATCHER_P(Contains, window, "") {
  return arg->Contains(window);
}

// Helpers ---------------------------------------------------------------------

ui::MouseEvent CreateMouseMovedEvent(aura::Window* target,
                                     const gfx::Point& location_in_screen) {
  gfx::Point location(location_in_screen);
  wm::ConvertPointFromScreen(target, &location);
  ui::MouseEvent mouse_moved_event(ui::EventType::kMouseMoved, location,
                                   location_in_screen, ui::EventTimeForNow(),
                                   /*flags=*/ui::EF_NONE,
                                   /*changed_button_flags=*/ui::EF_NONE);
  ui::Event::DispatcherApi(&mouse_moved_event).set_target(target);
  return mouse_moved_event;
}

std::vector<gfx::Point> GetPerimeterPoints(const gfx::Rect& rect) {
  return std::vector<gfx::Point>({rect.origin(), rect.top_center(),
                                  rect.top_right(), rect.right_center(),
                                  rect.bottom_right(), rect.bottom_center(),
                                  rect.bottom_left(), rect.left_center()});
}

}  // namespace

// HelpBubbleViewAshTest -------------------------------------------------------

// Base class for tests of `HelpBubbleViewAsh`.
using HelpBubbleViewAshTest = HelpBubbleViewAshTestBase;

// Tests -----------------------------------------------------------------------

// Verifies that help bubbles are contained within the correct parent window.
TEST_F(HelpBubbleViewAshTest, ParentWindow) {
  auto* const help_bubble_view = CreateHelpBubbleView();
  EXPECT_TRUE(help_bubble_view->anchor_widget()
                  ->GetNativeWindow()
                  ->GetRootWindow()
                  ->GetChildById(kShellWindowId_HelpBubbleContainer)
                  ->Contains(help_bubble_view->GetWidget()->GetNativeWindow()));
}

// HelpBubbleViewAshBodyIconTest -----------------------------------------------

// Base class for tests of `HelpBubbleViewAsh` which are primarily concerned
// with body icons, parameterized by:
// (a) the body icon from help bubble params, and
// (b) the body icon from extended properties.
class HelpBubbleViewAshBodyIconTest
    : public HelpBubbleViewAshTestBase,
      public ::testing::WithParamInterface<
          std::tuple</*body_icon_from_params=*/std::optional<
                         std::reference_wrapper<const gfx::VectorIcon>>,
                     /*body_icon_from_extended_properties=*/std::optional<
                         std::reference_wrapper<const gfx::VectorIcon>>>> {
 public:
  // Returns the body icon from help bubble params given test parameterization.
  const std::optional<std::reference_wrapper<const gfx::VectorIcon>>
  body_icon_from_params() const {
    return std::get<0>(GetParam());
  }

  // Returns the body icon from extended properties given test parameterization.
  const std::optional<std::reference_wrapper<const gfx::VectorIcon>>
  body_icon_from_extended_properties() const {
    return std::get<1>(GetParam());
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    HelpBubbleViewAshBodyIconTest,
    ::testing::Combine(
        /*body_icon_from_params=*/::testing::Values(
            std::make_optional(std::cref(gfx::kNoneIcon)),
            std::make_optional(std::cref(vector_icons::kCelebrationIcon)),
            std::make_optional(std::cref(vector_icons::kHelpIcon)),
            std::nullopt),
        /*body_icon_from_extended_properties=*/::testing::Values(
            std::make_optional(std::cref(gfx::kNoneIcon)),
            std::make_optional(std::cref(vector_icons::kCelebrationIcon)),
            std::make_optional(std::cref(vector_icons::kHelpIcon)),
            std::nullopt)));

// Tests -----------------------------------------------------------------------

// Verifies that help bubble view body icons are configured as expected.
TEST_P(HelpBubbleViewAshBodyIconTest, BodyIcon) {
  // Construct help bubble `params`.
  user_education::HelpBubbleParams params;
  params.extended_properties =
      user_education_util::CreateExtendedProperties(HelpBubbleId::kTest);

  // Populate `body_icon_from_params` based on parameterization.
  if (const auto& body_icon_from_params = this->body_icon_from_params()) {
    params.body_icon = &body_icon_from_params->get();
  }

  // Populate `body_icon_from_extended_properties` based on parameterization.
  if (const auto& body_icon_from_extended_properties =
          this->body_icon_from_extended_properties()) {
    params.extended_properties = user_education_util::CreateExtendedProperties(
        std::move(params.extended_properties),
        user_education_util::CreateExtendedProperties(
            body_icon_from_extended_properties->get()));
  }

  // Create `help_bubble_view`.
  auto* const help_bubble_view = CreateHelpBubbleView(std::move(params));
  ASSERT_NE(help_bubble_view, nullptr);

  // Cache `expected_body_icon` based on order of precedence.
  const gfx::VectorIcon& expected_body_icon =
      body_icon_from_extended_properties().value_or(
          body_icon_from_params().value_or(gfx::kNoneIcon));

  // Confirm body icon exists iff expected and is configured as expected.
  EXPECT_THAT(
      views::ElementTrackerViews::GetInstance()
          ->GetUniqueViewAs<views::ImageView>(
              HelpBubbleViewAsh::kBodyIconIdForTesting,
              views::ElementTrackerViews::GetContextForView(help_bubble_view)),
      Conditional(&expected_body_icon != &gfx::kNoneIcon,
                  Property(&views::ImageView::GetImageModel,
                           Eq(ui::ImageModel::FromVectorIcon(
                               expected_body_icon,
                               cros_tokens::kCrosSysDialogContainer, 20))),
                  IsNull()));
}

// Verifies that help bubbles have the appropriate background color.
TEST_F(HelpBubbleViewAshTest, BackgroundColor) {
  const auto* const help_bubble_view = CreateHelpBubbleView();
  const auto* const color_provider = help_bubble_view->GetColorProvider();
  EXPECT_EQ(help_bubble_view->color(),
            color_provider->GetColor(cros_tokens::kCrosSysDialogContainer));
}

// Verifies that help bubbles can activate.
TEST_F(HelpBubbleViewAshTest, CanActivate) {
  const auto* const help_bubble_view = CreateHelpBubbleView();
  EXPECT_TRUE(help_bubble_view->CanActivate());
}

// Verifies that help bubbles do not handle events within their shadows.
TEST_F(HelpBubbleViewAshTest, HitTest) {
  auto* const help_bubble_view = CreateHelpBubbleView();
  auto* const help_bubble_widget = help_bubble_view->GetWidget();
  auto* const help_bubble_window = help_bubble_widget->GetNativeWindow();
  auto* const root_window = help_bubble_window->GetRootWindow();
  auto* const root_window_targeter = root_window->targeter();

  // Case: Event within help bubble contents bounds.
  // NOTE: We inset `contents_bounds` to account for fractional pixel rounding
  // in event processing. This ensures the event is within `contents_bounds`.
  gfx::Rect contents_bounds(help_bubble_view->GetBoundsInScreen());
  contents_bounds.Inset(1);

  // Events within help bubble `contents_bounds` should be handled.
  for (const gfx::Point& point : GetPerimeterPoints(contents_bounds)) {
    EXPECT_THAT(window_util::GetEventHandlerForEvent(
                    CreateMouseMovedEvent(root_window, point)),
                AnyOf(Eq(help_bubble_window), Contains(help_bubble_window)));

    // Confirm that the help bubble window will be targeted for the event so its
    // events don't leak through to windows behind it.
    // TODO(http://b/307780200): Possibly remove this when `WindowTargeter` is
    // updated, since it should be tested at that level
    ui::MouseEvent press(ui::EventType::kMousePressed, point, point,
                         base::TimeTicks::Now(), ui::EF_NONE,
                         ui::EF_LEFT_MOUSE_BUTTON);
    EXPECT_EQ(root_window_targeter->FindTargetForEvent(root_window, &press),
              help_bubble_window);
  }

  // Case: Event within help bubble shadow bounds.
  // NOTE: We outset contents bounds to enlarge the rect into the shadow.
  gfx::Rect shadow_bounds(help_bubble_view->GetBoundsInScreen());
  shadow_bounds.Outset(1);
  // Events within help bubble `shadow_bounds` should *not* be handled, nor
  // should they target that window.
  for (const gfx::Point& point : GetPerimeterPoints(shadow_bounds)) {
    EXPECT_THAT(
        window_util::GetEventHandlerForEvent(
            CreateMouseMovedEvent(root_window, point)),
        Not(AnyOf(Eq(help_bubble_window), Contains(help_bubble_window))));

    // Also confirm that the help bubble window will not be targeted for the
    // event so it doesn't block events in its shadow.
    // TODO(http://b/307780200): Possibly remove this when `WindowTargeter` is
    // updated, since it should be tested at that level
    ui::MouseEvent press(ui::EventType::kMousePressed, point, point,
                         base::TimeTicks::Now(), ui::EF_NONE,
                         ui::EF_LEFT_MOUSE_BUTTON);
    EXPECT_NE(root_window_targeter->FindTargetForEvent(root_window, &press),
              help_bubble_window);
  }
}

}  // namespace ash
