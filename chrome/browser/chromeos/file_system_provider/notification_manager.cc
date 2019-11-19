// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/notification_manager.h"

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/chrome_app_icon_loader.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "components/account_id/account_id.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace chromeos {
namespace file_system_provider {
namespace {

// Extension icon size for the notification.
const int kIconSize = 48;

}  // namespace

NotificationManager::NotificationManager(
    Profile* profile,
    const ProvidedFileSystemInfo& file_system_info)
    : profile_(profile),
      file_system_info_(file_system_info),
      icon_loader_(
          new extensions::ChromeAppIconLoader(profile, kIconSize, this)) {
  DCHECK_EQ(ProviderId::EXTENSION, file_system_info.provider_id().GetType());
}

NotificationManager::~NotificationManager() {
  if (callbacks_.size()) {
    NotificationDisplayService::GetForProfile(profile_)->Close(
        NotificationHandler::Type::TRANSIENT, GetNotificationId());
  }
}

void NotificationManager::ShowUnresponsiveNotification(
    int id,
    const NotificationCallback& callback) {
  callbacks_[id] = callback;
  ShowNotification();
}

void NotificationManager::HideUnresponsiveNotification(int id) {
  callbacks_.erase(id);

  if (callbacks_.size()) {
    ShowNotification();
  } else {
    NotificationDisplayService::GetForProfile(profile_)->Close(
        NotificationHandler::Type::TRANSIENT, GetNotificationId());
  }
}

void NotificationManager::Click(const base::Optional<int>& button_index,
                                const base::Optional<base::string16>& reply) {
  if (!button_index)
    return;

  OnNotificationResult(ABORT);
}

void NotificationManager::Close(bool by_user) {
  OnNotificationResult(CONTINUE);
}

void NotificationManager::OnAppImageUpdated(const std::string& id,
                                            const gfx::ImageSkia& image) {
  extension_icon_ = gfx::Image(image);
  ShowNotification();
}

std::string NotificationManager::GetNotificationId() {
  return file_system_info_.mount_path().value();
}

void NotificationManager::ShowNotification() {
  if (extension_icon_.IsEmpty())
    icon_loader_->FetchImage(file_system_info_.provider_id().GetExtensionId());

  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.buttons.push_back(
      message_center::ButtonInfo(l10n_util::GetStringUTF16(
          IDS_FILE_SYSTEM_PROVIDER_UNRESPONSIVE_ABORT_BUTTON)));

  message_center::NotifierId notifier_id(
      message_center::NotifierType::SYSTEM_COMPONENT,
      "chrome://file_system_provider_notification");
  notifier_id.profile_id =
      multi_user_util::GetAccountIdFromProfile(profile_).GetUserEmail();

  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, GetNotificationId(),
      base::UTF8ToUTF16(file_system_info_.display_name()),
      l10n_util::GetStringUTF16(
          callbacks_.size() == 1
              ? IDS_FILE_SYSTEM_PROVIDER_UNRESPONSIVE_WARNING
              : IDS_FILE_SYSTEM_PROVIDER_MANY_UNRESPONSIVE_WARNING),
      extension_icon_,
      base::string16(),  // display_source
      GURL(), notifier_id, rich_notification_data,
      base::MakeRefCounted<message_center::ThunkNotificationDelegate>(
          weak_factory_.GetWeakPtr()));
  notification.SetSystemPriority();

  NotificationDisplayService::GetForProfile(profile_)->Display(
      NotificationHandler::Type::TRANSIENT, notification, /*metadata=*/nullptr);
}

void NotificationManager::OnNotificationResult(NotificationResult result) {
  CallbackMap::iterator it = callbacks_.begin();
  while (it != callbacks_.end()) {
    CallbackMap::iterator current_it = it++;
    NotificationCallback callback = current_it->second;
    callbacks_.erase(current_it);
    callback.Run(result);
  }
}

}  // namespace file_system_provider
}  // namespace chromeos
