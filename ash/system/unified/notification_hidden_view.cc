// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/notification_hidden_view.h"

#include "ash/bubble/bubble_constants.h"
#include "ash/login/login_screen_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/pill_button.h"
#include "ash/system/message_center/ash_message_center_lock_screen_controller.h"
#include "ash/system/message_center/message_center_constants.h"
#include "ash/system/tray/tray_constants.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/lock_screen/lock_screen_controller.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_impl.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

void ShowLockScreenNotificationSettings() {
  ash::Shell::Get()
      ->login_screen_controller()
      ->ShowLockScreenNotificationSettings();
}

SkColor GetBackgroundColor() {
  return AshColorProvider::Get()->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive);
}

}  // namespace

NotificationHiddenView::NotificationHiddenView()
    : container_(AddChildView(std::make_unique<views::View>())),
      label_(container_->AddChildView(std::make_unique<views::Label>())) {
  label_->SetAutoColorReadabilityEnabled(false);
  label_->SetText(
      l10n_util::GetStringUTF16(IDS_ASH_MESSAGE_CENTER_LOCKSCREEN_UNIFIED));
  label_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  label_->SetLineHeight(kUnifiedNotificationHiddenLineHeight);
  label_->SetBorder(
      views::CreateEmptyBorder(kUnifiedNotificationHiddenPadding));

  container_->SetBackground(views::CreateRoundedRectBackground(
      GetBackgroundColor(), kBubbleCornerRadius));

  auto* layout =
      container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
  layout->SetFlexForView(label_, 1);

  // Shows the "Change" button, unless the locks screen notification is
  // prohibited by policy or flag.
  if (AshMessageCenterLockScreenController::IsAllowed()) {
    change_button_ = container_->AddChildView(std::make_unique<PillButton>(
        base::BindRepeating(&NotificationHiddenView::ChangeButtonPressed,
                            base::Unretained(this)),
        l10n_util::GetStringUTF16(IDS_ASH_MESSAGE_CENTER_LOCKSCREEN_CHANGE),
        PillButton::Type::kDefaultWithoutIcon, /*icon=*/nullptr,
        kNotificationPillButtonHorizontalSpacing));
    change_button_->SetTooltipText(l10n_util::GetStringUTF16(
        IDS_ASH_MESSAGE_CENTER_LOCKSCREEN_CHANGE_TOOLTIP));
  }

  SetBorder(views::CreateEmptyBorder(kUnifiedNotificationCenterSpacing));
  SetLayoutManager(std::make_unique<views::FillLayout>());
}

const char* NotificationHiddenView::GetClassName() const {
  return "NotificationHiddenView";
}

void NotificationHiddenView::OnThemeChanged() {
  views::View::OnThemeChanged();
  label_->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary));
  container_->background()->SetNativeControlColor(GetBackgroundColor());
}

void NotificationHiddenView::ChangeButtonPressed() {
  // TODO(yoshiki): Refactor LockScreenController and remove the static cast.
  // TODO(yoshiki): Show the setting after unlocking.
  static_cast<message_center::MessageCenterImpl*>(
      message_center::MessageCenter::Get())
      ->lock_screen_controller()
      ->DismissLockScreenThenExecute(
          base::BindOnce(&ShowLockScreenNotificationSettings),
          base::DoNothing(), IDS_ASH_MESSAGE_CENTER_UNLOCK_TO_CHANGE_SETTING);
}

}  // namespace ash
