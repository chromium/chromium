// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/message_center_ash.h"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "chromeos/crosapi/mojom/message_center.mojom.h"
#include "chromeos/crosapi/mojom/notification.mojom-shared.h"
#include "chromeos/crosapi/mojom/notification.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "url/gurl.h"

namespace mc = message_center;

namespace crosapi {
namespace {

mc::NotificationType FromMojo(mojom::NotificationType type) {
  switch (type) {
    case mojom::NotificationType::kSimple:
      return mc::NOTIFICATION_TYPE_SIMPLE;
    case mojom::NotificationType::kImage:
      return mc::NOTIFICATION_TYPE_IMAGE;
    case mojom::NotificationType::kList:
      return mc::NOTIFICATION_TYPE_MULTIPLE;
    case mojom::NotificationType::kProgress:
      return mc::NOTIFICATION_TYPE_PROGRESS;
  }
}

mc::NotifierType FromMojo(mojom::NotifierType type) {
  switch (type) {
    case mojom::NotifierType::kApplication:
      return mc::NotifierType::APPLICATION;
    case mojom::NotifierType::kArcApplication:
      return mc::NotifierType::ARC_APPLICATION;
    case mojom::NotifierType::kWebPage:
      return mc::NotifierType::WEB_PAGE;
    case mojom::NotifierType::kSystemComponent:
      return mc::NotifierType::SYSTEM_COMPONENT;
    case mojom::NotifierType::kCrostiniApplication:
      return mc::NotifierType::CROSTINI_APPLICATION;
    case mojom::NotifierType::kPhoneHub:
      return mc::NotifierType::PHONE_HUB;
  }
}

mc::FullscreenVisibility FromMojo(mojom::FullscreenVisibility visibility) {
  switch (visibility) {
    case mojom::FullscreenVisibility::kNone:
      return mc::FullscreenVisibility::NONE;
    case mojom::FullscreenVisibility::kOverUser:
      return mc::FullscreenVisibility::OVER_USER;
  }
}

mc::SettingsButtonHandler FromMojo(
    mojom::SettingsButtonHandler settings_button_handler) {
  switch (settings_button_handler) {
    case mojom::SettingsButtonHandler::kNone:
      return mc::SettingsButtonHandler::NONE;
    case mojom::SettingsButtonHandler::kInline:
      return mc::SettingsButtonHandler::INLINE;
    case mojom::SettingsButtonHandler::kDelegate:
      return mc::SettingsButtonHandler::DELEGATE;
  }
}

std::unique_ptr<mc::Notification> FromMojo(
    mojom::NotificationPtr notification) {
  mc::RichNotificationData rich_data;
  rich_data.priority = std::clamp(notification->priority, -2, 2);
  rich_data.never_timeout = notification->require_interaction;
  rich_data.timestamp = notification->timestamp;
  if (!notification->image.isNull())
    rich_data.image = gfx::Image(notification->image);
  if (!notification->badge.isNull()) {
    rich_data.small_image = gfx::Image(notification->badge);
    if (notification->badge_needs_additional_masking_has_value) {
      rich_data.small_image_needs_additional_masking =
          notification->badge_needs_additional_masking;
    }
  }
  for (const auto& mojo_item : notification->items) {
    mc::NotificationItem item(mojo_item->title, mojo_item->message);
    rich_data.items.push_back(item);
  }
  rich_data.progress = std::clamp(notification->progress, -1, 100);
  rich_data.progress_status = notification->progress_status;
  for (const auto& mojo_button : notification->buttons) {
    mc::ButtonInfo button;
    button.title = mojo_button->title;
    button.placeholder = mojo_button->placeholder;
    rich_data.buttons.push_back(button);
  }
  rich_data.pinned = notification->pinned;
  rich_data.renotify = notification->renotify;
  rich_data.silent = notification->silent;
  rich_data.accessible_name = notification->accessible_name;
  rich_data.fullscreen_visibility =
      FromMojo(notification->fullscreen_visibility);
  rich_data.accent_color = notification->accent_color;
  rich_data.settings_button_handler =
      FromMojo(notification->settings_button_handler);
  gfx::Image icon;
  if (!notification->icon.isNull())
    icon = gfx::Image(notification->icon);
  GURL origin_url = notification->origin_url.value_or(GURL());

  mc::NotifierId notifier_id = mc::NotifierId();
  if (notification->notifier_id) {
    notifier_id.type = FromMojo(notification->notifier_id->type);
    notifier_id.id = notification->notifier_id->id;
    if (notification->notifier_id->url.has_value())
      notifier_id.url = notification->notifier_id->url.value();
    if (notification->notifier_id->title.has_value())
      notifier_id.title = notification->notifier_id->title;
    notifier_id.profile_id = notification->notifier_id->profile_id;
    if (notification->notifier_id->group_key.has_value()) {
      notifier_id.group_key = notification->notifier_id->group_key.value();
    }
  }

  if (notification->image_path) {
    rich_data.image_path = notification->image_path;
  }

  return std::make_unique<mc::Notification>(
      FromMojo(notification->type), notification->id, notification->title,
      notification->message, ui::ImageModel::FromImage(icon),
      notification->display_source, origin_url, notifier_id, rich_data,
      /*delegate=*/nullptr);
}

// Forwards NotificationDelegate methods to a remote delegate over mojo. If the
// remote delegate disconnects (e.g. lacros-chrome crashes) the corresponding
// notification will be removed.
class ForwardingDelegate : public message_center::NotificationDelegate {
 public:
  ForwardingDelegate(const std::string& notification_id,
                     mojo::PendingRemote<mojom::NotificationDelegate> delegate)
      : notification_id_(notification_id),
        remote_delegate_(std::move(delegate)) {
    DCHECK(!notification_id_.empty());
    DCHECK(remote_delegate_);
  }
  ForwardingDelegate(const ForwardingDelegate&) = delete;
  ForwardingDelegate& operator=(const ForwardingDelegate&) = delete;

  void Init() {
    // Cannot be done in constructor because base::BindOnce() requires a
    // non-zero reference count.
    remote_delegate_.set_disconnect_handler(
        base::BindOnce(&ForwardingDelegate::OnDisconnect, this));
  }

 private:
  // Private due to ref-counting.
  ~ForwardingDelegate() override = default;

  void OnDisconnect() {
    mc::Notification* notification =
        mc::MessageCenter::Get()->FindNotificationById(notification_id_);
    if (!notification)
      return;
    // If the disconnect occurred because an existing notification was updated
    // with new content, don't close it. https://crbug.com/1270544
    if (notification->delegate() != this)
      return;
    // NOTE: Triggers a call to Close() if the notification is still showing.
    mc::MessageCenter::Get()->RemoveNotification(notification_id_,
                                                 /*by_user=*/false);
  }

  // message_center::NotificationDelegate:
  void Close(bool by_user) override {
    // Can be called after |remote_delegate_| is disconnected.
    if (remote_delegate_)
      remote_delegate_->OnNotificationClosed(by_user);
  }

  void Click(const std::optional<int>& button_index,
             const std::optional<std::u16string>& reply) override {
    // The button index comes out of
    // trusted ash-side message center UI code and is guaranteed not to be
    // negative.
    if (button_index) {
      remote_delegate_->OnNotificationButtonClicked(
          base::checked_cast<uint32_t>(*button_index), reply);
    } else {
      remote_delegate_->OnNotificationClicked();
    }
  }

  void SettingsClick() override {
    remote_delegate_->OnNotificationSettingsButtonClicked();
  }

  void DisableNotification() override {
    remote_delegate_->OnNotificationDisabled();
  }

  const std::string notification_id_;
  mojo::Remote<mojom::NotificationDelegate> remote_delegate_;
};

}  // namespace

MessageCenterAsh::MessageCenterAsh() = default;

MessageCenterAsh::~MessageCenterAsh() = default;

void MessageCenterAsh::BindReceiver(
    mojo::PendingReceiver<mojom::MessageCenter> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void MessageCenterAsh::DisplayNotification(
    mojom::NotificationPtr notification,
    mojo::PendingRemote<mojom::NotificationDelegate> delegate) {
  auto forwarding_delegate = base::MakeRefCounted<ForwardingDelegate>(
      notification->id, std::move(delegate));
  forwarding_delegate->Init();

  std::unique_ptr<mc::Notification> mc_notification =
      FromMojo(std::move(notification));
  mc_notification->set_delegate(forwarding_delegate);
  mc::MessageCenter::Get()->AddNotification(std::move(mc_notification));
}

void MessageCenterAsh::CloseNotification(const std::string& id) {
  mc::MessageCenter::Get()->RemoveNotification(id, /*by_user=*/false);
}

void MessageCenterAsh::GetDisplayedNotifications(
    GetDisplayedNotificationsCallback callback) {
  mc::NotificationList::Notifications notifications =
      mc::MessageCenter::Get()->GetNotifications();
  std::vector<std::string> ids;
  for (mc::Notification* notification : notifications)
    ids.push_back(notification->id());
  std::move(callback).Run(ids);
}

}  // namespace crosapi
