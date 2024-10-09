// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_remove_account_dialog.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/login/ui/views_utils.h"
#include "ash/public/cpp/login_types.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/pill_button.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {
namespace {
// Vertical margin between username and mail.
constexpr int kVerticalMarginUsernameMailDp = 8;

// Vertical margin between labels.
constexpr int kVerticalMarginBetweenLabelsDp = 16;

// Horizontal and vertical padding of the remove account dialog.
constexpr int kHorizontalPaddingRemoveAccountDialogDp = 8;
constexpr int kVerticalPaddingRemoveAccountDialogDp = 8;

constexpr int kRemoveUserButtonIdForTest = 1;

// Font size of the username headline.
constexpr int kFontSizeUsername = 15;

// Line height of the username headline.
constexpr int kLineHeightUsername = 22;

// Traps the focus so it does not move from the |trapped_focus| view.
class TrappedFocusSearch : public views::FocusSearch {
 public:
  explicit TrappedFocusSearch(views::View* trapped_focus)
      : FocusSearch(trapped_focus->parent(), true, true),
        trapped_focus_(trapped_focus) {}
  TrappedFocusSearch(const TrappedFocusSearch&) = delete;
  TrappedFocusSearch& operator=(const TrappedFocusSearch&) = delete;
  ~TrappedFocusSearch() override = default;

  // views::FocusSearch:
  views::View* FindNextFocusableView(
      views::View* starting_view,
      views::FocusSearch::SearchDirection search_direction,
      views::FocusSearch::TraversalDirection traversal_direction,
      views::FocusSearch::StartingViewPolicy check_starting_view,
      views::FocusSearch::AnchoredDialogPolicy can_go_into_anchored_dialog,
      views::FocusTraversable** focus_traversable,
      views::View** focus_traversable_view) override {
    return trapped_focus_;
  }

 private:
  const raw_ptr<views::View> trapped_focus_;
};

}  // namespace

// A system label button that dismisses its bubble dialog parent on key event.
class RemoveUserButton : public PillButton {
  METADATA_HEADER(RemoveUserButton, PillButton)

 public:
  RemoveUserButton(PressedCallback callback, LoginRemoveAccountDialog* bubble)
      : PillButton(std::move(callback),
                   l10n_util::GetStringUTF16(
                       IDS_ASH_LOGIN_POD_REMOVE_ACCOUNT_ACCESSIBLE_NAME)),
        bubble_(bubble) {}

  RemoveUserButton(const RemoveUserButton&) = delete;
  RemoveUserButton& operator=(const RemoveUserButton&) = delete;
  ~RemoveUserButton() override = default;

  void SetAlert(bool alert) {
    SetPillButtonType(alert ? PillButton::Type::kAlertWithoutIcon
                            : PillButton::Type::kDefaultWithoutIcon);
  }

 private:
  void OnKeyEvent(ui::KeyEvent* event) override {
    if (event->type() != ui::EventType::kKeyPressed ||
        event->key_code() == ui::VKEY_PROCESSKEY) {
      return;
    }

    if (event->key_code() == ui::VKEY_ESCAPE) {
      bubble_->Hide();
      // We explicitly move focus back to the dropdown button so the Tab
      // traversal works correctly.
      bubble_->GetBubbleOpener()->RequestFocus();
    }

    if (event->key_code() == ui::VKEY_RETURN) {
      views::Button::OnKeyEvent(event);
    }
  }

  raw_ptr<LoginRemoveAccountDialog> bubble_;
};

BEGIN_METADATA(RemoveUserButton)
END_METADATA

LoginRemoveAccountDialog::TestApi::TestApi(LoginRemoveAccountDialog* bubble)
    : bubble_(bubble) {}

views::View* LoginRemoveAccountDialog::TestApi::remove_user_button() {
  return bubble_->remove_user_button_;
}

views::View* LoginRemoveAccountDialog::TestApi::remove_user_confirm_data() {
  return bubble_->remove_user_confirm_data_;
}

views::Label* LoginRemoveAccountDialog::TestApi::username_label() {
  return bubble_->username_label_;
}

views::Label* LoginRemoveAccountDialog::TestApi::management_disclosure_label() {
  return bubble_->management_disclosure_label_;
}

LoginRemoveAccountDialog::LoginRemoveAccountDialog(
    const LoginUserInfo& user,
    base::WeakPtr<views::View> anchor_view,
    LoginButton* bubble_opener,
    base::RepeatingClosure on_remove_user_warning_shown,
    base::RepeatingClosure on_remove_user_requested)
    : LoginBaseBubbleView(std::move(anchor_view)),
      bubble_opener_(bubble_opener),
      on_remove_user_warning_shown_(on_remove_user_warning_shown),
      on_remove_user_requested_(on_remove_user_requested) {
  const std::u16string& email =
      base::UTF8ToUTF16(user.basic_user_info.display_email);
  bool is_owner = user.is_device_owner;

  // Add texts with user information such as username, email and ownership.
  {
    const std::u16string& username =
        base::UTF8ToUTF16(user.basic_user_info.display_name);
    std::u16string display_username =
        is_owner
            ? l10n_util::GetStringFUTF16(IDS_ASH_LOGIN_POD_OWNER_USER, username)
            : username;

    views::View* container =
        new NonAccessibleView("UsernameLabel MarginContainer");
    container->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(),
        kVerticalMarginUsernameMailDp));
    AddChildView(container);
    const bool is_jelly = chromeos::features::IsJellyrollEnabled();
    username_label_ =
        container->AddChildView(login_views_utils::CreateThemedBubbleLabel(
            display_username, nullptr,
            is_jelly ? static_cast<ui::ColorId>(cros_tokens::kCrosSysOnSurface)
                     : kColorAshTextColorPrimary,
            gfx::FontList({login_views_utils::kGoogleSansFont},
                          gfx::Font::FontStyle::NORMAL, kFontSizeUsername,
                          gfx::Font::Weight::MEDIUM),
            kLineHeightUsername));
    email_label_ =
        container->AddChildView(login_views_utils::CreateThemedBubbleLabel(
            email, nullptr,
            is_jelly ? static_cast<ui::ColorId>(cros_tokens::kCrosSysSecondary)
                     : kColorAshTextColorSecondary));
  }

  // Add a warning text if the user is managed.
  if (user.user_account_manager) {
    std::u16string managed_text = l10n_util::GetStringFUTF16(
        IDS_ASH_LOGIN_MANAGED_SESSION_MONITORING_USER_WARNING,
        base::UTF8ToUTF16(user.user_account_manager.value()));
    management_disclosure_label_ =
        AddChildView(login_views_utils::CreateThemedBubbleLabel(
            managed_text, this, kColorAshTextColorPrimary));
  }

  // If we can remove the user, the focus will be trapped by the bubble, and
  // button. If we can't, and there is no button, we set this so that the bubble
  // accessible data is displayed by accessibility tools.
  set_notify_alert_on_show(!user.can_remove);

  // Add the remove user button and texts about consequences of user removal.
  if (user.can_remove) {
    DCHECK(!is_owner);
    user_manager::UserType type = user.basic_user_info.type;
    std::u16string part1 = l10n_util::GetStringUTF16(
        IDS_ASH_LOGIN_POD_NON_OWNER_USER_REMOVE_WARNING_PART_1);
    std::u16string part2 = l10n_util::GetStringFUTF16(
        type == user_manager::UserType::kChild
            ? IDS_ASH_LOGIN_POD_NON_OWNER_USER_REMOVE_WARNING_PART_2_SUPERVISED_USER
            : IDS_ASH_LOGIN_POD_NON_OWNER_USER_REMOVE_WARNING_PART_2,
        email);
    warning_message_ = base::StrCat({part1, u" ", part2});

    remove_user_confirm_data_ = AddChildView(std::make_unique<views::View>());
    remove_user_confirm_data_->SetLayoutManager(
        std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kVertical, gfx::Insets(),
            kVerticalMarginBetweenLabelsDp));
    remove_user_confirm_data_->SetVisible(false);

    remove_user_confirm_data_->AddChildView(
        login_views_utils::CreateThemedBubbleLabel(part1, this,
                                                   kColorAshTextColorPrimary));

    remove_user_confirm_data_->AddChildView(
        login_views_utils::CreateThemedBubbleLabel(part2, this,
                                                   kColorAshTextColorPrimary));

    remove_user_button_ = new RemoveUserButton(
        base::BindRepeating(&LoginRemoveAccountDialog::RemoveUserButtonPressed,
                            base::Unretained(this)),
        this);
    remove_user_button_->SetID(kRemoveUserButtonIdForTest);
    AddChildView(remove_user_button_.get());

    // Traps the focus on the remove user button.
    focus_search_ = std::make_unique<TrappedFocusSearch>(remove_user_button_);
  }

  set_positioning_strategy(PositioningStrategy::kTryAfterThenBefore);
  SetPadding(kHorizontalPaddingRemoveAccountDialogDp,
             kVerticalPaddingRemoveAccountDialogDp);

  GetViewAccessibility().SetRole(ax::mojom::Role::kDialog);
  UpdateAccessibleName();
  UpdateAccessibleDescription();
  GetViewAccessibility().SetIsModal(true);
}

LoginRemoveAccountDialog::~LoginRemoveAccountDialog() = default;

LoginButton* LoginRemoveAccountDialog::GetBubbleOpener() const {
  return bubble_opener_;
}

void LoginRemoveAccountDialog::RequestFocus() {
  // This view has no actual interesting contents to focus, so immediately
  // forward to the button.
  if (remove_user_button_) {
    remove_user_button_->RequestFocus();
  }
}

bool LoginRemoveAccountDialog::HasFocus() const {
  return remove_user_button_ && remove_user_button_->HasFocus();
}

views::FocusTraversable* LoginRemoveAccountDialog::GetPaneFocusTraversable() {
  return this;
}

views::FocusSearch* LoginRemoveAccountDialog::GetFocusSearch() {
  return focus_search_.get();
}

views::FocusTraversable* LoginRemoveAccountDialog::GetFocusTraversableParent() {
  return nullptr;
}

views::View* LoginRemoveAccountDialog::GetFocusTraversableParentView() {
  return nullptr;
}

void LoginRemoveAccountDialog::RemoveUserButtonPressed() {
  // Show confirmation warning. The user has to click the button again before
  // we actually allow the exit.
  if (!remove_user_confirm_data_->GetVisible()) {
    remove_user_confirm_data_->SetVisible(true);
    if (management_disclosure_label_) {
      management_disclosure_label_->SetVisible(false);
    }
    remove_user_button_->SetAlert(true);

    DeprecatedLayoutImmediately();

    // Change the node's description to force assistive technologies, like
    // ChromeVox, to report the updated description.
    remove_user_button_->GetViewAccessibility().SetDescription(
        warning_message_);
    if (on_remove_user_warning_shown_) {
      std::move(on_remove_user_warning_shown_).Run();
    }
    return;
  }

  // Immediately hide the bubble with no animation before running the remove
  // user callback. If an animation is triggered while the the views hierarchy
  // for this bubble is being torn down, we can get a crash.
  SetVisible(false);

  if (on_remove_user_requested_) {
    std::move(on_remove_user_requested_).Run();
  }
}

void LoginRemoveAccountDialog::UpdateAccessibleDescription() {
  if (remove_user_button_) {
    GetViewAccessibility().SetDescription(l10n_util::GetStringUTF16(
        IDS_ASH_LOGIN_POD_REMOVE_ACCOUNT_DIALOG_ACCESSIBLE_DESCRIPTION));
  } else {
    if (management_disclosure_label_) {
      GetViewAccessibility().SetDescription(
          base::StrCat({email_label_->GetText(), u" ",
                        management_disclosure_label_->GetText()}));
    } else {
      GetViewAccessibility().SetDescription(email_label_->GetText());
    }
  }
}

void LoginRemoveAccountDialog::UpdateAccessibleName() {
  if (remove_user_button_) {
    GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
        IDS_ASH_LOGIN_POD_REMOVE_ACCOUNT_ACCESSIBLE_NAME));
  } else {
    std::u16string accessible_name = username_label_->GetText();
    if (!accessible_name.empty()) {
      GetViewAccessibility().SetName(accessible_name);
    } else {
      GetViewAccessibility().SetName(
          std::u16string(), ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
    }
  }
}

BEGIN_METADATA(LoginRemoveAccountDialog)
END_METADATA

}  // namespace ash
