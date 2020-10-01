// Copyright 2018 The Chromium Authors. All rights reserved.
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
#include "ash/style/ash_color_provider.h"
#include "ash/system/model/enterprise_domain_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tri_view.h"
#include "ash/system/unified/top_shortcut_button.h"
#include "ash/system/unified/top_shortcuts_view.h"
#include "ash/system/unified/user_chooser_detailed_view_controller.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/vector_icons.h"
#include "ui/views/widget/widget.h"

namespace ash {

using ContentLayerType = AshColorProvider::ContentLayerType;

namespace {

class CloseButton : public TopShortcutButton, public views::ButtonListener {
 public:
  explicit CloseButton(UserChooserDetailedViewController* controller);

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  UserChooserDetailedViewController* const controller_;

  DISALLOW_COPY_AND_ASSIGN(CloseButton);
};

CloseButton::CloseButton(UserChooserDetailedViewController* controller)
    : TopShortcutButton(this, views::kIcCloseIcon, IDS_APP_ACCNAME_CLOSE),
      controller_(controller) {}

void CloseButton::ButtonPressed(views::Button* sender, const ui::Event& event) {
  controller_->TransitionToMainView();
}

// A button that will transition to multi profile login UI.
class AddUserButton : public views::Button, public views::ButtonListener {
 public:
  explicit AddUserButton(UserChooserDetailedViewController* controller);
  ~AddUserButton() override = default;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  UserChooserDetailedViewController* const controller_;

  DISALLOW_COPY_AND_ASSIGN(AddUserButton);
};

AddUserButton::AddUserButton(UserChooserDetailedViewController* controller)
    : Button(this), controller_(controller) {
  SetID(VIEW_ID_ADD_USER_BUTTON);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets(kUnifiedTopShortcutSpacing), kUnifiedTopShortcutSpacing));

  auto* icon = new views::ImageView;
  icon->SetImage(gfx::CreateVectorIcon(
      kSystemMenuNewUserIcon, AshColorProvider::Get()->GetContentLayerColor(
                                  ContentLayerType::kIconColorPrimary)));
  AddChildView(icon);

  auto* label = new views::Label(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SIGN_IN_ANOTHER_ACCOUNT));
  label->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      ContentLayerType::kTextColorPrimary));
  label->SetAutoColorReadabilityEnabled(false);
  label->SetSubpixelRenderingEnabled(false);
  AddChildView(label);

  SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SIGN_IN_ANOTHER_ACCOUNT));
  SetFocusPainter(TrayPopupUtils::CreateFocusPainter());
  SetFocusForPlatform();
}

void AddUserButton::ButtonPressed(views::Button* sender,
                                  const ui::Event& event) {
  controller_->HandleAddUserAction();
}

class Separator : public views::View {
 public:
  explicit Separator(bool between_user) {
    SetLayoutManager(std::make_unique<views::FillLayout>());
    SetBorder(views::CreateEmptyBorder(
        between_user
            ? gfx::Insets(0, kUnifiedUserChooserSeparatorSideMargin)
            : gfx::Insets(kUnifiedUserChooserLargeSeparatorVerticalSpacing,
                          0)));
    views::View* child = new views::View();
    // make sure that the view is displayed by setting non-zero size
    child->SetPreferredSize(gfx::Size(1, 1));
    AddChildView(child);
    child->SetBorder(views::CreateSolidSidedBorder(
        0, 0, kUnifiedNotificationSeparatorThickness, 0,
        AshColorProvider::Get()->GetContentLayerColor(
            ContentLayerType::kSeparatorColor)));
  }

  DISALLOW_COPY_AND_ASSIGN(Separator);
};

views::View* CreateAddUserErrorView(const base::string16& message) {
  auto* label = new views::Label(message);
  label->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      ContentLayerType::kTextColorPrimary));
  label->SetAutoColorReadabilityEnabled(false);
  label->SetSubpixelRenderingEnabled(false);
  label->SetBorder(
      views::CreateEmptyBorder(gfx::Insets(kUnifiedTopShortcutSpacing)));
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

  if (user_session->user_info.type == user_manager::USER_TYPE_GUEST) {
    // In guest mode, the user avatar is just a disabled button pod.
    return new TopShortcutButton(kSystemMenuGuestIcon,
                                 IDS_ASH_STATUS_TRAY_GUEST_LABEL);
  } else {
    auto* image_view = new RoundedImageView(kTrayItemSize / 2);
    image_view->SetCanProcessEventsWithinSubtree(false);
    image_view->SetImage(user_session->user_info.avatar.image,
                         gfx::Size(kTrayItemSize, kTrayItemSize));
    return image_view;
  }
}

base::string16 GetUserItemAccessibleString(int user_index) {
  DCHECK(Shell::Get());
  const UserSession* const user_session =
      Shell::Get()->session_controller()->GetUserSession(user_index);
  DCHECK(user_session);

  if (user_session->user_info.type == user_manager::USER_TYPE_GUEST)
    return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_GUEST_LABEL);

  if (user_session->user_info.type == user_manager::USER_TYPE_PUBLIC_ACCOUNT) {
    std::string display_domain = Shell::Get()
                                     ->system_tray_model()
                                     ->enterprise_domain()
                                     ->enterprise_display_domain();
    return l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_PUBLIC_LABEL,
        base::UTF8ToUTF16(user_session->user_info.display_name),
        base::UTF8ToUTF16(display_domain));
  }

  return l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_USER_INFO_ACCESSIBILITY,
      base::UTF8ToUTF16(user_session->user_info.display_name),
      base::UTF8ToUTF16(user_session->user_info.display_email));
}

UserItemButton::UserItemButton(int user_index,
                               UserChooserDetailedViewController* controller,
                               bool has_close_button)
    : Button(this),
      user_index_(user_index),
      controller_(controller),
      capture_icon_(new views::ImageView),
      name_(new views::Label),
      email_(new views::Label) {
  DCHECK_GT(VIEW_ID_USER_ITEM_BUTTON_END,
            VIEW_ID_USER_ITEM_BUTTON_START + user_index);
  SetID(VIEW_ID_USER_ITEM_BUTTON_START + user_index);
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets(0, kUnifiedTopShortcutSpacing), kUnifiedTopShortcutSpacing));
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
  name_->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      ContentLayerType::kTextColorPrimary));
  name_->SetAutoColorReadabilityEnabled(false);
  name_->SetSubpixelRenderingEnabled(false);
  vertical_labels->AddChildView(name_);

  email_->SetText(base::UTF8ToUTF16(user_session->user_info.display_email));
  email_->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      ContentLayerType::kTextColorSecondary));
  email_->SetAutoColorReadabilityEnabled(false);
  email_->SetSubpixelRenderingEnabled(false);
  vertical_labels->AddChildView(email_);

  AddChildView(vertical_labels);
  layout->SetFlexForView(vertical_labels, 1);

  capture_icon_->SetImage(gfx::CreateVectorIcon(
      kSystemTrayRecordingIcon, AshColorProvider::Get()->GetContentLayerColor(
                                    ContentLayerType::kIconColorAlert)));
  if (!has_close_button) {
    // Add a padding with the same size as the close button,
    // so as to align all media indicators in a column.
    capture_icon_->SetBorder(views::CreateEmptyBorder(
        gfx::Insets(0, 0, 0, kTrayItemSize + kUnifiedTopShortcutSpacing)));
  }
  capture_icon_->SetVisible(false);
  AddChildView(capture_icon_);

  if (has_close_button)
    AddChildView(new CloseButton(controller_));

  SetTooltipText(GetUserItemAccessibleString(user_index));
  SetFocusPainter(TrayPopupUtils::CreateFocusPainter());
  SetFocusForPlatform();
}

void UserItemButton::SetCaptureState(MediaCaptureState capture_state) {
  capture_icon_->SetVisible(capture_state != MediaCaptureState::kNone);
  Layout();

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
  if (res_id)
    capture_icon_->SetTooltipText(l10n_util::GetStringUTF16(res_id));
}

base::string16 UserItemButton::GetTooltipText(const gfx::Point& p) const {
  // If both of them are full shown, hide the tooltip.
  if (name_->GetPreferredSize().width() <= name_->width() &&
      email_->GetPreferredSize().width() <= email_->width()) {
    return base::string16();
  }
  return views::Button::GetTooltipText(p);
}

void UserItemButton::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  // The button for the currently active user is not clickable.
  if (user_index_ == 0)
    node_data->role = ax::mojom::Role::kLabelText;
  else
    node_data->role = ax::mojom::Role::kButton;
}

void UserItemButton::ButtonPressed(views::Button* sender,
                                   const ui::Event& event) {
  if (user_index_ > 0)
    controller_->HandleUserSwitch(user_index_);
}

UserChooserView::UserChooserView(
    UserChooserDetailedViewController* controller) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  const int num_users =
      Shell::Get()->session_controller()->NumberOfLoggedInUsers();
  for (int i = 0; i < num_users; ++i) {
    auto* button = new UserItemButton(i, controller, i == 0);
    user_item_buttons_.push_back(button);

    AddChildView(button);
    AddChildView(new Separator(i < num_users - 1));
  }

  switch (Shell::Get()->session_controller()->GetAddUserPolicy()) {
    case AddUserSessionPolicy::ALLOWED:
      AddChildView(new AddUserButton(controller));
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
  }

  Shell::Get()->media_controller()->AddObserver(this);
  Shell::Get()->media_controller()->RequestCaptureState();
}

UserChooserView::~UserChooserView() {
  Shell::Get()->media_controller()->RemoveObserver(this);
}

void UserChooserView::OnMediaCaptureChanged(
    const base::flat_map<AccountId, MediaCaptureState>& capture_states) {
  if (user_item_buttons_.size() != capture_states.size())
    return;

  for (size_t i = 0; i < user_item_buttons_.size(); ++i) {
    const UserSession* const user_session =
        Shell::Get()->session_controller()->GetUserSession(i);
    auto matched = capture_states.find(user_session->user_info.account_id);
    if (matched != capture_states.end()) {
      user_item_buttons_[i]->SetCaptureState(matched->second);
    }
  }
}

const char* UserChooserView::GetClassName() const {
  return "UserChooserView";
}

}  // namespace ash
