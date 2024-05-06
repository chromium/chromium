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
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/browser/tailored_security_service/tailored_security_outcome.h"
#include "components/safe_browsing/core/common/features.h"
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
const char kTailoredSecurityUnconsentedPromotionNotificationId[] =
    "TailoredSecurityUnconsentedPromotionNotification";
const char kTailoredSecurityNotifierId[] =
    "chrome://settings/security/notification/id-notifier";
const char kTailoredSecurityNotificationOrigin[] =
    "chrome://settings/security?q=enhanced";

void LogUnconsentedOutcome(TailoredSecurityOutcome outcome) {
  base::UmaHistogramEnumeration(
      "SafeBrowsing.TailoredSecurityUnconsentedPromotionNotificationOutcome",
      outcome);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

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
  LogUnconsentedOutcome(TailoredSecurityOutcome::kDismissed);
  std::move(completed_closure).Run();
}

void TailoredSecurityNotificationHandler::OnClick(
    Profile* profile,
    const GURL& origin,
    const std::string& notification_id,
    const std::optional<int>& action_index,
    const std::optional<std::u16string>& reply,
    base::OnceClosure completed_closure) {
  if (!action_index) {
    std::move(completed_closure).Run();
    return;
  }

  if (*action_index == 0) {
    LogUnconsentedOutcome(TailoredSecurityOutcome::kAccepted);
    chrome::ShowSafeBrowsingEnhancedProtection(
        chrome::ScopedTabbedBrowserDisplayer(profile).browser());
  } else {
    LogUnconsentedOutcome(TailoredSecurityOutcome::kRejected);
  }

  std::move(completed_closure).Run();
}

ui::ImageModel GetNotificationIcon() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return ui::ImageModel::FromResourceId(
      IDR_TAILORED_SECURITY_UNCONSENTED_NOTIFICATION);
#else
  return ui::ImageModel::FromVectorIcon(kSafetyCheckIcon, ui::kColorAccent,
                                        message_center::kNotificationIconSize);
#endif
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
  auto icon = GetNotificationIcon();
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
