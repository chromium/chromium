// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/action_button_view.h"

#include <string>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/system_shadow.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/layer.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

class ActionButtonViewTest : public views::ViewsTestBase {
 private:
  // Required by `ActionButtonView`.
  AshColorProvider color_provider_;
};

TEST_F(ActionButtonViewTest, ShowsIconAndLabelByDefault) {
  ActionButtonView action_button(
      views::Button::PressedCallback(), u"text", &kCaptureModeImageIcon,
      ActionButtonRank(ActionButtonType::kOther, /*weight=*/0));

  EXPECT_TRUE(action_button.image_view_for_testing()->GetVisible());
  EXPECT_TRUE(action_button.label_for_testing()->GetVisible());
}

TEST_F(ActionButtonViewTest, ShowsIconOnlyAfterCollapsed) {
  ActionButtonView action_button(
      views::Button::PressedCallback(), u"text", &kCaptureModeImageIcon,
      ActionButtonRank(ActionButtonType::kOther, /*weight=*/0));

  action_button.CollapseToIconButton();

  EXPECT_TRUE(action_button.image_view_for_testing()->GetVisible());
  EXPECT_FALSE(action_button.label_for_testing()->GetVisible());
}

TEST_F(ActionButtonViewTest, ShadowBoundsAlignedWithButton) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetBounds(gfx::Rect(0, 0, 400, 200));
  widget->Show();

  auto* action_button =
      widget->SetContentsView(std::make_unique<ActionButtonView>(
          views::Button::PressedCallback(), u"text", &kCaptureModeImageIcon,
          ActionButtonRank(ActionButtonType::kOther, /*weight=*/0)));

  const gfx::Rect button_bounds(20, 30, 120, 36);
  action_button->SetBoundsRect(button_bounds);

  SystemShadow* shadow = action_button->shadow_for_testing();
  ASSERT_TRUE(shadow);
  EXPECT_EQ(shadow->GetLayer()->parent(), action_button->layer()->parent());
  EXPECT_EQ(shadow->GetContentBounds(), button_bounds);
}

}  // namespace
}  // namespace ash
