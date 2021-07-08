// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/system_notification_manager.h"

#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"

namespace file_manager {

SystemNotificationManager::SystemNotificationManager(Profile* profile)
    : profile_(profile),
      swa_enabled_(ash::features::IsFileManagerSwaEnabled()) {}

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
SystemNotificationManager::CreateProgressNotification(
    const std::string& notification_id,
    const std::u16string& title,
    const std::u16string& message,
    int progress) {
  std::unique_ptr<message_center::RichNotificationData> rich_data =
      std::make_unique<message_center::RichNotificationData>();

  rich_data->progress = progress;
  return ash::CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_PROGRESS, notification_id, title,
      message, std::u16string(), GURL(), message_center::NotifierId(),
      *rich_data.get(),
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
  GetNotificationDisplayService()->Close(NotificationHandler::Type::TRANSIENT,
                                         notification_id);
}

void SystemNotificationManager::HandleDeviceEvent(
    const file_manager_private::DeviceEvent& event) {
  if (!swa_enabled_) {
    return;
  }
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

void SystemNotificationManager::HandleCopyStart(
    int copy_id,
    file_manager_private::CopyOrMoveProgressStatus& status) {
  if (!swa_enabled_) {
    return;
  }
  if (status.size) {
    required_copy_space_[copy_id] = *status.size;
  }
}

const char* kSwaFileOperationPrefix = "swa-file-operation-";

namespace file_manager_private = extensions::api::file_manager_private;

void SystemNotificationManager::HandleCopyEvent(
    int copy_id,
    file_manager_private::CopyOrMoveProgressStatus& status) {
  if (!swa_enabled_) {
    return;
  }
  std::unique_ptr<message_center::Notification> notification;
  int progress = 0;
  std::string id =
      base::StrCat({kSwaFileOperationPrefix, base::NumberToString(copy_id)});
  // TODO(b/187656842) In legacy Files App this comes from
  // chrome.runtime.getManifest().name FIX.
  std::u16string title =
      l10n_util::GetStringUTF16(IDS_FILE_BROWSER_GRID_VIEW_FILES_TITLE);

  std::u16string message;
  if (status.source_url) {
    GURL source_gurl(*status.source_url);
    message = l10n_util::GetStringFUTF16(
        IDS_FILE_BROWSER_COPY_FILE_NAME,
        base::UTF8ToUTF16(source_gurl.ExtractFileName()));
  } else {
    message = l10n_util::GetStringUTF16(IDS_FILE_BROWSER_FILE_ERROR_GENERIC);
  }

  auto copy_operation = required_copy_space_.find(copy_id);
  switch (status.type) {
    case file_manager_private::COPY_OR_MOVE_PROGRESS_STATUS_TYPE_BEGIN:
      notification = CreateProgressNotification(id, title, message, 0);
      break;
    case file_manager_private::COPY_OR_MOVE_PROGRESS_STATUS_TYPE_PROGRESS:
      if (copy_operation != required_copy_space_.end()) {
        if (status.size) {
          progress =
              static_cast<int>((*status.size / copy_operation->second) * 100.0);
        }
      }
      notification = CreateProgressNotification(id, title, message, progress);
      break;
    case file_manager_private::COPY_OR_MOVE_PROGRESS_STATUS_TYPE_END_COPY:
      notification = CreateProgressNotification(id, title, message, 100);
      break;
    case file_manager_private::COPY_OR_MOVE_PROGRESS_STATUS_TYPE_SUCCESS:
    case file_manager_private::COPY_OR_MOVE_PROGRESS_STATUS_TYPE_ERROR:
      GetNotificationDisplayService()->Close(
          NotificationHandler::Type::TRANSIENT, id);
      required_copy_space_.erase(copy_id);
      break;
    default:
      DLOG(WARNING) << "Unhandled copy event for type " << status.type;
      break;
  }

  if (notification) {
    GetNotificationDisplayService()->Display(
        NotificationHandler::Type::TRANSIENT, *notification,
        /*metadata=*/nullptr);
  }
}

NotificationDisplayService*
SystemNotificationManager::GetNotificationDisplayService() {
  return NotificationDisplayServiceFactory::GetForProfile(profile_);
}

}  // namespace file_manager
