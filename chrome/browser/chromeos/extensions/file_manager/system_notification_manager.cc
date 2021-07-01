// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/system_notification_manager.h"

#include "base/bind.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"

namespace file_manager {

SystemNotificationManager::SystemNotificationManager(Profile* profile)
    : profile_(profile) {}

SystemNotificationManager::~SystemNotificationManager() = default;

bool SystemNotificationManager::DoFilesSwaWindowsExist() {
  return false;
}

std::unique_ptr<message_center::Notification>
SystemNotificationManager::CreateNotification(
    const std::string& notification_id,
    const std::u16string& title,
    const std::u16string& message) {
  return ash::CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id, title, message,
      std::u16string(), GURL(), message_center::NotifierId(),
      message_center::RichNotificationData(),
      new message_center::HandleNotificationClickDelegate(
          base::BindRepeating(&SystemNotificationManager::Dismiss,
                              weak_ptr_factory_.GetWeakPtr(), notification_id)),
      kNotificationGoogleIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);
}

std::unique_ptr<message_center::Notification>
SystemNotificationManager::CreateNotification(
    const std::string& notification_id,
    int title_id,
    int message_id) {
  return ash::CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
      l10n_util::GetStringUTF16(title_id),
      l10n_util::GetStringUTF16(message_id), std::u16string(), GURL(),
      message_center::NotifierId(), message_center::RichNotificationData(),
      new message_center::HandleNotificationClickDelegate(
          base::BindRepeating(&SystemNotificationManager::Dismiss,
                              weak_ptr_factory_.GetWeakPtr(), notification_id)),
      kNotificationGoogleIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);
}

void SystemNotificationManager::Dismiss(const std::string& notification_id) {
  SystemNotificationHelper::GetInstance()->Close(notification_id);
}

void SystemNotificationManager::HandleDeviceEvent(
    const file_manager_private::DeviceEvent& event) {
  std::unique_ptr<message_center::Notification> notification;

  const char* id = file_manager_private::ToString(event.type);
  switch (event.type) {
    case file_manager_private::DEVICE_EVENT_TYPE_DISABLED:
      notification =
          CreateNotification(id, IDS_REMOVABLE_DEVICE_DETECTION_TITLE,
                             IDS_EXTERNAL_STORAGE_DISABLED_MESSAGE);
      break;
    case file_manager_private::DEVICE_EVENT_TYPE_REMOVED:
      // TODO(b/188487301) Hide device fail & storage disabled notifications.
      break;
    case file_manager_private::DEVICE_EVENT_TYPE_HARD_UNPLUGGED:
      notification = CreateNotification(id, IDS_DEVICE_HARD_UNPLUGGED_TITLE,
                                        IDS_DEVICE_HARD_UNPLUGGED_MESSAGE);
      break;
    case file_manager_private::DEVICE_EVENT_TYPE_FORMAT_START:
      notification =
          CreateNotification(id, IDS_FILE_BROWSER_FORMAT_DIALOG_TITLE,
                             IDS_FILE_BROWSER_FORMAT_PROGRESS_MESSAGE);
      break;
    case file_manager_private::DEVICE_EVENT_TYPE_FORMAT_SUCCESS:
      notification =
          CreateNotification(id, IDS_FILE_BROWSER_FORMAT_DIALOG_TITLE,
                             IDS_FILE_BROWSER_FORMAT_SUCCESS_MESSAGE);
      break;
    case file_manager_private::DEVICE_EVENT_TYPE_FORMAT_FAIL:
      notification =
          CreateNotification(id, IDS_FILE_BROWSER_FORMAT_DIALOG_TITLE,
                             IDS_FILE_BROWSER_FORMAT_FAILURE_MESSAGE);
      break;
    case file_manager_private::DEVICE_EVENT_TYPE_RENAME_FAIL:
      notification =
          CreateNotification(id, IDS_RENAMING_OF_DEVICE_FAILED_TITLE,
                             IDS_RENAMING_OF_DEVICE_FINISHED_FAILURE_MESSAGE);
      break;
    case file_manager_private::DEVICE_EVENT_TYPE_PARTITION_START:
      notification =
          CreateNotification(id, IDS_FILE_BROWSER_FORMAT_DIALOG_TITLE,
                             IDS_FILE_BROWSER_FORMAT_PROGRESS_MESSAGE);
      break;
    case file_manager_private::DEVICE_EVENT_TYPE_PARTITION_SUCCESS:
      notification =
          CreateNotification(id, IDS_FILE_BROWSER_FORMAT_DIALOG_TITLE,
                             IDS_FILE_BROWSER_FORMAT_SUCCESS_MESSAGE);
      break;
    case file_manager_private::DEVICE_EVENT_TYPE_PARTITION_FAIL:
      notification =
          CreateNotification(id, IDS_FILE_BROWSER_FORMAT_DIALOG_TITLE,
                             IDS_FILE_BROWSER_FORMAT_FAILURE_MESSAGE);
      break;
    default:
      DLOG(WARNING) << "Unable to generate notification for " << id;
      break;
  }

  if (notification) {
    GetNotificationDisplayService()->Display(
        NotificationHandler::Type::TRANSIENT, *notification,
        /*metadata=*/nullptr);
  }
}

void SystemNotificationManager::HandleEvent(const extensions::Event& event) {}

void SystemNotificationManager::HandleCopyEvent(
    int copy_id,
    file_manager_private::CopyOrMoveProgressStatus& status) {}

NotificationDisplayService*
SystemNotificationManager::GetNotificationDisplayService() {
  return NotificationDisplayServiceFactory::GetForProfile(profile_);
}

}  // namespace file_manager
