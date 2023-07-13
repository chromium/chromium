// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_NOTIFICATION_CONTROLLER_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_NOTIFICATION_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/events/ash/mojom/simulate_right_click_modifier.mojom-shared.h"
#include "ui/events/ash/mojom/six_pack_shortcut_modifier.mojom-shared.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

class PrefRegistrySimple;

namespace message_center {
class MessageCenter;
}  // namespace message_center

namespace ash {

// Manages showing notifications for Six Pack/right-click event rewrites.
// Notifications are shown when the user's setting is inconsistent with
// the matched modifier key or the setting is disabled.
class ASH_EXPORT InputDeviceSettingsNotificationController {
 public:
  explicit InputDeviceSettingsNotificationController(
      message_center::MessageCenter* message_center);
  InputDeviceSettingsNotificationController(
      const InputDeviceSettingsNotificationController&) = delete;
  InputDeviceSettingsNotificationController& operator=(
      const InputDeviceSettingsNotificationController&) = delete;
  virtual ~InputDeviceSettingsNotificationController();

  static void RegisterProfilePrefs(PrefRegistrySimple* pref_registry);

  // Used to display a notification when an incoming event would have been
  // remapped to a right click but either the user's setting is inconsistent
  // with the matched modifier key or remapping to right click is disabled.
  void NotifyRightClickRewriteBlockedBySetting(
      ui::mojom::SimulateRightClickModifier blocked_modifier,
      ui::mojom::SimulateRightClickModifier active_modifier);

  // Used to display a notification when an incoming event would have been
  // remapped to a Six Pack key action but either the user's setting is
  // inconsistent with the matched modifier key or remapping to right click
  // is disabled. `key_code` is used to lookup the correct Six Pack key and
  // the `device_id` is provided to route the user to the correct remap keys
  // subpage when the notification is clicked on.
  void NotifySixPackRewriteBlockedBySetting(
      ui::KeyboardCode key_code,
      ui::mojom::SixPackShortcutModifier blocked_modifier,
      ui::mojom::SixPackShortcutModifier active_modifier,
      int device_id);

 private:
  // MessageCenter for adding notifications.
  const raw_ptr<message_center::MessageCenter, ExperimentalAsh> message_center_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_NOTIFICATION_CONTROLLER_H_
