// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/ash_notification_control_button_factory.h"

#include "ash/style/icon_button.h"

namespace ash {

std::unique_ptr<views::ImageButton>
AshNotificationControlButtonFactory::CreateButton(
    views::Button::PressedCallback callback) {
  return std::make_unique<ash::IconButton>(
      std::move(callback), ash::IconButton::Type::kSmallFloating, nullptr,
      false, false);
}

}  // namespace ash
