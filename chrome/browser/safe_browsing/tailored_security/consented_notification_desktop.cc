// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/tailored_security/consented_notification_desktop.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/tailored_security/tailored_security_outcome.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/common/referrer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/color/color_provider.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/native_theme/common_theme.h"
#include "ui/native_theme/native_theme.h"

namespace safe_browsing {

namespace {
const char kTailoredSecurityEnableNotificationId[] =
    "TailoredSecurityEnableNotification";
const char kTailoredSecurityDisableNotificationId[] =
    "TailoredSecurityDisableNotification";
const char kTailoredSecurityNotifierId[] =
    "chrome://settings/security/notification/id-notifier";
const char kTailoredSecurityNotificationOrigin[] =
    "chrome://settings/security?q=enhanced";

// |is_esb_enabled_in_sync| records if ESB was enabled in sync with Account-ESB
void TurnOnESBAndOpenSettings(Profile* profile, bool is_esb_enabled_in_sync) {
  SetSafeBrowsingState(profile->GetPrefs(),
                       SafeBrowsingState::ENHANCED_PROTECTION,
                       is_esb_enabled_in_sync);
  Browser* browser = chrome::ScopedTabbedBrowserDisplayer(profile).browser();
  chrome::ShowSafeBrowsingEnhancedProtection(browser);
}

void LogOutcome(TailoredSecurityOutcome outcome, bool enable) {
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

}  // namespace

TailoredSecurityConsentedNotificationHandler::
    TailoredSecurityConsentedNotificationHandler() = default;

TailoredSecurityConsentedNotificationHandler::
    ~TailoredSecurityConsentedNotificationHandler() = default;

void TailoredSecurityConsentedNotificationHandler::OnClose(
    Profile* profile,
    const GURL& origin,
    const std::string& notification_id,
    bool by_user,
    base::OnceClosure completed_closure) {
  bool is_enable = (notification_id == kTailoredSecurityEnableNotificationId);
  LogOutcome(TailoredSecurityOutcome::kDismissed, is_enable);
  std::move(completed_closure).Run();
}

void TailoredSecurityConsentedNotificationHandler::OnClick(
    Profile* profile,
    const GURL& origin,
    const std::string& notification_id,
    const absl::optional<int>& action_index,
    const absl::optional<std::u16string>& reply,
    base::OnceClosure completed_closure) {
  bool is_enable = (notification_id == kTailoredSecurityEnableNotificationId);
  if (action_index) {
    if (*action_index == 0) {
      // Enable: Pressed Turn on is acceptance, Disable: Turn back on is
      // rejection
      TailoredSecurityOutcome outcome =
          is_enable ? TailoredSecurityOutcome::kAccepted
                    : TailoredSecurityOutcome::kRejected;
      LogOutcome(outcome, is_enable);
      TurnOnESBAndOpenSettings(profile, is_enable);
    } else {
      // Enable: Pressed No Thanks is rejection, Disable: Pressed OK is
      // acceptance
      TailoredSecurityOutcome outcome =
          is_enable ? TailoredSecurityOutcome::kRejected
                    : TailoredSecurityOutcome::kAccepted;
      LogOutcome(outcome, is_enable);
    }
  }
  std::move(completed_closure).Run();
}

void DisplayTailoredSecurityConsentedModalDesktop(Profile* profile,
                                                  bool enable) {
  Browser* browser = chrome::FindLastActiveWithProfile(profile);
  if (!browser)
    return;

  const ui::ColorProvider* color_provider =
      browser->window()->GetColorProvider();

  std::u16string title, description, primary_button, secondary_button;
  gfx::Image icon;
  std::string notification_id;
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
    SkColor icon_color = color_provider->GetColor(ui::kColorAccent);
    icon = gfx::Image(gfx::CreateVectorIcon(
        kSafetyCheckIcon, message_center::kNotificationIconSize, icon_color));
  } else {
    notification_id = kTailoredSecurityDisableNotificationId;
    title = l10n_util::GetStringUTF16(
        IDS_TAILORED_SECURITY_CONSENTED_DISABLE_NOTIFICATION_TITLE);
    description = l10n_util::GetStringUTF16(
        IDS_TAILORED_SECURITY_CONSENTED_DISABLE_NOTIFICATION_DESCRIPTION);
    primary_button = l10n_util::GetStringUTF16(
        IDS_TAILORED_SECURITY_CONSENTED_DISABLE_NOTIFICATION_TURN_ON);
    secondary_button = l10n_util::GetStringUTF16(IDS_OK);
    SkColor icon_color = color_provider->GetColor(ui::kColorAlertHighSeverity);
    icon = gfx::Image(gfx::CreateVectorIcon(
        kShieldBadIcon, message_center::kNotificationIconSize, icon_color));
  }
  LogOutcome(TailoredSecurityOutcome::kShown, enable);
  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id, title,
      description, icon,
      l10n_util::GetStringUTF16(IDS_TAILORED_SECURITY_DISPLAY_SOURCE),
      GURL(kTailoredSecurityNotificationOrigin),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kTailoredSecurityNotifierId),
      message_center::RichNotificationData(),
      /*delegate=*/nullptr);
  notification.set_buttons({message_center::ButtonInfo(primary_button),
                            message_center::ButtonInfo(secondary_button)});
  NotificationDisplayServiceFactory::GetForProfile(profile)->Display(
      NotificationHandler::Type::TAILORED_SECURITY_CONSENTED, notification,
      /*metadata=*/nullptr);
}

}  // namespace safe_browsing
