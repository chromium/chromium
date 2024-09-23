// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/file_manager/system_notification_manager.h"

#include <optional>
#include <string>

#include "ash/components/arc/arc_prefs.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/webui/file_manager/file_manager_ui.h"
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/cxx23_to_underlying.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/extensions/file_manager/drivefs_event_router.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/io_task_controller.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_dialog.h"
#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager.h"
#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager_factory.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"
#include "ui/message_center/public/cpp/notification.h"

namespace file_manager {
namespace {

namespace fmp = extensions::api::file_manager_private;

using base::BindRepeating;
using base::MakeRefCounted;
using base::RepeatingClosure;
using base::UTF8ToUTF16;
using extensions::Event;
using file_manager::io_task::IOTaskController;
using file_manager::io_task::IOTaskId;
using file_manager::io_task::OperationType;
using file_manager::io_task::ProgressStatus;
using file_manager::util::GetDisplayablePath;
using fmp::MountCompletedEvent;
using fmp::ToString;
using l10n_util::GetStringFUTF16;
using l10n_util::GetStringUTF16;
using message_center::ButtonInfo;
using message_center::HandleNotificationClickDelegate;
using message_center::Notification;
using message_center::NotificationDelegate;
using message_center::NotifierId;
using message_center::RichNotificationData;
using message_center::SystemNotificationWarningLevel;
using NotificationPtr = std::unique_ptr<Notification>;
using DelegatePtr = scoped_refptr<NotificationDelegate>;
using OperationID = storage::FileSystemOperationRunner::OperationID;
using FileSystemContextPtr = scoped_refptr<storage::FileSystemContext>;

using enum extensions::events::HistogramValue;
using enum message_center::NotificationType;

void CancelCopyOnIOThread(FileSystemContextPtr file_system_context,
                          OperationID operation_id) {
  file_system_context->operation_runner()->Cancel(
      operation_id, base::BindOnce([](base::File::Error error) {
        DLOG_IF(WARNING, error != base::File::FILE_OK)
            << "Failed to cancel copy: " << error;
      }));
}

constexpr char kSwaFileOperationPrefix[] = "swa-file-operation-";

bool NotificationIdToOperationId(const std::string& notification_id,
                                 OperationID* operation_id) {
  *operation_id = 0;
  std::string id_string;
  if (base::RemoveChars(notification_id, kSwaFileOperationPrefix, &id_string)) {
    if (base::StringToUint64(id_string, operation_id)) {
      return true;
    }
  }

  return false;
}

void RecordDeviceNotificationMetric(DeviceNotificationUmaType type) {
  UMA_HISTOGRAM_ENUMERATION(kNotificationShowHistogramName, type);
}

void RecordDeviceNotificationUserActionMetric(
    DeviceNotificationUserActionUmaType type) {
  UMA_HISTOGRAM_ENUMERATION(kNotificationUserActionHistogramName, type);
}

std::u16string GetIOTaskMessage(Profile* profile,
                                const ProgressStatus& status) {
  int single_file_message_id;
  int multiple_file_message_id;

  // Display special copy to help users understand that pasting files to "My
  // Drive" does not mean that they are immediately synced.
  drive::DriveIntegrationService* const service =
      drive::util::GetIntegrationServiceByProfile(profile);
  bool is_destination_drive =
      service && service->GetMountPointPath().IsParent(
                     status.GetDestinationFolder().path());

  switch (status.type) {
    case OperationType::kCopy:
      if (is_destination_drive) {
        single_file_message_id = IDS_FILE_BROWSER_PREPARING_FILE_NAME_MY_DRIVE;
        multiple_file_message_id = IDS_FILE_BROWSER_PREPARING_ITEMS_MY_DRIVE;
      } else {
        single_file_message_id = IDS_FILE_BROWSER_COPY_FILE_NAME;
        multiple_file_message_id = IDS_FILE_BROWSER_COPY_ITEMS_REMAINING;
      }
      break;

    case OperationType::kMove:
      if (is_destination_drive) {
        single_file_message_id = IDS_FILE_BROWSER_PREPARING_FILE_NAME_MY_DRIVE;
        multiple_file_message_id = IDS_FILE_BROWSER_PREPARING_ITEMS_MY_DRIVE;
      } else {
        single_file_message_id = IDS_FILE_BROWSER_MOVE_FILE_NAME;
        multiple_file_message_id = IDS_FILE_BROWSER_MOVE_ITEMS_REMAINING;
      }
      break;

    case OperationType::kDelete:
      single_file_message_id = IDS_FILE_BROWSER_DELETE_FILE_NAME;
      multiple_file_message_id = IDS_FILE_BROWSER_DELETE_ITEMS_REMAINING;
      break;

    case OperationType::kExtract:
      single_file_message_id = IDS_FILE_BROWSER_EXTRACT_FILE_NAME;
      multiple_file_message_id = IDS_FILE_BROWSER_EXTRACT_ITEMS_REMAINING;
      break;

    case OperationType::kZip:
      single_file_message_id = IDS_FILE_BROWSER_ZIP_FILE_NAME;
      multiple_file_message_id = IDS_FILE_BROWSER_ZIP_ITEMS_REMAINING;
      break;

    case OperationType::kRestoreToDestination:
      single_file_message_id = IDS_FILE_BROWSER_RESTORING_FROM_TRASH_FILE_NAME;
      multiple_file_message_id =
          IDS_FILE_BROWSER_RESTORING_FROM_TRASH_ITEMS_REMAINING;
      break;

    case OperationType::kTrash:
      single_file_message_id = IDS_FILE_BROWSER_MOVE_TO_TRASH_FILE_NAME;
      multiple_file_message_id = IDS_FILE_BROWSER_MOVE_TO_TRASH_ITEMS_REMAINING;
      break;

    case OperationType::kEmptyTrash:
    case OperationType::kRestore:
    default:
      NOTREACHED_IN_MIGRATION() << "Unexpected operation type " << status.type;
      return u"Unknown operation type";
  }

  if (status.sources.size() > 1) {
    return GetStringFUTF16(multiple_file_message_id,
                           base::NumberToString16(status.sources.size()));
  }

  return GetStringFUTF16(
      single_file_message_id,
      UTF8ToUTF16(GetDisplayablePath(profile, status.sources.back().url)
                      .value_or(base::FilePath())
                      .BaseName()
                      .value()));
}
}  // namespace

std::string GetNotificationId(io_task::IOTaskId task_id) {
  return base::StrCat({kSwaFileOperationPrefix, base::NumberToString(task_id)});
}

NotificationPtr CreateSystemNotification(
    const std::string& notification_id,
    const std::u16string& title,
    const std::u16string& message,
    DelegatePtr delegate,
    message_center::RichNotificationData optional_fields) {
  return ash::CreateSystemNotificationPtr(
      NOTIFICATION_TYPE_SIMPLE, notification_id, title, message,
      GetStringUTF16(IDS_FILEMANAGER_APP_NAME), GURL(), NotifierId(),
      optional_fields, std::move(delegate), ash::kFolderIcon,
      SystemNotificationWarningLevel::NORMAL);
}

NotificationPtr CreateSystemNotification(const std::string& notification_id,
                                         int title_id,
                                         int message_id,
                                         DelegatePtr delegate) {
  return CreateSystemNotification(notification_id, GetStringUTF16(title_id),
                                  GetStringUTF16(message_id),
                                  std::move(delegate));
}

NotificationPtr CreateSystemNotification(
    const std::string& notification_id,
    const std::u16string& title,
    const std::u16string& message,
    const RepeatingClosure& click_callback) {
  return CreateSystemNotification(
      notification_id, title, message,
      MakeRefCounted<HandleNotificationClickDelegate>(click_callback));
}

SystemNotificationManager::SystemNotificationManager(Profile* profile)
    : profile_(profile), app_name_(GetStringUTF16(IDS_FILEMANAGER_APP_NAME)) {}

SystemNotificationManager::~SystemNotificationManager() = default;

bool SystemNotificationManager::DoFilesSwaWindowsExist() {
  return ash::file_manager::FileManagerUI::GetNumInstances() != 0;
}

NotificationPtr SystemNotificationManager::CreateNotification(
    const std::string& notification_id,
    const std::u16string& title,
    const std::u16string& message) {
  return CreateSystemNotification(
      notification_id, title, message,
      BindRepeating(&SystemNotificationManager::Dismiss,
                    weak_ptr_factory_.GetWeakPtr(), notification_id));
}

NotificationPtr SystemNotificationManager::CreateNotification(
    const std::string& notification_id,
    int title_id,
    int message_id) {
  return CreateNotification(notification_id, GetStringUTF16(title_id),
                            GetStringUTF16(message_id));
}

void SystemNotificationManager::HandleProgressClick(
    const std::string& notification_id,
    std::optional<int> button_index) {
  if (button_index) {
    // Cancel the copy operation.
    FileSystemContextPtr file_system_context =
        util::GetFileManagerFileSystemContext(profile_);
    OperationID operation_id;
    if (NotificationIdToOperationId(notification_id, &operation_id)) {
      content::GetIOThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(&CancelCopyOnIOThread, file_system_context,
                                    operation_id));
    }
  }
}

NotificationPtr SystemNotificationManager::CreateProgressNotification(
    const std::string& notification_id,
    const std::u16string& title,
    const std::u16string& message,
    int progress) {
  RichNotificationData rich_data;
  rich_data.progress = progress;
  rich_data.progress_status = message;

  return ash::CreateSystemNotificationPtr(
      NOTIFICATION_TYPE_PROGRESS, notification_id, title, message, app_name_,
      GURL(), NotifierId(), rich_data,
      MakeRefCounted<HandleNotificationClickDelegate>(
          BindRepeating(&SystemNotificationManager::HandleProgressClick,
                        weak_ptr_factory_.GetWeakPtr(), notification_id)),
      ash::kFolderIcon, SystemNotificationWarningLevel::NORMAL);
}

NotificationPtr SystemNotificationManager::CreateIOTaskProgressNotification(
    IOTaskId task_id,
    const std::string& notification_id,
    const std::u16string& title,
    const std::u16string& message,
    const bool paused,
    int progress) {
  RichNotificationData rich_data;
  rich_data.progress = progress;
  rich_data.progress_status = message;

  // Button click delegate to handle the state::PAUSED IOTask case, where the
  // user [X] closes this system notification, but did not press its buttons.
  // In that case, default behavior is to auto-click button 1.
  // TODO(b/255264604): ask UX here, which button should be the default?
  class IOTaskProgressNotificationClickDelegate
      : public HandleNotificationClickDelegate {
   public:
    IOTaskProgressNotificationClickDelegate(const ButtonClickCallback& callback,
                                            bool paused)
        : HandleNotificationClickDelegate(callback), paused_(paused) {}

    void Close(bool by_user) override {
      if (paused_ && by_user) {  // Click button at index 1.
        HandleNotificationClickDelegate::Click(1, {});
      }
    }

   protected:
    ~IOTaskProgressNotificationClickDelegate() override = default;

   private:
    bool paused_;  // True if the IOTask is in state::PAUSED.
  };

  auto notification_click_handler = BindRepeating(
      &SystemNotificationManager::HandleIOTaskProgressNotificationClick,
      weak_ptr_factory_.GetWeakPtr(), task_id, notification_id, paused);

  auto notification = ash::CreateSystemNotificationPtr(
      NOTIFICATION_TYPE_PROGRESS, notification_id, title, message, app_name_,
      GURL(), NotifierId(), rich_data,
      MakeRefCounted<IOTaskProgressNotificationClickDelegate>(
          std::move(notification_click_handler), paused),
      ash::kFolderIcon, SystemNotificationWarningLevel::NORMAL);

  std::vector<ButtonInfo> notification_buttons;

  // Add "Cancel" button.
  notification_buttons.emplace_back(
      GetStringUTF16(IDS_FILE_BROWSER_CANCEL_LABEL));

  if (paused) {  // For paused tasks, add "Open Files app" button.
    notification_buttons.emplace_back(
        GetStringUTF16(IDS_REMOVABLE_DEVICE_NAVIGATION_BUTTON_LABEL));
  }

  notification->set_buttons(notification_buttons);
  return notification;
}

void SystemNotificationManager::HandleIOTaskProgressNotificationClick(
    IOTaskId task_id,
    const std::string& notification_id,
    const bool paused,
    std::optional<int> button_index) {
  if (!button_index.has_value()) {
    return;
  }

  if (button_index.value() == 0) {
    CancelTask(task_id);
  }

  if (paused && button_index.value() == 1) {
    platform_util::ShowItemInFolder(
        profile_, file_manager::util::GetMyFilesFolderForProfile(profile_));
    Dismiss(notification_id);
  }
}

void SystemNotificationManager::Dismiss(const std::string& notification_id) {
  GetNotificationDisplayService()->Close(NotificationHandler::Type::TRANSIENT,
                                         notification_id);
}

static const char kDeviceFailNotificationId[] = "swa-device-fail-id";

void SystemNotificationManager::HandleDeviceEvent(
    const fmp::DeviceEvent& event) {
  NotificationPtr notification;
  const std::string id = ToString(event.type);
  switch (event.type) {
    case fmp::DeviceEventType::kDisabled:
      notification =
          CreateNotification(id, IDS_REMOVABLE_DEVICE_DETECTION_TITLE,
                             IDS_EXTERNAL_STORAGE_DISABLED_MESSAGE);
      RecordDeviceNotificationMetric(
          DeviceNotificationUmaType::DEVICE_EXTERNAL_STORAGE_DISABLED);
      break;

    case fmp::DeviceEventType::kRemoved:
      // Hide device fail & storage disabled notifications.
      GetNotificationDisplayService()->Close(
          NotificationHandler::Type::TRANSIENT, kDeviceFailNotificationId);
      GetNotificationDisplayService()->Close(
          NotificationHandler::Type::TRANSIENT,
          ToString(fmp::DeviceEventType::kDisabled));
      // Remove the device from the mount status map.
      mount_status_.erase(event.device_path);
      break;

    case fmp::DeviceEventType::kHardUnplugged:
      notification = CreateNotification(id, IDS_DEVICE_HARD_UNPLUGGED_TITLE,
                                        IDS_DEVICE_HARD_UNPLUGGED_MESSAGE);
      RecordDeviceNotificationMetric(
          DeviceNotificationUmaType::DEVICE_HARD_UNPLUGGED);
      break;

    case fmp::DeviceEventType::kFormatStart:
      notification = CreateNotification(
          id,
          GetStringFUTF16(IDS_FILE_BROWSER_FORMAT_DIALOG_TITLE,
                          UTF8ToUTF16(event.device_label)),
          GetStringFUTF16(IDS_FILE_BROWSER_FORMAT_PROGRESS_MESSAGE,
                          UTF8ToUTF16(event.device_label)));
      RecordDeviceNotificationMetric(DeviceNotificationUmaType::FORMAT_START);
      break;

    case fmp::DeviceEventType::kFormatSuccess:
    case fmp::DeviceEventType::kFormatFail:
    case fmp::DeviceEventType::kPartitionFail: {
      // Hide the formatting notification.
      GetNotificationDisplayService()->Close(
          NotificationHandler::Type::TRANSIENT,
          ToString(fmp::DeviceEventType::kFormatStart));
      std::u16string message;
      if (event.type == fmp::DeviceEventType::kFormatSuccess) {
        message = GetStringFUTF16(IDS_FILE_BROWSER_FORMAT_SUCCESS_MESSAGE,
                                  UTF8ToUTF16(event.device_label));
        RecordDeviceNotificationMetric(
            DeviceNotificationUmaType::FORMAT_SUCCESS);
      } else {
        message = GetStringFUTF16(IDS_FILE_BROWSER_FORMAT_FAILURE_MESSAGE,
                                  UTF8ToUTF16(event.device_label));
        RecordDeviceNotificationMetric(
            event.type == fmp::DeviceEventType::kFormatFail
                ? DeviceNotificationUmaType::FORMAT_FAIL
                : DeviceNotificationUmaType::PARTITION_FAIL);
      }
      notification = CreateNotification(
          id,
          GetStringFUTF16(IDS_FILE_BROWSER_FORMAT_DIALOG_TITLE,
                          UTF8ToUTF16(event.device_label)),
          std::move(message));
      break;
    }

    case fmp::DeviceEventType::kPartitionStart:
    case fmp::DeviceEventType::kPartitionSuccess:
      // No-op.
      break;

    case fmp::DeviceEventType::kRenameFail:
      notification =
          CreateNotification(id, IDS_RENAMING_OF_DEVICE_FAILED_TITLE,
                             IDS_RENAMING_OF_DEVICE_FINISHED_FAILURE_MESSAGE);
      RecordDeviceNotificationMetric(DeviceNotificationUmaType::RENAME_FAIL);
      break;

    case fmp::DeviceEventType::kNone:
    case fmp::DeviceEventType::kRenameStart:
    case fmp::DeviceEventType::kRenameSuccess:
    default:
      VLOG(1) << "No notification for device event " << id;
      break;
  }

  if (notification) {
    GetNotificationDisplayService()->Display(
        NotificationHandler::Type::TRANSIENT, *notification,
        /*metadata=*/nullptr);
  }
}

static const char kBulkPinningNotificationId[] = "drive-bulk-pinning-error";

void SystemNotificationManager::HandleBulkPinningNotificationClick() {
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      profile_, chromeos::settings::mojom::kGoogleDriveSubpagePath);
  GetNotificationDisplayService()->Close(NotificationHandler::Type::TRANSIENT,
                                         kBulkPinningNotificationId);
}

NotificationPtr SystemNotificationManager::MakeBulkPinningErrorNotification(
    const Event& event) {
  // Parse the event args as a bulk-pinning progress struct.
  DCHECK(!event.event_args.empty());
  auto progress = fmp::BulkPinProgress::FromValue(event.event_args[0]);
  if (!progress) {
    LOG(ERROR) << "Cannot parse BulkPinProgress from " << event.event_args[0];
    return nullptr;
  }

  // Remember the bulk-pinning stage.
  using enum BulkPinStage;
  const BulkPinStage old_stage = bulk_pin_stage_;
  bulk_pin_stage_ = progress->stage;

  if (!progress->should_pin) {
    return nullptr;
  }

  if (old_stage != BulkPinStage::kListingFiles &&
      old_stage != BulkPinStage::kSyncing) {
    return nullptr;
  }

  // Check the bulk-pinning stage.
  switch (bulk_pin_stage_) {
    case BulkPinStage::kNone:
    case BulkPinStage::kStopped:
    case BulkPinStage::kPausedOffline:
    case BulkPinStage::kPausedBatterySaver:
    case BulkPinStage::kGettingFreeSpace:
    case BulkPinStage::kListingFiles:
    case BulkPinStage::kSyncing:
    case BulkPinStage::kSuccess:
      return nullptr;

    case BulkPinStage::kNotEnoughSpace:
    case BulkPinStage::kCannotGetFreeSpace:
    case BulkPinStage::kCannotListFiles:
    case BulkPinStage::kCannotEnableDocsOffline:
      break;
  }

  VLOG(1) << "Creating bulk-pinning error notification";
  int title_id, message_id;

  if (bulk_pin_stage_ == BulkPinStage::kNotEnoughSpace) {
    if (progress->emptied_queue) {
      title_id = IDS_FILE_BROWSER_DRIVE_SYNC_TURNED_OFF_TITLE;
      message_id =
          IDS_FILE_BROWSER_BULK_PINNING_NOT_ENOUGH_SPACE_NOTIFICATION_2;
    } else {
      title_id = IDS_FILE_BROWSER_DRIVE_SYNC_ERROR_TITLE;
      message_id = IDS_FILE_BROWSER_BULK_PINNING_NOT_ENOUGH_SPACE_NOTIFICATION;
    }
  } else {
    title_id = IDS_FILE_BROWSER_DRIVE_SYNC_TURNED_OFF_TITLE;
    message_id = IDS_FILE_BROWSER_BULK_PINNING_ERROR;
  }

  NotificationPtr notification = CreateSystemNotification(
      kBulkPinningNotificationId, GetStringUTF16(title_id),
      GetStringUTF16(message_id),
      BindRepeating(
          &SystemNotificationManager::HandleBulkPinningNotificationClick,
          weak_ptr_factory_.GetWeakPtr()));

  return notification;
}

NotificationPtr SystemNotificationManager::MakeDriveSyncErrorNotification(
    const Event& event) {
  DCHECK(!event.event_args.empty());
  auto sync_error = fmp::DriveSyncErrorEvent::FromValue(event.event_args[0]);
  if (!sync_error) {
    LOG(ERROR) << "Cannot parse DriveSyncErrorEvent from "
               << event.event_args[0];
    return nullptr;
  }

  const std::u16string title =
      GetStringUTF16(IDS_FILE_BROWSER_DRIVE_DIRECTORY_LABEL);
  const std::string id = ToString(sync_error->type);
  const GURL file_url(sync_error->file_url);

  switch (sync_error->type) {
    case fmp::DriveSyncErrorType::kDeleteWithoutPermission:
      return CreateNotification(
          id, title,
          GetStringFUTF16(IDS_FILE_BROWSER_SYNC_DELETE_WITHOUT_PERMISSION_ERROR,
                          util::GetDisplayableFileName16(file_url)));

    case fmp::DriveSyncErrorType::kServiceUnavailable:
      return CreateNotification(
          id, IDS_FILE_BROWSER_DRIVE_DIRECTORY_LABEL,
          IDS_FILE_BROWSER_SYNC_SERVICE_UNAVAILABLE_ERROR);

    case fmp::DriveSyncErrorType::kNoServerSpace:
      return CreateNotification(
          id, title, GetStringUTF16(IDS_FILE_BROWSER_SYNC_NO_SERVER_SPACE));

    case fmp::DriveSyncErrorType::kNoServerSpaceOrganization:
      return CreateNotification(
          id, title,
          GetStringUTF16(IDS_FILE_BROWSER_SYNC_NO_SERVER_SPACE_ORGANIZATION));

    case fmp::DriveSyncErrorType::kNoLocalSpace:
      return CreateNotification(id, IDS_FILE_BROWSER_DRIVE_DIRECTORY_LABEL,
                                IDS_FILE_BROWSER_DRIVE_OUT_OF_SPACE_HEADER);

    case fmp::DriveSyncErrorType::kMisc:
      return CreateNotification(
          id, title,
          GetStringFUTF16(IDS_FILE_BROWSER_SYNC_MISC_ERROR,
                          util::GetDisplayableFileName16(file_url)));

    case fmp::DriveSyncErrorType::kNoSharedDriveSpace:
      if (!sync_error->shared_drive.has_value()) {
        DLOG(WARNING) << "No shared drive provided for error notification";
        return nullptr;
      }

      return CreateNotification(
          id, title,
          GetStringFUTF16(IDS_FILE_BROWSER_SYNC_ERROR_SHARED_DRIVE_OUT_OF_SPACE,
                          UTF8ToUTF16(sync_error->shared_drive.value())));

    case fmp::DriveSyncErrorType::kNone:
      break;
  }

  LOG(ERROR) << "Unexpected Drive sync error: "
             << base::to_underlying(sync_error->type);
  return nullptr;
}

static const char kDriveDialogId[] = "swa-drive-confirm-dialog";

void SystemNotificationManager::HandleDriveDialogClick(
    std::optional<int> button_index) {
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

NotificationPtr SystemNotificationManager::MakeDriveConfirmDialogNotification(
    const Event& event) {
  DCHECK(!event.event_args.empty());
  auto dialog_event =
      fmp::DriveConfirmDialogEvent::FromValue(event.event_args[0]);
  if (!dialog_event) {
    LOG(ERROR) << "Cannot parse DriveConfirmDialogEvent from "
               << event.event_args[0];
    return nullptr;
  }

  NotificationPtr notification = CreateSystemNotification(
      kDriveDialogId, IDS_FILE_BROWSER_DRIVE_DIRECTORY_LABEL,
      IDS_FILE_BROWSER_OFFLINE_ENABLE_MESSAGE,
      MakeRefCounted<HandleNotificationClickDelegate>(
          BindRepeating(&SystemNotificationManager::HandleDriveDialogClick,
                        weak_ptr_factory_.GetWeakPtr())));

  std::vector<ButtonInfo> buttons;
  buttons.emplace_back(GetStringUTF16(IDS_FILE_BROWSER_OFFLINE_ENABLE_REJECT));
  buttons.emplace_back(GetStringUTF16(IDS_FILE_BROWSER_OFFLINE_ENABLE_ACCEPT));
  notification->set_buttons(buttons);

  return notification;
}

void SystemNotificationManager::HandleEvent(const Event& event) {
  if (event.event_args.empty()) {
    DLOG(WARNING) << "Ignored empty Event {name: " << event.event_name
                  << ", histogram_value: " << event.histogram_value << "}";
    return;
  }

  // For some events we always display a system notification regardless of if
  // there are any SWA windows open.
  bool force_as_system_notification = false;
  NotificationPtr notification;
  switch (event.histogram_value) {
    case FILE_MANAGER_PRIVATE_ON_DRIVE_SYNC_ERROR:
      notification = MakeDriveSyncErrorNotification(event);
      break;

    case FILE_MANAGER_PRIVATE_ON_DRIVE_CONFIRM_DIALOG:
      notification = MakeDriveConfirmDialogNotification(event);
      force_as_system_notification = true;
      break;

    case FILE_MANAGER_PRIVATE_ON_BULK_PIN_PROGRESS:
      notification = MakeBulkPinningErrorNotification(event);
      force_as_system_notification = true;
      break;

    default:
      VLOG(1) << "Ignored Event {name: " << event.event_name
              << ", histogram_value: " << event.histogram_value
              << ", args: " << event.event_args << "}";
      return;
  }

  if (!notification) {
    return;
  }

  // Check if we need to remove any progress notification when there
  // are active SWA windows.
  if (!force_as_system_notification && DoFilesSwaWindowsExist()) {
    GetNotificationDisplayService()->Close(NotificationHandler::Type::TRANSIENT,
                                           notification->id());
    return;
  }

  GetNotificationDisplayService()->Display(NotificationHandler::Type::TRANSIENT,
                                           *notification, nullptr);
}

void SystemNotificationManager::HandleIOTaskProgress(
    const ProgressStatus& status) {
  std::string id = GetNotificationId(status.task_id);

  // If there are any SWA windows open, remove the IOTask progress from system
  // notifications.
  if (!status.show_notification || DoFilesSwaWindowsExist()) {
    Dismiss(id);
    return;
  }

  // If there's a warning or security error, show a data protection
  // notification.
  if (status.HasWarning() || status.HasPolicyError()) {
    policy::FilesPolicyNotificationManager* manager =
        policy::FilesPolicyNotificationManagerFactory::GetForBrowserContext(
            profile_);
    if (!manager) {
      LOG(ERROR) << "No FilesPolicyNotificationManager instantiated,"
                    "can't show policy dialog for task_id "
                 << status.task_id;
      return;
    }
    Dismiss(id);
    manager->ShowFilesPolicyNotification(id, status);
    return;
  }

  // If the task is currently in the scanning state, show a data protection
  // progress notification.
  if (status.IsScanning()) {
    Dismiss(id);
    NotificationPtr notification =
        MakeDataProtectionPolicyProgressNotification(id, status);
    GetNotificationDisplayService()->Display(
        NotificationHandler::Type::TRANSIENT, *notification,
        /*metadata=*/nullptr);
    return;
  }

  // If the IOTask state has completed, remove the IOTask progress from system
  // notifications.
  if (status.IsCompleted()) {
    Dismiss(id);
    return;
  }

  // From here state is kQueued, kInProgress, or kPaused.
  const bool paused = status.IsPaused();

  std::u16string title;
  std::u16string message;
  if (!paused) {
    title = app_name_;
    message = GetIOTaskMessage(profile_, status);
  } else {
    title = GetIOTaskMessage(profile_, status);
    int message_id = IDS_FILE_BROWSER_CONFLICT_DIALOG_MESSAGE;
    if (status.pause_params.conflict_params.has_value() &&
        status.pause_params.conflict_params->conflict_is_directory) {
      message_id = IDS_FILE_BROWSER_CONFLICT_DIALOG_FOLDER_MESSAGE;
    }
    auto& item_name = status.pause_params.conflict_params->conflict_name;
    message = GetStringFUTF16(message_id, UTF8ToUTF16(item_name));
  }

  int progress = 0;
  if (status.total_bytes > 0) {
    progress = status.bytes_transferred * 100.0 / status.total_bytes;
  }

  NotificationPtr notification = CreateIOTaskProgressNotification(
      status.task_id, id, title, message, paused, progress);

  GetNotificationDisplayService()->Display(NotificationHandler::Type::TRANSIENT,
                                           *notification,
                                           /*metadata=*/nullptr);
}

constexpr char kRemovableNotificationId[] = "swa-removable-device-id";

void SystemNotificationManager::HandleRemovableNotificationClick(
    const std::string& path,
    const std::vector<DeviceNotificationUserActionUmaType>&
        uma_types_for_buttons,
    std::optional<int> button_index) {
  if (button_index) {
    if (button_index.value() == 0) {
      base::FilePath volume_root(path);
      platform_util::ShowItemInFolder(profile_, volume_root);
    } else {
      chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
          profile_, chromeos::settings::mojom::kExternalStorageSubpagePath);
    }
    if (base::checked_cast<size_t>(button_index.value()) <
        uma_types_for_buttons.size()) {
      RecordDeviceNotificationUserActionMetric(
          uma_types_for_buttons.at(button_index.value()));
    }
  }

  GetNotificationDisplayService()->Close(NotificationHandler::Type::TRANSIENT,
                                         kRemovableNotificationId);
}

void SystemNotificationManager::HandleDataProtectionPolicyNotificationClick(
    RepeatingClosure proceed_callback,
    RepeatingClosure cancel_callback,
    std::optional<int> button_index) {
  if (!button_index.has_value()) {
    return;
  }

  if (button_index.value() == 0) {
    proceed_callback.Run();
  }

  if (button_index.value() == 1 && cancel_callback) {
    cancel_callback.Run();
  }
}

NotificationPtr SystemNotificationManager::MakeMountErrorNotification(
    MountCompletedEvent& event,
    const Volume& volume) {
  const auto it = mount_status_.find(volume.storage_device_path().value());
  if (it == mount_status_.end()) {
    return nullptr;
  }

  const std::u16string title =
      GetStringUTF16(IDS_REMOVABLE_DEVICE_DETECTION_TITLE);
  std::u16string message;
  std::vector<ButtonInfo> buttons;
  std::vector<DeviceNotificationUserActionUmaType> uma_types_for_buttons;
  switch (it->second) {
    // We have either an unsupported or unknown filesystem on the mount.
    case MOUNT_STATUS_ONLY_PARENT_ERROR:
    case MOUNT_STATUS_CHILD_ERROR:
      if (event.status == fmp::MountError::kUnsupportedFilesystem) {
        if (volume.drive_label().empty()) {
          message = GetStringUTF16(IDS_DEVICE_UNSUPPORTED_DEFAULT_MESSAGE);
        } else {
          message = GetStringFUTF16(IDS_DEVICE_UNSUPPORTED_MESSAGE,
                                    UTF8ToUTF16(volume.drive_label()));
        }
        RecordDeviceNotificationMetric(DeviceNotificationUmaType::DEVICE_FAIL);
      } else {
        if (volume.drive_label().empty()) {
          message = GetStringUTF16(IDS_DEVICE_UNKNOWN_DEFAULT_MESSAGE);
        } else {
          message = GetStringFUTF16(IDS_DEVICE_UNKNOWN_MESSAGE,
                                    UTF8ToUTF16(volume.drive_label()));
        }

        if (!volume.is_read_only()) {
          // Give a format device button on the notification.
          buttons.emplace_back(GetStringUTF16(IDS_DEVICE_UNKNOWN_BUTTON_LABEL));
          uma_types_for_buttons.push_back(
              DeviceNotificationUserActionUmaType::OPEN_MEDIA_DEVICE_FAIL);
          RecordDeviceNotificationMetric(
              DeviceNotificationUmaType::DEVICE_FAIL_UNKNOWN);
        } else {
          RecordDeviceNotificationMetric(
              DeviceNotificationUmaType::DEVICE_FAIL_UNKNOWN_READONLY);
        }
      }
      break;

    // We have a multi-partition device for which at least one mount
    // failed.
    case MOUNT_STATUS_MULTIPART_ERROR:
      if (volume.drive_label().empty()) {
        message =
            GetStringUTF16(IDS_MULTIPART_DEVICE_UNSUPPORTED_DEFAULT_MESSAGE);
      } else {
        message = GetStringFUTF16(IDS_MULTIPART_DEVICE_UNSUPPORTED_MESSAGE,
                                  UTF8ToUTF16(volume.drive_label()));
      }
      RecordDeviceNotificationMetric(DeviceNotificationUmaType::DEVICE_FAIL);
      break;

    case MOUNT_STATUS_NO_RESULT:
    case MOUNT_STATUS_SUCCESS:
    default:
      VLOG(1) << "Unhandled mount status " << it->second;
      return nullptr;
  }

  NotificationPtr notification = CreateSystemNotification(
      kDeviceFailNotificationId, title, message,
      MakeRefCounted<HandleNotificationClickDelegate>(BindRepeating(
          &SystemNotificationManager::HandleRemovableNotificationClick,
          weak_ptr_factory_.GetWeakPtr(), volume.mount_path().value(),
          uma_types_for_buttons)));

  DCHECK_EQ(buttons.size(), uma_types_for_buttons.size());
  notification->set_buttons(buttons);

  return notification;
}

SystemNotificationManagerMountStatus
SystemNotificationManager::UpdateDeviceMountStatus(MountCompletedEvent& event,
                                                   const Volume& volume) {
  SystemNotificationManagerMountStatus status = MOUNT_STATUS_NO_RESULT;
  const std::string& device_path = volume.storage_device_path().value();
  auto device_mount_status = mount_status_.find(device_path);
  if (device_mount_status == mount_status_.end()) {
    status = MOUNT_STATUS_NO_RESULT;
  } else {
    status = device_mount_status->second;
  }
  switch (status) {
    case MOUNT_STATUS_MULTIPART_ERROR:
      // Do nothing, status has already been detected.
      break;
    case MOUNT_STATUS_ONLY_PARENT_ERROR:
      if (!volume.is_parent()) {
        // Hide Device Fail notification.
        GetNotificationDisplayService()->Close(
            NotificationHandler::Type::TRANSIENT, kDeviceFailNotificationId);
      }
      [[fallthrough]];
    case MOUNT_STATUS_NO_RESULT:
      if (event.status == fmp::MountError::kSuccess) {
        status = MOUNT_STATUS_SUCCESS;
      } else if (event.volume_metadata.is_parent_device) {
        status = MOUNT_STATUS_ONLY_PARENT_ERROR;
      } else {
        status = MOUNT_STATUS_CHILD_ERROR;
      }
      break;
    case MOUNT_STATUS_SUCCESS:
    case MOUNT_STATUS_CHILD_ERROR:
      if (status == MOUNT_STATUS_SUCCESS &&
          event.status == fmp::MountError::kSuccess) {
        status = MOUNT_STATUS_SUCCESS;
      } else {
        // Multi partition device with at least one partition in error.
        status = MOUNT_STATUS_MULTIPART_ERROR;
      }
      break;
  }
  mount_status_[device_path] = status;

  return status;
}

NotificationPtr SystemNotificationManager::MakeRemovableNotification(
    MountCompletedEvent& event,
    const Volume& volume) {
  NotificationPtr notification;
  if (event.status == fmp::MountError::kSuccess) {
    bool show_settings_button = false;
    std::u16string title = GetStringUTF16(IDS_REMOVABLE_DEVICE_DETECTION_TITLE);
    std::u16string message;
    std::vector<DeviceNotificationUserActionUmaType> uma_types_for_buttons;
    if (volume.is_read_only() && !volume.is_read_only_removable_device()) {
      message = GetStringUTF16(
          IDS_REMOVABLE_DEVICE_NAVIGATION_MESSAGE_READONLY_POLICY);
      RecordDeviceNotificationMetric(
          DeviceNotificationUmaType::DEVICE_NAVIGATION_READONLY_POLICY);
      uma_types_for_buttons.push_back(
          DeviceNotificationUserActionUmaType::OPEN_MEDIA_DEVICE_NAVIGATION);
    } else {
      const PrefService* const service = profile_->GetPrefs();
      DCHECK(service);
      bool arc_enabled = service->GetBoolean(arc::prefs::kArcEnabled);
      bool arc_removable_media_access_enabled =
          service->GetBoolean(arc::prefs::kArcHasAccessToRemovableMedia);
      if (!arc_enabled) {
        message = GetStringUTF16(IDS_REMOVABLE_DEVICE_NAVIGATION_MESSAGE);
        RecordDeviceNotificationMetric(
            DeviceNotificationUmaType::DEVICE_NAVIGATION);
        uma_types_for_buttons.push_back(
            DeviceNotificationUserActionUmaType::OPEN_MEDIA_DEVICE_NAVIGATION);
      } else if (arc_removable_media_access_enabled) {
        message = base::StrCat(
            {GetStringUTF16(IDS_REMOVABLE_DEVICE_NAVIGATION_MESSAGE), u" ",
             GetStringUTF16(
                 IDS_REMOVABLE_DEVICE_PLAY_STORE_APPS_HAVE_ACCESS_MESSAGE)});
        show_settings_button = true;
        RecordDeviceNotificationMetric(
            DeviceNotificationUmaType::DEVICE_NAVIGATION_APPS_HAVE_ACCESS);
        uma_types_for_buttons.insert(uma_types_for_buttons.end(),
                                     {DeviceNotificationUserActionUmaType::
                                          OPEN_MEDIA_DEVICE_NAVIGATION_ARC,
                                      DeviceNotificationUserActionUmaType::
                                          OPEN_SETTINGS_FOR_ARC_STORAGE});
      } else {
        message = base::StrCat(
            {GetStringUTF16(IDS_REMOVABLE_DEVICE_NAVIGATION_MESSAGE), u" ",
             GetStringUTF16(
                 IDS_REMOVABLE_DEVICE_ALLOW_PLAY_STORE_ACCESS_MESSAGE)});
        show_settings_button = true;
        RecordDeviceNotificationMetric(
            DeviceNotificationUmaType::DEVICE_NAVIGATION_ALLOW_APP_ACCESS);
        uma_types_for_buttons.insert(uma_types_for_buttons.end(),
                                     {DeviceNotificationUserActionUmaType::
                                          OPEN_MEDIA_DEVICE_NAVIGATION_ARC,
                                      DeviceNotificationUserActionUmaType::
                                          OPEN_SETTINGS_FOR_ARC_STORAGE});
      }
    }

    notification = CreateSystemNotification(
        kRemovableNotificationId, title, message,
        MakeRefCounted<HandleNotificationClickDelegate>(BindRepeating(
            &SystemNotificationManager::HandleRemovableNotificationClick,
            weak_ptr_factory_.GetWeakPtr(), volume.mount_path().value(),
            uma_types_for_buttons)));
    std::vector<ButtonInfo> notification_buttons;
    notification_buttons.emplace_back(
        GetStringUTF16(IDS_REMOVABLE_DEVICE_NAVIGATION_BUTTON_LABEL));
    if (show_settings_button) {
      notification_buttons.emplace_back(
          GetStringUTF16(IDS_REMOVABLE_DEVICE_OPEN_SETTTINGS_BUTTON_LABEL));
    }
    DCHECK_EQ(notification_buttons.size(), uma_types_for_buttons.size());
    notification->set_buttons(notification_buttons);
  }
  if (volume.device_type() != ash::DeviceType::kUnknown &&
      !volume.storage_device_path().empty()) {
    if (UpdateDeviceMountStatus(event, volume) != MOUNT_STATUS_SUCCESS) {
      notification = MakeMountErrorNotification(event, volume);
    }
  }

  return notification;
}

NotificationPtr
SystemNotificationManager::MakeDataProtectionPolicyProgressNotification(
    const std::string& notification_id,
    const ProgressStatus& status) {
  std::u16string message =
      status.sources.size() > 1
          ? GetStringUTF16(IDS_FILE_BROWSER_SCANNING_LABEL_PLURAL)
          : GetStringUTF16(IDS_FILE_BROWSER_SCANNING_LABEL);
  int progress = status.sources_scanned * 100.0 / status.sources.size();
  return CreateIOTaskProgressNotification(status.task_id, notification_id,
                                          app_name_, message, /*paused=*/false,
                                          progress);
}

void SystemNotificationManager::ShowDataProtectionPolicyDialog(
    IOTaskId task_id,
    policy::FilesDialogType type) {
  policy::FilesPolicyNotificationManager* manager =
      policy::FilesPolicyNotificationManagerFactory::GetForBrowserContext(
          profile_);
  if (!manager) {
    LOG(ERROR) << "No FilesPolicyNotificationManager instantiated,"
                  "can't show policy dialog for task_id "
               << task_id;
    return;
  }
  manager->ShowDialog(task_id, type);
}

void SystemNotificationManager::CancelTask(IOTaskId task_id) {
  if (io_task_controller_) {
    io_task_controller_->Cancel(task_id);
  } else {
    LOG(ERROR) << "No TaskController, can't cancel task_id: " << task_id;
  }
}

void SystemNotificationManager::ResumeTask(IOTaskId task_id,
                                           policy::Policy policy) {
  if (io_task_controller_) {
    io_task::ResumeParams params;
    params.policy_params->type = policy;
    io_task_controller_->Resume(task_id, std::move(params));
  } else {
    LOG(ERROR) << "No TaskController, can't resume task_id: " << task_id;
  }
}

void SystemNotificationManager::HandleMountCompletedEvent(
    MountCompletedEvent& event,
    const Volume& volume) {
  NotificationPtr notification;

  switch (event.event_type) {
    case fmp::MountCompletedEventType::kMount:
      if (event.should_notify) {
        notification = MakeRemovableNotification(event, volume);
      }
      break;

    case fmp::MountCompletedEventType::kUnmount:
      GetNotificationDisplayService()->Close(
          NotificationHandler::Type::TRANSIENT, kRemovableNotificationId);

      if (volume.device_type() != ash::DeviceType::kUnknown &&
          !volume.storage_device_path().empty()) {
        UpdateDeviceMountStatus(event, volume);
      }
      break;

    case fmp::MountCompletedEventType::kNone:
    default:
      VLOG(1) << "Unexpected mount event "
              << base::to_underlying(event.event_type);
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

void SystemNotificationManager::SetIOTaskController(
    IOTaskController* io_task_controller) {
  io_task_controller_ = io_task_controller;
}

}  // namespace file_manager
