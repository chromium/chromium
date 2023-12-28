// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_CENTER_ASH_NOTIFICATION_CONTROL_BUTTON_FACTORY_H_
#define ASH_SYSTEM_NOTIFICATION_CENTER_ASH_NOTIFICATION_CONTROL_BUTTON_FACTORY_H_

#include "ui/message_center/views/notification_control_button_factory.h"

#include "ash/ash_export.h"

namespace views {
class ImageButton;
}

namespace ash {

class ASH_EXPORT AshNotificationControlButtonFactory
    : public message_center::NotificationControlButtonFactory {
 public:
  AshNotificationControlButtonFactory() = default;
  AshNotificationControlButtonFactory(
      const AshNotificationControlButtonFactory&) = delete;
  AshNotificationControlButtonFactory& operator=(
      const AshNotificationControlButtonFactory&) = delete;

  // NotificationControlButtonFactory:
  std::unique_ptr<views::ImageButton> CreateButton(
      views::Button::PressedCallback callback) override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NOTIFICATION_CENTER_ASH_NOTIFICATION_CONTROL_BUTTON_FACTORY_H_