// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/eol_notification.h"

#include "ash/public/cpp/notification_utils.h"
#include "base/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/time/default_clock.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"

using l10n_util::GetStringUTF16;

namespace chromeos {
namespace {

const char kEolNotificationId[] = "chrome://product_eol";

constexpr int kFirstWarningDaysInAdvance = 180;
constexpr int kSecondWarningDaysInAdvance = 90;

base::Time FirstWarningDate(base::Time eol_date) {
  return eol_date - base::TimeDelta::FromDays(kFirstWarningDaysInAdvance);
}

base::Time SecondWarningDate(const base::Time& eol_date) {
  return eol_date - base::TimeDelta::FromDays(kSecondWarningDaysInAdvance);
}

base::string16 FormatMonthAndYearWithOffset(base::Time eol_date) {
  // TODO(crbug/998983): This is not the ideal way to correct months shifts.
  // A follow up CL will modify base/i18n/time_formatting.h so that
  // setting the time can be formatted based off UTC rather than only local.
  //
  // If the EOL date is on the first day of the month, then notifications with
  // different month names may be shown to different users by
  // base::TimeFormatMonthAndYear(), depending on their time zone.  There are
  // devices in Goldeneye with EOL dates on the first and last day of the month.
  // Since only the month is shown, the day is set to the 15th to prevent any
  // forward or backward month shifts.
  constexpr int kApproxMidPointDayInMonth = 15;

  base::Time adjusted_date;
  base::Time::Exploded exploded;
  eol_date.UTCExplode(&exploded);
  exploded.day_of_month = kApproxMidPointDayInMonth;
  if (!base::Time::FromUTCExploded(exploded, &adjusted_date)) {
    return base::TimeFormatMonthAndYear(eol_date);
  }
  return base::TimeFormatMonthAndYear(adjusted_date);
}

}  // namespace

// static
bool EolNotification::ShouldShowEolNotification() {
  // Do not show end of life notification if this device is managed by
  // enterprise user.
  if (g_browser_process->platform_part()
          ->browser_policy_connector_chromeos()
          ->IsEnterpriseManaged()) {
    return false;
  }

  return true;
}

EolNotification::EolNotification(Profile* profile)
    : clock_(base::DefaultClock::GetInstance()), profile_(profile) {}

EolNotification::~EolNotification() {}

void EolNotification::CheckEolInfo() {
  UpdateEngineClient* update_engine_client =
      DBusThreadManager::Get()->GetUpdateEngineClient();

  // Request the Eol Info.
  update_engine_client->GetEolInfo(base::BindOnce(
      &EolNotification::OnEolInfo, weak_ptr_factory_.GetWeakPtr()));
}

void EolNotification::OnEolInfo(UpdateEngineClient::EolInfo eol_info) {
  // Do not show warning Eol notification if invalid |eol_info.eol_date|.
  if (eol_info.eol_date.is_null())
    return;

  const base::Time now = clock_->Now();
  const base::Time eol_date = eol_info.eol_date;
  const base::Time prev_eol_date =
      profile_->GetPrefs()->GetTime(prefs::kEndOfLifeDate);

  profile_->GetPrefs()->SetTime(prefs::kEndOfLifeDate, eol_date);

  if (!now.is_null() && eol_date != prev_eol_date && now < eol_date) {
    // Reset showed warning prefs if the Eol date changed.
    profile_->GetPrefs()->SetBoolean(prefs::kFirstEolWarningDismissed, false);
    profile_->GetPrefs()->SetBoolean(prefs::kSecondEolWarningDismissed, false);
    profile_->GetPrefs()->SetBoolean(prefs::kEolNotificationDismissed, false);
  }

  if (eol_date <= now) {
    dismiss_pref_ = prefs::kEolNotificationDismissed;
  } else if (SecondWarningDate(eol_date) <= now) {
    dismiss_pref_ = prefs::kSecondEolWarningDismissed;
  } else if (FirstWarningDate(eol_date) <= now) {
    dismiss_pref_ = prefs::kFirstEolWarningDismissed;
  } else {
    // |now| < FirstWarningDate() so don't show anything.
    dismiss_pref_ = base::nullopt;
    return;
  }

  // Do not show if notification has already been dismissed or is out of range.
  if (!dismiss_pref_ || profile_->GetPrefs()->GetBoolean(*dismiss_pref_))
    return;

  CreateNotification(eol_date, now);
}

void EolNotification::CreateNotification(base::Time eol_date, base::Time now) {
  CHECK(!eol_date.is_null());
  CHECK(!now.is_null());

  message_center::RichNotificationData data;
  std::unique_ptr<message_center::Notification> notification;

  DCHECK_EQ(BUTTON_MORE_INFO, data.buttons.size());
  data.buttons.emplace_back(GetStringUTF16(IDS_LEARN_MORE));

  if (now < eol_date) {
    // Notifies user that updates will stop occurring at a month and year.
    notification = ash::CreateSystemNotification(
        message_center::NOTIFICATION_TYPE_SIMPLE, kEolNotificationId,
        l10n_util::GetStringFUTF16(IDS_PENDING_EOL_NOTIFICATION_TITLE,
                                   FormatMonthAndYearWithOffset(eol_date)),
        l10n_util::GetStringFUTF16(IDS_PENDING_EOL_NOTIFICATION_MESSAGE,
                                   ui::GetChromeOSDeviceName()),
        base::string16() /* display_source */, GURL(kEolNotificationId),
        message_center::NotifierId(
            message_center::NotifierType::SYSTEM_COMPONENT, kEolNotificationId),
        data,
        base::MakeRefCounted<message_center::ThunkNotificationDelegate>(
            weak_ptr_factory_.GetWeakPtr()),
        vector_icons::kBusinessIcon,
        message_center::SystemNotificationWarningLevel::NORMAL);
  } else {
    DCHECK_EQ(BUTTON_DISMISS, data.buttons.size());
    data.buttons.emplace_back(GetStringUTF16(IDS_EOL_DISMISS_BUTTON));

    // Notifies user that updates will no longer occur after this final update.
    notification = ash::CreateSystemNotification(
        message_center::NOTIFICATION_TYPE_SIMPLE, kEolNotificationId,
        GetStringUTF16(IDS_EOL_NOTIFICATION_TITLE),
        l10n_util::GetStringFUTF16(IDS_EOL_NOTIFICATION_EOL,
                                   ui::GetChromeOSDeviceName()),
        base::string16() /* display_source */, GURL(kEolNotificationId),
        message_center::NotifierId(
            message_center::NotifierType::SYSTEM_COMPONENT, kEolNotificationId),
        data,
        base::MakeRefCounted<message_center::ThunkNotificationDelegate>(
            weak_ptr_factory_.GetWeakPtr()),
        kNotificationEndOfSupportIcon,
        message_center::SystemNotificationWarningLevel::NORMAL);
  }

  NotificationDisplayServiceFactory::GetForProfile(profile_)->Display(
      NotificationHandler::Type::TRANSIENT, *notification,
      /*metadata=*/nullptr);
}

void EolNotification::Close(bool by_user) {
  // Only the final Eol notification has an explicit dismiss button, and
  // is only dismissible by that button.  The first and second warning
  // buttons do not have an explicit dismiss button.
  if (!by_user || !dismiss_pref_ ||
      dismiss_pref_ == prefs::kEolNotificationDismissed) {
    return;
  }

  profile_->GetPrefs()->SetBoolean(*dismiss_pref_, true);
}

void EolNotification::Click(const base::Optional<int>& button_index,
                            const base::Optional<base::string16>& reply) {
  if (!button_index)
    return;

  switch (*button_index) {
    case BUTTON_MORE_INFO: {
      // show eol link
      NavigateParams params(profile_, GURL(chrome::kEolNotificationURL),
                            ui::PAGE_TRANSITION_LINK);
      params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
      params.window_action = NavigateParams::SHOW_WINDOW;
      Navigate(&params);
      break;
    }
    case BUTTON_DISMISS:
      CHECK(dismiss_pref_);
      // set dismiss pref.
      profile_->GetPrefs()->SetBoolean(*dismiss_pref_, true);
      break;
  }

  if (dismiss_pref_ && (*dismiss_pref_ != prefs::kEolNotificationDismissed))
    profile_->GetPrefs()->SetBoolean(*dismiss_pref_, true);

  NotificationDisplayServiceFactory::GetForProfile(profile_)->Close(
      NotificationHandler::Type::TRANSIENT, kEolNotificationId);
}

}  // namespace chromeos
