// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/kerberos/kerberos_ticket_expiry_notification.h"

#include <string>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "url/gurl.h"

using message_center::ButtonInfo;
using message_center::HandleNotificationClickDelegate;
using message_center::Notification;
using message_center::NotificationType;
using message_center::NotifierId;
using message_center::NotifierType;
using message_center::RichNotificationData;
using message_center::SystemNotificationWarningLevel;

namespace ash {
namespace kerberos_ticket_expiry_notification {

namespace {

// Unique ID for this notification.
constexpr char kNotificationId[] = "kerberos.ticket-expiry-notification";

// Simplest type of notification UI - no progress bars, images etc.
constexpr NotificationType kNotificationType =
    message_center::NOTIFICATION_TYPE_SIMPLE;

// Generic type for notifications that are not from web pages etc.
const NotificationHandler::Type kNotificationHandlerType =
    NotificationHandler::Type::TRANSIENT;

// This notification is a regular warning. It's not critical as users can still
// authenticate in most cases using username/password.
constexpr SystemNotificationWarningLevel kWarningLevel =
    SystemNotificationWarningLevel::WARNING;

void OnClick(ClickCallback click_callback,
             const std::string& passed_principal_name,
             std::optional<int> /* button_idx */) {
  click_callback.Run(passed_principal_name);
}

}  // namespace

void Show(Profile* profile,
          const std::string& principal_name,
          ClickCallback click_callback) {
  const std::u16string kTitle =
      l10n_util::GetStringUTF16(IDS_KERBEROS_TICKET_EXPIRY_TITLE);
  const std::u16string kBody = l10n_util::GetStringFUTF16(
      IDS_KERBEROS_TICKET_EXPIRY_BODY, base::UTF8ToUTF16(principal_name));
  const std::u16string kButton =
      l10n_util::GetStringUTF16(IDS_KERBEROS_TICKET_EXPIRY_BUTTON);

  // For histogram reporting.
  const NotifierId kNotifierId(NotifierType::SYSTEM_COMPONENT, kNotificationId,
                               NotificationCatalogName::kKerberosTicketExpiry);

  // No origin URL is needed since the notification comes from the system.
  const GURL kEmptyOriginUrl;

  // Empty display source to show OS name as source.
  const std::u16string kEmptyDisplaySource;

  // Office building.
  const gfx::VectorIcon& kIcon = vector_icons::kBusinessIcon;

  // Show button with proper text.
  RichNotificationData notification_data;
  notification_data.buttons = std::vector<ButtonInfo>{ButtonInfo(kButton)};

  // Wrapper to call the |click_callback| with the |principal_name|.
  HandleNotificationClickDelegate::ButtonClickCallback callback_wrapper =
      base::BindRepeating(&OnClick, click_callback, principal_name);

  Notification notification = ash::CreateSystemNotification(
      kNotificationType, kNotificationId, kTitle, kBody, kEmptyDisplaySource,
      kEmptyOriginUrl, kNotifierId, notification_data,
      base::MakeRefCounted<HandleNotificationClickDelegate>(callback_wrapper),
      kIcon, kWarningLevel);

  NotificationDisplayService* nds =
      NotificationDisplayServiceFactory::GetForProfile(profile);
  // Calling close before display ensures that the notification pops up again
  // even if it is already shown.
  nds->Close(kNotificationHandlerType, kNotificationId);
  nds->Display(kNotificationHandlerType, notification, nullptr /* metadata */);
}

void Close(Profile* profile) {
  NotificationDisplayServiceFactory::GetForProfile(profile)->Close(
      kNotificationHandlerType, kNotificationId);
}

}  // namespace kerberos_ticket_expiry_notification
}  // namespace ash
