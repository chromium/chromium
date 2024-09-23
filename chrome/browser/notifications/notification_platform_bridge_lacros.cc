// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge_lacros.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "chrome/browser/notifications/notification_platform_bridge_delegate.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chromeos/crosapi/mojom/message_center.mojom.h"
#include "chromeos/crosapi/mojom/notification.mojom-shared.h"
#include "chromeos/crosapi/mojom/notification.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_manager.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/native_theme/native_theme.h"

namespace {

crosapi::mojom::NotificationType ToMojo(message_center::NotificationType type) {
  switch (type) {
    case message_center::NOTIFICATION_TYPE_SIMPLE:
    case message_center::DEPRECATED_NOTIFICATION_TYPE_BASE_FORMAT:
      return crosapi::mojom::NotificationType::kSimple;
    case message_center::NOTIFICATION_TYPE_IMAGE:
      return crosapi::mojom::NotificationType::kImage;
    case message_center::NOTIFICATION_TYPE_MULTIPLE:
      return crosapi::mojom::NotificationType::kList;
    case message_center::NOTIFICATION_TYPE_PROGRESS:
      return crosapi::mojom::NotificationType::kProgress;
    case message_center::NOTIFICATION_TYPE_CUSTOM:
      // TYPE_CUSTOM exists only within ash.
      NOTREACHED_IN_MIGRATION();
      return crosapi::mojom::NotificationType::kSimple;
    case message_center::NOTIFICATION_TYPE_CONVERSATION:
      // TYPE_CONVERSATION is not currently supported for Lacros.
      NOTREACHED_IN_MIGRATION();
      return crosapi::mojom::NotificationType::kSimple;
  }
}

crosapi::mojom::NotifierType ToMojo(message_center::NotifierType type) {
  switch (type) {
    case message_center::NotifierType::APPLICATION:
      return crosapi::mojom::NotifierType::kApplication;
    case message_center::NotifierType::ARC_APPLICATION:
      return crosapi::mojom::NotifierType::kArcApplication;
    case message_center::NotifierType::WEB_PAGE:
      return crosapi::mojom::NotifierType::kWebPage;
    case message_center::NotifierType::SYSTEM_COMPONENT:
      return crosapi::mojom::NotifierType::kSystemComponent;
    case message_center::NotifierType::CROSTINI_APPLICATION:
      return crosapi::mojom::NotifierType::kCrostiniApplication;
    case message_center::NotifierType::PHONE_HUB:
      return crosapi::mojom::NotifierType::kPhoneHub;
  }
}

crosapi::mojom::FullscreenVisibility ToMojo(
    message_center::FullscreenVisibility visibility) {
  switch (visibility) {
    case message_center::FullscreenVisibility::NONE:
      return crosapi::mojom::FullscreenVisibility::kNone;
    case message_center::FullscreenVisibility::OVER_USER:
      return crosapi::mojom::FullscreenVisibility::kOverUser;
  }
}

crosapi::mojom::SettingsButtonHandler ToMojo(
    message_center::SettingsButtonHandler settings_button_handler) {
  switch (settings_button_handler) {
    case message_center::SettingsButtonHandler::NONE:
      return crosapi::mojom::SettingsButtonHandler::kNone;
    case message_center::SettingsButtonHandler::INLINE:
      return crosapi::mojom::SettingsButtonHandler::kInline;
    case message_center::SettingsButtonHandler::DELEGATE:
      return crosapi::mojom::SettingsButtonHandler::kDelegate;
  }
}

crosapi::mojom::NotificationPtr ToMojo(
    const message_center::Notification& notification,
    const ui::ColorProvider* color_provider) {
  auto mojo_note = crosapi::mojom::Notification::New();
  mojo_note->type = ToMojo(notification.type());
  mojo_note->id = notification.id();
  mojo_note->title = notification.title();
  mojo_note->message = notification.message();
  mojo_note->display_source = notification.display_source();
  mojo_note->origin_url = notification.origin_url();
  if (!notification.icon().IsEmpty())
    mojo_note->icon = notification.icon().Rasterize(color_provider);
  mojo_note->priority = std::clamp(notification.priority(), -2, 2);
  mojo_note->require_interaction = notification.never_timeout();
  mojo_note->timestamp = notification.timestamp();
  if (!notification.image().IsEmpty())
    mojo_note->image = notification.image().AsImageSkia();
  if (!notification.small_image().IsEmpty()) {
    mojo_note->badge = notification.small_image().AsImageSkia();
    mojo_note->badge_needs_additional_masking_has_value = true;
    mojo_note->badge_needs_additional_masking =
        notification.small_image_needs_additional_masking();
  }
  for (const auto& item : notification.items()) {
    auto mojo_item = crosapi::mojom::NotificationItem::New();
    mojo_item->title = item.title();
    mojo_item->message = item.message();
    mojo_note->items.push_back(std::move(mojo_item));
  }
  mojo_note->progress = std::clamp(notification.progress(), -1, 100);
  mojo_note->progress_status = notification.progress_status();
  for (const auto& button : notification.buttons()) {
    auto mojo_button = crosapi::mojom::ButtonInfo::New();
    mojo_button->title = button.title;
    mojo_button->placeholder = button.placeholder;
    mojo_note->buttons.push_back(std::move(mojo_button));
  }
  mojo_note->pinned = notification.pinned();
  mojo_note->renotify = notification.renotify();
  mojo_note->silent = notification.silent();
  mojo_note->accessible_name = notification.accessible_name();
  mojo_note->fullscreen_visibility =
      ToMojo(notification.fullscreen_visibility());
  if (notification.accent_color_id().has_value()) {
    // Colors have to be resolved in lacros since color ids are not guaranteed
    // to be stable across the process boundary.
    mojo_note->accent_color =
        color_provider->GetColor(*notification.accent_color_id());
  } else {
    // TODO(b/308208767): Remove when this isn't used anymore.
    mojo_note->accent_color = notification.accent_color();
  }

  mojo_note->notifier_id = crosapi::mojom::NotifierId::New();
  mojo_note->notifier_id->type = ToMojo(notification.notifier_id().type);
  mojo_note->notifier_id->id = notification.notifier_id().id;
  mojo_note->notifier_id->url = notification.notifier_id().url;
  mojo_note->notifier_id->title = notification.notifier_id().title;
  mojo_note->notifier_id->profile_id = notification.notifier_id().profile_id;

  const std::optional<base::FilePath>& image_path =
      notification.rich_notification_data().image_path;
  if (image_path.has_value()) {
    mojo_note->image_path = image_path;
  }

  mojo_note->settings_button_handler =
      ToMojo(notification.rich_notification_data().settings_button_handler);

  return mojo_note;
}

}  // namespace

// Keeps track of notifications being displayed in the remote message center.
class NotificationPlatformBridgeLacros::RemoteNotificationDelegate
    : public crosapi::mojom::NotificationDelegate {
 public:
  RemoteNotificationDelegate(
      const std::string& notification_id,
      NotificationPlatformBridgeDelegate* bridge_delegate,
      base::WeakPtr<NotificationPlatformBridgeLacros> owner)
      : notification_id_(notification_id),
        bridge_delegate_(bridge_delegate),
        owner_(owner) {
    DCHECK(!notification_id_.empty());
    DCHECK(bridge_delegate_);
    DCHECK(owner_);
  }
  RemoteNotificationDelegate(const RemoteNotificationDelegate&) = delete;
  RemoteNotificationDelegate& operator=(const RemoteNotificationDelegate&) =
      delete;
  ~RemoteNotificationDelegate() override = default;

  mojo::PendingRemote<crosapi::mojom::NotificationDelegate>
  BindNotificationDelegate() {
    return receiver_.BindNewPipeAndPassRemoteWithVersion();
  }

  // crosapi::mojom::NotificationDelegate:
  void OnNotificationClosed(bool by_user) override {
    bridge_delegate_->HandleNotificationClosed(notification_id_, by_user);
    if (owner_)
      owner_->OnRemoteNotificationClosed(notification_id_);
    // NOTE: |this| is deleted.
  }

  void OnNotificationClicked() override {
    bridge_delegate_->HandleNotificationClicked(notification_id_);
  }

  void OnNotificationButtonClicked(
      uint32_t button_index,
      const std::optional<::std::u16string>& reply) override {
    bridge_delegate_->HandleNotificationButtonClicked(
        notification_id_, base::checked_cast<int>(button_index), reply);
  }

  void OnNotificationSettingsButtonClicked() override {
    bridge_delegate_->HandleNotificationSettingsButtonClicked(notification_id_);
  }

  void OnNotificationDisabled() override {
    bridge_delegate_->DisableNotification(notification_id_);
  }

 private:
  const std::string notification_id_;
  const raw_ptr<NotificationPlatformBridgeDelegate> bridge_delegate_;
  base::WeakPtr<NotificationPlatformBridgeLacros> owner_;
  mojo::Receiver<crosapi::mojom::NotificationDelegate> receiver_{this};
};

NotificationPlatformBridgeLacros::NotificationPlatformBridgeLacros(
    NotificationPlatformBridgeDelegate* delegate,
    mojo::Remote<crosapi::mojom::MessageCenter>* message_center_remote)
    : bridge_delegate_(delegate),
      message_center_remote_(message_center_remote) {
  DCHECK(bridge_delegate_);
}

NotificationPlatformBridgeLacros::~NotificationPlatformBridgeLacros() = default;

void NotificationPlatformBridgeLacros::Display(
    NotificationHandler::Type notification_type,
    Profile* profile,
    const message_center::Notification& notification,
    std::unique_ptr<NotificationCommon::Metadata> metadata) {
  if (!message_center_remote_)
    return;

  // |profile| is ignored because Profile management is handled in
  // NotificationPlatformBridgeChromeOs, which includes a profile ID as part of
  // the notification ID. Lacros does not support Chrome OS multi-signin, so we
  // don't need to handle inactive user notification blockers in ash.

  auto pending_notification = std::make_unique<RemoteNotificationDelegate>(
      notification.id(), bridge_delegate_, weak_factory_.GetWeakPtr());
  // Display the notification, or update an existing one with the same ID.
  // `profile` may be null for e.g. system notifications.
  const auto* const color_provider =
      profile
          ? ThemeServiceFactory::GetForProfile(profile)->GetColorProvider()
          : ui::ColorProviderManager::Get().GetColorProviderFor(
                ui::NativeTheme::GetInstanceForNativeUi()->GetColorProviderKey(
                    nullptr));
  (*message_center_remote_)
      ->DisplayNotification(ToMojo(notification, color_provider),
                            pending_notification->BindNotificationDelegate());
  remote_notifications_[notification.id()] = std::move(pending_notification);
}

void NotificationPlatformBridgeLacros::Close(
    Profile* profile,
    const std::string& notification_id) {
  if (!message_center_remote_)
    return;

  (*message_center_remote_)->CloseNotification(notification_id);
  // |remote_notifications_| is cleaned up after the remote notification closes
  // and notifies us via the delegate.
}

void NotificationPlatformBridgeLacros::GetDisplayed(
    Profile* profile,
    GetDisplayedNotificationsCallback callback) const {
  NOTIMPLEMENTED();
  std::move(callback).Run(/*notification_ids=*/{}, /*supports_sync=*/false);
}

void NotificationPlatformBridgeLacros::GetDisplayedForOrigin(
    Profile* profile,
    const GURL& origin,
    GetDisplayedNotificationsCallback callback) const {
  NOTIMPLEMENTED();
  std::move(callback).Run(/*notification_ids=*/{}, /*supports_sync=*/false);
}

void NotificationPlatformBridgeLacros::SetReadyCallback(
    NotificationBridgeReadyCallback callback) {
  // Always return success even if |message_center_remote_| is not valid as we
  // don't have another way of displaying notifications on ChromeOS via Lacros.
  std::move(callback).Run(/*success=*/true);
}

void NotificationPlatformBridgeLacros::DisplayServiceShutDown(
    Profile* profile) {
  remote_notifications_.clear();
}

void NotificationPlatformBridgeLacros::OnRemoteNotificationClosed(
    const std::string& id) {
  remote_notifications_.erase(id);
}
