// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/notifications/tpm_auto_update_notification.h"

#include <string>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "base/functional/bind.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

constexpr char kTPMPlannedAutoUpdateNotificationId[] =
    "chrome://tpm_planned_firmware_auto_update";
constexpr char kTPMAutoUpdateOnRebootNotificationId[] =
    "chrome://tpm_firmware_auto_update_on_reboot";

void ShowAutoUpdateNotification(
    TpmAutoUpdateUserNotification notification_type) {
  std::u16string title, text;
  std::string notification_id;
  bool pinned = false;
  auto catalog_name = NotificationCatalogName::kNone;

  switch (notification_type) {
    case TpmAutoUpdateUserNotification::kNone:
      NOTREACHED_IN_MIGRATION();
      return;
    case TpmAutoUpdateUserNotification::kPlanned:
      title = l10n_util::GetStringUTF16(
          IDS_TPM_AUTO_UPDATE_PLANNED_NOTIFICATION_TITLE);
      text = l10n_util::GetStringUTF16(
          IDS_TPM_AUTO_UPDATE_PLANNED_NOTIFICATION_MESSAGE);
      notification_id = kTPMPlannedAutoUpdateNotificationId;
      catalog_name = NotificationCatalogName::kTPMAutoUpdatePlanned;
      break;
    case TpmAutoUpdateUserNotification::kOnNextReboot:
      title = l10n_util::GetStringUTF16(
          IDS_TPM_AUTO_UPDATE_REBOOT_NOTIFICATION_TITLE);
      text = l10n_util::GetStringUTF16(
          IDS_TPM_AUTO_UPDATE_REBOOT_NOTIFICATION_MESSAGE);
      notification_id = kTPMAutoUpdateOnRebootNotificationId;
      pinned = true;
      catalog_name = NotificationCatalogName::kTPMAutoUpdateOnReboot;
      break;
  }

  message_center::Notification notification = ash::CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id, title, text,
      std::u16string() /*display_source*/, GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 notification_id, catalog_name),
      message_center::RichNotificationData(),
      new message_center::NotificationDelegate(), vector_icons::kBusinessIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);
  notification.set_priority(message_center::SYSTEM_PRIORITY);
  notification.set_pinned(pinned);

  SystemNotificationHelper::GetInstance()->Display(notification);
}
}  // namespace ash
