// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/user_chooser_view.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tri_view.h"
#include "ash/system/unified/top_shortcut_button.h"
#include "ash/system/unified/top_shortcuts_view.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/user/rounded_image_view.h"
#include "base/strings/utf_string_conversions.h"
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

namespace {

class CloseButton : public TopShortcutButton, public views::ButtonListener {
 public:
  explicit CloseButton(UnifiedSystemTrayController* controller);

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  UnifiedSystemTrayController* const controller_;

  DISALLOW_COPY_AND_ASSIGN(CloseButton);
};

CloseButton::CloseButton(UnifiedSystemTrayController* controller)
    : TopShortcutButton(this, views::kIcCloseIcon, IDS_APP_ACCNAME_CLOSE),
      controller_(controller) {}

void CloseButton::ButtonPressed(views::Button* sender, const ui::Event& event) {
  controller_->TransitionToMainView(true /* restore_focus */);
}

// A button that will transition to multi profile login UI.
class AddUserButton : public views::Button, public views::ButtonListener {
 public:
  explicit AddUserButton(UnifiedSystemTrayController* controller);
  ~AddUserButton() override = default;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  UnifiedSystemTrayController* const controller_;

  DISALLOW_COPY_AND_ASSIGN(AddUserButton);
};

AddUserButton::AddUserButton(UnifiedSystemTrayController* controller)
    : Button(this), controller_(controller) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kHorizontal, gfx::Insets(kUnifiedTopShortcutSpacing),
      kUnifiedTopShortcutSpacing));

  auto* icon = new views::ImageView;
  icon->SetImage(
      gfx::CreateVectorIcon(kSystemMenuNewUserIcon, kUnifiedMenuIconColor));
  icon->set_tooltip_text(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SIGN_IN_ANOTHER_ACCOUNT));
  AddChildView(icon);

  auto* label = new views::Label(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SIGN_IN_ANOTHER_ACCOUNT));
  label->SetEnabledColor(kUnifiedMenuTextColor);
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
        kUnifiedMenuSeparatorColor));
  }

  DISALLOW_COPY_AND_ASSIGN(Separator);
};

views::View* CreateAddUserErrorView(const base::string16& message) {
  auto* label = new views::Label(message);
  label->SetEnabledColor(kUnifiedMenuTextColor);
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
  const mojom::UserSession* const user_session =
      Shell::Get()->session_controller()->GetUserSession(user_index);
  DCHECK(user_session);

  auto* image_view = new tray::RoundedImageView(kTrayItemSize / 2);
  image_view->set_can_process_events_within_subtree(false);
  if (user_session->user_info->type == user_manager::USER_TYPE_GUEST) {
    gfx::ImageSkia icon =
        gfx::CreateVectorIcon(kSystemMenuGuestIcon, kMenuIconColor);
    image_view->SetImage(icon, icon.size());
  } else {
    image_view->SetImage(user_session->user_info->avatar->image,
                         gfx::Size(kTrayItemSize, kTrayItemSize));
  }
  return image_view;
}

base::string16 GetUserItemAccessibleString(int user_index) {
  DCHECK(Shell::Get());
  const mojom::UserSession* const user_session =
      Shell::Get()->session_controller()->GetUserSession(user_index);
  DCHECK(user_session);

  if (user_session->user_info->type == user_manager::USER_TYPE_GUEST)
    return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_GUEST_LABEL);

  return l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_USER_INFO_ACCESSIBILITY,
      base::UTF8ToUTF16(user_session->user_info->display_name),
      base::UTF8ToUTF16(user_session->user_info->display_email));
}

UserItemButton::UserItemButton(int user_index,
                               UnifiedSystemTrayController* controller,
                               bool has_close_button)
    : Button(this),
      user_index_(user_index),
      controller_(controller),
      capture_icon_(new views::ImageView) {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kHorizontal, gfx::Insets(0, kUnifiedTopShortcutSpacing),
      kUnifiedTopShortcutSpacing));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
  layout->set_minimum_cross_axis_size(kUnifiedUserChooserRowHeight);
  AddChildView(CreateUserAvatarView(user_index));

  views::View* vertical_labels = new views::View;
  vertical_labels->set_can_process_events_within_subtree(false);
  auto* vertical_layout = vertical_labels->SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));
  vertical_layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_START);

  const mojom::UserSession* const user_session =
      Shell::Get()->session_controller()->GetUserSession(user_index);

  auto* name = new views::Label(
      base::UTF8ToUTF16(user_session->user_info->display_name));
  name->SetEnabledColor(kUnifiedMenuTextColor);
  name->SetAutoColorReadabilityEnabled(false);
  name->SetSubpixelRenderingEnabled(false);
  vertical_labels->AddChildView(name);

  auto* email = new views::Label(
      base::UTF8ToUTF16(user_session->user_info->display_email));
  email->SetEnabledColor(kUnifiedMenuSecondaryTextColor);
  email->SetAutoColorReadabilityEnabled(false);
  email->SetSubpixelRenderingEnabled(false);
  vertical_labels->AddChildView(email);

  AddChildView(vertical_labels);
  layout->SetFlexForView(vertical_labels, 1);

  capture_icon_->SetImage(
      gfx::CreateVectorIcon(kSystemTrayRecordingIcon, kUnifiedMenuIconColor));
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

void UserItemButton::SetCaptureState(mojom::MediaCaptureState capture_state) {
  capture_icon_->SetVisible(capture_state != mojom::MediaCaptureState::NONE);
  Layout();

  int res_id = 0;
  switch (capture_state) {
    case mojom::MediaCaptureState::AUDIO_VIDEO:
      res_id = IDS_ASH_STATUS_TRAY_MEDIA_RECORDING_AUDIO_VIDEO;
      break;
    case mojom::MediaCaptureState::AUDIO:
      res_id = IDS_ASH_STATUS_TRAY_MEDIA_RECORDING_AUDIO;
      break;
    case mojom::MediaCaptureState::VIDEO:
      res_id = IDS_ASH_STATUS_TRAY_MEDIA_RECORDING_VIDEO;
      break;
    case mojom::MediaCaptureState::NONE:
      break;
  }
  if (res_id)
    capture_icon_->set_tooltip_text(l10n_util::GetStringUTF16(res_id));
}

void UserItemButton::ButtonPressed(views::Button* sender,
                                   const ui::Event& event) {
  if (user_index_ > 0)
    controller_->HandleUserSwitch(user_index_);
}

UserChooserView::UserChooserView(UnifiedSystemTrayController* controller) {
  SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));
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
  }

  Shell::Get()->media_controller()->AddObserver(this);
  Shell::Get()->media_controller()->RequestCaptureState();
}

UserChooserView::~UserChooserView() {
  Shell::Get()->media_controller()->RemoveObserver(this);
}

void UserChooserView::OnMediaCaptureChanged(
    const base::flat_map<AccountId, mojom::MediaCaptureState>& capture_states) {
  if (user_item_buttons_.size() != capture_states.size())
    return;

  for (size_t i = 0; i < user_item_buttons_.size(); ++i) {
    const mojom::UserSession* const user_session =
        Shell::Get()->session_controller()->GetUserSession(i);
    auto matched = capture_states.find(user_session->user_info->account_id);
    if (matched != capture_states.end()) {
      user_item_buttons_[i]->SetCaptureState(matched->second);
    }
  }
}

}  // namespace ash
