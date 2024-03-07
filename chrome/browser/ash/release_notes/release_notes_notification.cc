// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/release_notes/release_notes_notification.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string_util.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

using message_center::Notification;

namespace {
const char kShowNotificationID[] = "show_release_notes_notification";
}  // namespace

namespace ash {

ReleaseNotesNotification::ReleaseNotesNotification(Profile* profile)
    : profile_(profile), weak_ptr_factory_(this) {}

ReleaseNotesNotification::~ReleaseNotesNotification() {}

void ReleaseNotesNotification::MaybeShowReleaseNotes() {
  release_notes_storage_ = std::make_unique<ReleaseNotesStorage>(profile_);
  if (!release_notes_storage_->ShouldNotify() ||
      features::IsForestFeatureEnabled()) {
    return;
  }
  ShowReleaseNotesNotification();
  base::RecordAction(base::UserMetricsAction("ReleaseNotes.NotificationShown"));
  release_notes_storage_->MarkNotificationShown();
  // When the notification is shown we should also show the suggestion chip a
  // number of times.
  release_notes_storage_->StartShowingSuggestionChip();
}

void ReleaseNotesNotification::HandleClickShowNotification() {
  SystemNotificationHelper::GetInstance()->Close(kShowNotificationID);
  base::RecordAction(
      base::UserMetricsAction("ReleaseNotes.LaunchedNotification"));
  chrome::LaunchReleaseNotes(profile_,
                             apps::LaunchSource::kFromReleaseNotesNotification);
}

void ReleaseNotesNotification::ShowReleaseNotesNotification() {
  std::u16string title = ui::SubstituteChromeOSDeviceType(
      IDS_RELEASE_NOTES_DEVICE_SPECIFIC_NOTIFICATION_TITLE);
  std::u16string message =
      l10n_util::GetStringUTF16(IDS_RELEASE_NOTES_NOTIFICATION_MESSAGE);

  release_notes_available_notification_ = ash::CreateSystemNotificationPtr(
      message_center::NOTIFICATION_TYPE_SIMPLE, kShowNotificationID,
      std::move(title), std::move(message),
      l10n_util::GetStringUTF16(IDS_HELP_APP_EXPLORE), GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kShowNotificationID,
                                 NotificationCatalogName::kReleaseNotes),
      message_center::RichNotificationData(),
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(
              &ReleaseNotesNotification::HandleClickShowNotification,
              weak_ptr_factory_.GetWeakPtr())),
      kNotificationHelpAppIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);
  SystemNotificationHelper::GetInstance()->Display(
      *release_notes_available_notification_);
}

}  // namespace ash
