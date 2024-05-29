// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/migration_notification_manager.h"

#include <memory>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/message_center/public/cpp/notification.h"

namespace policy::local_user_files {

namespace {
constexpr char kSkyVaultNotificationId[] = "skyvault";

// Creates a notification with `kSkyvaultNotificationId`, `title` and `message`,
// that invokes `callback` when clicked on.
std::unique_ptr<message_center::Notification> CreateNotificationPtr(
    const std::u16string title,
    const std::u16string message,
    base::RepeatingCallback<void(std::optional<int>)> callback) {
  message_center::RichNotificationData optional_fields;
  optional_fields.never_timeout = true;
  return ash::CreateSystemNotificationPtr(
      message_center::NotificationType::NOTIFICATION_TYPE_SIMPLE,
      kSkyVaultNotificationId, title, message,
      /*display_source=*/std::u16string(), /*origin_url=*/GURL(),
      message_center::NotifierId(), optional_fields,
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          callback),
      vector_icons::kBusinessIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);
}

// Closes the notification with `kSkyvaultNotificationId`.
void CloseNotification() {
  NotificationDisplayService::GetForProfile(
      ProfileManager::GetActiveUserProfile())
      ->Close(NotificationHandler::Type::TRANSIENT, kSkyVaultNotificationId);
}

}  // namespace

MigrationNotificationManager::MigrationNotificationManager() = default;

MigrationNotificationManager::~MigrationNotificationManager() = default;

void MigrationNotificationManager::ShowMigrationProgressNotification() {
  // TODO(aidazolic): Use i18n strings.
  // TODO(aidazolic): Use FileSaveDestination.
  auto notification = CreateNotificationPtr(
      /*title=*/u"Your files are being uploaded to OneDrive",
      /*message=*/
      u"Local storage will be restricted. You can only modify "
      u"these files once the upload has been completed.",
      /*callback=*/base::DoNothing());

  NotificationDisplayService::GetForProfile(
      ProfileManager::GetActiveUserProfile())
      ->Display(NotificationHandler::Type::TRANSIENT, *notification,
                /*metadata=*/nullptr);
}

void MigrationNotificationManager::ShowMigrationCompletedNotification(
    const base::FilePath& destination_path) {
  // TODO(aidazolic): Use i18n strings.
  // TODO(aidazolic): Use FileSaveDestination.
  auto notification = CreateNotificationPtr(
      /*title=*/u"All files have been uploaded to OneDrive",
      /*message=*/u"Local storage has been disabled.",
      base::BindRepeating(
          &MigrationNotificationManager::HandleCompletedNotificationClick,
          weak_factory_.GetWeakPtr(), destination_path));
  notification->set_buttons(
      {message_center::ButtonInfo(u"View files in OneDrive")});

  NotificationDisplayService::GetForProfile(
      ProfileManager::GetActiveUserProfile())
      ->Display(NotificationHandler::Type::TRANSIENT, *notification,
                /*metadata=*/nullptr);
}

void MigrationNotificationManager::ShowMigrationErrorNotification(
    const std::string& message) {
  // TODO(aidazolic): Handle different error states.
}

void MigrationNotificationManager::HandleCompletedNotificationClick(
    const base::FilePath& destination_path,
    std::optional<int> button_index) {
  // If "View files in..." was clicked.
  if (button_index) {
    platform_util::ShowItemInFolder(ProfileManager::GetActiveUserProfile(),
                                    destination_path);
  }

  CloseNotification();
}

}  // namespace policy::local_user_files
