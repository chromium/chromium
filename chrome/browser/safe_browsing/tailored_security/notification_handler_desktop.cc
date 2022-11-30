// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/tailored_security/notification_handler_desktop.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/browser/tailored_security_service/tailored_security_outcome.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/common/referrer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/color/color_provider.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/native_theme/common_theme.h"
#include "ui/native_theme/native_theme.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/notifier_catalogs.h"
#endif

namespace safe_browsing {

namespace {
const char kTailoredSecurityEnableNotificationId[] =
    "TailoredSecurityEnableNotification";
const char kTailoredSecurityDisableNotificationId[] =
    "TailoredSecurityDisableNotification";
const char kTailoredSecurityUnconsentedPromotionNotificationId[] =
    "TailoredSecurityUnconsentedPromotionNotification";
const char kTailoredSecurityNotifierId[] =
    "chrome://settings/security/notification/id-notifier";
const char kTailoredSecurityNotificationOrigin[] =
    "chrome://settings/security?q=enhanced";

// |is_esb_enabled_in_sync| records if ESB was enabled in sync with Account-ESB
void ChangeSafeBrowsingStateAndOpenSettings(Profile* profile,
                                            SafeBrowsingState new_state,
                                            bool is_esb_enabled_in_sync) {
  SetSafeBrowsingState(profile->GetPrefs(), new_state, is_esb_enabled_in_sync);
  Browser* browser = chrome::ScopedTabbedBrowserDisplayer(profile).browser();
  chrome::ShowSafeBrowsingEnhancedProtection(browser);
}

void LogConsentedOutcome(TailoredSecurityOutcome outcome, bool enable) {
  if (enable) {
    base::UmaHistogramEnumeration(
        "SafeBrowsing.TailoredSecurityConsentedEnabledNotificationOutcome",
        outcome);
  } else {
    base::UmaHistogramEnumeration(
        "SafeBrowsing.TailoredSecurityConsentedDisabledNotificationOutcome",
        outcome);
  }
}

void LogUnconsentedOutcome(TailoredSecurityOutcome outcome) {
  base::UmaHistogramEnumeration(
      "SafeBrowsing.TailoredSecurityUnconsentedPromotionNotificationOutcome",
      outcome);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

message_center::NotifierId GetDisabledNotifierId() {
  return message_center::NotifierId(
      message_center::NotifierType::SYSTEM_COMPONENT,
      kTailoredSecurityNotifierId,
      ash::NotificationCatalogName::kTailoredSecurityDisabled);
}
message_center::NotifierId GetEnabledNotifierId() {
  return message_center::NotifierId(
      message_center::NotifierType::SYSTEM_COMPONENT,
      kTailoredSecurityNotifierId,
      ash::NotificationCatalogName::kTailoredSecurityEnabled);
}
message_center::NotifierId GetPromotionNotifierId() {
  return message_center::NotifierId(
      message_center::NotifierType::SYSTEM_COMPONENT,
      kTailoredSecurityNotifierId,
      ash::NotificationCatalogName::kTailoredSecurityPromotion);
}
#else
message_center::NotifierId GetNotifierId() {
  return message_center::NotifierId(
      message_center::NotifierType::SYSTEM_COMPONENT,
      kTailoredSecurityNotifierId);
}
#endif

}  // namespace

TailoredSecurityNotificationHandler::TailoredSecurityNotificationHandler() =
    default;

TailoredSecurityNotificationHandler::~TailoredSecurityNotificationHandler() =
    default;

void TailoredSecurityNotificationHandler::OnClose(
    Profile* profile,
    const GURL& origin,
    const std::string& notification_id,
    bool by_user,
    base::OnceClosure completed_closure) {
  if (notification_id == kTailoredSecurityUnconsentedPromotionNotificationId) {
    LogUnconsentedOutcome(TailoredSecurityOutcome::kDismissed);
  } else {
    bool is_enable = (notification_id == kTailoredSecurityEnableNotificationId);
    LogConsentedOutcome(TailoredSecurityOutcome::kDismissed, is_enable);
  }

  std::move(completed_closure).Run();
}

void TailoredSecurityNotificationHandler::OnClick(
    Profile* profile,
    const GURL& origin,
    const std::string& notification_id,
    const absl::optional<int>& action_index,
    const absl::optional<std::u16string>& reply,
    base::OnceClosure completed_closure) {
  if (!action_index) {
    std::move(completed_closure).Run();
    return;
  }

  if (notification_id == kTailoredSecurityUnconsentedPromotionNotificationId) {
    if (*action_index == 0) {
      LogUnconsentedOutcome(TailoredSecurityOutcome::kAccepted);
      chrome::ShowSafeBrowsingEnhancedProtection(
          chrome::ScopedTabbedBrowserDisplayer(profile).browser());
    } else {
      LogUnconsentedOutcome(TailoredSecurityOutcome::kRejected);
    }
  } else {
    bool is_enable = (notification_id == kTailoredSecurityEnableNotificationId);
    if (*action_index == 0) {
      // Enable: Pressed Turn on, Disable: Pressed Turn off. Both are
      // acceptance
      LogConsentedOutcome(TailoredSecurityOutcome::kAccepted, is_enable);
      if (is_enable) {
        ChangeSafeBrowsingStateAndOpenSettings(
            profile, SafeBrowsingState::ENHANCED_PROTECTION,
            /*is_esb_enabled_in_sync=*/true);
      } else {
        ChangeSafeBrowsingStateAndOpenSettings(
            profile, SafeBrowsingState::STANDARD_PROTECTION,
            /*is_esb_enabled_in_sync=*/false);
      }
    } else {
      // Both enable and disable dialogs display No Thanks, and are rejecting.
      LogConsentedOutcome(TailoredSecurityOutcome::kRejected, is_enable);
    }
  }

  std::move(completed_closure).Run();
}

void DisplayTailoredSecurityConsentedModalDesktop(Profile* profile,
                                                  bool enable) {
  std::u16string title, description, primary_button, secondary_button;
  ui::ImageModel icon;
  std::string notification_id;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const message_center::NotifierId notifier_id =
      enable ? GetEnabledNotifierId() : GetDisabledNotifierId();
#else
  const message_center::NotifierId notifier_id = GetNotifierId();
#endif
  if (enable) {
    notification_id = kTailoredSecurityEnableNotificationId;
    title = l10n_util::GetStringUTF16(
        IDS_TAILORED_SECURITY_CONSENTED_ENABLE_NOTIFICATION_TITLE);
    description = l10n_util::GetStringUTF16(
        IDS_TAILORED_SECURITY_CONSENTED_ENABLE_NOTIFICATION_DESCRIPTION);
    primary_button = l10n_util::GetStringUTF16(
        IDS_TAILORED_SECURITY_CONSENTED_ENABLE_NOTIFICATION_ACCEPT);
    secondary_button = l10n_util::GetStringUTF16(IDS_NO_THANKS);
    // TODO(crbug/1257621): Confirm with UX that it's appropriate to use the
    // blue color here.
    icon =
        ui::ImageModel::FromVectorIcon(kSafetyCheckIcon, ui::kColorAccent,
                                       message_center::kNotificationIconSize);
  } else {
    notification_id = kTailoredSecurityDisableNotificationId;
    title = l10n_util::GetStringUTF16(
        IDS_TAILORED_SECURITY_CONSENTED_DISABLE_NOTIFICATION_TITLE);
    description = l10n_util::GetStringUTF16(
        IDS_TAILORED_SECURITY_CONSENTED_DISABLE_NOTIFICATION_DESCRIPTION);
    primary_button = l10n_util::GetStringUTF16(
        IDS_TAILORED_SECURITY_CONSENTED_DISABLE_NOTIFICATION_TURN_OFF);
    secondary_button = l10n_util::GetStringUTF16(IDS_NO_THANKS);
    icon = ui::ImageModel::FromVectorIcon(
        vector_icons::kGppMaybeIcon, ui::kColorSecondaryForeground,
        message_center::kNotificationIconSize);
  }
  LogConsentedOutcome(TailoredSecurityOutcome::kShown, enable);
  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id, title,
      description, icon,
      l10n_util::GetStringUTF16(IDS_TAILORED_SECURITY_DISPLAY_SOURCE),
      GURL(kTailoredSecurityNotificationOrigin), notifier_id,
      message_center::RichNotificationData(),
      /*delegate=*/nullptr);
  notification.set_buttons({message_center::ButtonInfo(primary_button),
                            message_center::ButtonInfo(secondary_button)});
  NotificationDisplayServiceFactory::GetForProfile(profile)->Display(
      NotificationHandler::Type::TAILORED_SECURITY, notification,
      /*metadata=*/nullptr);
}

void DisplayTailoredSecurityUnconsentedPromotionNotification(Profile* profile) {
  std::string notification_id =
      kTailoredSecurityUnconsentedPromotionNotificationId;
  const std::u16string& title = l10n_util::GetStringUTF16(
      IDS_TAILORED_SECURITY_UNCONSENTED_PROMOTION_NOTIFICATION_TITLE);
  const std::u16string& description = l10n_util::GetStringUTF16(
      IDS_TAILORED_SECURITY_UNCONSENTED_PROMOTION_NOTIFICATION_DESCRIPTION);
  const std::u16string& primary_button = l10n_util::GetStringUTF16(
      IDS_TAILORED_SECURITY_UNCONSENTED_PROMOTION_NOTIFICATION_ACCEPT);
  const std::u16string& secondary_button =
      l10n_util::GetStringUTF16(IDS_NO_THANKS);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const message_center::NotifierId notifier_id = GetPromotionNotifierId();
#else
  const message_center::NotifierId notifier_id = GetNotifierId();
#endif

  // TODO(crbug/1257622): Confirm with UX that it's appropriate to use the
  // blue color here.
  auto icon =
      ui::ImageModel::FromVectorIcon(kSafetyCheckIcon, ui::kColorAccent,
                                     message_center::kNotificationIconSize);
  LogUnconsentedOutcome(TailoredSecurityOutcome::kShown);
  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id, title,
      description, icon,
      l10n_util::GetStringUTF16(IDS_TAILORED_SECURITY_DISPLAY_SOURCE),
      GURL(kTailoredSecurityNotificationOrigin), notifier_id,
      message_center::RichNotificationData(),
      /*delegate=*/nullptr);
  notification.set_buttons({message_center::ButtonInfo(primary_button),
                            message_center::ButtonInfo(secondary_button)});
  NotificationDisplayServiceFactory::GetForProfile(profile)->Display(
      NotificationHandler::Type::TAILORED_SECURITY, notification,
      /*metadata=*/nullptr);
}

}  // namespace safe_browsing
