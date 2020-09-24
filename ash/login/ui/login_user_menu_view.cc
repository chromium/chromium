// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_user_menu_view.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/login/ui/system_label_button.h"
#include "ash/login/ui/views_utils.h"
#include "ash/public/cpp/login_types.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {
namespace {
constexpr char kLegacySupervisedUserManagementDisplayURL[] =
    "www.chrome.com/manage";

// Vertical margin between username and mail.
constexpr int kUserMenuVerticalMarginUsernameMailDp = 8;

// Vertical margin between labels.
constexpr int kUserMenuVerticalMarginBetweenLabelsDp = 16;

// Horizontal and vertical padding of login user menu view.
constexpr int kHorizontalPaddingLoginUserMenuViewDp = 8;
constexpr int kVerticalPaddingLoginUserMenuViewDp = 8;

constexpr int kUserMenuRemoveUserButtonIdForTest = 1;

// Font size delta from normal for the username headline.
constexpr int kUserMenuFontSizeDeltaUsername = 2;

}  // namespace

// A button that holds a child view.
class RemoveUserButton : public SystemLabelButton {
 public:
  RemoveUserButton(views::ButtonListener* listener, LoginUserMenuView* bubble)
      : SystemLabelButton(
            listener,
            l10n_util::GetStringUTF16(
                IDS_ASH_LOGIN_POD_MENU_REMOVE_ITEM_ACCESSIBLE_NAME),
            SystemLabelButton::DisplayType::DEFAULT,
            /*multiline*/ true),
        bubble_(bubble) {}

  ~RemoveUserButton() override = default;

 private:
  void OnKeyEvent(ui::KeyEvent* event) override {
    if (event->type() != ui::ET_KEY_PRESSED ||
        event->key_code() == ui::VKEY_PROCESSKEY) {
      return;
    }

    if (event->key_code() == ui::VKEY_ESCAPE ||
        event->key_code() == ui::VKEY_TAB) {
      bubble_->Hide();
      // We explicitly move focus back to the dropdown button so the Tab
      // traversal works correctly.
      bubble_->GetBubbleOpener()->RequestFocus();
    }

    if (event->key_code() == ui::VKEY_RETURN)
      views::Button::OnKeyEvent(event);
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

views::View* LoginUserMenuView::TestApi::managed_user_data() {
  return bubble_->managed_user_data_;
}

views::Label* LoginUserMenuView::TestApi::username_label() {
  return bubble_->username_label_;
}

LoginUserMenuView::LoginUserMenuView(
    const LoginUserInfo& user,
    views::View* anchor_view,
    LoginButton* bubble_opener,
    base::RepeatingClosure on_remove_user_warning_shown,
    base::RepeatingClosure on_remove_user_requested)
    : LoginBaseBubbleView(anchor_view),
      bubble_opener_(bubble_opener),
      on_remove_user_warning_shown_(on_remove_user_warning_shown),
      on_remove_user_requested_(on_remove_user_requested) {
  const base::string16& email =
      base::UTF8ToUTF16(user.basic_user_info.display_email);
  bool is_owner = user.is_device_owner;

  // User information.
  {
    const base::string16& username =
        base::UTF8ToUTF16(user.basic_user_info.display_name);
    base::string16 display_username =
        is_owner
            ? l10n_util::GetStringFUTF16(IDS_ASH_LOGIN_POD_OWNER_USER, username)
            : username;

    views::View* container =
        new NonAccessibleView("UsernameLabel MarginContainer");
    container->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(),
        kUserMenuVerticalMarginUsernameMailDp));
    AddChildView(container);
    username_label_ = login_views_utils::CreateBubbleLabel(
        display_username, gfx::kGoogleGrey200, nullptr,
        kUserMenuFontSizeDeltaUsername, gfx::Font::Weight::BOLD);
    container->AddChildView(username_label_);
    views::Label* email_label =
        login_views_utils::CreateBubbleLabel(email, gfx::kGoogleGrey500);
    container->AddChildView(email_label);
  }

  // User is managed.
  if (user.user_enterprise_domain) {
    managed_user_data_ = new views::View();
    managed_user_data_->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical));
    base::string16 managed_text = l10n_util::GetStringFUTF16(
        IDS_ASH_LOGIN_MANAGED_SESSION_MONITORING_USER_WARNING,
        base::UTF8ToUTF16(user.user_enterprise_domain.value()));
    views::Label* managed_label = login_views_utils::CreateBubbleLabel(
        managed_text, gfx::kGoogleGrey200, this);
    managed_user_data_->AddChildView(managed_label);
    AddChildView(managed_user_data_);
  }

  // Remove user.
  if (user.can_remove) {
    DCHECK(!is_owner);
    user_manager::UserType type = user.basic_user_info.type;
    base::string16 part1 = l10n_util::GetStringUTF16(
        IDS_ASH_LOGIN_POD_NON_OWNER_USER_REMOVE_WARNING_PART_1);
    if (type == user_manager::UserType::USER_TYPE_SUPERVISED) {
      part1 = l10n_util::GetStringFUTF16(
          IDS_ASH_LOGIN_POD_LEGACY_SUPERVISED_USER_REMOVE_WARNING,
          base::UTF8ToUTF16(kLegacySupervisedUserManagementDisplayURL));
    }
    base::string16 part2 = l10n_util::GetStringFUTF16(
        type == user_manager::UserType::USER_TYPE_CHILD
            ? IDS_ASH_LOGIN_POD_NON_OWNER_USER_REMOVE_WARNING_PART_2_SUPERVISED_USER
            : IDS_ASH_LOGIN_POD_NON_OWNER_USER_REMOVE_WARNING_PART_2,
        email);
    warning_message_ = base::StrCat({part1, base::ASCIIToUTF16(" "), part2});

    remove_user_confirm_data_ = new views::View();
    remove_user_confirm_data_->SetLayoutManager(
        std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kVertical, gfx::Insets(),
            kUserMenuVerticalMarginBetweenLabelsDp));
    AddChildView(remove_user_confirm_data_);
    remove_user_confirm_data_->SetVisible(false);

    remove_user_confirm_data_->AddChildView(
        login_views_utils::CreateBubbleLabel(part1, gfx::kGoogleGrey200, this));

    remove_user_confirm_data_->AddChildView(
        login_views_utils::CreateBubbleLabel(part2, gfx::kGoogleGrey200, this));

    remove_user_button_ = new RemoveUserButton(this, this);
    remove_user_button_->SetID(kUserMenuRemoveUserButtonIdForTest);
    AddChildView(remove_user_button_);
  }
}

LoginUserMenuView::~LoginUserMenuView() = default;

void LoginUserMenuView::ResetState() {
  if (managed_user_data_)
    managed_user_data_->SetVisible(true);
  if (remove_user_confirm_data_) {
    remove_user_confirm_data_->SetVisible(false);
    remove_user_button_->SetDisplayType(
        SystemLabelButton::DisplayType::DEFAULT);
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
    if (managed_user_data_)
      managed_user_data_->SetVisible(false);
    remove_user_button_->SetDisplayType(
        SystemLabelButton::DisplayType::ALERT_NO_ICON);

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
  return CalculatePositionUsingDefaultStrategy(
      PositioningStrategy::kShowOnRightSideOrLeftSide,
      kHorizontalPaddingLoginUserMenuViewDp,
      kVerticalPaddingLoginUserMenuViewDp);
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
