// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/notification/arc_vm_data_migration_notifier.h"

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/session/arc_vm_data_migration_status.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/policy/arc_policy_util.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/ui/ash/arc/arc_vm_data_migration_confirmation_dialog.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
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
      return !policy_util::IsAccountManaged(profile) ||
             GetArcVmDataMigrationStrategy(profile->GetPrefs()) ==
                 ArcVmDataMigrationStrategy::kPrompt;
    case ArcVmDataMigrationStatus::kStarted:
    case ArcVmDataMigrationStatus::kFinished:
      return false;
  }
}

void ReportNotificationShownForTheFirstTime() {
  base::UmaHistogramBoolean(
      "Arc.VmDataMigration.NotificationShownForTheFirstTime", true);
}

void ReportNotificationShown(int days_until_deadline) {
  base::UmaHistogramExactLinear(
      "Arc.VmDataMigration.RemainingDays.NotificationShown",
      days_until_deadline, kArcVmDataMigrationNumberOfDismissibleDays);
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

  // Report the migration status at the beginning of each ARC session.
  // The status might have been changed to kFinished by ArcSessionManager in its
  // initialization step when there is no Android /data to migrate.
  // The status might be changed to kNotified within this function call when the
  // notification is shown.
  base::UmaHistogramEnumeration(
      GetHistogramNameByUserType(
          kArcVmDataMigrationStatusOnArcStartedHistogramName, profile_),
      GetArcVmDataMigrationStatus(profile_->GetPrefs()));

  // Do not show a notification if virtio-blk /data is forcibly enabled, in
  // which case the migration is not needed.
  if (base::FeatureList::IsEnabled(kEnableVirtioBlkForData))
    return;

  if (!ShouldShowNotification(profile_)) {
    return;
  }

  if (GetArcVmDataMigrationStatus(profile_->GetPrefs()) ==
      ArcVmDataMigrationStatus::kUnnotified) {
    ReportNotificationShownForTheFirstTime();
    profile_->GetPrefs()->SetTime(
        prefs::kArcVmDataMigrationNotificationFirstShownTime,
        base::Time::Now());
  }
  SetArcVmDataMigrationStatus(profile_->GetPrefs(),
                              ArcVmDataMigrationStatus::kNotified);
  ShowNotification();
}

void ArcVmDataMigrationNotifier::OnArcSessionStopped(ArcStopReason reason) {
  CloseNotification();
}

void ArcVmDataMigrationNotifier::OnArcSessionBlockedByArcVmDataMigration(
    bool auto_resume_enabled) {
  if (auto_resume_enabled) {
    // No need to show a notification.
    return;
  }
  LOG(WARNING) << "Showing a non-dismissible notification for ARCVM /data "
                  "migration, because auto-resume is disabled";
  // TODO(b/258278176): Implement appropriate UI.
  base::UmaHistogramBoolean("Arc.VmDataMigration.ResumeNotificationShown",
                            true);
  ShowNotification();
}

void ArcVmDataMigrationNotifier::ShowNotification() {
  const int days_until_deadline =
      GetDaysUntilArcVmDataMigrationDeadline(profile_->GetPrefs());
  ReportNotificationShown(days_until_deadline);

  message_center::Notification notification = ash::CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE, kNotificationId,
      l10n_util::GetStringUTF16(IDS_ARC_VM_DATA_MIGRATION_NOTIFICATION_TITLE),
      l10n_util::GetPluralStringFUTF16(
          IDS_ARC_VM_DATA_MIGRATION_NOTIFICATION_DAYS_UNTIL_DEADLINE,
          days_until_deadline),
      l10n_util::GetStringUTF16(IDS_ASH_MESSAGE_CENTER_SYSTEM_APP_NAME),
      GURL() /* origin_url */,
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
  notification.set_buttons(
      {message_center::ButtonInfo(l10n_util::GetStringUTF16(
          IDS_ARC_VM_DATA_MIGRATION_NOTIFICATION_ACCEPT_BUTTON_LABEL))});

  // Set the highest (system) priority.
  notification.SetSystemPriority();
  // Set no timeout so that the notification never disappears spontaneously.
  notification.set_never_timeout(true);

  NotificationDisplayServiceFactory::GetForProfile(profile_)->Display(
      NotificationHandler::Type::TRANSIENT, notification,
      nullptr /* metadata */);
}

void ArcVmDataMigrationNotifier::CloseNotification() {
  auto* notification_display_service =
      NotificationDisplayServiceFactory::GetForProfile(profile_);
  if (notification_display_service) {
    notification_display_service->Close(NotificationHandler::Type::TRANSIENT,
                                        kNotificationId);
  }
}

void ArcVmDataMigrationNotifier::OnNotificationClicked(
    std::optional<int> button_index) {
  if (!button_index) {
    // Notification message body is clicked.
    return;
  }

  CloseNotification();

  ShowArcVmDataMigrationConfirmationDialog(
      profile_->GetPrefs(),
      base::BindOnce(&ArcVmDataMigrationNotifier::OnRestartAccepted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcVmDataMigrationNotifier::OnRestartAccepted(bool accepted) {
  if (accepted) {
    auto* prefs = profile_->GetPrefs();
    if (GetArcVmDataMigrationStatus(prefs) !=
        ArcVmDataMigrationStatus::kStarted) {
      SetArcVmDataMigrationStatus(prefs, ArcVmDataMigrationStatus::kConfirmed);
    }
    chrome::AttemptRestart();
  }
}

}  // namespace arc
