// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/saml/password_change_success_notification.h"

#include <memory>
#include <string>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace ash {
namespace {

using ::message_center::Notification;
using ::message_center::NotificationDelegate;
using ::message_center::NotificationType;
using ::message_center::NotifierId;
using ::message_center::NotifierType;
using ::message_center::RichNotificationData;
using ::message_center::SystemNotificationWarningLevel;

// Unique ID for this notification.
const char kNotificationId[] = "saml.password-change-success-notification";

// Simplest type of notification UI - no progress bars, images etc.
const NotificationType kNotificationType =
    message_center::NOTIFICATION_TYPE_SIMPLE;

// Generic type for notifications that are not from web pages etc.
const NotificationHandler::Type kNotificationHandlerType =
    NotificationHandler::Type::TRANSIENT;

// The icon to use for this notification - looks like an office building.
const gfx::VectorIcon& kIcon = vector_icons::kBusinessIcon;

// Warning level of WARNING makes the title orange.
constexpr SystemNotificationWarningLevel kWarningLevel =
    SystemNotificationWarningLevel::WARNING;

}  // namespace

// static
void PasswordChangeSuccessNotification::Show(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // NotifierId for histogram reporting.
  static const base::NoDestructor<NotifierId> kNotifierId(
      NotifierType::SYSTEM_COMPONENT, kNotificationId,
      NotificationCatalogName::kPasswordChange);

  // Leaving this empty means the notification is attributed to the system -
  // ie "Chromium OS" or similar.
  static const base::NoDestructor<std::u16string> kEmptyDisplaySource;

  // No origin URL is needed since the notification comes from the system.
  static const base::NoDestructor<GURL> kEmptyOriginUrl;

  const std::u16string title =
      l10n_util::GetStringUTF16(IDS_PASSWORD_CHANGE_NOTIFICATION_TITLE);

  const std::u16string body =
      l10n_util::GetStringUTF16(IDS_PASSWORD_CHANGE_NOTIFICATION_BODY);

  RichNotificationData rich_notification_data;

  const scoped_refptr<NotificationDelegate> delegate =
      base::MakeRefCounted<NotificationDelegate>();

  Notification notification = CreateSystemNotification(
      kNotificationType, kNotificationId, title, body, *kEmptyDisplaySource,
      *kEmptyOriginUrl, *kNotifierId, rich_notification_data, delegate, kIcon,
      kWarningLevel);

  NotificationDisplayService* nds =
      NotificationDisplayServiceFactory::GetForProfile(profile);
  // Calling close before display ensures that the notification pops up again
  // even if it is already shown.
  nds->Close(kNotificationHandlerType, kNotificationId);
  nds->Display(kNotificationHandlerType, notification, /*metadata=*/nullptr);
}

}  // namespace ash
