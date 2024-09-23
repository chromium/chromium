// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/eol/eol_notification.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "ash/public/cpp/system_notification_builder.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/eol/eol_incentive_util.h"
#include "chrome/browser/ash/extended_updates/extended_updates_controller.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/system/system_tray_client_impl.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"

namespace ash {
namespace {

using ::l10n_util::GetStringUTF16;

const char kEolNotificationId[] = "chrome://product_eol";

constexpr int kFirstWarningDaysInAdvance = 180;
constexpr int kSecondWarningDaysInAdvance = 90;

// The first and second incentive notification button indices.
constexpr int kButtonClaim = 0;
constexpr int kButtonAboutUpdates = 1;

// The number of days past the EOL within which the last incentive notification
// is shown.
constexpr int kLastIncentiveEndDaysPastEol = -5;

base::Time FirstWarningDate(base::Time eol_date) {
  return eol_date - base::Days(kFirstWarningDaysInAdvance);
}

base::Time SecondWarningDate(const base::Time& eol_date) {
  return eol_date - base::Days(kSecondWarningDaysInAdvance);
}

}  // namespace

// static
bool EolNotification::ShouldShowEolNotification() {
  // Do not show end of life notification if this device is managed by
  // enterprise user.
  if (g_browser_process->platform_part()
          ->browser_policy_connector_ash()
          ->IsDeviceEnterpriseManaged()) {
    return false;
  }

  return true;
}

EolNotification::EolNotification(Profile* profile)
    : clock_(base::DefaultClock::GetInstance()), profile_(profile) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEolResetDismissedPrefs)) {
    ResetDismissedPrefs();
  }
}

EolNotification::~EolNotification() = default;

void EolNotification::CheckEolInfo() {
  // Request the Eol Info.
  UpdateEngineClient::Get()->GetEolInfo(base::BindOnce(
      &EolNotification::OnEolInfo, weak_ptr_factory_.GetWeakPtr()));
}

void EolNotification::OnEolInfo(UpdateEngineClient::EolInfo eol_info) {
  MaybeShowEolNotification(eol_info.eol_date);

  ExtendedUpdatesController::Get()->OnEolInfo(profile_, eol_info);
}

void EolNotification::MaybeShowEolNotification(base::Time eol_date) {
  // Do not show warning Eol notification if invalid |eol_date|.
  if (eol_date.is_null()) {
    return;
  }

  const base::Time now = clock_->Now();
  const base::Time prev_eol_date =
      profile_->GetPrefs()->GetTime(prefs::kEndOfLifeDate);

  profile_->GetPrefs()->SetTime(prefs::kEndOfLifeDate, eol_date);

  if (!now.is_null() && eol_date != prev_eol_date && now < eol_date) {
    // Reset showed warning prefs if the Eol date changed.
    ResetDismissedPrefs();
  }

  eol_incentive_util::EolIncentiveType incentive_type =
      eol_incentive_util::ShouldShowEolIncentive(profile_, eol_date, now);

  SystemTrayClientImpl* tray_client = SystemTrayClientImpl::Get();
  if (tray_client) {
    tray_client->SetShowEolNotice(
        incentive_type ==
                ash::eol_incentive_util::EolIncentiveType::kEolPassed ||
            incentive_type ==
                ash::eol_incentive_util::EolIncentiveType::kEolPassedRecently,
        incentive_type ==
            ash::eol_incentive_util::EolIncentiveType::kEolPassedRecently);
  }

  if (incentive_type != eol_incentive_util::EolIncentiveType::kNone) {
    MaybeShowEolIncentiveNotification(eol_date, incentive_type);
    return;
  }

  if (eol_date <= now) {
    dismiss_pref_ = prefs::kEolNotificationDismissed;
  } else if (SecondWarningDate(eol_date) <= now) {
    dismiss_pref_ = prefs::kSecondEolWarningDismissed;
  } else if (FirstWarningDate(eol_date) <= now) {
    if (base::FeatureList::IsEnabled(features::kSuppressFirstEolWarning)) {
      dismiss_pref_ = std::nullopt;
      return;
    }
    dismiss_pref_ = prefs::kFirstEolWarningDismissed;
  } else {
    // |now| < FirstWarningDate() so don't show anything.
    dismiss_pref_ = std::nullopt;
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
  ash::SystemNotificationBuilder notification_builder;

  DCHECK_EQ(BUTTON_MORE_INFO, data.buttons.size());
  data.buttons.emplace_back(GetStringUTF16(IDS_LEARN_MORE));

  if (now < eol_date) {
    // Notifies user that updates will stop occurring at a month and year.
    notification_builder
        .SetTitleWithArgs(IDS_PENDING_EOL_NOTIFICATION_TITLE,
                          {TimeFormatMonthAndYearForTimeZone(
                              eol_date, icu::TimeZone::getGMT())})
        .SetMessageWithArgs(IDS_PENDING_EOL_NOTIFICATION_MESSAGE,
                            {ui::GetChromeOSDeviceName()})
        .SetCatalogName(NotificationCatalogName::kPendingEOL)
        .SetSmallImage(vector_icons::kBusinessIcon);
  } else {
    DCHECK_EQ(BUTTON_DISMISS, data.buttons.size());
    data.buttons.emplace_back(GetStringUTF16(IDS_EOL_DISMISS_BUTTON));

    // Notifies user that updates will no longer occur after this final update.
    notification_builder.SetTitleId(IDS_EOL_NOTIFICATION_TITLE)
        .SetMessageWithArgs(IDS_EOL_NOTIFICATION_EOL,
                            {ui::GetChromeOSDeviceName()})
        .SetCatalogName(NotificationCatalogName::kEOL)
        .SetSmallImage(kNotificationEndOfSupportIcon);
  }

  NotificationDisplayServiceFactory::GetForProfile(profile_)->Display(
      NotificationHandler::Type::TRANSIENT,
      notification_builder.SetId(kEolNotificationId)
          .SetOriginUrl(GURL(kEolNotificationId))
          .SetOptionalFields(data)
          .SetDelegate(
              base::MakeRefCounted<message_center::ThunkNotificationDelegate>(
                  weak_ptr_factory_.GetWeakPtr()))
          .Build(false),
      /*metadata=*/nullptr);

  eol_incentive_util::RecordShowSourceHistogram(
      eol_incentive_util::EolIncentiveShowSource::kNotification_Original);
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

void EolNotification::Click(const std::optional<int>& button_index,
                            const std::optional<std::u16string>& reply) {
  if (!button_index) {
    return;
  }

  if (dismiss_pref_ == prefs::kEolApproachingIncentiveNotificationDismissed ||
      dismiss_pref_ == prefs::kEolPassedFinalIncentiveDismissed) {
    bool use_offer_url = features::kEolIncentiveParam.Get() !=
                         features::EolIncentiveParam::kNoOffer;
    switch (*button_index) {
      case kButtonClaim:
        // Open link for eol incentive notification.
        NewWindowDelegate::GetPrimary()->OpenUrl(
            GURL(use_offer_url ? chrome::kEolIncentiveNotificationOfferURL
                               : chrome::kEolIncentiveNotificationNoOfferURL),
            NewWindowDelegate::OpenUrlFrom::kUserInteraction,
            NewWindowDelegate::Disposition::kNewForegroundTab);

        if (dismiss_pref_ ==
            prefs::kEolApproachingIncentiveNotificationDismissed) {
          // Record button pressed for eol approaching.
          eol_incentive_util::RecordButtonClicked(
              use_offer_url ? eol_incentive_util::EolIncentiveButtonType::
                                  kNotification_Offer_Approaching
                            : eol_incentive_util::EolIncentiveButtonType::
                                  kNotification_NoOffer_Approaching);
        } else {
          // Record button pressed for eol recently passed.
          eol_incentive_util::RecordButtonClicked(
              use_offer_url ? eol_incentive_util::EolIncentiveButtonType::
                                  kNotification_Offer_RecentlyPassed
                            : eol_incentive_util::EolIncentiveButtonType::
                                  kNotification_NoOffer_RecentlyPassed);
        }
        break;
      case kButtonAboutUpdates:
        // Open link to learn more about updates.
        NewWindowDelegate::GetPrimary()->OpenUrl(
            GURL(chrome::kEolNotificationURL),
            NewWindowDelegate::OpenUrlFrom::kUserInteraction,
            NewWindowDelegate::Disposition::kNewForegroundTab);

        eol_incentive_util::RecordButtonClicked(
            dismiss_pref_ ==
                    prefs::kEolApproachingIncentiveNotificationDismissed
                ? eol_incentive_util::EolIncentiveButtonType::
                      kNotification_AboutUpdates_Approaching
                : eol_incentive_util::EolIncentiveButtonType::
                      kNotification_AboutUpdates_RecentlyPassed);
        break;
    }
    profile_->GetPrefs()->SetBoolean(prefs::kEolNotificationDismissed, true);
  } else {
    switch (*button_index) {
      case BUTTON_MORE_INFO: {
        const GURL url(dismiss_pref_ == prefs::kEolNotificationDismissed
                           ? chrome::kEolNotificationURL
                           : chrome::kAutoUpdatePolicyURL);
        // Show eol link.
        NewWindowDelegate::GetPrimary()->OpenUrl(
            url, NewWindowDelegate::OpenUrlFrom::kUserInteraction,
            NewWindowDelegate::Disposition::kNewForegroundTab);

        eol_incentive_util::RecordButtonClicked(
            eol_incentive_util::EolIncentiveButtonType::
                kNotification_Original_LearnMore);
        break;
      }
      case BUTTON_DISMISS:
        CHECK(dismiss_pref_);
        eol_incentive_util::RecordButtonClicked(
            eol_incentive_util::EolIncentiveButtonType::
                kNotification_Original_Dismiss);
        // Set dismiss pref.
        profile_->GetPrefs()->SetBoolean(*dismiss_pref_, true);
        break;
    }
  }

  if (dismiss_pref_ && (*dismiss_pref_ != prefs::kEolNotificationDismissed)) {
    profile_->GetPrefs()->SetBoolean(*dismiss_pref_, true);
  }

  NotificationDisplayServiceFactory::GetForProfile(profile_)->Close(
      NotificationHandler::Type::TRANSIENT, kEolNotificationId);
}

void EolNotification::OverrideClockForTesting(base::Clock* clock) {
  if (!clock) {
    clock_ = base::DefaultClock::GetInstance();
  } else {
    clock_ = clock;
  }
}

void EolNotification::MaybeShowEolIncentiveNotification(
    base::Time eol_date,
    eol_incentive_util::EolIncentiveType incentive_type) {
  const base::Time now = clock_->Now();
  const base::TimeDelta time_to_eol = eol_date - now;
  const int days_to_eol = time_to_eol.InDays();

  switch (incentive_type) {
    case eol_incentive_util::EolIncentiveType::kNone:
    case eol_incentive_util::EolIncentiveType::kEolPassed:
      if (days_to_eol < kLastIncentiveEndDaysPastEol &&
          !profile_->GetPrefs()->GetBoolean(
              prefs::kEolPassedFinalIncentiveDismissed) &&
          !profile_->GetPrefs()->GetBoolean(prefs::kEolNotificationDismissed)) {
        // Once the timeframe for showing the final incentive notification has
        // passed, if the final incentive notification was not dismissed, and
        // the final EOL notification has not been dismissed, then show the
        // final EOL notification.
        dismiss_pref_ = prefs::kEolNotificationDismissed;
        CreateNotification(eol_date, now);
      }
      return;
    case eol_incentive_util::EolIncentiveType::kEolApproaching:
      dismiss_pref_ = prefs::kEolApproachingIncentiveNotificationDismissed;
      break;
    case eol_incentive_util::EolIncentiveType::kEolPassedRecently:
      dismiss_pref_ = prefs::kEolPassedFinalIncentiveDismissed;
      break;
  }

  if (!dismiss_pref_ || profile_->GetPrefs()->GetBoolean(*dismiss_pref_)) {
    return;
  }

  ShowIncentiveNotification(eol_date, incentive_type);
}

void EolNotification::ShowIncentiveNotification(
    base::Time eol_date,
    eol_incentive_util::EolIncentiveType incentive_type) {
  message_center::RichNotificationData data;
  ash::SystemNotificationBuilder notification_builder;

  gfx::ImageSkia incentive_image =
      *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_EOL_INCENTIVE_NOTIFICATION);
  SkBitmap background_bitmap;
  background_bitmap.allocN32Pixels(incentive_image.width(),
                                   incentive_image.height());
  background_bitmap.eraseColor(
      DarkLightModeController::Get()->IsDarkModeEnabled() ? gfx::kGoogleGrey800
                                                          : SK_ColorWHITE);
  gfx::ImageSkia background =
      gfx::ImageSkia::CreateFrom1xBitmap(background_bitmap);
  data.image = gfx::Image(gfx::ImageSkiaOperations::CreateSuperimposedImage(
      background, incentive_image));

  features::EolIncentiveParam incentive_param =
      ash::features::kEolIncentiveParam.Get();

  switch (incentive_param) {
    case features::EolIncentiveParam::kNoOffer:
      data.buttons.emplace_back(GetStringUTF16(IDS_LEARN_MORE));

      if (incentive_type ==
          eol_incentive_util::EolIncentiveType::kEolApproaching) {
        notification_builder
            .SetTitle(GetStringUTF16(
                IDS_EOL_INCENTIVE_NOTIFICATION_TITLE_NO_OFFER_EXPIRING_SOON))
            .SetMessageWithArgs(
                IDS_EOL_INCENTIVE_NOTIFICATION_MESSAGE_NO_OFFER_EXPIRING_SOON,
                {TimeFormatMonthAndYearForTimeZone(eol_date,
                                                   icu::TimeZone::getGMT())});
      } else {
        notification_builder
            .SetTitle(GetStringUTF16(
                IDS_EOL_INCENTIVE_NOTIFICATION_TITLE_NO_OFFER_EXPIRED))
            .SetMessage(GetStringUTF16(
                IDS_EOL_INCENTIVE_NOTIFICATION_MESSAGE_NO_OFFER_EXPIRED));
      }
      break;
    case features::EolIncentiveParam::kOffer:
      data.buttons.emplace_back(
          GetStringUTF16(IDS_EOL_INCENTIVE_NOTIFICATION_OFFER_SHOP_BUTTON));
      data.buttons.emplace_back(
          GetStringUTF16(IDS_EOL_INCENTIVE_NOTIFICATION_OFFER_ABOUT_BUTTON));
      notification_builder.SetTitle(
          GetStringUTF16(IDS_EOL_INCENTIVE_NOTIFICATION_TITLE_OFFER));

      if (incentive_type ==
          eol_incentive_util::EolIncentiveType::kEolApproaching) {
        notification_builder.SetMessageWithArgs(
            IDS_EOL_INCENTIVE_NOTIFICATION_MESSAGE_OFFER_EXPIRING_SOON,
            {TimeFormatMonthAndYearForTimeZone(eol_date,
                                               icu::TimeZone::getGMT())});
      } else {
        notification_builder.SetMessage(GetStringUTF16(
            IDS_EOL_INCENTIVE_NOTIFICATION_MESSAGE_OFFER_EXPIRED));
      }
      break;
    case features::EolIncentiveParam::kOfferWithWarning:
      data.buttons.emplace_back(
          GetStringUTF16(IDS_EOL_INCENTIVE_NOTIFICATION_OFFER_SHOP_BUTTON));
      data.buttons.emplace_back(
          GetStringUTF16(IDS_EOL_INCENTIVE_NOTIFICATION_OFFER_ABOUT_BUTTON));

      if (incentive_type ==
          eol_incentive_util::EolIncentiveType::kEolApproaching) {
        notification_builder
            .SetTitle(GetStringUTF16(
                IDS_EOL_INCENTIVE_NOTIFICATION_TITLE_OFFER_WITH_WARNING_EXPIRING_SOON))
            .SetMessageWithArgs(
                IDS_EOL_INCENTIVE_NOTIFICATION_MESSAGE_OFFER_EXPIRING_SOON,
                {TimeFormatMonthAndYearForTimeZone(eol_date,
                                                   icu::TimeZone::getGMT())});
      } else {
        notification_builder
            .SetTitle(GetStringUTF16(
                IDS_EOL_INCENTIVE_NOTIFICATION_TITLE_OFFER_WITH_WARNING_EXPIRED))
            .SetMessage(GetStringUTF16(
                IDS_EOL_INCENTIVE_NOTIFICATION_MESSAGE_OFFER_EXPIRED));
      }
      break;
  }

  NotificationDisplayServiceFactory::GetForProfile(profile_)->Display(
      NotificationHandler::Type::TRANSIENT,
      notification_builder.SetId(kEolNotificationId)
          .SetCatalogName(NotificationCatalogName::kEOLIncentive)
          .SetOriginUrl(GURL(kEolNotificationId))
          .SetOptionalFields(data)
          .SetDelegate(
              base::MakeRefCounted<message_center::ThunkNotificationDelegate>(
                  weak_ptr_factory_.GetWeakPtr()))
          .Build(false),
      /*metadata=*/nullptr);

  if (incentive_type == eol_incentive_util::EolIncentiveType::kEolApproaching) {
    // Record approaching eol notification shown.
    eol_incentive_util::RecordShowSourceHistogram(
        eol_incentive_util::EolIncentiveShowSource::kNotification_Approaching);
  } else {
    // Record recently passed eol notification shown.
    eol_incentive_util::RecordShowSourceHistogram(
        eol_incentive_util::EolIncentiveShowSource::
            kNotification_RecentlyPassed);
  }
}

void EolNotification::ResetDismissedPrefs() {
  profile_->GetPrefs()->SetBoolean(prefs::kFirstEolWarningDismissed, false);
  profile_->GetPrefs()->SetBoolean(prefs::kSecondEolWarningDismissed, false);
  profile_->GetPrefs()->SetBoolean(prefs::kEolNotificationDismissed, false);
  profile_->GetPrefs()->SetBoolean(
      prefs::kEolApproachingIncentiveNotificationDismissed, false);
  profile_->GetPrefs()->SetBoolean(prefs::kEolPassedFinalIncentiveDismissed,
                                   false);
}

}  // namespace ash
