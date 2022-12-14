// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/do_not_disturb_notification_controller.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/strings/grit/ash_strings.h"
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

std::unique_ptr<message_center::Notification>
DoNotDisturbNotificationController::CreateNotification() {
  message_center::RichNotificationData optional_fields;
  optional_fields.buttons.emplace_back(
      l10n_util::GetStringUTF16(IDS_ASH_DO_NOT_DISTURB_NOTIFICATION_TURN_OFF));
  optional_fields.pinned = true;
  return ash::CreateSystemNotificationPtr(
      message_center::NotificationType::NOTIFICATION_TYPE_SIMPLE,
      kDoNotDisturbNotificationId,
      l10n_util::GetStringUTF16(IDS_ASH_DO_NOT_DISTURB_NOTIFICATION_TITLE),
      l10n_util::GetStringUTF16(
          IDS_ASH_DO_NOT_DISTURB_NOTIFICATION_DESCRIPTION),
      /*display_source=*/std::u16string(), /*origin_url=*/GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kDoNotDisturbNotifierId,
                                 NotificationCatalogName::kDoNotDisturb),
      optional_fields,
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating([](absl::optional<int> button_index) {
            if (!button_index.has_value())
              return;
            // The notification only has one button (the "Turn off" button), so
            // the presence of any value in `button_index` means this is the
            // button that was pressed.
            DCHECK_EQ(button_index.value(), 0);
            MessageCenter::Get()->SetQuietMode(false);
          })),
      vector_icons::kSettingsOutlineIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);
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

}  // namespace ash
