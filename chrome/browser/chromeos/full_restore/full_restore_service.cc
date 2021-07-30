// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/full_restore/full_restore_service.h"

#include "ash/public/cpp/notification_utils.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/full_restore/full_restore_app_launch_handler.h"
#include "chrome/browser/chromeos/full_restore/full_restore_data_handler.h"
#include "chrome/browser/chromeos/full_restore/full_restore_prefs.h"
#include "chrome/browser/chromeos/full_restore/full_restore_service_factory.h"
#include "chrome/browser/chromeos/full_restore/new_user_restore_pref_handler.h"
#include "chrome/browser/first_run/first_run.h"
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
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/message_center/public/cpp/notification.h"

namespace chromeos {
namespace full_restore {

bool g_restore_for_testing = true;

const char kRestoreForCrashNotificationId[] = "restore_for_crash_notification";
const char kRestoreNotificationId[] = "restore_notification";

const char kRestoreNotificationHistogramName[] = "Apps.RestoreNotification";
const char kRestoreForCrashNotificationHistogramName[] =
    "Apps.RestoreForCrashNotification";
const char kRestoreSettingHistogramName[] = "Apps.RestoreSetting";
const char kRestoreInitSettingHistogramName[] = "Apps.RestoreInitSetting";

constexpr char kWindowCountHistogramPrefix[] = "Apps.WindowCount.";
constexpr char kRestoreHistogramSuffix[] = "Restore";
constexpr char kNotRestoreHistogramSuffix[] = "NotRestore";
constexpr char kCloseByUserHistogramSuffix[] = "CloseByUser";
constexpr char kCloseNotByUserHistogramSuffix[] = "CloseNotByUser";

// static
FullRestoreService* FullRestoreService::GetForProfile(Profile* profile) {
  return static_cast<FullRestoreService*>(
      FullRestoreServiceFactory::GetInstance()->GetForProfile(profile));
}

FullRestoreService::FullRestoreService(Profile* profile)
    : profile_(profile),
      app_launch_handler_(std::make_unique<FullRestoreAppLaunchHandler>(
          profile_,
          /*should_init_service=*/true)),
      restore_data_handler_(
          std::make_unique<FullRestoreDataHandler>(profile_)) {
  notification_registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
                              content::NotificationService::AllSources());

  PrefService* prefs = profile_->GetPrefs();
  DCHECK(prefs);

  pref_change_registrar_.Init(prefs);
  pref_change_registrar_.Add(
      kRestoreAppsAndPagesPrefName,
      base::BindRepeating(&FullRestoreService::OnPreferenceChanged,
                          weak_ptr_factory_.GetWeakPtr()));

  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile_);
  if (user) {
    ::full_restore::FullRestoreInfo::GetInstance()->SetRestorePref(
        user->GetAccountId(), CanPerformRestore(prefs));
  }

  if (!HasRestorePref(prefs) && HasSessionStartupPref(prefs)) {
    // If there is no full restore pref, but there is a session restore setting,
    // set the first run flag to maintain the previous behavior for the first
    // time running the full restore feature when migrate to the full restore
    // release. Restore browsers and web apps by the browser session restore.
    first_run_full_restore_ = true;
    SetDefaultRestorePrefIfNecessary(prefs);
  }
}

FullRestoreService::~FullRestoreService() = default;

void FullRestoreService::Init() {
  // If it is the first time to migrate to the full restore release, we don't
  // have other restore data, so we don't need to consider restoration.
  if (first_run_full_restore_)
    return;

  PrefService* prefs = profile_->GetPrefs();
  DCHECK(prefs);

  // If the system crashed before reboot, show the restore notification.
  if (profile_->GetLastSessionExitType() == Profile::EXIT_CRASHED) {
    if (!HasRestorePref(prefs))
      SetDefaultRestorePrefIfNecessary(prefs);

    MaybeShowRestoreNotification(kRestoreForCrashNotificationId);
    return;
  }

  // If either OS pref setting nor Chrome pref setting exist, that means we
  // don't have restore data, so we don't need to consider restoration, and call
  // NewUserRestorePrefHandler to set OS pref setting.
  if (!HasRestorePref(prefs) && !HasSessionStartupPref(prefs)) {
    new_user_pref_handler_ =
        std::make_unique<NewUserRestorePrefHandler>(profile_);
    return;
  }

  RestoreOption restore_pref = static_cast<RestoreOption>(
      prefs->GetInteger(kRestoreAppsAndPagesPrefName));
  base::UmaHistogramEnumeration(kRestoreInitSettingHistogramName, restore_pref);
  switch (restore_pref) {
    case RestoreOption::kAlways:
      Restore();
      break;
    case RestoreOption::kAskEveryTime:
      MaybeShowRestoreNotification(kRestoreNotificationId);
      break;
    case RestoreOption::kDoNotRestore:
      return;
  }
}

void FullRestoreService::LaunchBrowserWhenReady() {
  if (!g_restore_for_testing)
    return;

  app_launch_handler_->LaunchBrowserWhenReady(first_run_full_restore_);
}

void FullRestoreService::Close(bool by_user) {
  if (!skip_notification_histogram_) {
    RecordRestoreAction(
        notification_->id(),
        by_user ? RestoreAction::kCloseByUser : RestoreAction::kCloseNotByUser);
    RecordWindowCount(by_user ? kCloseByUserHistogramSuffix
                              : kCloseNotByUserHistogramSuffix);
  }
}
void FullRestoreService::Click(const absl::optional<int>& button_index,
                               const absl::optional<std::u16string>& reply) {
  DCHECK(notification_);

  if (!button_index.has_value()) {
    if (notification_->id() == kRestoreNotificationId) {
      // Show the 'On Startup' OS setting page.
      chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
          profile_, chromeos::settings::mojom::kAppsSectionPath);
    }
    return;
  }

  skip_notification_histogram_ = true;
  RecordRestoreAction(
      notification_->id(),
      button_index.value() ==
              static_cast<int>(RestoreNotificationButtonIndex::kRestore)
          ? RestoreAction::kRestore
          : RestoreAction::kCancel);

  if (button_index.value() ==
      static_cast<int>(RestoreNotificationButtonIndex::kRestore)) {
    RecordWindowCount(kRestoreHistogramSuffix);
    Restore();
  } else {
    RecordWindowCount(kNotRestoreHistogramSuffix);
  }

  if (!is_shut_down_) {
    NotificationDisplayService::GetForProfile(profile_)->Close(
        NotificationHandler::Type::TRANSIENT, notification_->id());
  }
}

void FullRestoreService::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_APP_TERMINATING, type);
  ::full_restore::FullRestoreSaveHandler::GetInstance()->SetShutDown();
}

void FullRestoreService::Shutdown() {
  is_shut_down_ = true;
}

void FullRestoreService::MaybeShowRestoreNotification(const std::string& id) {
  if (!ShouldShowNotification())
    return;

  message_center::RichNotificationData notification_data;

  message_center::ButtonInfo restore_button(l10n_util::GetStringUTF16(
      base::ToUpperASCII(IDS_RESTORE_NOTIFICATION_RESTORE_BUTTON)));
  notification_data.buttons.push_back(restore_button);

  message_center::ButtonInfo cancel_button(l10n_util::GetStringUTF16(
      base::ToUpperASCII(IDS_RESTORE_NOTIFICATION_CANCEL_BUTTON)));
  notification_data.buttons.push_back(cancel_button);

  std::u16string title;
  if (id == kRestoreForCrashNotificationId) {
    title = l10n_util::GetStringFUTF16(IDS_RESTORE_CRASH_NOTIFICATION_TITLE,
                                       ui::GetChromeOSDeviceName());
  } else {
    title = l10n_util::GetStringUTF16(IDS_RESTORE_NOTIFICATION_TITLE);
  }

  int message_id;
  if (id == kRestoreForCrashNotificationId)
    message_id = IDS_RESTORE_CRASH_NOTIFICATION_MESSAGE;
  else
    message_id = IDS_RESTORE_NOTIFICATION_MESSAGE;

  notification_ = ash::CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE, id, title,
      l10n_util::GetStringUTF16(message_id),
      l10n_util::GetStringUTF16(IDS_RESTORE_NOTIFICATION_DISPLAY_SOURCE),
      GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 id),
      notification_data,
      base::MakeRefCounted<message_center::ThunkNotificationDelegate>(
          weak_ptr_factory_.GetWeakPtr()),
      kFullRestoreNotificationIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);

  auto* notification_display_service =
      NotificationDisplayService::GetForProfile(profile_);
  DCHECK(notification_display_service);
  notification_display_service->Display(NotificationHandler::Type::TRANSIENT,
                                        *notification_,
                                        /*metadata=*/nullptr);
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

void FullRestoreService::RecordRestoreAction(const std::string& notification_id,
                                             RestoreAction restore_action) {
  base::UmaHistogramEnumeration(notification_id == kRestoreNotificationId
                                    ? kRestoreNotificationHistogramName
                                    : kRestoreForCrashNotificationHistogramName,
                                restore_action);
}

void FullRestoreService::OnPreferenceChanged(const std::string& pref_name) {
  DCHECK_EQ(pref_name, kRestoreAppsAndPagesPrefName);

  RestoreOption restore_option = static_cast<RestoreOption>(
      profile_->GetPrefs()->GetInteger(kRestoreAppsAndPagesPrefName));
  base::UmaHistogramEnumeration(kRestoreSettingHistogramName, restore_option);

  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile_);
  if (user) {
    ::full_restore::FullRestoreInfo::GetInstance()->SetRestorePref(
        user->GetAccountId(), CanPerformRestore(profile_->GetPrefs()));
  }
}

bool FullRestoreService::ShouldShowNotification() {
  return app_launch_handler_->HasRestoreData() &&
         !::first_run::IsChromeFirstRun();
}

void FullRestoreService::RecordWindowCount(const std::string& restore_action) {
  base::UmaHistogramCounts100(
      kWindowCountHistogramPrefix + restore_action,
      ::full_restore::FullRestoreSaveHandler::GetInstance()->window_count());
}

ScopedRestoreForTesting::ScopedRestoreForTesting() {
  g_restore_for_testing = false;
}

ScopedRestoreForTesting::~ScopedRestoreForTesting() {
  g_restore_for_testing = true;
}

}  // namespace full_restore
}  // namespace chromeos
