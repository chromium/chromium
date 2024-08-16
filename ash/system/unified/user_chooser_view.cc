// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/user_chooser_view.h"

#include <memory>
#include <string>

#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/rounded_image_view.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/icon_button.h"
#include "ash/style/typography.h"
#include "ash/system/model/enterprise_domain_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tri_view.h"
#include "ash/system/unified/user_chooser_detailed_view_controller.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/vector_icons.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// A button that will transition to multi profile login UI.
class AddUserButton : public views::Button {
  METADATA_HEADER(AddUserButton, views::Button)

 public:
  explicit AddUserButton(UserChooserDetailedViewController* controller);

  AddUserButton(const AddUserButton&) = delete;
  AddUserButton& operator=(const AddUserButton&) = delete;

  ~AddUserButton() override = default;
};

AddUserButton::AddUserButton(UserChooserDetailedViewController* controller)
    : Button(base::BindRepeating(
          &UserChooserDetailedViewController::HandleAddUserAction,
          base::Unretained(controller))) {
  SetID(VIEW_ID_ADD_USER_BUTTON);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets(kUnifiedTopShortcutSpacing), kUnifiedTopShortcutSpacing));
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SIGN_IN_ANOTHER_ACCOUNT));
  SetFocusPainter(TrayPopupUtils::CreateFocusPainter());

  auto* icon = AddChildView(std::make_unique<views::ImageView>());
  icon->SetImage(ui::ImageModel::FromVectorIcon(
      kSystemMenuNewUserIcon, cros_tokens::kCrosSysOnSurface));

  auto* label = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SIGN_IN_ANOTHER_ACCOUNT)));
  label->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosButton2, *label);
  label->SetAutoColorReadabilityEnabled(false);
  label->SetSubpixelRenderingEnabled(false);
}

BEGIN_METADATA(AddUserButton)
END_METADATA

class Separator : public views::View {
  METADATA_HEADER(Separator, views::View)

 public:
  explicit Separator(bool between_user) {
    SetUseDefaultFillLayout(true);
    SetBorder(views::CreateEmptyBorder(
        between_user
            ? gfx::Insets::VH(0, kUnifiedUserChooserSeparatorSideMargin)
            : gfx::Insets::VH(kUnifiedUserChooserLargeSeparatorVerticalSpacing,
                              0)));
    AddChildView(
        views::Builder<views::View>()
            // make sure that the view is displayed by setting non-zero size
            .SetPreferredSize(gfx::Size(1, 1))
            .SetBorder(views::CreateThemedSolidSidedBorder(
                gfx::Insets::TLBR(0, 0, kUnifiedNotificationSeparatorThickness,
                                  0),
                cros_tokens::kCrosSysSeparator))
            .Build());
  }

  Separator(const Separator&) = delete;
  Separator& operator=(const Separator&) = delete;
};

BEGIN_METADATA(Separator)
END_METADATA

views::View* CreateAddUserErrorView(const std::u16string& message) {
  auto* label = new views::Label(message);
  label->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);
  label->SetAutoColorReadabilityEnabled(false);
  label->SetSubpixelRenderingEnabled(false);
  label->SetBorder(views::CreateEmptyBorder(kUnifiedTopShortcutSpacing));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  return label;
}

}  // namespace

views::View* CreateUserAvatarView(int user_index) {
  DCHECK(Shell::Get());
  const UserSession* const user_session =
      Shell::Get()->session_controller()->GetUserSession(user_index);
  DCHECK(user_session);

  if (user_session->user_info.type == user_manager::UserType::kGuest) {
    // In guest mode, the user avatar is just a disabled button pod.
    auto* image_view = new IconButton(
        views::Button::PressedCallback(), IconButton::Type::kMedium,
        &kSystemMenuGuestIcon, IDS_ASH_STATUS_TRAY_GUEST_LABEL);
    image_view->SetEnabled(false);
    return image_view;
  }
  auto* image_view = new RoundedImageView(
      kTrayItemSize / 2, RoundedImageView::Alignment::kLeading);
  image_view->SetCanProcessEventsWithinSubtree(false);
  image_view->SetImage(user_session->user_info.avatar.image,
                       gfx::Size(kTrayItemSize, kTrayItemSize));
  return image_view;
}

std::u16string GetUserItemAccessibleString(int user_index) {
  DCHECK(Shell::Get());
  const UserSession* const user_session =
      Shell::Get()->session_controller()->GetUserSession(user_index);
  DCHECK(user_session);

  if (user_session->user_info.type == user_manager::UserType::kGuest) {
    return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_GUEST_LABEL);
  }

  if (user_session->user_info.type == user_manager::UserType::kPublicAccount) {
    std::string domain_manager = Shell::Get()
                                     ->system_tray_model()
                                     ->enterprise_domain()
                                     ->enterprise_domain_manager();
    return l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_PUBLIC_LABEL,
        base::UTF8ToUTF16(user_session->user_info.display_name),
        base::UTF8ToUTF16(domain_manager));
  }

  return l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_USER_INFO_ACCESSIBILITY,
      base::UTF8ToUTF16(user_session->user_info.display_name),
      base::UTF8ToUTF16(user_session->user_info.display_email));
}

UserItemButton::UserItemButton(PressedCallback callback,
                               UserChooserDetailedViewController* controller,
                               int user_index,
                               ax::mojom::Role role,
                               bool has_close_button)
    : Button(user_index == 0
                 ? views::Button::PressedCallback()
                 : base::BindRepeating(
                       &UserChooserDetailedViewController::HandleUserSwitch,
                       base::Unretained(controller),
                       user_index)),
      user_index_(user_index),
      capture_icon_(new views::ImageView),
      name_(new views::Label),
      email_(new views::Label) {
  DCHECK_GT(VIEW_ID_USER_ITEM_BUTTON_END,
            VIEW_ID_USER_ITEM_BUTTON_START + user_index);
  SetID(VIEW_ID_USER_ITEM_BUTTON_START + user_index);
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets::VH(0, kUnifiedTopShortcutSpacing),
      kUnifiedTopShortcutSpacing));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  layout->set_minimum_cross_axis_size(kUnifiedUserChooserRowHeight);
  AddChildView(CreateUserAvatarView(user_index));

  views::View* vertical_labels = new views::View;
  vertical_labels->SetCanProcessEventsWithinSubtree(false);
  auto* vertical_layout =
      vertical_labels->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  vertical_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  const UserSession* const user_session =
      Shell::Get()->session_controller()->GetUserSession(user_index);

  name_->SetText(base::UTF8ToUTF16(user_session->user_info.display_name));
  name_->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosButton2, *name_);
  name_->SetAutoColorReadabilityEnabled(false);
  name_->SetSubpixelRenderingEnabled(false);
  vertical_labels->AddChildView(name_.get());

  email_->SetText(base::UTF8ToUTF16(user_session->user_info.display_email));
  email_->SetEnabledColorId(cros_tokens::kCrosSysOnSurfaceVariant);
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosAnnotation1,
                                        *email_);
  email_->SetAutoColorReadabilityEnabled(false);
  email_->SetSubpixelRenderingEnabled(false);
  vertical_labels->AddChildView(email_.get());

  AddChildView(vertical_labels);
  layout->SetFlexForView(vertical_labels, 1);

  capture_icon_->SetImage(ui::ImageModel::FromVectorIcon(
      kSystemTrayRecordingIcon, cros_tokens::kCrosSysError));
  if (!has_close_button) {
    // Add a padding with the same size as the close button,
    // so as to align all media indicators in a column.
    capture_icon_->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
        0, 0, 0, kTrayItemSize + kUnifiedTopShortcutSpacing)));
  }
  capture_icon_->SetVisible(false);
  AddChildView(capture_icon_.get());

  if (has_close_button) {
    AddChildView(std::make_unique<IconButton>(
        base::BindRepeating(
            &UserChooserDetailedViewController::TransitionToMainView,
            base::Unretained(controller)),
        IconButton::Type::kMedium, &views::kIcCloseIcon,
        IDS_APP_ACCNAME_CLOSE));
  }

  SetTooltipText(GetUserItemAccessibleString(user_index));
  SetFocusPainter(TrayPopupUtils::CreateFocusPainter());

  // The button for the currently active user is not clickable.
  GetViewAccessibility().SetRole(user_index_ == 0 ? ax::mojom::Role::kLabelText
                                                  : ax::mojom::Role::kButton);
  GetViewAccessibility().SetName(GetUserItemAccessibleString(user_index_));
}

void UserItemButton::SetCaptureState(MediaCaptureState capture_state) {
  capture_icon_->SetVisible(capture_state != MediaCaptureState::kNone);
  DeprecatedLayoutImmediately();

  int res_id = 0;
  switch (capture_state) {
    case MediaCaptureState::kAudioVideo:
      res_id = IDS_ASH_STATUS_TRAY_MEDIA_RECORDING_AUDIO_VIDEO;
      break;
    case MediaCaptureState::kAudio:
      res_id = IDS_ASH_STATUS_TRAY_MEDIA_RECORDING_AUDIO;
      break;
    case MediaCaptureState::kVideo:
      res_id = IDS_ASH_STATUS_TRAY_MEDIA_RECORDING_VIDEO;
      break;
    case MediaCaptureState::kNone:
      break;
  }
  if (res_id) {
    capture_icon_->SetTooltipText(l10n_util::GetStringUTF16(res_id));
  }
}

std::u16string UserItemButton::GetTooltipText(const gfx::Point& p) const {
  // If both of them are full shown, hide the tooltip.
  if (name_->GetPreferredSize(views::SizeBounds(name_->width(), {})).width() <=
          name_->width() &&
      email_->GetPreferredSize(views::SizeBounds(email_->width(), {}))
              .width() <= email_->width()) {
    return std::u16string();
  }
  return views::Button::GetTooltipText(p);
}

BEGIN_METADATA(UserItemButton)
END_METADATA

UserChooserView::UserChooserView(
    UserChooserDetailedViewController* controller) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  const int num_users =
      Shell::Get()->session_controller()->NumberOfLoggedInUsers();
  for (int i = 0; i < num_users; ++i) {
    std::unique_ptr<UserItemButton> button;
    if (i == 0) {
      button = std::make_unique<UserItemButton>(
          views::Button::PressedCallback(), controller, 0,
          // The button for the currently active user is not clickable.
          ax::mojom::Role::kLabelText, true);
    } else {
      button = std::make_unique<UserItemButton>(
          base::BindRepeating(
              &UserChooserDetailedViewController::HandleUserSwitch,
              base::Unretained(controller), i),
          controller, i, ax::mojom::Role::kButton, false);
    }
    user_item_buttons_.push_back(AddChildView(std::move(button)));
    AddChildView(std::make_unique<Separator>(i < num_users - 1));
  }

  switch (Shell::Get()->session_controller()->GetAddUserPolicy()) {
    case AddUserSessionPolicy::ALLOWED:
      AddChildView(std::make_unique<AddUserButton>(controller));
      break;
    case AddUserSessionPolicy::ERROR_NOT_ALLOWED_PRIMARY_USER:
      AddChildView(CreateAddUserErrorView(l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_MESSAGE_NOT_ALLOWED_PRIMARY_USER)));
      break;
    case AddUserSessionPolicy::ERROR_MAXIMUM_USERS_REACHED:
      AddChildView(CreateAddUserErrorView(l10n_util::GetStringFUTF16Int(
          IDS_ASH_STATUS_TRAY_MESSAGE_CANNOT_ADD_USER,
          session_manager::kMaximumNumberOfUserSessions)));
      break;
    case AddUserSessionPolicy::ERROR_NO_ELIGIBLE_USERS:
      AddChildView(CreateAddUserErrorView(
          l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_MESSAGE_OUT_OF_USERS)));
      break;
    case AddUserSessionPolicy::ERROR_LOCKED_TO_SINGLE_USER:
      AddChildView(CreateAddUserErrorView(l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_MESSAGE_NOT_ALLOWED_PRIMARY_USER)));
      break;
    case AddUserSessionPolicy::ERROR_LACROS_ENABLED:
      AddChildView(CreateAddUserErrorView(l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_MESSAGE_NOT_ALLOWED_LACROS)));
      break;
  }

  Shell::Get()->media_controller()->AddObserver(this);
  Shell::Get()->media_controller()->RequestCaptureState();
}

UserChooserView::~UserChooserView() {
  Shell::Get()->media_controller()->RemoveObserver(this);
}

void UserChooserView::OnMediaCaptureChanged(
    const base::flat_map<AccountId, MediaCaptureState>& capture_states) {
  if (user_item_buttons_.size() != capture_states.size()) {
    return;
  }

  for (size_t i = 0; i < user_item_buttons_.size(); ++i) {
    const UserSession* const user_session =
        Shell::Get()->session_controller()->GetUserSession(i);
    auto matched = capture_states.find(user_session->user_info.account_id);
    if (matched != capture_states.end()) {
      user_item_buttons_[i]->SetCaptureState(matched->second);
    }
  }
}

BEGIN_METADATA(UserChooserView)
END_METADATA

}  // namespace ash
