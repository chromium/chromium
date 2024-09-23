// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_SYSTEM_NOTIFICATION_MANAGER_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_SYSTEM_NOTIFICATION_MANAGER_H_

#include "ash/public/cpp/notification_utils.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/io_task_controller.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_dialog.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "extensions/browser/event_router.h"
#include "storage/browser/file_system/file_system_url.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace file_manager {

namespace file_manager_private = extensions::api::file_manager_private;

class DriveFsEventRouter;

// Status of mounted removable devices.
enum SystemNotificationManagerMountStatus {
  // Initial state.
  MOUNT_STATUS_NO_RESULT,
  // No errors on the device.
  MOUNT_STATUS_SUCCESS,
  // Parent errors exist that may be overridden by child partitions.
  MOUNT_STATUS_ONLY_PARENT_ERROR,
  // A single child partition error.
  MOUNT_STATUS_CHILD_ERROR,
  // Multiple child partitions with at least one in error.
  MOUNT_STATUS_MULTIPART_ERROR,
};

// Enum of possible UMA values for histogram Notification.Show.
// Keep the order of this in sync with FileManagerNotificationType in
// tools/metrics/histograms/enums.xml.
enum class DeviceNotificationUmaType {
  DEVICE_NAVIGATION_ALLOW_APP_ACCESS = 0,
  DEVICE_NAVIGATION_APPS_HAVE_ACCESS = 1,
  DEVICE_NAVIGATION = 2,
  DEVICE_NAVIGATION_READONLY_POLICY = 3,
  DEVICE_IMPORT = 4,
  DEVICE_FAIL = 5,
  DEVICE_FAIL_UNKNOWN = 6,
  DEVICE_FAIL_UNKNOWN_READONLY = 7,
  DEVICE_EXTERNAL_STORAGE_DISABLED = 8,
  DEVICE_HARD_UNPLUGGED = 9,
  FORMAT_START = 10,
  FORMAT_SUCCESS = 11,
  FORMAT_FAIL = 12,
  RENAME_FAIL = 13,
  PARTITION_START = 14,
  PARTITION_SUCCESS = 15,
  PARTITION_FAIL = 16,
  kMaxValue = PARTITION_FAIL,
};

// Enum of possible UMA values for histogram Notification.UserAction.
// Keep the order of this in sync with FileManagerNotificationUserAction in
// tools/metrics/histograms/enums.xml.
enum class DeviceNotificationUserActionUmaType {
  OPEN_SETTINGS_FOR_ARC_STORAGE = 0,  // OPEN_EXTERNAL_STORAGE_PREFERENCES.
  OPEN_MEDIA_DEVICE_NAVIGATION = 1,
  OPEN_MEDIA_DEVICE_NAVIGATION_ARC = 2,
  OPEN_MEDIA_DEVICE_FAIL = 3,
  OPEN_MEDIA_DEVICE_IMPORT = 4,
  kMaxValue = OPEN_MEDIA_DEVICE_IMPORT,
};

// Histogram name for Notification.Show.
inline constexpr char kNotificationShowHistogramName[] =
    "FileBrowser.Notification.Show";

// Histogram name for Notification.UserAction.
inline constexpr char kNotificationUserActionHistogramName[] =
    "FileBrowser.Notification.UserAction";

// Generates a notification id based on `task_id`.
std::string GetNotificationId(io_task::IOTaskId task_id);

// Returns an instance of an 'ash' Notification with a bound click delegate.
// The notification will have Files app system notification theme.
std::unique_ptr<message_center::Notification> CreateSystemNotification(
    const std::string& notification_id,
    const std::u16string& title,
    const std::u16string& message,
    scoped_refptr<message_center::NotificationDelegate> delegate,
    message_center::RichNotificationData optional_fields =
        message_center::RichNotificationData());

// Returns an instance of an 'ash' Notification with title and message specified
// by string ID values (for 110n) with a bound click delegate.
// The notification will have Files app system notification theme.
std::unique_ptr<message_center::Notification> CreateSystemNotification(
    const std::string& notification_id,
    int title_id,
    int message_id,
    scoped_refptr<message_center::NotificationDelegate> delegate);

// Returns an instance of an 'ash' Notification with a bound click callback.
// The notification will have Files app system notification theme.
std::unique_ptr<message_center::Notification> CreateSystemNotification(
    const std::string& notification_id,
    const std::u16string& title,
    const std::u16string& message,
    const base::RepeatingClosure& click_callback);

// Manages creation/deletion and update of system notifications on behalf
// of the File Manager application.
class SystemNotificationManager {
 public:
  explicit SystemNotificationManager(Profile* profile);
  ~SystemNotificationManager();

  // Returns whether or not ANY SWA windows are opened. Does this by checking
  // the URL of all opened windows.
  bool DoFilesSwaWindowsExist();

  // Processes a device event to generate a system notification if needed.
  void HandleDeviceEvent(const file_manager_private::DeviceEvent& event);

  using NotificationPtr = std::unique_ptr<message_center::Notification>;

  // Returns an instance of an 'ash' Notification.
  NotificationPtr CreateNotification(const std::string& notification_id,
                                     const std::u16string& title,
                                     const std::u16string& message);

  // Returns an instance of an 'ash' Notification with progress value.
  NotificationPtr CreateProgressNotification(const std::string& notification_id,
                                             const std::u16string& title,
                                             const std::u16string& message,
                                             int progress);

  // Returns an instance of an 'ash' Notification with IOTask progress value.
  NotificationPtr CreateIOTaskProgressNotification(
      file_manager::io_task::IOTaskId task_id,
      const std::string& notification_id,
      const std::u16string& title,
      const std::u16string& message,
      const bool paused,
      int progress);

  // Click handler for the IOTask progress notification.
  void HandleIOTaskProgressNotificationClick(
      file_manager::io_task::IOTaskId task_id,
      const std::string& notification_id,
      const bool paused,
      std::optional<int> button_index);

  // Returns an instance of an 'ash' Notification with title and message
  // specified by string ID values (for 110n).
  NotificationPtr CreateNotification(const std::string& notification_id,
                                     int title_id,
                                     int message_id);

  using Event = extensions::Event;

  // Processes general extension events and can create a system notification.
  void HandleEvent(const Event& event);

  // Processes progress event from IOTaskController.
  void HandleIOTaskProgress(
      const file_manager::io_task::ProgressStatus& status);

  // Stores and updates the state of a device based on mount events for the top
  // level or any child partitions.
  SystemNotificationManagerMountStatus UpdateDeviceMountStatus(
      file_manager_private::MountCompletedEvent& event,
      const Volume& volume);

  // Processes volume mount completed events.
  void HandleMountCompletedEvent(
      file_manager_private::MountCompletedEvent& event,
      const Volume& volume);

  // Returns the message center display service that manages notifications.
  NotificationDisplayService* GetNotificationDisplayService();

  // Stores a reference to the DriveFS event router instance.
  void SetDriveFSEventRouter(DriveFsEventRouter* drivefs_event_router);

  // Stores a pointer to the IOTaskController instance to be able to cancel
  // tasks.
  void SetIOTaskController(
      file_manager::io_task::IOTaskController* io_task_controller);

 private:
  // Handles clicks on the DriveFS bulk-pinning error notification.
  void HandleBulkPinningNotificationClick();

  // Make notification for DriveFS bulk-pinning error.
  NotificationPtr MakeBulkPinningErrorNotification(const Event& event);

  // Make notifications for DriveFS sync errors.
  NotificationPtr MakeDriveSyncErrorNotification(const Event& event);

  // Click handler for the Drive offline confirmation dialog notification.
  void HandleDriveDialogClick(std::optional<int> button_index);

  // Make notification from the DriveFS offline settings event.
  NotificationPtr MakeDriveConfirmDialogNotification(const Event& event);

  // Click handler for the removable device notification.
  void HandleRemovableNotificationClick(
      const std::string& path,
      const std::vector<DeviceNotificationUserActionUmaType>&
          uma_types_for_buttons,
      std::optional<int> button_index);

  // Click handler for Data Leak Prevention or Enterprise Connectors policy
  // notifications.
  void HandleDataProtectionPolicyNotificationClick(
      base::RepeatingClosure proceed_callback,
      base::RepeatingClosure cancel_callback,
      std::optional<int> button_index);

  // Click handler for the progress notification.
  void HandleProgressClick(const std::string& notification_id,
                           std::optional<int> button_index);

  // Makes a notification instance for mount errors.
  NotificationPtr MakeMountErrorNotification(
      file_manager_private::MountCompletedEvent& event,
      const Volume& volume);

  // Makes a notification instance for removable devices.
  NotificationPtr MakeRemovableNotification(
      file_manager_private::MountCompletedEvent& event,
      const Volume& volume);

  // Makes a notification instance for Data Protection progress notifications.
  NotificationPtr MakeDataProtectionPolicyProgressNotification(
      const std::string& notification_id,
      const file_manager::io_task::ProgressStatus& status);

  // Helper function to show a data protection policy dialog.
  void ShowDataProtectionPolicyDialog(file_manager::io_task::IOTaskId task_id,
                                      policy::FilesDialogType type);

  // Helper function bound to notification instances that hides notifications.
  void Dismiss(const std::string& notification_id);

  // Helper function to cancel a task.
  void CancelTask(file_manager::io_task::IOTaskId task_id);

  // Helper function to resume a task.
  void ResumeTask(file_manager::io_task::IOTaskId task_id,
                  policy::Policy policy);

  // Maps device paths to their mount status.
  // This is used for removable devices with single/multiple partitions.
  // e.g. the same device path could have 2 partitions that each generate a
  // mount event. One partition could have a known file system and the other an
  //      unknown file system. Different combinations of known/unknown file
  //      systems on a multi-partition devices require this map to generate
  //      the correct system notification when errors occur.
  std::map<std::string, SystemNotificationManagerMountStatus> mount_status_;

  // User profile.
  const raw_ptr<Profile, DanglingUntriaged> profile_;

  // Application name (used for notification display source).
  std::u16string const app_name_;

  // DriveFS event router: not owned.
  raw_ptr<DriveFsEventRouter, DanglingUntriaged> drivefs_event_router_ =
      nullptr;

  // IOTaskController is owned by VolumeManager.
  raw_ptr<file_manager::io_task::IOTaskController, DanglingUntriaged>
      io_task_controller_ = nullptr;

  // Keep track of the bulk-pinning stage.
  using BulkPinStage = file_manager_private::BulkPinStage;
  BulkPinStage bulk_pin_stage_ = BulkPinStage::kNone;

  // base::WeakPtr{this} factory.
  base::WeakPtrFactory<SystemNotificationManager> weak_ptr_factory_{this};
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_SYSTEM_NOTIFICATION_MANAGER_H_
