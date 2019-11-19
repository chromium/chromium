// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/login/ui/login_user_menu_view.h"
#include "ash/login/ui/login_button.h"
#include "ash/login/ui/login_test_base.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "components/user_manager/user_type.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/animation/test/ink_drop_host_view_test_api.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {
constexpr int kBubbleAnchorViewSizeDp = 100;
}  // namespace

using LoginUserMenuViewTest = LoginTestBase;

TEST_F(LoginUserMenuViewTest, RemoveUserRequiresTwoActivations) {
  auto* anchor = new views::View;
  anchor->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  SetWidget(CreateWidgetWithContent(anchor));

  bool remove_warning_called = false;
  bool remove_called = false;

  auto* bubble = new LoginUserMenuView(
      base::string16(), base::string16(), user_manager::USER_TYPE_REGULAR,
      false /*is_owner*/, anchor, nullptr /*bubble_opener*/,
      true /*show_remove_user*/,
      base::BindRepeating([](bool* warning_called) { *warning_called = true; },
                          &remove_warning_called),
      base::BindRepeating([](bool* remove_called) { *remove_called = true; },
                          &remove_called));
  anchor->AddChildView(bubble);

  bubble->Show();

  EXPECT_TRUE(bubble->GetVisible());

  // Focus the remove user button (the menu should forward focus to the remove
  // button).
  bubble->RequestFocus();

  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_RETURN, 0);

  // First click calls remove warning only
  EXPECT_TRUE(remove_warning_called);
  EXPECT_FALSE(remove_called);
  remove_warning_called = false;

  // Second click calls remove only
  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_RETURN, 0);
  EXPECT_FALSE(remove_warning_called);
  EXPECT_TRUE(remove_called);
}

TEST_F(LoginUserMenuViewTest, LongUserNameAndEmailLaidOutCorrectly) {
  auto* anchor = new views::View;
  anchor->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  SetWidget(CreateWidgetWithContent(anchor));

  auto* bubble = new LoginUserMenuView(
      base::UTF8ToUTF16("NedHasAReallyLongName StarkHasAReallyLongName"),
      base::UTF8ToUTF16("reallyreallyextralonggaianame@gmail.com"),
      user_manager::USER_TYPE_REGULAR, false /*is_owner*/, anchor,
      nullptr /*bubble_opener*/, true /*show_remove_user*/, base::DoNothing(),
      base::DoNothing());

  anchor->AddChildView(bubble);
  bubble->Show();

  EXPECT_TRUE(bubble->GetVisible());

  LoginUserMenuView::TestApi test_api(bubble);
  views::View* remove_user_button = test_api.remove_user_button();
  views::View* remove_user_confirm_data = test_api.remove_user_confirm_data();
  views::View* username_label = test_api.username_label();

  EXPECT_TRUE(bubble->GetBoundsInScreen().Contains(
      remove_user_button->GetBoundsInScreen()));
  EXPECT_FALSE(remove_user_confirm_data->GetVisible());
  EXPECT_TRUE(username_label->GetVisible());

  bubble->RequestFocus();
  EXPECT_TRUE(remove_user_button->HasFocus());

  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_RETURN, 0);
  EXPECT_TRUE(username_label->GetVisible());
  EXPECT_TRUE(remove_user_confirm_data->GetVisible());
  EXPECT_TRUE(remove_user_button->GetBoundsInScreen().y() >=
              remove_user_confirm_data->GetBoundsInScreen().y() +
                  remove_user_confirm_data->GetBoundsInScreen().height());
  EXPECT_TRUE(bubble->GetBoundsInScreen().Contains(
      remove_user_button->GetBoundsInScreen()));
}

TEST_F(LoginUserMenuViewTest, LoginButtonRipple) {
  auto* container = new views::View();
  container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  LoginButton* bubble_opener = new LoginButton(nullptr /*listener*/);
  bubble_opener->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  bubble_opener->SetPreferredSize(
      gfx::Size(kBubbleAnchorViewSizeDp, kBubbleAnchorViewSizeDp));

  container->AddChildView(bubble_opener);
  SetWidget(CreateWidgetWithContent(container));

  views::test::InkDropHostViewTestApi ink_drop_api(bubble_opener);
  EXPECT_EQ(ink_drop_api.ink_drop_mode(),
            views::InkDropHostView::InkDropMode::ON);
  EXPECT_TRUE(ink_drop_api.HasInkDrop());

  auto* bubble = new LoginUserMenuView(
      base::string16(), base::string16(), user_manager::USER_TYPE_REGULAR,
      false /*is_owner*/, container /*anchor*/, bubble_opener,
      true /*show_remove_user*/, base::DoNothing(), base::DoNothing());

  container->AddChildView(bubble);

  bubble->Show();
  EXPECT_TRUE(bubble->GetVisible());
  EXPECT_TRUE(ink_drop_api.HasInkDrop());
  EXPECT_EQ(ink_drop_api.GetInkDrop()->GetTargetInkDropState(),
            views::InkDropState::ACTIVATED);
  EXPECT_TRUE(ink_drop_api.GetInkDrop()->IsHighlightFadingInOrVisible());

  bubble->Hide();
  EXPECT_FALSE(bubble->GetVisible());
  EXPECT_EQ(ink_drop_api.GetInkDrop()->GetTargetInkDropState(),
            views::InkDropState::HIDDEN);
  EXPECT_FALSE(ink_drop_api.GetInkDrop()->IsHighlightFadingInOrVisible());
}

TEST_F(LoginUserMenuViewTest, ResetStateHidesConfirmData) {
  auto* container = new views::View;
  container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  SetWidget(CreateWidgetWithContent(container));

  auto* bubble = new LoginUserMenuView(
      base::string16(), base::string16(), user_manager::USER_TYPE_REGULAR,
      false /*is_owner*/, nullptr /*anchor*/, nullptr /*bubble_opener*/,
      true /*show_remove_user*/, base::DoNothing(), base::DoNothing());
  container->AddChildView(bubble);

  bubble->Show();

  LoginUserMenuView::TestApi test_api(bubble);
  EXPECT_FALSE(test_api.remove_user_confirm_data()->GetVisible());

  test_api.remove_user_button()->RequestFocus();
  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_RETURN, 0);
  EXPECT_TRUE(test_api.remove_user_confirm_data()->GetVisible());

  bubble->ResetState();
  EXPECT_FALSE(test_api.remove_user_confirm_data()->GetVisible());
}

}  // namespace ash
