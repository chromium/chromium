// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_base_bubble_view.h"
#include "ash/login/ui/login_test_base.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {
// Total width of the bubble view.
constexpr int kBubbleTotalWidthDp = 178;
}  // namespace

class LoginBaseBubbleViewTest : public LoginTestBase {
 protected:
  LoginBaseBubbleViewTest() = default;
  ~LoginBaseBubbleViewTest() override = default;

  // LoginTestBase:
  void SetUp() override {
    LoginTestBase::SetUp();

    anchor_ = new views::View();
    anchor_->SetSize(gfx::Size(0, 25));
    container_ = new views::View();
    container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical));
    container_->AddChildView(anchor_);

    SetWidget(CreateWidgetWithContent(container_));

    bubble_ = new LoginBaseBubbleView(anchor_, widget()->GetNativeView());
    auto* label = new views::Label(base::UTF8ToUTF16("A message"),
                                   views::style::CONTEXT_LABEL,
                                   views::style::STYLE_PRIMARY);
    bubble_->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical));
    bubble_->AddChildView(label);

    container_->AddChildView(bubble_);
  }

  LoginBaseBubbleView* bubble_;
  views::View* container_;
  views::View* anchor_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginBaseBubbleViewTest);
};

TEST_F(LoginBaseBubbleViewTest, BasicProperties) {
  EXPECT_FALSE(bubble_->GetVisible());

  bubble_->Show();
  EXPECT_TRUE(bubble_->GetVisible());

  EXPECT_EQ(bubble_->width(), kBubbleTotalWidthDp);
  EXPECT_EQ(bubble_->background()->get_color(), SK_ColorBLACK);

  bubble_->Hide();
  EXPECT_FALSE(bubble_->GetVisible());
}

TEST_F(LoginBaseBubbleViewTest, KeyEventHandling) {
  EXPECT_FALSE(bubble_->GetVisible());

  // Verify that a random key event won't open the bubble.
  ui::test::EventGenerator* generator = GetEventGenerator();
  container_->RequestFocus();
  generator->PressKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  EXPECT_FALSE(bubble_->GetVisible());

  // Verify that a key event will close the bubble if it is open.
  bubble_->Show();
  EXPECT_TRUE(bubble_->GetVisible());
  generator->PressKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  EXPECT_FALSE(bubble_->GetVisible());
}

TEST_F(LoginBaseBubbleViewTest, MouseEventHandling) {
  EXPECT_FALSE(bubble_->GetVisible());

  // Verify that a random mouse event won't open the bubble.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(container_->GetBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();
  EXPECT_FALSE(bubble_->GetVisible());

  // Verify that a click event on the bubble won't close it.
  bubble_->Show();
  EXPECT_TRUE(bubble_->GetVisible());
  generator->MoveMouseTo(bubble_->GetBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();
  EXPECT_TRUE(bubble_->GetVisible());

  // Verify that a click event outside the bubble will close it if it is open.
  generator->MoveMouseTo(anchor_->GetBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();
  EXPECT_FALSE(bubble_->GetVisible());
}

}  // namespace ash
