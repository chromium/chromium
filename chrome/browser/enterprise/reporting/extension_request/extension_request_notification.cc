// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/enterprise/reporting/extension_request/extension_request_notification.h"

#include <memory>

#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_id.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/native_theme/native_theme.h"

namespace enterprise_reporting {
namespace {

constexpr char kApprovedNotificationId[] = "extension_approved_notificaiton";
constexpr char kRejectedNotificationId[] = "extension_rejected_notificaiton";
constexpr char kInstalledNotificationId[] = "extension_installed_notificaiton";
constexpr char kExtensionRequestNotifierId[] =
    "chrome_browser_cloud_management_extension_request";
constexpr char kChromeWebstoreUrl[] =
    "https://chrome.google.com/webstore/detail/";

// The elements order of array below must match the order in enum
// ExtensionRequestNotification::NotifyType.
const char* const kNotificationIds[] = {
    kApprovedNotificationId, kRejectedNotificationId, kInstalledNotificationId};
constexpr int kNotificationTitles[] = {
    IDS_ENTERPRISE_EXTENSION_REQUEST_APPROVED_TITLE,
    IDS_ENTERPRISE_EXTENSION_REQUEST_REJECTED_TITLE,
    IDS_ENTERPRISE_EXTENSION_REQUEST_FORCE_INSTALLED_TITLE};
constexpr int kNotificationBodies[] = {
    IDS_ENTERPRISE_EXTENSION_REQUEST_CLICK_TO_INSTALL,
    IDS_ENTERPRISE_EXTENSION_REQUEST_CLICK_TO_VIEW,
    IDS_ENTERPRISE_EXTENSION_REQUEST_CLICK_TO_VIEW};

}  // namespace

ExtensionRequestNotification::ExtensionRequestNotification(
    Profile* profile,
    const NotifyType notify_type,
    const ExtensionIds& extension_ids)
    : profile_(profile),
      notify_type_(notify_type),
      extension_ids_(extension_ids) {}

ExtensionRequestNotification::~ExtensionRequestNotification() = default;

void ExtensionRequestNotification::Show(NotificationCloseCallback callback) {
  CHECK(!extension_ids_.empty());

  callback_ = std::move(callback);

  const std::u16string title = l10n_util::GetPluralStringFUTF16(
      kNotificationTitles[notify_type_], extension_ids_.size());
  const std::u16string body = l10n_util::GetPluralStringFUTF16(
      kNotificationBodies[notify_type_], extension_ids_.size());
  GURL original_url("https://chrome.google.com/webstore");
  auto icon = ui::ImageModel::FromVectorIcon(vector_icons::kBusinessIcon,
                                             ui::kColorIcon,
                                             message_center::kSmallImageSize);

  notification_ = std::make_unique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_SIMPLE, kNotificationIds[notify_type_],
      title, body, icon, /*source=*/std::u16string(), original_url,
      message_center::NotifierId(message_center::NotifierType::APPLICATION,
                                 kExtensionRequestNotifierId),
      message_center::RichNotificationData(),
      base::MakeRefCounted<message_center::ThunkNotificationDelegate>(
          weak_factory_.GetWeakPtr()));
  notification_->set_never_timeout(true);

  NotificationDisplayServiceFactory::GetForProfile(profile_)->Display(
      NotificationHandler::Type::TRANSIENT, *notification_, nullptr);
}

void ExtensionRequestNotification::CloseNotification() {
  NotificationDisplayServiceFactory::GetForProfile(profile_)->Close(
      NotificationHandler::Type::TRANSIENT, kNotificationIds[notify_type_]);
  notification_.reset();
}

void ExtensionRequestNotification::Click(
    const std::optional<int>& button_index,
    const std::optional<std::u16string>& reply) {
  for (const std::string& extension_id : extension_ids_) {
    NavigateParams params(profile_, GURL(kChromeWebstoreUrl + extension_id),
                          ui::PAGE_TRANSITION_LINK);
    params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    params.window_action = NavigateParams::SHOW_WINDOW;
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
