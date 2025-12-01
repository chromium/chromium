// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/action_button_container_view.h"

#include <memory>
#include <string>

#include "ash/capture_mode/action_button_view.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/ash_color_provider.h"
#include "ash/test/view_drawn_waiter.h"
#include "base/test/test_future.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

namespace ash {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::SizeIs;

// Returns true if `action_button` is collapsed (i.e. label hidden, only icon
// visible).
bool IsActionButtonCollapsed(const ActionButtonView* action_button) {
  return !action_button->label_for_testing()->GetVisible();
}

ActionButtonView* AddCopyTextButton(
    ActionButtonContainerView& action_button_container) {
  return action_button_container.AddActionButton(
      views::Button::PressedCallback(), u"Copy Text", &kCaptureModeImageIcon,
      ActionButtonRank(ActionButtonType::kCopyText, 0),
      ActionButtonViewID::kCopyTextButton);
}

ActionButtonView* AddSearchButton(
    ActionButtonContainerView& action_button_container) {
  return action_button_container.AddActionButton(
      views::Button::PressedCallback(), u"Search", &kCaptureModeImageIcon,
      ActionButtonRank(ActionButtonType::kSunfish, 0),
      ActionButtonViewID::kSearchButton);
}
ActionButtonView* AddSmartActionsButton(
    ActionButtonContainerView& action_button_container) {
  return action_button_container.AddActionButton(
      views::Button::PressedCallback(), u"Smart Actions",
      &kCaptureModeImageIcon, ActionButtonRank(ActionButtonType::kScanner, 0),
      ActionButtonViewID::kSmartActionsButton);
}

class ActionButtonContainerViewTest : public views::ViewsTestBase {
 private:
  // Required by `ActionButtonView`.
  AshColorProvider color_provider_;
};

TEST_F(ActionButtonContainerViewTest, AddsActionButton) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetBounds(gfx::Rect(50, 50, 300, 200));
  widget->Show();
  auto* action_button_container =
      widget->SetContentsView(std::make_unique<ActionButtonContainerView>());
  base::test::TestFuture<void> action_future;

  ActionButtonView* action_button = action_button_container->AddActionButton(
      action_future.GetCallback(), u"Button Text", &kCaptureModeImageIcon,
      ActionButtonRank(ActionButtonType::kScanner, 0),
      ActionButtonViewID::kScannerButton);

  // Check that the action button has been successfully created.
  ASSERT_TRUE(action_button);
  EXPECT_THAT(action_button_container->GetActionButtons(),
              ElementsAre(action_button));
  EXPECT_EQ(action_button->label_for_testing()->GetText(), u"Button Text");

  // Check that clicking the action button runs the action callback.
  ViewDrawnWaiter().Wait(action_button);
  ui::test::EventGenerator event_generator(GetRootWindow(widget.get()));
  event_generator.MoveMouseTo(action_button->GetBoundsInScreen().CenterPoint());
  event_generator.ClickLeftButton();

  EXPECT_TRUE(action_future.Wait());
}

TEST_F(ActionButtonContainerViewTest, ActionButtonsOrderedByRank) {
  ActionButtonContainerView action_button_container;

  ActionButtonView* copy_text_button =
      AddCopyTextButton(action_button_container);
  ActionButtonView* search_button = AddSearchButton(action_button_container);
  ActionButtonView* scanner_button = action_button_container.AddActionButton(
      views::Button::PressedCallback(), u"Scanner Button",
      &kCaptureModeImageIcon, ActionButtonRank(ActionButtonType::kScanner, 0),
      ActionButtonViewID::kScannerButton);

  EXPECT_THAT(action_button_container.GetActionButtons(),
              ElementsAre(scanner_button, copy_text_button, search_button));
}

TEST_F(ActionButtonContainerViewTest, SmartActionsButtonTransition) {
  gfx::ScopedAnimationDurationScaleMode animation_scale(
      gfx::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetBounds(gfx::Rect(50, 50, 300, 200));
  widget->Show();
  auto* action_button_container =
      widget->SetContentsView(std::make_unique<ActionButtonContainerView>());
  // Set up action buttons.
  ActionButtonView* copy_text_button =
      AddCopyTextButton(*action_button_container);
  ActionButtonView* search_button = AddSearchButton(*action_button_container);
  ActionButtonView* smart_actions_button =
      AddSmartActionsButton(*action_button_container);
  EXPECT_THAT(
      action_button_container->GetActionButtons(),
      ElementsAre(smart_actions_button, copy_text_button, search_button));

  action_button_container->StartSmartActionsButtonTransition();

  // The smart actions button should be removed and other buttons should be
  // collapsed.
  EXPECT_THAT(action_button_container->GetActionButtons(),
              ElementsAre(copy_text_button, search_button));
  EXPECT_TRUE(IsActionButtonCollapsed(copy_text_button));
  EXPECT_TRUE(IsActionButtonCollapsed(search_button));
}

TEST_F(ActionButtonContainerViewTest, RemoveSmartActionsButton) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetBounds(gfx::Rect(50, 50, 300, 200));
  widget->Show();
  auto* action_button_container =
      widget->SetContentsView(std::make_unique<ActionButtonContainerView>());
  // Set up action buttons.
  ActionButtonView* copy_text_button =
      AddCopyTextButton(*action_button_container);
  ActionButtonView* search_button = AddSearchButton(*action_button_container);
  ActionButtonView* smart_actions_button =
      AddSmartActionsButton(*action_button_container);
  EXPECT_THAT(
      action_button_container->GetActionButtons(),
      ElementsAre(smart_actions_button, copy_text_button, search_button));

  action_button_container->RemoveSmartActionsButton();

  EXPECT_THAT(action_button_container->GetActionButtons(),
              ElementsAre(copy_text_button, search_button));
}

TEST_F(ActionButtonContainerViewTest, ShowsErrorView) {
  ActionButtonContainerView action_button_container;
  ActionButtonContainerView::ErrorView* error_view =
      action_button_container.error_view_for_testing();

  EXPECT_FALSE(error_view->GetVisible());

  action_button_container.ShowErrorView(u"Error message");

  EXPECT_TRUE(error_view->GetVisible());
  EXPECT_EQ(error_view->GetErrorMessageForTesting(), u"Error message");
  EXPECT_FALSE(error_view->try_again_link()->GetVisible());

  action_button_container.HideErrorView();

  EXPECT_FALSE(error_view->GetVisible());
}

TEST_F(ActionButtonContainerViewTest, ShowsErrorViewWithTryAgainLink) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetBounds(gfx::Rect(50, 50, 300, 200));
  widget->Show();
  auto* action_button_container =
      widget->SetContentsView(std::make_unique<ActionButtonContainerView>());
  base::test::TestFuture<void> try_again_future;

  action_button_container->ShowErrorView(
      u"Error message", try_again_future.GetRepeatingCallback());

  ActionButtonContainerView::ErrorView* error_view =
      action_button_container->error_view_for_testing();
  EXPECT_TRUE(error_view->GetVisible());
  views::Link* try_again_link = error_view->try_again_link();
  EXPECT_TRUE(try_again_link->GetVisible());

  // Check that clicking the try again link runs the try again callback.
  ViewDrawnWaiter().Wait(try_again_link);
  ui::test::EventGenerator event_generator(GetRootWindow(widget.get()));
  event_generator.MoveMouseTo(
      try_again_link->GetBoundsInScreen().CenterPoint());
  event_generator.ClickLeftButton();

  EXPECT_TRUE(try_again_future.Wait());
}

TEST_F(ActionButtonContainerViewTest, ClearsContainer) {
  ActionButtonContainerView action_button_container;
  AddCopyTextButton(action_button_container);
  action_button_container.ShowErrorView(u"Error message");

  EXPECT_THAT(action_button_container.GetActionButtons(), SizeIs(1));
  EXPECT_TRUE(action_button_container.error_view_for_testing()->GetVisible());

  action_button_container.ClearContainer();

  EXPECT_THAT(action_button_container.GetActionButtons(), IsEmpty());
  EXPECT_FALSE(action_button_container.error_view_for_testing()->GetVisible());
}

TEST_F(ActionButtonContainerViewTest, GetsFocusableViews) {
  ActionButtonContainerView action_button_container;
  ActionButtonView* copy_text_button =
      AddCopyTextButton(action_button_container);

  EXPECT_THAT(action_button_container.GetFocusableViews(),
              ElementsAre(copy_text_button));

  action_button_container.ShowErrorView(
      u"Error message", /*try_again_callback=*/base::DoNothing());

  EXPECT_THAT(
      action_button_container.GetFocusableViews(),
      ElementsAre(
          action_button_container.error_view_for_testing()->try_again_link(),
          copy_text_button));
}

}  // namespace
}  // namespace ash
