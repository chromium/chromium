// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/notifications/update_required_notification.h"

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "base/functional/bind.h"
#include "base/i18n/message_formatter.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"

using NotificationType = policy::MinimumVersionPolicyHandler::NotificationType;
using MessageFormatter = base::i18n::MessageFormatter;

namespace ash {
namespace {

const char kUpdateRequiredNotificationId[] = "policy.update_required";

std::u16string GetTitle(NotificationType type,
                        int days_remaining,
                        const std::u16string& device_type) {
  // |days_remaining| could be zero if we are very close to the deadline, like
  // 10 minutes as we round of the time remaining into days. In this case, we
  // need to show the last day notification title which does not mention the
  // number of remaining days but is rather a generic string like 'Immediate
  // update required'.
  days_remaining = days_remaining > 1 ? days_remaining : 1;
  switch (type) {
    case NotificationType::kNoConnection:
    case NotificationType::kMeteredConnection:
      return (days_remaining % 7)
                 ? MessageFormatter::FormatWithNumberedArgs(
                       l10n_util::GetStringUTF16(
                           IDS_UPDATE_REQUIRED_NETWORK_LIMITATION_TITLE_DAYS),
                       days_remaining, device_type)
                 : MessageFormatter::FormatWithNumberedArgs(
                       l10n_util::GetStringUTF16(
                           IDS_UPDATE_REQUIRED_NETWORK_LIMITATION_TITLE_WEEKS),
                       days_remaining / 7, device_type);
    case NotificationType::kEolReached:
      return (days_remaining % 7)
                 ? MessageFormatter::FormatWithNumberedArgs(
                       l10n_util::GetStringUTF16(
                           IDS_UPDATE_REQUIRED_EOL_TITLE_DAYS),
                       days_remaining, device_type)
                 : MessageFormatter::FormatWithNumberedArgs(
                       l10n_util::GetStringUTF16(
                           IDS_UPDATE_REQUIRED_EOL_TITLE_WEEKS),
                       days_remaining / 7, device_type);
  }
}

std::u16string GetMessage(NotificationType type,
                          const std::string& manager,
                          int days_remaining,
                          const std::u16string& device_type) {
  // |days_remaining| could be zero if we are very close to the deadline, like
  // 10 minutes as we round of the time remaining into days. In this case, we
  // need to show the last day notification.
  days_remaining = days_remaining > 1 ? days_remaining : 1;
  switch (type) {
    case NotificationType::kNoConnection:
      return MessageFormatter::FormatWithNumberedArgs(
          l10n_util::GetStringUTF16(IDS_UPDATE_REQUIRED_NO_NETWORK_MESSAGE),
          days_remaining, base::UTF8ToUTF16(manager));
    case NotificationType::kMeteredConnection:
      return MessageFormatter::FormatWithNumberedArgs(
          l10n_util::GetStringUTF16(
              IDS_UPDATE_REQUIRED_METERED_NETWORK_MESSAGE),
          days_remaining, base::UTF8ToUTF16(manager));
    case NotificationType::kEolReached:
      return MessageFormatter::FormatWithNumberedArgs(
          l10n_util::GetStringUTF16(IDS_UPDATE_REQUIRED_EOL_MESSAGE),
          days_remaining, base::UTF8ToUTF16(manager), device_type);
  }
}

std::u16string GetButtonText(NotificationType type) {
  switch (type) {
    case NotificationType::kNoConnection:
      return l10n_util::GetStringUTF16(
          IDS_UPDATE_REQUIRED_SCREEN_OPEN_NETWORK_SETTINGS);
    case NotificationType::kMeteredConnection:
      return l10n_util::GetStringUTF16(
          IDS_UPDATE_REQUIRED_SCREEN_ALLOW_METERED);
    case NotificationType::kEolReached:
      return l10n_util::GetStringUTF16(IDS_UPDATE_REQUIRED_EOL_SEE_DETAILS);
  }
}

message_center::NotificationPriority GetNotificationPriority(
    int days_remaining) {
  return days_remaining > 1 ? message_center::HIGH_PRIORITY
                            : message_center::SYSTEM_PRIORITY;
}

message_center::SystemNotificationWarningLevel GetWarningLevel(
    NotificationType type,
    int days_remaining) {
  if ((NotificationType::kEolReached == type && days_remaining <= 7) ||
      days_remaining <= 1) {
    return message_center::SystemNotificationWarningLevel::CRITICAL_WARNING;
  }
  return message_center::SystemNotificationWarningLevel::WARNING;
}

}  // namespace

UpdateRequiredNotification::UpdateRequiredNotification() = default;

UpdateRequiredNotification::~UpdateRequiredNotification() = default;

void UpdateRequiredNotification::Show(NotificationType type,
                                      base::TimeDelta warning_time,
                                      const std::string& manager,
                                      const std::u16string& device_type,
                                      base::OnceClosure button_click_callback,
                                      base::OnceClosure close_callback) {
  const int days_remaining = warning_time.InDays();
  notification_button_click_callback_ = std::move(button_click_callback);
  notification_close_callback_ = std::move(close_callback);

  std::u16string title = GetTitle(type, days_remaining, device_type);
  std::u16string body = GetMessage(type, manager, days_remaining, device_type);
  std::u16string button = GetButtonText(type);
  if (title.empty() || body.empty() || button.empty()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  DisplayNotification(title, body, button,
                      GetWarningLevel(type, days_remaining),
                      GetNotificationPriority(days_remaining));
}

void UpdateRequiredNotification::DisplayNotification(
    const std::u16string& title,
    const std::u16string& message,
    const std::u16string& button_text,
    message_center::SystemNotificationWarningLevel color_type,
    message_center::NotificationPriority priority) {
  message_center::RichNotificationData data;
  data.buttons.push_back(message_center::ButtonInfo(button_text));

  message_center::Notification notification = ash::CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE, kUpdateRequiredNotificationId,
      title, message, std::u16string() /*display_source*/, GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kUpdateRequiredNotificationId,
                                 NotificationCatalogName::kUpdateRequired),
      data,
      base::MakeRefCounted<message_center::ThunkNotificationDelegate>(
          weak_factory_.GetWeakPtr()),
      vector_icons::kBusinessIcon, color_type);
  notification.set_priority(priority);

  SystemNotificationHelper::GetInstance()->Display(notification);
}

void UpdateRequiredNotification::Hide() {
  SystemNotificationHelper::GetInstance()->Close(kUpdateRequiredNotificationId);
}

void UpdateRequiredNotification::Close(bool by_user) {
  if (!notification_close_callback_.is_null())
    std::move(notification_close_callback_).Run();
}

void UpdateRequiredNotification::Click(
    const std::optional<int>& button_index,
    const std::optional<std::u16string>& reply) {
  // |button_index| may be empty if the notification body was clicked.
  if (!button_index)
    return;

  Hide();
  if (!notification_button_click_callback_.is_null())
    std::move(notification_button_click_callback_).Run();
}

}  // namespace ash
