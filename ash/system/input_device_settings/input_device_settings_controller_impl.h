// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_CONTROLLER_IMPL_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_CONTROLLER_IMPL_H_

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
  void SetKeyboardSettings(DeviceId id,
                           const mojom::KeyboardSettings& settings) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  void OnKeyboardListUpdated(std::vector<ui::InputDevice> keyboards_to_add,
                             std::vector<DeviceId> keyboard_ids_to_remove);

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  void SetPrefHandlersForTesting(
      std::unique_ptr<KeyboardPrefHandler> keyboard_pref_handler);

 private:
  void Init();

  void DispatchKeyboardConnected(DeviceId id);
  void DispatchKeyboardDisconnected(DeviceId id);

  std::unique_ptr<KeyboardPrefHandler> keyboard_pref_handler_;
  base::flat_map<DeviceId, mojom::KeyboardPtr> keyboards_;
  base::ObserverList<InputDeviceSettingsController::Observer> observers_;

  std::unique_ptr<InputDeviceNotifier<mojom::KeyboardPtr>> keyboard_notifier_;

  raw_ptr<PrefService> active_pref_service_ = nullptr;  // Not owned.
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_CONTROLLER_IMPL_H_
