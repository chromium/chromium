// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crosapi/message_center_ash.h"

#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/ranges.h"
#include "base/optional.h"
#include "chromeos/crosapi/mojom/message_center.mojom.h"
#include "chromeos/crosapi/mojom/notification.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
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

mc::FullscreenVisibility FromMojo(mojom::FullscreenVisibility visibility) {
  switch (visibility) {
    case mojom::FullscreenVisibility::kNone:
      return mc::FullscreenVisibility::NONE;
    case mojom::FullscreenVisibility::kOverUser:
      return mc::FullscreenVisibility::OVER_USER;
  }
}

std::unique_ptr<mc::Notification> FromMojo(
    mojom::NotificationPtr notification) {
  mc::RichNotificationData rich_data;
  rich_data.priority = base::ClampToRange(notification->priority, -2, 2);
  rich_data.never_timeout = notification->require_interaction;
  rich_data.timestamp = notification->timestamp;
  if (!notification->image.isNull())
    rich_data.image = gfx::Image(notification->image);
  if (!notification->badge.isNull())
    rich_data.small_image = gfx::Image(notification->badge);
  for (const auto& mojo_item : notification->items) {
    mc::NotificationItem item;
    item.title = mojo_item->title;
    item.message = mojo_item->message;
    rich_data.items.push_back(item);
  }
  rich_data.progress = base::ClampToRange(notification->progress, -1, 100);
  rich_data.progress_status = notification->progress_status;
  for (const auto& mojo_button : notification->buttons) {
    mc::ButtonInfo button;
    button.title = mojo_button->title;
    rich_data.buttons.push_back(button);
  }
  rich_data.pinned = notification->pinned;
  rich_data.renotify = notification->renotify;
  rich_data.silent = notification->silent;
  rich_data.accessible_name = notification->accessible_name;
  rich_data.fullscreen_visibility =
      FromMojo(notification->fullscreen_visibility);

  gfx::Image icon;
  if (!notification->icon.isNull())
    icon = gfx::Image(notification->icon);
  GURL origin_url = notification->origin_url.value_or(GURL());
  // TODO(crbug.com/1113889): NotifierId support.
  return std::make_unique<mc::Notification>(
      FromMojo(notification->type), notification->id, notification->title,
      notification->message, icon, notification->display_source, origin_url,
      mc::NotifierId(), rich_data, /*delegate=*/nullptr);
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

  void Click(const base::Optional<int>& button_index,
             const base::Optional<base::string16>& reply) override {
    if (button_index) {
      // Chrome OS does not support inline reply. The button index comes out of
      // trusted ash-side message center UI code and is guaranteed not to be
      // negative.
      remote_delegate_->OnNotificationButtonClicked(
          base::checked_cast<uint32_t>(*button_index));
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

MessageCenterAsh::MessageCenterAsh(
    mojo::PendingReceiver<mojom::MessageCenter> receiver)
    : receiver_(this, std::move(receiver)) {}

MessageCenterAsh::~MessageCenterAsh() = default;

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

}  // namespace crosapi
