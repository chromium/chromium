// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_NOTIFICATION_CONTROLLER_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_NOTIFICATION_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/events/ash/mojom/simulate_right_click_modifier.mojom-shared.h"

namespace message_center {
class MessageCenter;
}  // namespace message_center

namespace ash {

// TODO(b/279503977): Show notification up to 3 times for each setting.
// TODO(b/279503977): Only display notification for active user sessions.
class ASH_EXPORT InputDeviceSettingsNotificationController {
 public:
  explicit InputDeviceSettingsNotificationController(
      message_center::MessageCenter* message_center);
  InputDeviceSettingsNotificationController(
      const InputDeviceSettingsNotificationController&) = delete;
  InputDeviceSettingsNotificationController& operator=(
      const InputDeviceSettingsNotificationController&) = delete;
  virtual ~InputDeviceSettingsNotificationController();

  void NotifyRightClickRewriteBlockedBySetting(
      ui::mojom::SimulateRightClickModifier blocked_modifier,
      ui::mojom::SimulateRightClickModifier active_modifier);

 private:
  // MessageCenter for adding notifications.
  const raw_ptr<message_center::MessageCenter, ExperimentalAsh> message_center_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_NOTIFICATION_CONTROLLER_H_
