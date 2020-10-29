// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_notification_helper.h"

#include "ash/public/cpp/notification_utils.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "url/gurl.h"

namespace policy {

namespace {

constexpr char kPrintBlockedNotificationId[] = "print_dlp_blocked";
constexpr char kDlpPolicyNotifierId[] = "policy.dlp";

}  // namespace

void ShowDlpPrintDisabledNotification() {
  std::unique_ptr<message_center::Notification> notification =
      ash::CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE, kPrintBlockedNotificationId,
          l10n_util::GetStringUTF16(IDS_POLICY_DLP_PRINTING_BLOCKED_TITLE),
          l10n_util::GetStringUTF16(IDS_POLICY_DLP_PRINTING_BLOCKED_MESSAGE),
          /*display_source=*/base::string16(), GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kDlpPolicyNotifierId),
          message_center::RichNotificationData(),
          base::MakeRefCounted<message_center::NotificationDelegate>(),
          vector_icons::kBusinessIcon,
          message_center::SystemNotificationWarningLevel::CRITICAL_WARNING);
  notification->set_renotify(true);

  NotificationDisplayService::GetForProfile(
      ProfileManager::GetActiveUserProfile())
      ->Display(NotificationHandler::Type::TRANSIENT, *notification,
                /*metadata=*/nullptr);
}

}  // namespace policy
