// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_SYSTEM_NOTIFICATION_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_SYSTEM_NOTIFICATION_MANAGER_H_

#include "ash/public/cpp/notification_utils.h"
#include "base/memory/weak_ptr.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "extensions/browser/event_router.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace file_manager {

namespace file_manager_private = extensions::api::file_manager_private;

// Manages creation/deletion and update of system notifications on behalf
// of the File Manager application.
class SystemNotificationManager {
 public:
  explicit SystemNotificationManager(Profile* profile);
  ~SystemNotificationManager();

  /**
   * Returns whether or not ANY SWA windows are opened. Does this by checking
   * the URL of all opened windows.
   */
  bool DoFilesSwaWindowsExist();

  /**
   * Processes a device event to generate a system notification if needed.
   */
  void HandleDeviceEvent(const file_manager_private::DeviceEvent& event);

  /**
   *  Returns an instance of an 'ash' Notification.
   */
  std::unique_ptr<message_center::Notification> CreateNotification(
      const std::string& notification_id,
      const std::u16string& title,
      const std::u16string& message);

  /**
   * Returns an instance of an 'ash' Notiifcation with progress value.
   */
  std::unique_ptr<message_center::Notification> CreateProgressNotification(
      const std::string& notification_id,
      const std::u16string& title,
      const std::u16string& message,
      int progress);

  /**
   *  Returns an instance of an 'ash' Notification with title and message
   *  specified by string ID values (for 110n).
   */
  std::unique_ptr<message_center::Notification> CreateNotification(
      const std::string& notification_id,
      int title_id,
      int message_id);

  /**
   * Processes general extension events and can create a system notification.
   */
  void HandleEvent(const extensions::Event& event);

  /**
   * Called at the start of a copy operation.
   */
  void HandleCopyStart(int copy_id,
                       file_manager_private::CopyOrMoveProgressStatus& status);

  /**
   * Processes copy progress events and updates the system notification.
   */
  void HandleCopyEvent(int copy_id,
                       file_manager_private::CopyOrMoveProgressStatus& status);

  /**
   * Returns the message center display service that manages notifications.
   */
  NotificationDisplayService* GetNotificationDisplayService();

 private:
  /**
   * Helper function bound to notification instances that hides notifications.
   */
  void Dismiss(const std::string& notification_id);

  /**
   * Maps the operation runner copy id to the total size (bytes) for the copy.
   */
  std::map<int, double> required_copy_space_;

  Profile* const profile_;
  // Caches the SWA feature flag.
  bool swa_enabled_;
  base::WeakPtrFactory<SystemNotificationManager> weak_ptr_factory_{this};
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_SYSTEM_NOTIFICATION_MANAGER_H_
