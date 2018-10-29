// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/notification_hidden_view.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/message_center/message_center_controller.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/sign_out_button.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/lock_screen/lock_screen_controller.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_impl.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

void ShowLockScreenNotificationSettings() {
  ash::Shell::Get()
      ->message_center_controller()
      ->ShowLockScreenNotificationSettings();
}

}  // namespace

NotificationHiddenView::NotificationHiddenView() {
  const bool lock_screen_notification_enabled =
      features::IsLockScreenNotificationsEnabled();

  auto* label = new views::Label;
  label->SetEnabledColor(kUnifiedMenuTextColor);
  label->SetAutoColorReadabilityEnabled(false);
  label->SetText(
      l10n_util::GetStringUTF16(IDS_ASH_MESSAGE_CENTER_LOCKSCREEN_UNIFIED));
  label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  label->SetLineHeight(kUnifiedNotificationHiddenLineHeight);
  label->SetBorder(views::CreateEmptyBorder(kUnifiedNotificationHiddenPadding));

  auto* container = new views::View;
  container->SetBackground(views::CreateBackgroundFromPainter(
      views::Painter::CreateSolidRoundRectPainter(kUnifiedMenuButtonColor,
                                                  kUnifiedTrayCornerRadius)));

  auto* layout = container->SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::BoxLayout::kHorizontal));

  container->AddChildView(label);
  layout->SetFlexForView(label, 1);

  if (lock_screen_notification_enabled) {
    auto* change_button = new RoundedLabelButton(
        this,
        l10n_util::GetStringUTF16(IDS_ASH_MESSAGE_CENTER_LOCKSCREEN_CHANGE));
    change_button->SetTooltipText(l10n_util::GetStringUTF16(
        IDS_ASH_MESSAGE_CENTER_LOCKSCREEN_CHANGE_TOOLTIP));

    container->AddChildView(change_button);
  }

  SetBorder(
      views::CreateEmptyBorder(gfx::Insets(kUnifiedNotificationCenterSpacing)));
  SetLayoutManager(std::make_unique<views::FillLayout>());
  AddChildView(container);
}

void NotificationHiddenView::ButtonPressed(views::Button* sender,
                                           const ui::Event& event) {
  // TODO(yoshiki): Refactor LockScreenController and remove the static cast.
  // TODO(yoshiki): Show the setting after unlocking.
  static_cast<message_center::MessageCenterImpl*>(
      message_center::MessageCenter::Get())
      ->lock_screen_controller()
      ->DismissLockScreenThenExecute(
          base::BindOnce(&ShowLockScreenNotificationSettings),
          base::DoNothing());
}

}  // namespace ash
