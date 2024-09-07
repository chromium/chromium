// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_remove_account_dialog.h"

#include <memory>

#include "ash/login/ui/login_button.h"
#include "ash/login/ui/login_test_base.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/user_manager/user_type.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/test/ink_drop_host_test_api.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

constexpr int kBubbleAnchorViewSizeDp = 100;

class AnchorView final : public views::View {
 public:
  base::WeakPtr<AnchorView> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  METADATA_HEADER(AnchorView, views::View)
  base::WeakPtrFactory<AnchorView> weak_ptr_factory_{this};
};

BEGIN_METADATA(AnchorView)
END_METADATA

}  // namespace

using LoginRemoveAccountDialogTest = LoginTestBase;

TEST_F(LoginRemoveAccountDialogTest, RemoveUserRequiresTwoActivations) {
  auto* anchor = new AnchorView();
  anchor->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  SetWidget(CreateWidgetWithContent(anchor));

  bool remove_warning_called = false;
  bool remove_called = false;

  LoginUserInfo login_user_info;
  login_user_info.can_remove = true;
  auto* bubble = new LoginRemoveAccountDialog(
      login_user_info, anchor->AsWeakPtr(), nullptr /*bubble_opener*/,
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

TEST_F(LoginRemoveAccountDialogTest, AccessibleProperties) {
  auto* anchor = new AnchorView();
  anchor->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  SetWidget(CreateWidgetWithContent(anchor));

  bool remove_warning_called = false;
  bool remove_called = false;

  LoginUserInfo login_user_info;
  login_user_info.basic_user_info.display_name =
      "NedHasAReallyLongName StarkHasAReallyLongName";
  login_user_info.basic_user_info.display_email =
      "reallyreallyextralonggaianame@gmail.com";
  login_user_info.can_remove = true;

  std::unique_ptr<LoginRemoveAccountDialog> remove_view =
      std::make_unique<LoginRemoveAccountDialog>(
          login_user_info, anchor->AsWeakPtr(), nullptr /*bubble_opener*/,
          base::BindRepeating(
              [](bool* warning_called) { *warning_called = true; },
              &remove_warning_called),
          base::BindRepeating(
              [](bool* remove_called) { *remove_called = true; },
              &remove_called));

  ui::AXNodeData data;
  remove_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kDialog);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            l10n_util ::GetStringUTF16(
                IDS_ASH_LOGIN_POD_REMOVE_ACCOUNT_ACCESSIBLE_NAME));

  EXPECT_EQ(
      remove_view->GetViewAccessibility().GetCachedDescription(),
      l10n_util::GetStringUTF16(
          IDS_ASH_LOGIN_POD_REMOVE_ACCOUNT_DIALOG_ACCESSIBLE_DESCRIPTION));

  login_user_info.can_remove = false;
  std::unique_ptr<LoginRemoveAccountDialog> non_remove_view =
      std::make_unique<LoginRemoveAccountDialog>(
          login_user_info, anchor->AsWeakPtr(), nullptr /*bubble_opener*/,
          base::BindRepeating(
              [](bool* warning_called) { *warning_called = true; },
              &remove_warning_called),
          base::BindRepeating(
              [](bool* remove_called) { *remove_called = true; },
              &remove_called));

  data = ui::AXNodeData();
  non_remove_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            u"NedHasAReallyLongName StarkHasAReallyLongName");
  EXPECT_EQ(non_remove_view->GetViewAccessibility().GetCachedDescription(),
            u"reallyreallyextralonggaianame@gmail.com");

  login_user_info.user_account_manager = "manager";
  std::unique_ptr<LoginRemoveAccountDialog> manager_view =
      std::make_unique<LoginRemoveAccountDialog>(
          login_user_info, anchor->AsWeakPtr(), nullptr /*bubble_opener*/,
          base::BindRepeating(
              [](bool* warning_called) { *warning_called = true; },
              &remove_warning_called),
          base::BindRepeating(
              [](bool* remove_called) { *remove_called = true; },
              &remove_called));

  EXPECT_EQ(manager_view->GetViewAccessibility().GetCachedDescription(),
            u"reallyreallyextralonggaianame@gmail.com " +
                l10n_util::GetStringFUTF16(
                    IDS_ASH_LOGIN_MANAGED_SESSION_MONITORING_USER_WARNING,
                    u"manager"));
}

TEST_F(LoginRemoveAccountDialogTest, LongUserNameAndEmailLaidOutCorrectly) {
  auto* anchor = new AnchorView();
  anchor->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  SetWidget(CreateWidgetWithContent(anchor));

  LoginUserInfo login_user_info;
  login_user_info.basic_user_info.display_name =
      "NedHasAReallyLongName StarkHasAReallyLongName";
  login_user_info.basic_user_info.display_email =
      "reallyreallyextralonggaianame@gmail.com";
  login_user_info.basic_user_info.type = user_manager::UserType::kRegular;
  login_user_info.is_device_owner = false;
  login_user_info.can_remove = true;
  auto* bubble = new LoginRemoveAccountDialog(
      login_user_info, anchor->AsWeakPtr(), nullptr /*bubble_opener*/,
      base::DoNothing(), base::DoNothing());

  anchor->AddChildView(bubble);
  bubble->Show();

  EXPECT_TRUE(bubble->GetVisible());

  LoginRemoveAccountDialog::TestApi test_api(bubble);
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

TEST_F(LoginRemoveAccountDialogTest, LoginButtonRipple) {
  auto* container = new AnchorView();
  container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  LoginButton* bubble_opener =
      new LoginButton(views::Button::PressedCallback());
  bubble_opener->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  bubble_opener->SetPreferredSize(
      gfx::Size(kBubbleAnchorViewSizeDp, kBubbleAnchorViewSizeDp));

  container->AddChildView(bubble_opener);
  SetWidget(CreateWidgetWithContent(container));

  views::test::InkDropHostTestApi ink_drop_api(
      views::InkDrop::Get(bubble_opener));
  EXPECT_EQ(ink_drop_api.ink_drop_mode(), views::InkDropHost::InkDropMode::ON);
  EXPECT_TRUE(ink_drop_api.HasInkDrop());

  auto* bubble = new LoginRemoveAccountDialog(
      LoginUserInfo(), container->AsWeakPtr() /*anchor*/, bubble_opener,
      base::DoNothing(), base::DoNothing());

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

TEST_F(LoginRemoveAccountDialogTest, ResetStateHidesConfirmData) {
  auto* container = new views::View;
  container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  SetWidget(CreateWidgetWithContent(container));

  LoginUserInfo login_user_info;
  login_user_info.can_remove = true;
  auto* bubble = new LoginRemoveAccountDialog(
      login_user_info, nullptr /*anchor*/, nullptr /*bubble_opener*/,
      base::DoNothing(), base::DoNothing());
  container->AddChildView(bubble);

  bubble->Show();

  LoginRemoveAccountDialog::TestApi test_api(bubble);
  EXPECT_FALSE(test_api.remove_user_confirm_data()->GetVisible());

  test_api.remove_user_button()->RequestFocus();
  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_RETURN, 0);
  EXPECT_TRUE(test_api.remove_user_confirm_data()->GetVisible());
}

TEST_F(LoginRemoveAccountDialogTest, AccessibleRole) {
  auto* container = new views::View;
  container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  SetWidget(CreateWidgetWithContent(container));

  LoginUserInfo login_user_info;
  login_user_info.can_remove = true;
  auto* dialog = new LoginRemoveAccountDialog(
      login_user_info, nullptr /*anchor*/, nullptr /*bubble_opener*/,
      base::DoNothing(), base::DoNothing());
  container->AddChildView(dialog);
  ui::AXNodeData data;

  dialog->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kDialog);
}

}  // namespace ash
