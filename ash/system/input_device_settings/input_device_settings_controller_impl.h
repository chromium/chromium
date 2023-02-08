// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_CONTROLLER_IMPL_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_CONTROLLER_IMPL_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/input_device_settings_controller.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/system/input_device_settings/input_device_notifier.h"
#include "ash/system/input_device_settings/pref_handlers/keyboard_pref_handler.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "ui/events/devices/input_device.h"

class PrefRegistrySimple;

namespace ash {

// Controller to manage input device settings.
class ASH_EXPORT InputDeviceSettingsControllerImpl
    : public InputDeviceSettingsController,
      public SessionObserver {
 public:
  InputDeviceSettingsControllerImpl();
  explicit InputDeviceSettingsControllerImpl(
      std::unique_ptr<KeyboardPrefHandler> keyboard_pref_handler);
  InputDeviceSettingsControllerImpl(const InputDeviceSettingsControllerImpl&) =
      delete;
  InputDeviceSettingsControllerImpl& operator=(
      const InputDeviceSettingsControllerImpl&) = delete;
  ~InputDeviceSettingsControllerImpl() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* pref_rCegistry);

  // InputDeviceSettingsController:
  std::vector<mojom::KeyboardPtr> GetConnectedKeyboards() override;
  std::vector<mojom::TouchpadPtr> GetConnectedTouchpads() override;
  std::vector<mojom::MousePtr> GetConnectedMice() override;
  std::vector<mojom::PointingStickPtr> GetConnectedPointingSticks() override;
  void SetKeyboardSettings(DeviceId id,
                           const mojom::KeyboardSettings& settings) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  void OnKeyboardListUpdated(std::vector<ui::InputDevice> keyboards_to_add,
                             std::vector<DeviceId> keyboard_ids_to_remove);
  void OnTouchpadListUpdated(std::vector<ui::InputDevice> touchpads_to_add,
                             std::vector<DeviceId> touchpad_ids_to_remove);
  void OnMouseListUpdated(std::vector<ui::InputDevice> mice_to_add,
                          std::vector<DeviceId> mouse_ids_to_remove);
  void OnPointingStickListUpdated(
      std::vector<ui::InputDevice> pointing_sticks_to_add,
      std::vector<DeviceId> pointing_stick_ids_to_remove);

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  void SetPrefHandlersForTesting(
      std::unique_ptr<KeyboardPrefHandler> keyboard_pref_handler);

 private:
  void Init();

  void DispatchKeyboardConnected(DeviceId id);
  void DispatchKeyboardDisconnected(DeviceId id);

  void DispatchTouchpadConnected(DeviceId id);
  void DispatchTouchpadDisconnected(DeviceId id);

  void DispatchMouseConnected(DeviceId id);
  void DispatchMouseDisconnected(DeviceId id);

  void DispatchPointingStickConnected(DeviceId id);
  void DispatchPointingStickDisconnected(DeviceId id);

  base::ObserverList<InputDeviceSettingsController::Observer> observers_;

  std::unique_ptr<KeyboardPrefHandler> keyboard_pref_handler_;

  base::flat_map<DeviceId, mojom::KeyboardPtr> keyboards_;
  base::flat_map<DeviceId, mojom::TouchpadPtr> touchpads_;
  base::flat_map<DeviceId, mojom::MousePtr> mice_;
  base::flat_map<DeviceId, mojom::PointingStickPtr> pointing_sticks_;

  // Notifiers must be declared after the `flat_map` objects as the notifiers
  // depend on these objects.
  std::unique_ptr<InputDeviceNotifier<mojom::KeyboardPtr>> keyboard_notifier_;
  std::unique_ptr<InputDeviceNotifier<mojom::TouchpadPtr>> touchpad_notifier_;
  std::unique_ptr<InputDeviceNotifier<mojom::MousePtr>> mouse_notifier_;
  std::unique_ptr<InputDeviceNotifier<mojom::PointingStickPtr>>
      pointing_stick_notifier_;

  raw_ptr<PrefService> active_pref_service_ = nullptr;  // Not owned.
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_CONTROLLER_IMPL_H_
