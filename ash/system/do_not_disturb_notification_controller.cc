// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/do_not_disturb_notification_controller.h"

#include <memory>
#include <string>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/focus_mode/focus_mode_util.h"
#include "ash/system/system_notification_controller.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace ash {

namespace {

using message_center::MessageCenter;

const char kDoNotDisturbNotifierId[] =
    "ash.do_not_disturb_notification_controller";

// Returns true if we need to show specific Focus Mode text in the notification.
bool ShouldShowFocusModeText() {
  auto* focus_mode_controller =
      features::IsFocusModeEnabled() ? FocusModeController::Get() : nullptr;

  return focus_mode_controller &&
         message_center::MessageCenter::Get()
                 ->GetLastQuietModeChangeSourceType() ==
             message_center::QuietModeSourceType::kFocusMode;
}

// Creates a notification for do not disturb. If a focus session is active, the
// title and the message of the notification will indicate if DND will be turned
// off when the focus session ends.
std::unique_ptr<message_center::Notification> CreateNotification() {
  // `should_show_focus_text` is true only when the notification needs to be
  // turned off when the focus session ends.
  const bool should_show_focus_text = ShouldShowFocusModeText();
  const std::u16string title =
      l10n_util::GetStringUTF16(IDS_ASH_DO_NOT_DISTURB_NOTIFICATION_TITLE);
  const std::u16string message =
      should_show_focus_text
          ? focus_mode_util::GetNotificationDescriptionForFocusSession(
                FocusModeController::Get()->GetActualEndTime())
          : l10n_util::GetStringUTF16(
                IDS_ASH_DO_NOT_DISTURB_NOTIFICATION_DESCRIPTION);

  message_center::RichNotificationData optional_fields;
  optional_fields.buttons.emplace_back(
      l10n_util::GetStringUTF16(IDS_ASH_DO_NOT_DISTURB_NOTIFICATION_TURN_OFF));
  optional_fields.pinned = true;
  return CreateSystemNotificationPtr(
      message_center::NotificationType::NOTIFICATION_TYPE_SIMPLE,
      DoNotDisturbNotificationController::kDoNotDisturbNotificationId, title,
      message, /*display_source=*/std::u16string(), /*origin_url=*/GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kDoNotDisturbNotifierId,
                                 NotificationCatalogName::kDoNotDisturb),
      optional_fields,
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating([](std::optional<int> button_index) {
            if (!button_index.has_value()) {
              return;
            }
            // The notification only has one button (the "Turn off" button), so
            // the presence of any value in `button_index` means this is the
            // button that was pressed.
            DCHECK_EQ(button_index.value(), 0);
            MessageCenter::Get()->SetQuietMode(false);
          })),
      vector_icons::kSettingsOutlineIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);
}

}  // namespace

DoNotDisturbNotificationController::DoNotDisturbNotificationController() {
  MessageCenter::Get()->AddObserver(this);
}

DoNotDisturbNotificationController::~DoNotDisturbNotificationController() {
  MessageCenter::Get()->RemoveObserver(this);
}

// static
const char DoNotDisturbNotificationController::kDoNotDisturbNotificationId[] =
    "do_not_disturb";

// static
DoNotDisturbNotificationController* DoNotDisturbNotificationController::Get() {
  SystemNotificationController* system_notification_controller =
      Shell::Get()->system_notification_controller();
  return system_notification_controller
             ? system_notification_controller->do_not_disturb()
             : nullptr;
}

void DoNotDisturbNotificationController::OnQuietModeChanged(
    bool in_quiet_mode) {
  auto* message_center = MessageCenter::Get();
  if (in_quiet_mode) {
    DCHECK(!message_center->FindNotificationById(kDoNotDisturbNotificationId));
    message_center->AddNotification(CreateNotification());
    return;
  }
  DCHECK(message_center->FindNotificationById(kDoNotDisturbNotificationId));
  message_center->RemoveNotification(kDoNotDisturbNotificationId,
                                     /*by_user=*/false);
}

void DoNotDisturbNotificationController::MaybeUpdateNotification() {
  auto* message_center = message_center::MessageCenter::Get();
  if (message_center->FindVisibleNotificationById(
          kDoNotDisturbNotificationId)) {
    message_center->UpdateNotification(kDoNotDisturbNotificationId,
                                       CreateNotification());
  }
}

}  // namespace ash
