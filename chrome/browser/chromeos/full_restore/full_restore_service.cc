// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/full_restore/full_restore_service.h"

#include "ash/public/cpp/notification_utils.h"
#include "base/strings/string_util.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/full_restore/app_launch_handler.h"
#include "chrome/browser/chromeos/full_restore/full_restore_data_handler.h"
#include "chrome/browser/chromeos/full_restore/full_restore_prefs.h"
#include "chrome/browser/chromeos/full_restore/full_restore_service_factory.h"
#include "chrome/browser/chromeos/full_restore/new_user_restore_pref_handler.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/account_id/account_id.h"
#include "components/full_restore/full_restore_info.h"
#include "components/full_restore/full_restore_save_handler.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"

namespace chromeos {
namespace full_restore {

const char kRestoreForCrashNotificationId[] = "restore_for_crash_notification";
const char kRestoreNotificationId[] = "restore_notification";
const char kSetRestorePrefNotificationId[] = "set_restore_pref_notification";

// If the user selected the 'Restore' button from the restore notification
// dialog for more than |kMaxConsecutiveRestoreSelectionCount| times, show the
// set restore pref notification.
const int kMaxConsecutiveRestoreSelectionCount = 3;

// static
FullRestoreService* FullRestoreService::GetForProfile(Profile* profile) {
  return static_cast<FullRestoreService*>(
      FullRestoreServiceFactory::GetInstance()->GetForProfile(profile));
}

FullRestoreService::FullRestoreService(Profile* profile)
    : profile_(profile),
      app_launch_handler_(std::make_unique<AppLaunchHandler>(profile_)),
      restore_data_handler_(
          std::make_unique<FullRestoreDataHandler>(profile_)) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&FullRestoreService::Init,
                                weak_ptr_factory_.GetWeakPtr()));
}

FullRestoreService::~FullRestoreService() = default;

void FullRestoreService::LaunchBrowserWhenReady() {
  app_launch_handler_->LaunchBrowserWhenReady();
}

void FullRestoreService::RestoreForTesting() {
  // If there is no browser launch info, the browser won't be launched. So call
  // SetForceLaunchBrowserForTesting to launch the browser for testing.
  app_launch_handler_->SetForceLaunchBrowserForTesting();

  Restore();
}

void FullRestoreService::Init() {
  // If the system crashed before reboot, show the restore notification.
  if (profile_->GetLastSessionExitType() == Profile::EXIT_CRASHED) {
    ShowRestoreNotification(kRestoreForCrashNotificationId);
    return;
  }

  PrefService* prefs = profile_->GetPrefs();
  DCHECK(prefs);

  // If it is the first time to run Chrome OS, we don't have restore data, so we
  // don't need to consider restoration.
  if (user_manager::UserManager::Get()->IsCurrentUserNew()) {
    new_user_pref_handler_ =
        std::make_unique<NewUserRestorePrefHandler>(profile_);
    return;
  }

  // If it is the first time to migrate to the full restore release, we don't
  // have other restore data, so we don't need to consider restoration.
  if (!HasRestorePref(prefs)) {
    SetDefaultRestorePrefIfNecessary(prefs);
    return;
  }

  RestoreOption restore_pref = static_cast<RestoreOption>(
      prefs->GetInteger(kRestoreAppsAndPagesPrefName));
  switch (restore_pref) {
    case RestoreOption::kAlways:
      Restore();
      break;
    case RestoreOption::kAskEveryTime:
      ShowRestoreNotification(kRestoreNotificationId);
      break;
    case RestoreOption::kDoNotRestore:
      return;
  }
}

void FullRestoreService::Shutdown() {
  is_shut_down_ = true;
}

void FullRestoreService::ShowRestoreNotification(const std::string& id) {
  message_center::RichNotificationData notification_data;

  message_center::ButtonInfo restore_button(l10n_util::GetStringUTF16(
      base::ToUpperASCII(id == kSetRestorePrefNotificationId
                             ? IDS_SET_RESTORE_NOTIFICATION_BUTTON
                             : IDS_RESTORE_NOTIFICATION_RESTORE_BUTTON)));
  notification_data.buttons.push_back(restore_button);

  message_center::ButtonInfo cancel_button(l10n_util::GetStringUTF16(
      base::ToUpperASCII(IDS_RESTORE_NOTIFICATION_CANCEL_BUTTON)));
  notification_data.buttons.push_back(cancel_button);

  int title_id = id == kSetRestorePrefNotificationId
                     ? IDS_SET_RESTORE_NOTIFICATION_TITLE
                     : IDS_RESTORE_NOTIFICATION_TITLE;

  int message_id;
  if (id == kRestoreForCrashNotificationId)
    message_id = IDS_RESTORE_FOR_CRASH_NOTIFICATION_MESSAGE;
  else if (id == kRestoreNotificationId)
    message_id = IDS_RESTORE_NOTIFICATION_MESSAGE;
  else
    message_id = IDS_SET_RESTORE_NOTIFICATION_MESSAGE;

  notification_ = ash::CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE, id,
      l10n_util::GetStringUTF16(title_id),
      l10n_util::GetStringUTF16(message_id),
      l10n_util::GetStringUTF16(IDS_RESTORE_NOTIFICATION_DISPLAY_SOURCE),
      GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 id),
      notification_data,
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(
              &FullRestoreService::HandleRestoreNotificationClicked,
              weak_ptr_factory_.GetWeakPtr())),
      kFullRestoreNotificationIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);
  notification_->set_priority(message_center::SYSTEM_PRIORITY);

  auto* notification_display_service =
      NotificationDisplayService::GetForProfile(profile_);
  DCHECK(notification_display_service);
  notification_display_service->Display(NotificationHandler::Type::TRANSIENT,
                                        *notification_,
                                        /*metadata=*/nullptr);
}

void FullRestoreService::HandleRestoreNotificationClicked(
    base::Optional<int> button_index) {
  DCHECK(notification_);
  if (!is_shut_down_) {
    NotificationDisplayService::GetForProfile(profile_)->Close(
        NotificationHandler::Type::TRANSIENT, notification_->id());
  }

  if (!button_index.has_value() ||
      button_index.value() !=
          static_cast<int>(RestoreNotificationButtonIndex::kRestore)) {
    return;
  }

  if (notification_->id() == kSetRestorePrefNotificationId) {
    // Show the 'On Startup' OS setting page.
    chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
        profile_, chromeos::settings::mojom::kOnStartupSectionPath);
    return;
  }

  int count = GetRestoreSelectedCountPref(profile_->GetPrefs());

  if (count < kMaxConsecutiveRestoreSelectionCount)
    SetRestoreSelectedCountPref(profile_->GetPrefs(), ++count);

  // If the user selects the 'restore' button for more than 3 times, show the
  // set restore pref notification.
  if (count >= kMaxConsecutiveRestoreSelectionCount)
    ShowRestoreNotification(kSetRestorePrefNotificationId);

  Restore();
}

void FullRestoreService::Restore() {
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile_);
  if (user) {
    ::full_restore::FullRestoreInfo::GetInstance()->SetRestoreFlag(
        user->GetAccountId(), true);
  }

  app_launch_handler_->SetShouldRestore();
}

}  // namespace full_restore
}  // namespace chromeos
