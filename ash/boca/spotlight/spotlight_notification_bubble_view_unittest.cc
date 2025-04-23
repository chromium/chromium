// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/boca/spotlight/spotlight_notification_bubble_view.h"

#include <memory>

#include "ash/test/ash_test_base.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

class SpotlightNotificationBubbleViewTest : public AshTestBase {
 protected:
  void SetUp() override {
    AshTestBase::SetUp();

    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
    auto notification_bubble_view =
        std::make_unique<SpotlightNotificationBubbleView>("Teacher");
    notification_bubble_view_ =
        widget_->SetContentsView(std::move(notification_bubble_view));
  }

  std::unique_ptr<views::Widget> widget_;
  raw_ptr<SpotlightNotificationBubbleView> notification_bubble_view_;
};

TEST_F(SpotlightNotificationBubbleViewTest, OrientationAndAlignment) {
  EXPECT_EQ(notification_bubble_view_->GetOrientation(),
            views::BoxLayout::Orientation::kHorizontal);
  EXPECT_EQ(notification_bubble_view_->GetMainAxisAlignment(),
            views::BoxLayout::MainAxisAlignment::kCenter);
  EXPECT_EQ(notification_bubble_view_->GetCrossAxisAlignment(),
            views::BoxLayout::CrossAxisAlignment::kCenter);
}

TEST_F(SpotlightNotificationBubbleViewTest, HasVisibilityIcon) {
  EXPECT_TRUE(notification_bubble_view_->get_visibility_icon());
}

TEST_F(SpotlightNotificationBubbleViewTest, HasLabel) {
  EXPECT_TRUE(notification_bubble_view_->get_notification_label());

  EXPECT_EQ(notification_bubble_view_->get_notification_label()->GetText(),
            u"Teacher is viewing your screen");
}
}  // namespace
}  // namespace ash
