// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/media/media_notification_controller.h"

#include "base/strings/string16.h"
#include "services/media_session/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "url/gurl.h"

namespace ash {

namespace {

// The ID associated with the media session notification.
const char kMediaSessionNotificationId[] = "media-session";

// The notifier ID associated with the media session service.
const char kMediaSessionNotifierId[] = "media-session";

bool IsMediaSessionNotificationVisible() {
  return message_center::MessageCenter::Get()->FindVisibleNotificationById(
             kMediaSessionNotificationId) != nullptr;
}

}  // namespace

MediaNotificationController::MediaNotificationController(
    service_manager::Connector* connector) {
  // |connector| can be null in tests.
  if (!connector)
    return;

  media_session::mojom::AudioFocusManagerPtr audio_focus_ptr;
  connector->BindInterface(media_session::mojom::kServiceName,
                           mojo::MakeRequest(&audio_focus_ptr));

  media_session::mojom::AudioFocusObserverPtr observer;
  binding_.Bind(mojo::MakeRequest(&observer));
  audio_focus_ptr->AddObserver(std::move(observer));
}

MediaNotificationController::~MediaNotificationController() = default;

void MediaNotificationController::OnFocusGained(
    media_session::mojom::MediaSessionInfoPtr media_session,
    media_session::mojom::AudioFocusType type) {
  if (IsMediaSessionNotificationVisible())
    return;

  std::unique_ptr<message_center::Notification> notification =
      message_center::Notification::CreateSystemNotification(
          message_center::NotificationType::NOTIFICATION_TYPE_SIMPLE,
          kMediaSessionNotificationId, base::string16(), base::string16(),
          base::string16(), GURL(),
          message_center::NotifierId(
              message_center::NotifierId::SYSTEM_COMPONENT,
              kMediaSessionNotifierId),
          message_center::RichNotificationData(),
          base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
              base::BindRepeating(
                  &MediaNotificationController::OnNotificationClicked,
                  weak_ptr_factory_.GetWeakPtr())),
          gfx::VectorIcon(),
          message_center::SystemNotificationWarningLevel::NORMAL);

  notification->set_pinned(true);

  // Set the priority to low to prevent the notification showing as a popup and
  // keep it at the bottom of the list.
  notification->set_priority(message_center::LOW_PRIORITY);

  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
}

void MediaNotificationController::OnFocusLost(
    media_session::mojom::MediaSessionInfoPtr media_session) {
  if (!IsMediaSessionNotificationVisible())
    return;

  message_center::MessageCenter::Get()->RemoveNotification(
      kMediaSessionNotificationId, false);
}

void MediaNotificationController::OnNotificationClicked(
    base::Optional<int> button_id) {
  NOTIMPLEMENTED();
}

}  // namespace ash
