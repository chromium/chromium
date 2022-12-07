// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/notification_utils.h"

#include "ui/gfx/vector_icon_types.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace ash {

message_center::Notification CreateSystemNotification(
    message_center::NotificationType type,
    const std::string& id,
    const std::u16string& title,
    const std::u16string& message,
    const std::u16string& display_source,
    const GURL& origin_url,
    const message_center::NotifierId& notifier_id,
    const message_center::RichNotificationData& optional_fields,
    scoped_refptr<message_center::NotificationDelegate> delegate,
    const gfx::VectorIcon& small_image,
    message_center::SystemNotificationWarningLevel warning_level) {
  DCHECK_EQ(message_center::NotifierType::SYSTEM_COMPONENT, notifier_id.type);
  SkColor color = kSystemNotificationColorNormal;
  switch (warning_level) {
    case message_center::SystemNotificationWarningLevel::NORMAL:
      color = kSystemNotificationColorNormal;
      break;
    case message_center::SystemNotificationWarningLevel::WARNING:
      color = kSystemNotificationColorWarning;
      break;
    case message_center::SystemNotificationWarningLevel::CRITICAL_WARNING:
      color = kSystemNotificationColorCriticalWarning;
      break;
  }
  message_center::Notification notification{type,
                                            id,
                                            title,
                                            message,
                                            ui::ImageModel(),
                                            display_source,
                                            origin_url,
                                            notifier_id,
                                            optional_fields,
                                            delegate};
  notification.set_accent_color(color);
  notification.set_system_notification_warning_level(warning_level);
  if (!small_image.is_empty())
    notification.set_vector_small_image(small_image);
  return notification;
}

std::unique_ptr<message_center::Notification> CreateSystemNotificationPtr(
    message_center::NotificationType type,
    const std::string& id,
    const std::u16string& title,
    const std::u16string& message,
    const std::u16string& display_source,
    const GURL& origin_url,
    const message_center::NotifierId& notifier_id,
    const message_center::RichNotificationData& optional_fields,
    scoped_refptr<message_center::NotificationDelegate> delegate,
    const gfx::VectorIcon& small_image,
    message_center::SystemNotificationWarningLevel warning_level) {
  return std::make_unique<message_center::Notification>(
      CreateSystemNotification(type, id, title, message, display_source,
                               origin_url, notifier_id, optional_fields,
                               delegate, small_image, warning_level));
}

}  // namespace ash
