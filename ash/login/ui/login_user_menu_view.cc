// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_user_menu_view.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/login/ui/views_utils.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace {
constexpr char kLegacySupervisedUserManagementDisplayURL[] =
    "www.chrome.com/manage";

// Spacing between the child view inside the bubble view.
constexpr int kBubbleBetweenChildSpacingDp = 6;

// An alpha value for the sub message in the user menu.
constexpr SkAlpha kSubMessageColorAlpha = 0x89;

// Color of the "Remove user" text.
constexpr SkColor kRemoveUserInitialColor = gfx::kGoogleBlueDark400;
constexpr SkColor kRemoveUserConfirmColor = gfx::kGoogleRedDark500;

// Margin/inset of the entries for the user menu.
constexpr int kUserMenuMarginWidth = 14;
constexpr int kUserMenuMarginHeight = 16;
// Distance above/below the separator.
constexpr int kUserMenuMarginAroundSeparatorDp = 16;
// Distance between labels.
constexpr int kUserMenuVerticalDistanceBetweenLabelsDp = 16;
// Margin around remove user button.
constexpr int kUserMenuMarginAroundRemoveUserButtonDp = 4;

// Vertical spacing between the anchor view and user menu.
constexpr int kAnchorViewUserMenuVerticalSpacingDp = 4;

constexpr int kUserMenuRemoveUserButtonIdForTest = 1;
}  // namespace

namespace ash {

// A button that holds a child view.
class RemoveUserButton : public views::Button {
 public:
  RemoveUserButton(views::ButtonListener* listener,
                   views::View* content,
                   LoginUserMenuView* bubble)
      : views::Button(listener), bubble_(bubble) {
    SetLayoutManager(std::make_unique<views::FillLayout>());
    AddChildView(content);

    // Increase the size of the button so that the focus is not rendered next to
    // the text.
    SetBorder(views::CreateEmptyBorder(
        gfx::Insets(kUserMenuMarginAroundRemoveUserButtonDp,
                    kUserMenuMarginAroundRemoveUserButtonDp)));
    SetInstallFocusRingOnFocus(true);
    focus_ring()->SetColor(ShelfConfig::Get()->shelf_focus_border_color());
  }

  ~RemoveUserButton() override = default;

 private:
  void OnKeyEvent(ui::KeyEvent* event) override {
    if (event->type() != ui::ET_KEY_PRESSED ||
        event->key_code() == ui::VKEY_PROCESSKEY) {
      return;
    }

    if (event->key_code() != ui::VKEY_RETURN) {
      // The remove-user button should handle bubble dismissal and stop
      // propagation, otherwise the event will propagate to the bubble widget,
      // which will close itself and invalidate the bubble pointer in
      // LoginUserMenuView.
      event->StopPropagation();
      bubble_->Hide();
    } else {
      views::Button::OnKeyEvent(event);
    }
  }

  LoginUserMenuView* bubble_;

  DISALLOW_COPY_AND_ASSIGN(RemoveUserButton);
};

LoginUserMenuView::TestApi::TestApi(LoginUserMenuView* bubble)
    : bubble_(bubble) {}

views::View* LoginUserMenuView::TestApi::remove_user_button() {
  return bubble_->remove_user_button_;
}

views::View* LoginUserMenuView::TestApi::remove_user_confirm_data() {
  return bubble_->remove_user_confirm_data_;
}

views::Label* LoginUserMenuView::TestApi::username_label() {
  return bubble_->username_label_;
}

LoginUserMenuView::LoginUserMenuView(
    const base::string16& username,
    const base::string16& email,
    user_manager::UserType type,
    bool is_owner,
    views::View* anchor_view,
    LoginButton* bubble_opener,
    bool show_remove_user,
    base::RepeatingClosure on_remove_user_warning_shown,
    base::RepeatingClosure on_remove_user_requested)
    : LoginBaseBubbleView(anchor_view),
      bubble_opener_(bubble_opener),
      on_remove_user_warning_shown_(on_remove_user_warning_shown),
      on_remove_user_requested_(on_remove_user_requested) {
  // LoginUserMenuView does not use the parent margins. Further, because the
  // splitter spans the entire view set_margins cannot be used.
  // The bottom margin is less the margin around the remove user button, which
  // is always visible.
  gfx::Insets margins(
      kUserMenuMarginHeight, kUserMenuMarginWidth,
      kUserMenuMarginHeight - kUserMenuMarginAroundRemoveUserButtonDp,
      kUserMenuMarginWidth);
  auto setup_horizontal_margin_container = [&](views::View* container) {
    container->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical,
        gfx::Insets(0, margins.left(), 0, margins.right())));
    AddChildView(container);
    return container;
  };

  // Add vertical whitespace.
  auto add_space = [](views::View* root, int amount) {
    auto* spacer = new NonAccessibleView("Whitespace");
    spacer->SetPreferredSize(gfx::Size(1, amount));
    root->AddChildView(spacer);
  };

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets(margins.top(), 0, margins.bottom(), 0)));

  // User information.
  {
    base::string16 display_username =
        is_owner
            ? l10n_util::GetStringFUTF16(IDS_ASH_LOGIN_POD_OWNER_USER, username)
            : username;

    views::View* container = setup_horizontal_margin_container(
        new NonAccessibleView("UsernameLabel MarginContainer"));
    username_label_ =
        login_views_utils::CreateBubbleLabel(display_username, SK_ColorWHITE);
    container->AddChildView(username_label_);
    add_space(container, kBubbleBetweenChildSpacingDp);
    views::Label* email_label = login_views_utils::CreateBubbleLabel(
        email, SkColorSetA(SK_ColorWHITE, kSubMessageColorAlpha));
    container->AddChildView(email_label);
  }

  // Remove user.
  if (show_remove_user) {
    DCHECK(!is_owner);

    // Add separator.
    add_space(this, kUserMenuMarginAroundSeparatorDp);
    auto* separator = new views::Separator();
    separator->SetColor(SkColorSetA(SK_ColorWHITE, 0x2B));
    AddChildView(separator);
    // The space below the separator is less the margin around remove user;
    // this is readded if showing confirmation.
    add_space(this, kUserMenuMarginAroundSeparatorDp -
                        kUserMenuMarginAroundRemoveUserButtonDp);

    auto make_label = [this](const base::string16& text) {
      views::Label* label =
          login_views_utils::CreateBubbleLabel(text, SK_ColorWHITE);
      label->SetMultiLine(true);
      label->SetAllowCharacterBreak(true);
      // Make sure to set a maximum label width, otherwise text wrapping will
      // significantly increase width and layout may not work correctly if
      // the input string is very long.
      label->SetMaximumWidth(GetPreferredSize().width());
      return label;
    };

    base::string16 part1 = l10n_util::GetStringUTF16(
        IDS_ASH_LOGIN_POD_NON_OWNER_USER_REMOVE_WARNING_PART_1);
    if (type == user_manager::UserType::USER_TYPE_SUPERVISED) {
      part1 = l10n_util::GetStringFUTF16(
          IDS_ASH_LOGIN_POD_LEGACY_SUPERVISED_USER_REMOVE_WARNING,
          base::UTF8ToUTF16(kLegacySupervisedUserManagementDisplayURL));
    }
    base::string16 part2 = l10n_util::GetStringFUTF16(
        IDS_ASH_LOGIN_POD_NON_OWNER_USER_REMOVE_WARNING_PART_2, email);

    warning_message_ = part1 + base::ASCIIToUTF16(" ") + part2;

    remove_user_confirm_data_ =
        setup_horizontal_margin_container(new views::View());
    remove_user_confirm_data_->SetVisible(false);

    // Account for margin that was removed below the separator for the add
    // user button.
    add_space(remove_user_confirm_data_,
              kUserMenuMarginAroundRemoveUserButtonDp);
    remove_user_confirm_data_->AddChildView(make_label(part1));
    add_space(remove_user_confirm_data_,
              kUserMenuVerticalDistanceBetweenLabelsDp);
    remove_user_confirm_data_->AddChildView(make_label(part2));
    // Reduce margin since the remove user button comes next.
    add_space(remove_user_confirm_data_,
              kUserMenuVerticalDistanceBetweenLabelsDp -
                  kUserMenuMarginAroundRemoveUserButtonDp);

    auto* container = setup_horizontal_margin_container(
        new NonAccessibleView("RemoveUserButton MarginContainer"));
    remove_user_label_ = login_views_utils::CreateBubbleLabel(
        l10n_util::GetStringUTF16(
            IDS_ASH_LOGIN_POD_MENU_REMOVE_ITEM_ACCESSIBLE_NAME),
        kRemoveUserInitialColor);
    remove_user_button_ = new RemoveUserButton(this, remove_user_label_, this);
    remove_user_button_->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
    remove_user_button_->SetID(kUserMenuRemoveUserButtonIdForTest);
    remove_user_button_->SetAccessibleName(remove_user_label_->GetText());
    container->AddChildView(remove_user_button_);
  }
}

LoginUserMenuView::~LoginUserMenuView() = default;

void LoginUserMenuView::ResetState() {
  if (remove_user_confirm_data_) {
    remove_user_confirm_data_->SetVisible(false);
    remove_user_label_->SetEnabledColor(kRemoveUserInitialColor);
    // Reset button's description to none.
    remove_user_button_->GetViewAccessibility().OverrideDescription(
        base::string16());
  }
}

LoginButton* LoginUserMenuView::GetBubbleOpener() const {
  return bubble_opener_;
}

void LoginUserMenuView::ButtonPressed(views::Button* sender,
                                      const ui::Event& event) {
  // Show confirmation warning. The user has to click the button again before
  // we actually allow the exit.
  if (!remove_user_confirm_data_->GetVisible()) {
    remove_user_confirm_data_->SetVisible(true);
    remove_user_label_->SetEnabledColor(kRemoveUserConfirmColor);

    Layout();

    // Change the node's description to force assistive technologies, like
    // ChromeVox, to report the updated description.
    remove_user_button_->GetViewAccessibility().OverrideDescription(
        warning_message_);
    if (on_remove_user_warning_shown_)
      std::move(on_remove_user_warning_shown_).Run();
    return;
  }

  // Immediately hide the bubble with no animation before running the remove
  // user callback. If an animation is triggered while the the views hierarchy
  // for this bubble is being torn down, we can get a crash.
  SetVisible(false);

  if (on_remove_user_requested_)
    std::move(on_remove_user_requested_).Run();
}

gfx::Point LoginUserMenuView::CalculatePosition() {
  gfx::Point position = LoginBaseBubbleView::CalculatePosition();

  if (GetAnchorView())
    position.set_y(position.y() + kAnchorViewUserMenuVerticalSpacingDp);

  gfx::Rect screen_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();

  // In handling the cases where the bubble could go off screen, we assume that
  // the bubble can go either off the right side or off the bottom side.
  if (position.x() + width() > screen_bounds.right() && GetAnchorView()) {
    // If bubble would go off the right side of the screen, go left instead
    position.set_x(position.x() + GetAnchorView()->width() - width());
  } else if (position.y() + height() > screen_bounds.bottom() &&
             GetAnchorView()) {
    // If bubble would go off the bottom of the screen, go to the right of the
    // anchor and upward.
    position.set_x(position.x() + kAnchorViewUserMenuVerticalSpacingDp +
                   GetAnchorView()->width());
    position.set_y(position.y() +
                   (screen_bounds.bottom() - (position.y() + height())));
  }

  return position;
}

void LoginUserMenuView::RequestFocus() {
  // This view has no actual interesting contents to focus, so immediately
  // forward to the button.
  if (remove_user_button_)
    remove_user_button_->RequestFocus();
}

bool LoginUserMenuView::HasFocus() const {
  return remove_user_button_ && remove_user_button_->HasFocus();
}

const char* LoginUserMenuView::GetClassName() const {
  return "LoginUserMenuView";
}
}  // namespace ash
