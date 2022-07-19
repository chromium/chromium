// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy/privacy_indicators_controller.h"

#include <string>

#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"

namespace ash {

namespace {
const char kPrivacyIndicatorsNotificationIdPrefix[] = "privacy-indicators";
const char kPrivacyIndicatorsNotifierId[] = "ash.privacy-indicators";
}  // namespace

void ModifyPrivacyIndicatorsNotification(const std::string& app_id,
                                         bool camera_is_used,
                                         bool microphone_is_used) {
  auto* message_center = message_center::MessageCenter::Get();
  std::string id = kPrivacyIndicatorsNotificationIdPrefix + app_id;
  bool notification_exist = message_center->FindVisibleNotificationById(id);

  if (!camera_is_used && !microphone_is_used) {
    if (notification_exist)
      message_center->RemoveNotification(id, /*by_user=*/false);
    return;
  }

  std::u16string title;
  if (camera_is_used && microphone_is_used) {
    title = l10n_util::GetStringUTF16(
        IDS_PRIVACY_NOTIFICATION_TITLE_CAMERA_AND_MIC);
  } else if (camera_is_used) {
    title = l10n_util::GetStringUTF16(IDS_PRIVACY_NOTIFICATION_TITLE_CAMERA);
  } else {
    title = l10n_util::GetStringUTF16(IDS_PRIVACY_NOTIFICATION_TITLE_MIC);
  }

  message_center::RichNotificationData optional_fields;
  optional_fields.pinned = true;
  // Make the notification low priority so that it is silently added (no popup).
  optional_fields.priority = message_center::LOW_PRIORITY;

  auto notification = CreateSystemNotification(
      message_center::NotificationType::NOTIFICATION_TYPE_SIMPLE, id, title,
      std::u16string(),
      /*display_source=*/std::u16string(),
      /*origin_url=*/GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kPrivacyIndicatorsNotifierId,
                                 NotificationCatalogName::kPrivacyIndicators),
      optional_fields,
      /*delegate=*/nullptr, kImeMenuMicrophoneIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);

  if (notification_exist) {
    message_center->UpdateNotification(id, std::move(notification));
    return;
  }
  message_center->AddNotification(std::move(notification));
}

}  // namespace ash