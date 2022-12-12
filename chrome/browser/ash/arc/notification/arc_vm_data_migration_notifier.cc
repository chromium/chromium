// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/notification/arc_vm_data_migration_notifier.h"

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/session/arc_vm_data_migration_confirmation_dialog.h"
#include "ash/components/arc/session/arc_vm_data_migration_status.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "components/prefs/pref_service.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace arc {

namespace {

constexpr char kNotifierId[] = "arc_vm_data_migration_notifier";
constexpr char kNotificationId[] = "arc_vm_data_migration_notification";

bool ShouldShowNotification(Profile* profile) {
  switch (GetArcVmDataMigrationStatus(profile->GetPrefs())) {
    case ArcVmDataMigrationStatus::kUnnotified:
    case ArcVmDataMigrationStatus::kNotified:
    case ArcVmDataMigrationStatus::kConfirmed:
      return true;
    case ArcVmDataMigrationStatus::kStarted:
    case ArcVmDataMigrationStatus::kFinished:
      return false;
  }
}

}  // namespace

ArcVmDataMigrationNotifier::ArcVmDataMigrationNotifier(Profile* profile)
    : profile_(profile) {
  DCHECK(ArcSessionManager::Get());
  arc_session_observation_.Observe(ArcSessionManager::Get());
}

ArcVmDataMigrationNotifier::~ArcVmDataMigrationNotifier() = default;

void ArcVmDataMigrationNotifier::OnArcStarted() {
  // Show a notification only when the migration is enabled.
  if (!base::FeatureList::IsEnabled(kEnableArcVmDataMigration))
    return;

  // Do not show a notification if virtio-blk /data is forcibly enabled, in
  // which case the migration is not needed.
  if (base::FeatureList::IsEnabled(kEnableVirtioBlkForData))
    return;

  // TODO(b/258278176): Check policies and eligibility (e.g. whether LVM
  // application containers are enabled) before showing a notification.
  if (ShouldShowNotification(profile_)) {
    SetArcVmDataMigrationStatus(profile_->GetPrefs(),
                                ArcVmDataMigrationStatus::kNotified);
    ShowNotification();
  }
}

void ArcVmDataMigrationNotifier::OnArcSessionStopped(ArcStopReason reason) {
  CloseNotification();
}

void ArcVmDataMigrationNotifier::ShowNotification() {
  // TODO(b/258278176): Replace strings with l10n ones.
  // TODO(b/258278176): Replace icons once the final design decision is made.
  message_center::Notification notification = ash::CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE, kNotificationId,
      u"Update ChromeOS" /* title */, u"Up to 10 minutes needed" /* message */,
      u"ChromeOS" /* display_source */, GURL() /* origin_url */,
      message_center::NotifierId(
          message_center::NotifierType::SYSTEM_COMPONENT, kNotifierId,
          ash::NotificationCatalogName::kArcVmDataMigration),
      message_center::RichNotificationData() /* optional_fields */,
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(
              &ArcVmDataMigrationNotifier::OnNotificationClicked,
              weak_ptr_factory_.GetWeakPtr())),
      ash::kSystemMenuUpdateIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);
  notification.set_buttons({message_center::ButtonInfo(u"Update")});

  // Make the notification persist.
  // TODO(b/259278176): Check and decide what is an appropriate behavior here.
  notification.set_never_timeout(true);
  notification.set_pinned(true);

  NotificationDisplayService::GetForProfile(profile_)->Display(
      NotificationHandler::Type::TRANSIENT, notification,
      nullptr /* metadata */);
}

void ArcVmDataMigrationNotifier::CloseNotification() {
  auto* notification_display_service =
      NotificationDisplayService::GetForProfile(profile_);
  if (notification_display_service) {
    notification_display_service->Close(NotificationHandler::Type::TRANSIENT,
                                        kNotificationId);
  }
}

void ArcVmDataMigrationNotifier::OnNotificationClicked(
    absl::optional<int> button_index) {
  if (!button_index) {
    // Notification message body is clicked.
    return;
  }

  CloseNotification();

  ShowArcVmDataMigrationConfirmationDialog(
      base::BindOnce(&ArcVmDataMigrationNotifier::OnRestartAccepted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcVmDataMigrationNotifier::OnRestartAccepted(bool accepted) {
  if (accepted) {
    SetArcVmDataMigrationStatus(profile_->GetPrefs(),
                                ArcVmDataMigrationStatus::kConfirmed);
    chrome::AttemptRestart();
  }
  // TODO(b/258278176): Report when the confirmation dialog is canceled.
}

}  // namespace arc
