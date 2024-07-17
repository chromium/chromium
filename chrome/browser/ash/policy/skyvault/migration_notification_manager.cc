// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/migration_notification_manager.h"

#include <memory>
#include <string>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "base/files/file_path.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/skyvault/local_files_migration_dialog.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/message_center/public/cpp/notification.h"

namespace policy::local_user_files {

namespace {

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
      kSkyVaultMigrationNotificationId, title, message,
      /*display_source=*/std::u16string(), /*origin_url=*/GURL(),
      message_center::NotifierId(), optional_fields,
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          callback),
      vector_icons::kBusinessIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);
}

// Closes the notification with `kSkyVaultMigrationNotificationId`.
void CloseNotification(Profile* profile) {
  NotificationDisplayService::GetForProfile(profile)->Close(
      NotificationHandler::Type::TRANSIENT, kSkyVaultMigrationNotificationId);
}

}  // namespace

MigrationNotificationManager::MigrationNotificationManager(Profile* profile)
    : profile_(profile) {}

MigrationNotificationManager::~MigrationNotificationManager() = default;

void MigrationNotificationManager::ShowMigrationInfoDialog(
    CloudProvider provider,
    base::TimeDelta migration_delay,
    base::OnceClosure migration_callback) {
  LocalFilesMigrationDialog::Show(provider, migration_delay,
                                  std::move(migration_callback));
}

void MigrationNotificationManager::ShowMigrationProgressNotification(
    CloudProvider provider) {
  std::u16string title;
  std::u16string message;
  switch (provider) {
    case CloudProvider::kGoogleDrive:
      title = u"Your files are being uploaded to Google Drive";
      message =
          u"If your file is not on your device, look for it on Google Drive. "
          u"Once files have been uploaded to Google Drive, they will no longer "
          u"exist on your device. From then on, save your files to Google "
          u"Drive.";
      break;
    case CloudProvider::kOneDrive:
      title = u"Your files are being uploaded to Microsoft OneDrive";
      message =
          u"If your file is not on your device, look for it on Microsoft "
          u"OneDrive. "
          u"Once files have been uploaded to Microsoft OneDrive, they will no "
          u"longer "
          u"exist on your device. From then on, save your files to Google "
          u"Drive.";
      break;
    case CloudProvider::kNotSpecified:
      LOG(ERROR) << "CloudProvider must be set.";
      return;
  }

  // TODO(334511998): Use i18n strings.
  auto notification = CreateNotificationPtr(title, message,
                                            /*callback=*/base::DoNothing());

  NotificationDisplayService::GetForProfile(profile_)->Display(
      NotificationHandler::Type::TRANSIENT, *notification,
      /*metadata=*/nullptr);
}

void MigrationNotificationManager::ShowMigrationCompletedNotification(
    CloudProvider provider,
    const base::FilePath& destination_path) {
  // TODO(334511998): Use i18n strings.
  std::u16string title;
  std::u16string message;
  switch (provider) {
    case CloudProvider::kGoogleDrive:
      title = u"Upload to Google Drive complete";
      message =
          u"All files from your device have been uploaded to "
          u"Google Drive. From now on, save your files to Google Drive.";
      break;
    case CloudProvider::kOneDrive:
      title = u"Upload to Microsoft OneDrive complete";
      message =
          u"All files from your device have been uploaded to Microsoft "
          u"OneDrive. From now on, save your files to Microsoft "
          u"OneDrive.";
      break;
    case CloudProvider::kNotSpecified:
      LOG(ERROR) << "CloudProvider must be set.";
      return;
  }

  auto notification = CreateNotificationPtr(title, message, base::DoNothing());

  NotificationDisplayService::GetForProfile(profile_)->Display(
      NotificationHandler::Type::TRANSIENT, *notification,
      /*metadata=*/nullptr);
}

void MigrationNotificationManager::ShowMigrationErrorNotification(
    CloudProvider provider,
    std::map<base::FilePath, MigrationUploadError> errors) {
  // TODO(aidazolic): Handle different error states.
}

void MigrationNotificationManager::CloseAll() {
  CloseNotification(profile_);
  CloseDialog();
}

void MigrationNotificationManager::CloseDialog() {
  LocalFilesMigrationDialog* dialog = LocalFilesMigrationDialog::GetDialog();
  if (dialog) {
    dialog->Close();
  }
}

}  // namespace policy::local_user_files
