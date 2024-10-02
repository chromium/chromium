// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/migration_notification_manager.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "chrome/browser/ash/file_manager/open_util.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/ash/policy/skyvault/signin_notification_helper.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/ui/webui/ash/skyvault/local_files_migration_dialog.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_context.h"
#include "ui/base/l10n/l10n_util.h"
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
  NotificationDisplayServiceFactory::GetForProfile(profile)->Close(
      NotificationHandler::Type::TRANSIENT, kSkyVaultMigrationNotificationId);
}

// Closes the notification and, if the button is clicked, opens `path`.
void HandleNotificationClick(Profile* profile,
                             const base::FilePath& path,
                             std::optional<int> button) {
  if (button.has_value() && button == 0) {
    file_manager::util::ShowItemInFolder(profile, path, base::DoNothing());
  }
  CloseNotification(profile);
}

// Returns the translation string corresponding to `provider`.
std::u16string CloudProviderToString(CloudProvider provider) {
  switch (provider) {
    case CloudProvider::kGoogleDrive:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_SKYVAULT_CLOUD_PROVIDER_GOOGLE_DRIVE);
    case CloudProvider::kOneDrive:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_SKYVAULT_CLOUD_PROVIDER_ONEDRIVE);
    case CloudProvider::kNotSpecified:
      NOTREACHED();
  }
}

}  // namespace

MigrationNotificationManager::MigrationNotificationManager(
    content::BrowserContext* context)
    : context_(context) {}

MigrationNotificationManager::~MigrationNotificationManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void MigrationNotificationManager::ShowMigrationInfoDialog(
    CloudProvider provider,
    base::Time migration_start_time,
    base::OnceClosure migration_callback) {
  LocalFilesMigrationDialog::Show(provider, migration_start_time,
                                  std::move(migration_callback));
}

void MigrationNotificationManager::ShowMigrationProgressNotification(
    CloudProvider provider) {
  std::u16string provider_str = CloudProviderToString(provider);

  std::u16string title = base::ReplaceStringPlaceholders(
      l10n_util::GetStringUTF16(IDS_POLICY_SKYVAULT_MIGRATION_PROGRESS_TITLE),
      provider_str,
      /*offset=*/nullptr);
  std::u16string message = base::ReplaceStringPlaceholders(
      l10n_util::GetStringUTF16(IDS_POLICY_SKYVAULT_MIGRATION_PROGRESS_MESSAGE),
      provider_str,
      /*offset=*/nullptr);

  auto notification = CreateNotificationPtr(title, message,
                                            /*callback=*/base::DoNothing());

  NotificationDisplayServiceFactory::GetForProfile(profile())->Display(
      NotificationHandler::Type::TRANSIENT, *notification,
      /*metadata=*/nullptr);
}

void MigrationNotificationManager::ShowMigrationCompletedNotification(
    CloudProvider provider,
    const base::FilePath& destination_path) {
  std::u16string provider_str = CloudProviderToString(provider);
  std::u16string folder_name = destination_path.BaseName().AsUTF16Unsafe();

  std::u16string title = base::ReplaceStringPlaceholders(
      l10n_util::GetStringUTF16(IDS_POLICY_SKYVAULT_MIGRATION_COMPLETED_TITLE),
      {folder_name, provider_str},
      /*offsets=*/nullptr);
  std::u16string message = base::ReplaceStringPlaceholders(
      l10n_util::GetStringUTF16(
          IDS_POLICY_SKYVAULT_MIGRATION_COMPLETED_MESSAGE),
      provider_str,
      /*offset=*/nullptr);
  std::u16string button = base::ReplaceStringPlaceholders(
      l10n_util::GetStringUTF16(IDS_POLICY_SKYVAULT_MIGRATION_COMPLETED_BUTTON),
      provider_str,
      /*offset=*/nullptr);

  auto notification =
      CreateNotificationPtr(title, message,
                            base::BindRepeating(&HandleNotificationClick,
                                                profile(), destination_path));
  notification->set_buttons({message_center::ButtonInfo(button)});

  NotificationDisplayServiceFactory::GetForProfile(profile())->Display(
      NotificationHandler::Type::TRANSIENT, *notification,
      /*metadata=*/nullptr);
}

void MigrationNotificationManager::ShowMigrationErrorNotification(
    CloudProvider provider,
    const base::FilePath& destination_path,
    std::map<base::FilePath, MigrationUploadError> errors) {
  // TODO(aidazolic): Pass error log path.
  const base::FilePath error_log_path = base::FilePath();

  std::u16string provider_str = CloudProviderToString(provider);

  std::u16string folder_name = destination_path.BaseName().AsUTF16Unsafe();
  std::u16string title = base::ReplaceStringPlaceholders(
      l10n_util::GetStringUTF16(IDS_POLICY_SKYVAULT_MIGRATION_ERROR_TITLE),
      provider_str,
      /*offset=*/nullptr);
  std::u16string message = base::ReplaceStringPlaceholders(
      l10n_util::GetStringUTF16(IDS_POLICY_SKYVAULT_MIGRATION_ERROR_MESSAGE),
      {folder_name, provider_str},
      /*offsets=*/nullptr);
  std::u16string button =
      l10n_util::GetStringUTF16(IDS_POLICY_SKYVAULT_MIGRATION_ERROR_BUTTON);

  auto notification = CreateNotificationPtr(
      title, message,
      base::BindRepeating(&HandleNotificationClick, profile(), error_log_path));
  notification->set_buttons({message_center::ButtonInfo(button)});

  NotificationDisplayServiceFactory::GetForProfile(profile())->Display(
      NotificationHandler::Type::TRANSIENT, *notification,
      /*metadata=*/nullptr);
}

void MigrationNotificationManager::ShowConfigurationErrorNotification(
    CloudProvider provider) {
  std::u16string provider_str = CloudProviderToString(provider);

  std::u16string title = base::ReplaceStringPlaceholders(
      l10n_util::GetStringUTF16(
          IDS_POLICY_SKYVAULT_MIGRATION_CONFIG_ERROR_TITLE),
      provider_str,
      /*offset=*/nullptr);
  std::u16string message = base::ReplaceStringPlaceholders(
      l10n_util::GetStringUTF16(
          IDS_POLICY_SKYVAULT_MIGRATION_CONFIG_ERROR_MESSAGE),
      provider_str,
      /*offset=*/nullptr);

  auto notification = CreateNotificationPtr(title, message,
                                            /*callback=*/base::DoNothing());

  NotificationDisplayServiceFactory::GetForProfile(profile())->Display(
      NotificationHandler::Type::TRANSIENT, *notification,
      /*metadata=*/nullptr);
}

base::CallbackListSubscription
MigrationNotificationManager::ShowOneDriveSignInNotification(
    SignInCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (sign_in_callbacks_.empty()) {
    policy::skyvault_ui_utils::ShowSignInNotification(
        Profile::FromBrowserContext(context_), /*id=*/0,
        UploadTrigger::kMigration,
        /*file_path=*/base::FilePath(),
        base::BindOnce(&MigrationNotificationManager::OnSignInResponse,
                       weak_factory_.GetWeakPtr()));
  }

  // sign_in_callback_subscriptions_.emplace_back(
  return sign_in_callbacks_.Add(std::move(callback));
}

void MigrationNotificationManager::CloseAll() {
  // TODO(b/349097807): Potential race condition. When migration stopping is
  // fully implemented, make sure this runs after uploads were already stopped
  // (otherwise upload might fail before it's cancelled) and/or post this to
  // same sequence & fail new requests that come in (if closing exactly when an
  // upload job was getting paused for sign in).
  CloseNotification(profile());
  CloseDialog();
}

void MigrationNotificationManager::CloseDialog() {
  LocalFilesMigrationDialog* dialog = LocalFilesMigrationDialog::GetDialog();
  if (dialog) {
    dialog->Close();
  }
}

Profile* MigrationNotificationManager::profile() {
  return Profile::FromBrowserContext(context_);
}

void MigrationNotificationManager::OnSignInResponse(base::File::Error error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (error == base::File::Error::FILE_OK) {
    // This is only reached for OneDrive.
    ShowMigrationProgressNotification(CloudProvider::kOneDrive);
  }
  // If there was an error, the notification will be shown when migration fails.
  sign_in_callbacks_.Notify(error);
}

// static
MigrationNotificationManagerFactory*
MigrationNotificationManagerFactory::GetInstance() {
  static base::NoDestructor<MigrationNotificationManagerFactory> factory;
  return factory.get();
}

MigrationNotificationManager*
MigrationNotificationManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<MigrationNotificationManager*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

MigrationNotificationManagerFactory::MigrationNotificationManagerFactory()
    : ProfileKeyedServiceFactory(
          "MigrationNotificationManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(NotificationDisplayServiceFactory::GetInstance());
}

MigrationNotificationManagerFactory::~MigrationNotificationManagerFactory() =
    default;

bool MigrationNotificationManagerFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

std::unique_ptr<KeyedService>
MigrationNotificationManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<MigrationNotificationManager>(context);
}

}  // namespace policy::local_user_files
