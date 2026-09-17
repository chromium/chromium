// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/chrome_ash_message_center_client.h"

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/notifications/notification_platform_bridge_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "ui/message_center/message_center.h"
#include "url/origin.h"

using message_center::MessageCenter;

namespace {

// The singleton instance, which is tracked to allow access from tests.
ChromeAshMessageCenterClient* g_chrome_ash_message_center_client = nullptr;

// This delegate forwards NotificationDelegate methods to their equivalent in
// NotificationPlatformBridgeDelegate.
class ForwardingNotificationDelegate
    : public message_center::NotificationDelegate {
 public:
  ForwardingNotificationDelegate(const std::string& notification_id,
                                 NotificationPlatformBridgeDelegate* delegate)
      : notification_id_(notification_id), delegate_(delegate) {}
  ForwardingNotificationDelegate(const ForwardingNotificationDelegate&) =
      delete;
  ForwardingNotificationDelegate& operator=(
      const ForwardingNotificationDelegate&) = delete;

  // message_center::NotificationDelegate:
  void Close(bool by_user) override {
    delegate_->HandleNotificationClosed(notification_id_, by_user);
  }

  void Click(const std::optional<int>& button_index,
             const std::optional<std::u16string>& reply) override {
    if (button_index) {
      delegate_->HandleNotificationButtonClicked(notification_id_,
                                                 *button_index, reply);
    } else {
      delegate_->HandleNotificationClicked(notification_id_);
    }
  }

  void SettingsClick() override {
    delegate_->HandleNotificationSettingsButtonClicked(notification_id_);
  }

  void DisableNotification() override {
    delegate_->DisableNotification(notification_id_);
  }

 private:
  ~ForwardingNotificationDelegate() override = default;

  // The ID of the notification.
  const std::string notification_id_;

  raw_ptr<NotificationPlatformBridgeDelegate> delegate_;
};

}  // namespace

ChromeAshMessageCenterClient::ChromeAshMessageCenterClient(
    NotificationPlatformBridgeDelegate* delegate)
    : delegate_(delegate) {
  DCHECK(!g_chrome_ash_message_center_client);
  g_chrome_ash_message_center_client = this;
}

ChromeAshMessageCenterClient::~ChromeAshMessageCenterClient() {
  DCHECK_EQ(this, g_chrome_ash_message_center_client);
  g_chrome_ash_message_center_client = nullptr;
}

void ChromeAshMessageCenterClient::Display(
    NotificationHandler::Type notification_type,
    Profile* profile,
    const message_center::Notification& notification,
    std::unique_ptr<NotificationCommon::Metadata> metadata) {
  auto message_center_notification =
      std::make_unique<message_center::Notification>(
          base::WrapRefCounted(
              new ForwardingNotificationDelegate(notification.id(), delegate_)),
          notification);

  // During shutdown, Ash is destroyed before |this|, taking the MessageCenter
  // with it.
  if (MessageCenter::Get()) {
    MessageCenter::Get()->AddNotification(
        std::move(message_center_notification));
  }
}

void ChromeAshMessageCenterClient::Close(Profile* profile,
                                         const std::string& notification_id) {
  // During shutdown, Ash is destroyed before |this|, taking the MessageCenter
  // with it.
  if (MessageCenter::Get()) {
    MessageCenter::Get()->RemoveNotification(notification_id,
                                             false /* by_user */);
  }
}

void ChromeAshMessageCenterClient::GetDisplayed(
    Profile* profile,
    GetDisplayedNotificationsCallback callback) const {
  message_center::NotificationList::Notifications notifications =
      MessageCenter::Get()->GetNotifications();

  std::set<std::string> notification_ids;
  for (message_center::Notification* notification : notifications) {
    notification_ids.insert(notification->id());
  }

  std::move(callback).Run(std::move(notification_ids), /*supports_sync=*/true);
}

void ChromeAshMessageCenterClient::GetDisplayedForOrigin(
    Profile* profile,
    const GURL& origin,
    GetDisplayedNotificationsCallback callback) const {
  message_center::NotificationList::Notifications notifications =
      MessageCenter::Get()->GetNotifications();

  std::set<std::string> notification_ids;
  for (message_center::Notification* notification : notifications) {
    if (url::IsSameOriginWith(notification->origin_url(), origin)) {
      notification_ids.insert(notification->id());
    }
  }

  std::move(callback).Run(std::move(notification_ids), /*supports_sync=*/true);
}

void ChromeAshMessageCenterClient::SetReadyCallback(
    NotificationBridgeReadyCallback callback) {
  // Ash is always available in-process, so report the client is ready.
  std::move(callback).Run(true);
}
