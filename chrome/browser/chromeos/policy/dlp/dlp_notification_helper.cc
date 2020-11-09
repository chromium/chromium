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
constexpr char kScreenCapturePausedNotificationId[] =
    "screen_capture_dlp_paused";
constexpr char kScreenCaptureResumedNotificationId[] =
    "screen_capture_dlp_resumed";
constexpr char kDlpPolicyNotifierId[] = "policy.dlp";

void ShowDlpNotification(const std::string& id,
                         const base::string16& title,
                         const base::string16& message) {
  std::unique_ptr<message_center::Notification> notification =
      ash::CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE, id, title, message,
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

}  // namespace

void ShowDlpPrintDisabledNotification() {
  ShowDlpNotification(
      kPrintBlockedNotificationId,
      l10n_util::GetStringUTF16(IDS_POLICY_DLP_PRINTING_BLOCKED_TITLE),
      l10n_util::GetStringUTF16(IDS_POLICY_DLP_PRINTING_BLOCKED_MESSAGE));
}

void HideDlpScreenCapturePausedNotification() {
  NotificationDisplayService::GetForProfile(
      ProfileManager::GetActiveUserProfile())
      ->Close(NotificationHandler::Type::TRANSIENT,
              kScreenCapturePausedNotificationId);
}

void ShowDlpScreenCapturePausedNotification() {
  ShowDlpNotification(
      kScreenCapturePausedNotificationId,
      l10n_util::GetStringUTF16(IDS_POLICY_DLP_SCREEN_CAPTURE_PAUSED_TITLE),
      l10n_util::GetStringUTF16(IDS_POLICY_DLP_SCREEN_CAPTURE_PAUSED_MESSAGE));
}

void HideDlpScreenCaptureResumedNotification() {
  NotificationDisplayService::GetForProfile(
      ProfileManager::GetActiveUserProfile())
      ->Close(NotificationHandler::Type::TRANSIENT,
              kScreenCaptureResumedNotificationId);
}

void ShowDlpScreenCaptureResumedNotification() {
  ShowDlpNotification(
      kScreenCaptureResumedNotificationId,
      l10n_util::GetStringUTF16(IDS_POLICY_DLP_SCREEN_CAPTURE_RESUMED_TITLE),
      l10n_util::GetStringUTF16(IDS_POLICY_DLP_SCREEN_CAPTURE_RESUMED_MESSAGE));
}

}  // namespace policy
