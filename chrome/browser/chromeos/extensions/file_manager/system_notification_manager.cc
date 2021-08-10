// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/system_notification_manager.h"

#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/drive/drivefs_native_message_host.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/extensions/file_manager/drivefs_event_router.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom-forward.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"

namespace {

void CancelCopyOnIOThread(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    storage::FileSystemOperationRunner::OperationID operation_id) {
  file_system_context->operation_runner()->Cancel(
      operation_id, base::BindOnce([](base::File::Error error) {
        DLOG_IF(WARNING, error != base::File::FILE_OK)
            << "Failed to cancel copy: " << error;
      }));
}

constexpr char kSwaFileOperationPrefix[] = "swa-file-operation-";

bool NotificationIdToOperationId(
    const std::string& notification_id,
    storage::FileSystemOperationRunner::OperationID* operation_id) {
  *operation_id = 0;
  std::string id_string;
  if (base::RemoveChars(notification_id, kSwaFileOperationPrefix, &id_string)) {
    if (base::StringToUint64(id_string, operation_id)) {
      return true;
    }
  }

  return false;
}

}  // namespace

namespace file_manager {

SystemNotificationManager::SystemNotificationManager(Profile* profile)
    : profile_(profile),
      swa_enabled_(ash::features::IsFileManagerSwaEnabled()) {}

SystemNotificationManager::~SystemNotificationManager() = default;

bool SystemNotificationManager::DoFilesSwaWindowsExist() {
  return FindSystemWebAppBrowser(profile_, web_app::SystemAppType::FILE_MANAGER,
                                 Browser::TYPE_APP) != nullptr;
}

std::unique_ptr<message_center::Notification>
SystemNotificationManager::CreateNotification(
    const std::string& notification_id,
    const std::u16string& title,
    const std::u16string& message,
    const base::RepeatingClosure& click_callback) {
  return ash::CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id, title, message,
      std::u16string(), GURL(), message_center::NotifierId(),
      message_center::RichNotificationData(),
      new message_center::HandleNotificationClickDelegate(click_callback),
      kNotificationGoogleIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);
}

std::unique_ptr<message_center::Notification>
SystemNotificationManager::CreateNotification(
    const std::string& notification_id,
    const std::u16string& title,
    const std::u16string& message) {
  return CreateNotification(
      notification_id, title, message,
      base::BindRepeating(&SystemNotificationManager::Dismiss,
                          weak_ptr_factory_.GetWeakPtr(), notification_id));
}

std::unique_ptr<message_center::Notification>
SystemNotificationManager::CreateNotification(
    const std::string& notification_id,
    int title_id,
    int message_id) {
  std::u16string title = l10n_util::GetStringUTF16(title_id);
  std::u16string message = l10n_util::GetStringUTF16(message_id);
  return CreateNotification(notification_id, title, message);
}

std::unique_ptr<message_center::Notification>
SystemNotificationManager::CreateNotification(
    const std::string& notification_id,
    int title_id,
    int message_id,
    const scoped_refptr<message_center::NotificationDelegate>& delegate) {
  std::u16string title = l10n_util::GetStringUTF16(title_id);
  std::u16string message = l10n_util::GetStringUTF16(message_id);
  return ash::CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id, title, message,
      std::u16string(), GURL(), message_center::NotifierId(),
      message_center::RichNotificationData(), delegate, kNotificationGoogleIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);
}

void SystemNotificationManager::HandleProgressClick(
    const std::string& notification_id,
    absl::optional<int> button_index) {
  if (button_index) {
    // Cancel the copy operation.
    scoped_refptr<storage::FileSystemContext> file_system_context =
        util::GetFileManagerFileSystemContext(profile_);
    storage::FileSystemOperationRunner::OperationID operation_id;
    if (NotificationIdToOperationId(notification_id, &operation_id)) {
      content::GetIOThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(&CancelCopyOnIOThread, file_system_context,
                                    operation_id));
    }
  }
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
          base::BindRepeating(&SystemNotificationManager::HandleProgressClick,
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

namespace file_manager_private = extensions::api::file_manager_private;

std::unique_ptr<message_center::Notification>
SystemNotificationManager::MakeDriveSyncErrorNotification(
    const extensions::Event& event,
    base::Value::ListView& event_arguments) {
  std::unique_ptr<message_center::Notification> notification;
  file_manager_private::DriveSyncErrorEvent sync_error;
  const char* id;
  std::u16string title =
      l10n_util::GetStringUTF16(IDS_FILE_BROWSER_DRIVE_DIRECTORY_LABEL);
  std::u16string message;
  if (file_manager_private::DriveSyncErrorEvent::Populate(event_arguments[0],
                                                          &sync_error)) {
    id = file_manager_private::ToString(sync_error.type);
    switch (sync_error.type) {
      case file_manager_private::
          DRIVE_SYNC_ERROR_TYPE_DELETE_WITHOUT_PERMISSION:
        message = l10n_util::GetStringFUTF16(
            IDS_FILE_BROWSER_SYNC_DELETE_WITHOUT_PERMISSION_ERROR,
            base::UTF8ToUTF16(event.event_url.ExtractFileName()));
        notification = CreateNotification(id, title, message);
        break;
      case file_manager_private::DRIVE_SYNC_ERROR_TYPE_SERVICE_UNAVAILABLE:
        notification =
            CreateNotification(id, IDS_FILE_BROWSER_DRIVE_DIRECTORY_LABEL,
                               IDS_FILE_BROWSER_SYNC_SERVICE_UNAVAILABLE_ERROR);
        break;
      case file_manager_private::DRIVE_SYNC_ERROR_TYPE_NO_SERVER_SPACE:
        message = l10n_util::GetStringFUTF16(
            IDS_FILE_BROWSER_SYNC_NO_SERVER_SPACE,
            base::UTF8ToUTF16(event.event_url.ExtractFileName()));
        notification = CreateNotification(id, title, message);
        break;
      case file_manager_private::DRIVE_SYNC_ERROR_TYPE_NO_LOCAL_SPACE:
        notification =
            CreateNotification(id, IDS_FILE_BROWSER_DRIVE_DIRECTORY_LABEL,
                               IDS_FILE_BROWSER_DRIVE_OUT_OF_SPACE_HEADER);
        break;
      case file_manager_private::DRIVE_SYNC_ERROR_TYPE_MISC:
        message = l10n_util::GetStringFUTF16(
            IDS_FILE_BROWSER_SYNC_MISC_ERROR,
            base::UTF8ToUTF16(event.event_url.ExtractFileName()));
        notification = CreateNotification(id, title, message);
        break;
      default:
        DLOG(WARNING) << "Unknown Drive Sync error: " << sync_error.type;
        break;
    }
  }
  return notification;
}

const char* kDriveDialogId = "swa-drive-confirm-dialog";

void SystemNotificationManager::HandleDriveDialogClick(
    absl::optional<int> button_index) {
  drivefs::mojom::DialogResult result = drivefs::mojom::DialogResult::kDismiss;
  if (button_index) {
    if (button_index.value() == 1) {
      result = drivefs::mojom::DialogResult::kAccept;
    } else {
      result = drivefs::mojom::DialogResult::kReject;
    }
  }
  // Send the dialog result to the callback stored in DriveFS on dialog
  // creation.
  if (drivefs_event_router_) {
    drivefs_event_router_->OnDialogResult(result);
  }
  GetNotificationDisplayService()->Close(NotificationHandler::Type::TRANSIENT,
                                         kDriveDialogId);
}

std::unique_ptr<message_center::Notification>
SystemNotificationManager::MakeDriveConfirmDialogNotification(
    const extensions::Event& event,
    base::Value::ListView& event_arguments) {
  std::unique_ptr<message_center::Notification> notification;
  file_manager_private::DriveConfirmDialogEvent dialog_event;
  const char* id;
  std::u16string title =
      l10n_util::GetStringUTF16(IDS_FILE_BROWSER_DRIVE_DIRECTORY_LABEL);
  std::u16string message;
  if (file_manager_private::DriveConfirmDialogEvent::Populate(
          event_arguments[0], &dialog_event)) {
    std::vector<message_center::ButtonInfo> notification_buttons;
    id = file_manager_private::ToString(dialog_event.type);
    scoped_refptr<message_center::NotificationDelegate> delegate =
        new message_center::HandleNotificationClickDelegate(base::BindRepeating(
            &SystemNotificationManager::HandleDriveDialogClick,
            weak_ptr_factory_.GetWeakPtr()));
    notification = CreateNotification(
        kDriveDialogId, IDS_FILE_BROWSER_DRIVE_DIRECTORY_LABEL,
        IDS_FILE_BROWSER_OFFLINE_ENABLE_MESSAGE, delegate);

    notification_buttons.push_back(message_center::ButtonInfo(
        l10n_util::GetStringUTF16(IDS_FILE_BROWSER_OFFLINE_ENABLE_REJECT)));
    notification_buttons.push_back(message_center::ButtonInfo(
        l10n_util::GetStringUTF16(IDS_FILE_BROWSER_OFFLINE_ENABLE_ACCEPT)));
    notification->set_buttons(notification_buttons);
  }
  return notification;
}

void SystemNotificationManager::HandleEvent(const extensions::Event& event) {
  if (!swa_enabled_) {
    return;
  }
  base::Value::ListView event_arguments;

  event_arguments = event.event_args->GetList();
  if (event_arguments.size() < 1) {
    return;
  }
  std::unique_ptr<message_center::Notification> notification;
  switch (event.histogram_value) {
    case extensions::events::FILE_MANAGER_PRIVATE_ON_DRIVE_SYNC_ERROR:
      notification = MakeDriveSyncErrorNotification(event, event_arguments);
      break;
    case extensions::events::FILE_MANAGER_PRIVATE_ON_DRIVE_CONFIRM_DIALOG:
      notification = MakeDriveConfirmDialogNotification(event, event_arguments);
      break;
    default:
      DLOG(WARNING) << "Unhandled event: " << event.event_name;
      break;
  }

  if (notification) {
    GetNotificationDisplayService()->Display(
        NotificationHandler::Type::TRANSIENT, *notification,
        /*metadata=*/nullptr);
  }
}

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
  // Check if we need to remove any progress notification when there
  // are active SWA windows.
  if (DoFilesSwaWindowsExist()) {
    GetNotificationDisplayService()->Close(NotificationHandler::Type::TRANSIENT,
                                           id);
    return;
  }
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
      notification->set_buttons({message_center::ButtonInfo(
          l10n_util::GetStringUTF16(IDS_FILE_BROWSER_CANCEL_LABEL))});
      break;
    case file_manager_private::COPY_OR_MOVE_PROGRESS_STATUS_TYPE_PROGRESS:
      if (copy_operation != required_copy_space_.end()) {
        if (status.size) {
          progress =
              static_cast<int>((*status.size / copy_operation->second) * 100.0);
        }
      }
      notification = CreateProgressNotification(id, title, message, progress);
      notification->set_buttons({message_center::ButtonInfo(
          l10n_util::GetStringUTF16(IDS_FILE_BROWSER_CANCEL_LABEL))});
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

const char* kRemovableNotificationId = "swa-removable-device-id";

void SystemNotificationManager::HandleRemovableNotificationClick(
    const std::string& path,
    absl::optional<int> button_index) {
  if (button_index) {
    if (button_index.value() == 0) {
      base::FilePath volume_root(path);
      platform_util::ShowItemInFolder(profile_, volume_root);
    } else {
      chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
          profile_, chromeos::settings::mojom::kExternalStorageSubpagePath);
    }
  }

  GetNotificationDisplayService()->Close(NotificationHandler::Type::TRANSIENT,
                                         kRemovableNotificationId);
}

std::unique_ptr<message_center::Notification>
SystemNotificationManager::MakeMountErrorNotification(
    file_manager_private::MountCompletedEvent& event,
    const Volume& volume) {
  std::unique_ptr<message_center::Notification> notification;
  scoped_refptr<message_center::NotificationDelegate> delegate =
      new message_center::HandleNotificationClickDelegate(base::BindRepeating(
          &SystemNotificationManager::HandleRemovableNotificationClick,
          weak_ptr_factory_.GetWeakPtr(), volume.mount_path().value()));
  switch (event.status) {
    case file_manager_private::
        MOUNT_COMPLETED_STATUS_ERROR_UNSUPPORTED_FILESYSTEM:
      notification = CreateNotification(
          kRemovableNotificationId, IDS_REMOVABLE_DEVICE_DETECTION_TITLE,
          IDS_DEVICE_UNSUPPORTED_DEFAULT_MESSAGE, delegate);
      break;
    default:
      DLOG(WARNING) << "Unhandled mount error for " << event.status;
      break;
  }
  return notification;
}

std::unique_ptr<message_center::Notification>
SystemNotificationManager::MakeRemovableNotification(
    file_manager_private::MountCompletedEvent& event,
    const Volume& volume) {
  if (event.status != file_manager_private::MOUNT_COMPLETED_STATUS_SUCCESS) {
    return MakeMountErrorNotification(event, volume);
  }
  int message_id;
  if (volume.is_read_only() && !volume.is_read_only_removable_device()) {
    message_id = IDS_REMOVABLE_DEVICE_NAVIGATION_MESSAGE_READONLY_POLICY;
  } else {
    message_id = IDS_REMOVABLE_DEVICE_NAVIGATION_MESSAGE;
  }
  scoped_refptr<message_center::NotificationDelegate> delegate =
      new message_center::HandleNotificationClickDelegate(base::BindRepeating(
          &SystemNotificationManager::HandleRemovableNotificationClick,
          weak_ptr_factory_.GetWeakPtr(), volume.mount_path().value()));
  std::unique_ptr<message_center::Notification> notification =
      CreateNotification(kRemovableNotificationId,
                         IDS_REMOVABLE_DEVICE_DETECTION_TITLE, message_id,
                         delegate);

  std::vector<message_center::ButtonInfo> notification_buttons;
  notification_buttons.push_back(message_center::ButtonInfo(
      l10n_util::GetStringUTF16(IDS_REMOVABLE_DEVICE_NAVIGATION_BUTTON_LABEL)));
  notification_buttons.push_back(
      message_center::ButtonInfo(l10n_util::GetStringUTF16(
          IDS_REMOVABLE_DEVICE_OPEN_SETTTINGS_BUTTON_LABEL)));
  notification->set_buttons(notification_buttons);

  return notification;
}

void SystemNotificationManager::HandleMountCompletedEvent(
    file_manager_private::MountCompletedEvent& event,
    const Volume& volume) {
  if (!swa_enabled_) {
    return;
  }
  std::unique_ptr<message_center::Notification> notification;

  switch (event.event_type) {
    case file_manager_private::MOUNT_COMPLETED_EVENT_TYPE_MOUNT:
      if (event.should_notify) {
        notification = MakeRemovableNotification(event, volume);
      }
      break;
    default:
      DLOG(WARNING) << "Unhandled mount event for type " << event.event_type;
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

void SystemNotificationManager::SetDriveFSEventRouter(
    DriveFsEventRouter* drivefs_event_router) {
  drivefs_event_router_ = drivefs_event_router;
}

}  // namespace file_manager
