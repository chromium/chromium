// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/boca/spotlight/spotlight_notification_bubble_controller.h"

#include <memory>

#include "ash/test/ash_test_base.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

class SpotlightNotificationBubbleControllerTest : public AshTestBase {
 protected:
  void SetUp() override {
    AshTestBase::SetUp();

    base_widget_ = CreateFramelessTestWidget();
    base_widget_->SetFullscreen(true);
    controller_ = std::make_unique<SpotlightNotificationBubbleController>();
  }

  std::unique_ptr<views::Widget> base_widget_;
  std::unique_ptr<SpotlightNotificationBubbleController> controller_;
};

TEST_F(SpotlightNotificationBubbleControllerTest, WidgetStartsHidden) {
  EXPECT_FALSE(controller_->IsNotificationBubbleVisible());
}

TEST_F(SpotlightNotificationBubbleControllerTest, CanShowWidget) {
  controller_->ShowNotificationBubble("Teacher");

  EXPECT_TRUE(controller_->IsNotificationBubbleVisible());
}

TEST_F(SpotlightNotificationBubbleControllerTest, CanHideWidget) {
  controller_->ShowNotificationBubble("Teacher");

  controller_->HideNotificationBubble();
  EXPECT_FALSE(controller_->IsNotificationBubbleVisible());
}

TEST_F(SpotlightNotificationBubbleControllerTest, WidgetClosesOnSessionEnd) {
  controller_->ShowNotificationBubble("Teacher");

  controller_->OnSessionEnded();
  EXPECT_FALSE(controller_->IsNotificationBubbleVisible());
}

TEST_F(SpotlightNotificationBubbleControllerTest,
       WidgetStartsInBottomRightCorner) {
  EXPECT_EQ(controller_->GetWidgetLocationForTesting(), WidgetLocation::kRight);
}

TEST_F(SpotlightNotificationBubbleControllerTest, WidgetMovesOnTouch) {
  controller_->ShowNotificationBubble("Teacher");
  ui::test::EventGenerator* event_generator = GetEventGenerator();

  event_generator->MoveMouseTo(controller_->GetNotificationWidgetForTesting()
                                   ->GetNativeWindow()
                                   ->GetBoundsInScreen()
                                   .CenterPoint());
  EXPECT_EQ(controller_->GetWidgetLocationForTesting(), WidgetLocation::kLeft);

  // Attempt to ensure mouse is moved out of widget before moving back in.
  event_generator->MoveMouseTo(
      base_widget_->GetNativeWindow()->GetBoundsInScreen().origin());
  event_generator->MoveMouseTo(controller_->GetNotificationWidgetForTesting()
                                   ->GetNativeWindow()
                                   ->GetBoundsInScreen()
                                   .CenterPoint());
  EXPECT_EQ(controller_->GetWidgetLocationForTesting(), WidgetLocation::kRight);
}

}  // namespace
}  // namespace ash
