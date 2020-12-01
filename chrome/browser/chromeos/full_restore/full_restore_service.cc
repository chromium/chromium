// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/full_restore/full_restore_service.h"

#include "ash/public/cpp/notification_utils.h"
#include "base/strings/string_util.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/chromeos/full_restore/full_restore_service_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"

namespace {

constexpr char kRestoreForCrashNotificationId[] =
    "restore_for_crash_notification";

}  // namespace

namespace chromeos {
namespace full_restore {

FullRestoreService::FullRestoreService(Profile* profile) : profile_(profile) {
  // If the system crashed before reboot, show the restore notification.
  if (profile->GetLastSessionExitType() == Profile::EXIT_CRASHED) {
    ShowRestoreNotification(kRestoreForCrashNotificationId);
    return;
  }

  // TODO(crbug.com/909794):On startup for normal reboot, read
  // |kRestoreAppsAndPagesPrefName| from the user pref, show the restore
  // notification for the 'ask every time' option.
}

FullRestoreService::~FullRestoreService() = default;

void FullRestoreService::Shutdown() {
  is_shut_down_ = true;
}

void FullRestoreService::ShowRestoreNotification(const std::string& id) {
  message_center::RichNotificationData notification_data;
  message_center::ButtonInfo restore_button(l10n_util::GetStringUTF16(
      base::ToUpperASCII(IDS_RESTORE_NOTIFICATION_RESTORE_BUTTON)));
  notification_data.buttons.push_back(restore_button);
  message_center::ButtonInfo cancel_button(l10n_util::GetStringUTF16(
      base::ToUpperASCII(IDS_RESTORE_NOTIFICATION_CANCEL_BUTTON)));
  notification_data.buttons.push_back(cancel_button);

  std::unique_ptr<message_center::Notification> notification =
      ash::CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE, id,
          l10n_util::GetStringUTF16(IDS_RESTORE_NOTIFICATION_TITLE),
          l10n_util::GetStringUTF16(IDS_RESTORE_FOR_CRASH_NOTIFICATION_MESSAGE),
          l10n_util::GetStringUTF16(IDS_RESTORE_NOTIFICATION_DISPLAY_SOURCE),
          GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kRestoreForCrashNotificationId),
          notification_data,
          base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
              base::BindRepeating(
                  &FullRestoreService::HandleRestoreNotificationClicked,
                  weak_ptr_factory_.GetWeakPtr(),
                  kRestoreForCrashNotificationId)),
          kFullRestoreNotificationIcon,
          message_center::SystemNotificationWarningLevel::NORMAL);
  notification->set_priority(message_center::SYSTEM_PRIORITY);

  auto* notification_display_service =
      NotificationDisplayService::GetForProfile(profile_);
  DCHECK(notification_display_service);
  notification_display_service->Display(NotificationHandler::Type::TRANSIENT,
                                        *notification,
                                        /*metadata=*/nullptr);
}

void FullRestoreService::HandleRestoreNotificationClicked(
    const std::string& id,
    base::Optional<int> button_index) {
  // TODO(crbug.com/909794):Get the user selection and start the restoration.

  if (!is_shut_down_) {
    NotificationDisplayService::GetForProfile(profile_)->Close(
        NotificationHandler::Type::TRANSIENT, id);
  }
}

}  // namespace full_restore
}  // namespace chromeos
