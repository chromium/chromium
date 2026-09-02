// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/extension_request/extension_request_notification.h"

#include <array>
#include <memory>

#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/window_open_disposition.h"
#include "ui/color/color_id.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/native_theme/native_theme.h"

namespace enterprise_reporting {
namespace {

constexpr char kApprovedNotificationId[] = "extension_approved_notification";
constexpr char kRejectedNotificationId[] = "extension_rejected_notification";
constexpr char kInstalledNotificationId[] = "extension_installed_notification";
constexpr char kExtensionRequestNotifierId[] =
    "chrome_browser_cloud_management_extension_request";
constexpr char kChromeWebstoreUrl[] =
    "https://chrome.google.com/webstore/detail/";

#if BUILDFLAG(IS_ANDROID)
constexpr NotificationHandler::Type kNotificationType =
    NotificationHandler::Type::EXTENSION_REQUEST;
constexpr char kAndroidNotificationIdPrefix[] = "ext_req|";
#else
constexpr NotificationHandler::Type kNotificationType =
    NotificationHandler::Type::TRANSIENT;
#endif

// The elements order of array below must match the order in enum
// ExtensionRequestNotification::NotifyType.
constexpr auto kNotificationIds = std::to_array<const char*>({
    kApprovedNotificationId,
    kRejectedNotificationId,
    kInstalledNotificationId,
});
constexpr auto kNotificationTitles = std::to_array<int>(
    {IDS_ENTERPRISE_EXTENSION_REQUEST_APPROVED_TITLE,
     IDS_ENTERPRISE_EXTENSION_REQUEST_REJECTED_TITLE,
     IDS_ENTERPRISE_EXTENSION_REQUEST_FORCE_INSTALLED_TITLE});
constexpr auto kNotificationBodies =
    std::to_array<int>({IDS_ENTERPRISE_EXTENSION_REQUEST_CLICK_TO_INSTALL,
                        IDS_ENTERPRISE_EXTENSION_REQUEST_CLICK_TO_VIEW,
                        IDS_ENTERPRISE_EXTENSION_REQUEST_CLICK_TO_VIEW});

}  // namespace

ExtensionRequestNotification::ExtensionRequestNotification(
    Profile* profile,
    const NotifyType notify_type,
    const ExtensionIds& extension_ids)
    : profile_(profile),
      notify_type_(notify_type),
      extension_ids_(extension_ids) {}

ExtensionRequestNotification::~ExtensionRequestNotification() = default;

std::string ExtensionRequestNotification::GetNotificationId() const {
  return CreateNotificationId(notify_type_, extension_ids_);
}

// Notification IDs on Android are formatted as:
//   "ext_req|<notification_type_id>|<comma_separated_extension_ids>"
// static
std::string ExtensionRequestNotification::CreateNotificationId(
    NotifyType notify_type,
    const ExtensionIds& extension_ids) {
#if BUILDFLAG(IS_ANDROID)
  return std::string(kAndroidNotificationIdPrefix) +
         kNotificationIds[notify_type] + "|" +
         base::JoinString(extension_ids, ",");
#else
  return kNotificationIds[notify_type];
#endif
}

// static
std::vector<std::string> ExtensionRequestNotification::ParseExtensionIds(
    const std::string& notification_id) {
#if BUILDFLAG(IS_ANDROID)
  DCHECK(base::StartsWith(notification_id, kAndroidNotificationIdPrefix));
  if (!base::StartsWith(notification_id, kAndroidNotificationIdPrefix)) {
    return {};
  }
  size_t last_separator = notification_id.rfind('|');
  if (last_separator == std::string::npos ||
      last_separator == notification_id.size() - 1) {
    return {};
  }
  std::string ids_str = notification_id.substr(last_separator + 1);
  return base::SplitString(ids_str, ",", base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY);
#else
  return {};
#endif
}

void ExtensionRequestNotification::Show(NotificationCloseCallback callback) {
  CHECK(!extension_ids_.empty());

  callback_ = std::move(callback);

  const std::u16string title = l10n_util::GetPluralStringFUTF16(
      kNotificationTitles[notify_type_], extension_ids_.size());
  const std::u16string body = l10n_util::GetPluralStringFUTF16(
      kNotificationBodies[notify_type_], extension_ids_.size());
  GURL original_url("https://chrome.google.com/webstore");
#if BUILDFLAG(IS_ANDROID)
  int icon_size = message_center::kNotificationIconSize;
#else
  int icon_size = message_center::kSmallImageSize;
#endif
  auto icon = ui::ImageModel::FromVectorIcon(
      features::IsRoundedIconsEnabled() ? vector_icons::kDomainIcon
                                        : vector_icons::kBusinessOldIcon,
      ui::kColorIcon, icon_size);

  notification_ = std::make_unique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_SIMPLE, GetNotificationId(), title,
      body, icon, /*source=*/std::u16string(), original_url,
      message_center::NotifierId(message_center::NotifierType::APPLICATION,
                                 kExtensionRequestNotifierId),
      message_center::RichNotificationData(),
      base::MakeRefCounted<message_center::ThunkNotificationDelegate>(
          weak_factory_.GetWeakPtr()));
  notification_->set_never_timeout(true);

  NotificationDisplayServiceFactory::GetForProfile(profile_)->Display(
      kNotificationType, *notification_, nullptr);
}

void ExtensionRequestNotification::CloseNotification() {
  NotificationDisplayServiceFactory::GetForProfile(profile_)->Close(
      kNotificationType, GetNotificationId());
  notification_.reset();
}

void ExtensionRequestNotification::Click(
    const std::optional<int>& button_index,
    const std::optional<std::u16string>& reply) {
  for (const std::string& extension_id : extension_ids_) {
    NavigateParams params(profile_, GURL(kChromeWebstoreUrl + extension_id),
                          ui::PAGE_TRANSITION_LINK);
    params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    params.window_action = NavigateParams::WindowAction::kShowWindow;
    Navigate(&params);
  }
  if (callback_)
    std::move(callback_).Run(true);
  CloseNotification();
}

void ExtensionRequestNotification::Close(bool by_user) {
  if (callback_) {
    std::move(callback_).Run(by_user);
  }
}

}  // namespace enterprise_reporting
