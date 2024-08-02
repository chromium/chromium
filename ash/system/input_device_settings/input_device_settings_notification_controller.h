// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_NOTIFICATION_CONTROLLER_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_NOTIFICATION_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/events/ash/mojom/simulate_right_click_modifier.mojom-shared.h"
#include "ui/events/ash/mojom/six_pack_shortcut_modifier.mojom-shared.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

class PrefRegistrySimple;

namespace message_center {
class MessageCenter;
}  // namespace message_center

namespace ash {

// The notification button index.
enum NotificationButtonIndex {
  BUTTON_EDIT_SHORTCUT = 0,
  BUTTON_LEARN_MORE,
};

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

  // Used to display a notification when a customizable mouse is connected to
  // the chromebook for the first time.
  void NotifyMouseIsCustomizable(const mojom::Mouse& mouse,
                                 const gfx::ImageSkia& device_image);

  // Used to display a notification when a customizable graphics tablet is
  // connected to the chromebook for the first time.
  void NotifyGraphicsTabletIsCustomizable(
      const mojom::GraphicsTablet& graphics_tablet,
      const gfx::ImageSkia& device_image);

  // Used to display a notification when a customizable keyboard is connected
  // to the chromebook for the first time.
  void ShowKeyboardSettingsNotification(const mojom::Keyboard& keyboard,
                                        const gfx::ImageSkia& device_image);

  // Used to display a notification when a customizable touchpad is connected
  // to the chromebook for the first time.
  void ShowTouchpadSettingsNotification(const mojom::Touchpad& touchpad,
                                        const gfx::ImageSkia& device_image);

  // Use to display a notification when a mouse is first connected.
  void NotifyMouseFirstTimeConnected(const mojom::Mouse& mouse,
                                     const gfx::ImageSkia& device_image = {});

  // Used to display a notification when a customizable pointing stick is
  // connected to the chromebook for the first time.
  void ShowPointingStickSettingsNotification(
      const mojom::PointingStick& pointing_stick);

  // Use to display a notification when a graphics table is first connected.
  void NotifyGraphicsTabletFirstTimeConnected(
      const mojom::GraphicsTablet& graphics_tablet,
      const gfx::ImageSkia& device_image = {});

  // Use to display a notification when a keyboard is first connected.
  void NotifyKeyboardFirstTimeConnected(const mojom::Keyboard& keyboard,
                                        const gfx::ImageSkia& device_image);

  // Use to display a notification when a touchpad is first connected.
  void NotifyTouchpadFirstTimeConnected(const mojom::Touchpad& touchpad,
                                        const gfx::ImageSkia& device_image);

  // Use to display a notification when a pointing stick is first connected.
  void NotifyPointingStickFirstTimeConnected(
      const mojom::PointingStick& pointing_stick);

  // Use to display a notification to remind users to press Fn key when users
  // press search key with top row keys and there is no matching behavior.
  void ShowTopRowRewritingNudge();

  // Use to display a notification to remind users to press Fn key when users
  // press search key or alt key with arrow keys and there is no matching
  // behavior.
  void ShowSixPackKeyRewritingNudge(
      ui::KeyboardCode key_code,
      ui::mojom::SixPackShortcutModifier blocked_modifier);

  // Use to display a notification to remind users to press Fn key when users
  // press search key or right-alt key with alt key to switch caps lock
  // and there is no matching.
  void ShowCapsLockRewritingNudge();

  std::optional<std::string> GetDeviceKeyForNotificationId(
      const std::string& notification_id);

 private:
  void HandleRightClickNotificationClicked(const std::string& notification_id,
                                           std::optional<int> button_index);

  void HandleSixPackNotificationClicked(int device_id,
                                        const char* pref_name,
                                        const std::string& notification_id,
                                        std::optional<int> button_index);

  base::flat_map<std::string, std::string> notification_id_to_device_key_map_;

  // MessageCenter for adding notifications.
  const raw_ptr<message_center::MessageCenter, DanglingUntriaged>
      message_center_;

  base::WeakPtrFactory<InputDeviceSettingsNotificationController>
      weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_NOTIFICATION_CONTROLLER_H_
