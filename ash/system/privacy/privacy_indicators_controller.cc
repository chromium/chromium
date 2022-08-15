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

// Keep track of the button indexes in the privacy indicators notification.
enum PrivacyIndicatorsNotificationButton { kAppLaunch, kAppSettings };

}  // namespace

PrivacyIndicatorsNotificationDelegate::PrivacyIndicatorsNotificationDelegate(
    const AppActionClosure& launch_app,
    const AppActionClosure& launch_settings)
    : launch_app_(launch_app), launch_settings_(launch_settings) {}

PrivacyIndicatorsNotificationDelegate::
    ~PrivacyIndicatorsNotificationDelegate() = default;

void PrivacyIndicatorsNotificationDelegate::Click(
    const absl::optional<int>& button_index,
    const absl::optional<std::u16string>& reply) {
  // Click on the notification body is no-op.
  if (!button_index)
    return;

  switch (button_index.value()) {
    case PrivacyIndicatorsNotificationButton::kAppLaunch:
      launch_app_.Run();
      break;
    case PrivacyIndicatorsNotificationButton::kAppSettings:
      launch_settings_.Run();
      break;
  }
}

void ModifyPrivacyIndicatorsNotification(
    const std::string& app_id,
    absl::optional<std::u16string> app_name,
    bool camera_is_used,
    bool microphone_is_used,
    scoped_refptr<PrivacyIndicatorsNotificationDelegate> delegate) {
  auto* message_center = message_center::MessageCenter::Get();
  std::string id = kPrivacyIndicatorsNotificationIdPrefix + app_id;
  bool notification_exist = message_center->FindVisibleNotificationById(id);

  if (!camera_is_used && !microphone_is_used) {
    if (notification_exist)
      message_center->RemoveNotification(id, /*by_user=*/false);
    return;
  }

  std::u16string app_name_str = app_name.value_or(l10n_util::GetStringUTF16(
      IDS_PRIVACY_NOTIFICATION_MESSAGE_DEFAULT_APP_NAME));

  std::u16string title;
  std::u16string message;
  if (camera_is_used && microphone_is_used) {
    title = l10n_util::GetStringUTF16(
        IDS_PRIVACY_NOTIFICATION_TITLE_CAMERA_AND_MIC);
    message = l10n_util::GetStringFUTF16(
        IDS_PRIVACY_NOTIFICATION_MESSAGE_CAMERA_AND_MIC, app_name_str);
  } else if (camera_is_used) {
    title = l10n_util::GetStringUTF16(IDS_PRIVACY_NOTIFICATION_TITLE_CAMERA);
    message = l10n_util::GetStringFUTF16(
        IDS_PRIVACY_NOTIFICATION_MESSAGE_CAMERA, app_name_str);
  } else {
    title = l10n_util::GetStringUTF16(IDS_PRIVACY_NOTIFICATION_TITLE_MIC);
    message = l10n_util::GetStringFUTF16(IDS_PRIVACY_NOTIFICATION_MESSAGE_MIC,
                                         app_name_str);
  }

  message_center::RichNotificationData optional_fields;
  optional_fields.pinned = true;
  // Make the notification low priority so that it is silently added (no popup).
  optional_fields.priority = message_center::LOW_PRIORITY;

  // Note: The order of buttons added here should match the order in
  // PrivacyIndicatorsNotificationButton.
  optional_fields.buttons.emplace_back(
      l10n_util::GetStringUTF16(IDS_PRIVACY_NOTIFICATION_BUTTON_APP_LAUNCH));
  optional_fields.buttons.emplace_back(
      l10n_util::GetStringUTF16(IDS_PRIVACY_NOTIFICATION_BUTTON_APP_SETTINGS));

  auto notification = CreateSystemNotification(
      message_center::NotificationType::NOTIFICATION_TYPE_SIMPLE, id, title,
      message,
      /*display_source=*/std::u16string(),
      /*origin_url=*/GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kPrivacyIndicatorsNotifierId,
                                 NotificationCatalogName::kPrivacyIndicators),
      optional_fields,
      /*delegate=*/delegate, kImeMenuMicrophoneIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);

  notification->set_accent_color_id(ui::kColorAshPrivacyIndicatorsBackground);

  if (notification_exist) {
    message_center->UpdateNotification(id, std::move(notification));
    return;
  }
  message_center->AddNotification(std::move(notification));
}

}  // namespace ash