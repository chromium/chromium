// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/website_approval_notifier.h"

#include <memory>
#include <string>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/notification_utils.h"
#include "base/functional/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "components/supervised_user/core/browser/supervised_user_settings_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"
#include "url/gurl.h"

namespace ash {

namespace {

// Notifier id representing the app.
constexpr char kWebsiteApprovalNotifierId[] = "family-link";

// Prefix for website approval notifications. Each hostname is associated with a
// different suffix, so that all notifications are shown (subsequent ones do NOT
// displace previous ones).
constexpr char kWebsiteApprovalNotificationIdPrefix[] = "website-approval-";

const char kNotificationClickedActionName[] =
    "SupervisedUsers_RemoteWebApproval_NotificationClicked";

const char kNotificationShownActionName[] =
    "SupervisedUsers_RemoteWebApproval_NotificationShown";

GURL GetURLToOpen(const std::string& allowed_host) {
  // When a match pattern containing * (e.g. *.google.*) is allowlisted, return
  // an empty URL because we can't know which URL to open.
  if (allowed_host.find('*') != std::string::npos) {
    return GURL();
  }

  // Constructs a URL that the user can open, defaulting to HTTPS.
  GURL url = GURL(base::StrCat(
      {url::kHttpsScheme, url::kStandardSchemeSeparator, allowed_host}));
  DLOG_IF(ERROR, !url.is_valid()) << "Invalid URL spec " << allowed_host;
  return url;
}

void OnNotificationClick(const GURL& url) {
  base::RecordAction(base::UserMetricsAction(kNotificationClickedActionName));
  NewWindowDelegate::GetPrimary()->OpenUrl(
      url, NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);
}

}  // namespace

WebsiteApprovalNotifier::WebsiteApprovalNotifier(Profile* profile)
    : profile_(profile) {
  supervised_user::SupervisedUserSettingsService* settings_service =
      SupervisedUserSettingsServiceFactory::GetForKey(
          profile_->GetProfileKey());
  website_approval_subscription_ =
      settings_service->SubscribeForNewWebsiteApproval(base::BindRepeating(
          &WebsiteApprovalNotifier::MaybeShowApprovalNotification,
          weak_ptr_factory_.GetWeakPtr()));
}

WebsiteApprovalNotifier::~WebsiteApprovalNotifier() = default;

void WebsiteApprovalNotifier::MaybeShowApprovalNotification(
    const std::string& allowed_host) {
  GURL url = GetURLToOpen(allowed_host);
  if (!url.is_valid()) {
    return;
  }
  message_center::RichNotificationData option_fields;
  option_fields.fullscreen_visibility =
      message_center::FullscreenVisibility::OVER_USER;
  message_center::Notification notification = CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      kWebsiteApprovalNotificationIdPrefix + allowed_host,
      l10n_util::GetStringUTF16(IDS_WEBSITE_APPROVED_NOTIFICATION_TITLE),
      l10n_util::GetStringFUTF16(IDS_WEBSITE_APPROVED_NOTIFICATION_MESSAGE,
                                 base::UTF8ToUTF16(allowed_host)),
      l10n_util::GetStringUTF16(
          IDS_WEBSITE_APPROVED_NOTIFICATION_DISPLAY_SOURCE),
      GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kWebsiteApprovalNotifierId,
                                 NotificationCatalogName::kWebsiteApproval),
      option_fields,
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(&OnNotificationClick, url)),
      chromeos::kNotificationSupervisedUserIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);
  base::RecordAction(base::UserMetricsAction(kNotificationShownActionName));
  NotificationDisplayServiceFactory::GetForProfile(profile_)->Display(
      NotificationHandler::Type::TRANSIENT, notification,
      /*metadata=*/nullptr);
}

}  // namespace ash
