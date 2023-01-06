// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_CONTROLLER_IMPL_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_CONTROLLER_IMPL_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/input_device_settings_controller.h"
#include "ash/system/input_device_settings/input_device_pref_manager.h"
#include "base/containers/flat_map.h"
#include "base/observer_list.h"
#include "ui/events/devices/input_device_event_observer.h"

namespace ash {

// Controller to manage input device settings.
class ASH_EXPORT InputDeviceSettingsControllerImpl
    : public InputDeviceSettingsController,
      public ui::InputDeviceEventObserver {
 public:
  InputDeviceSettingsControllerImpl();
  explicit InputDeviceSettingsControllerImpl(
      std::unique_ptr<InputDevicePrefManager> pref_manager);
  InputDeviceSettingsControllerImpl(const InputDeviceSettingsControllerImpl&) =
      delete;
  InputDeviceSettingsControllerImpl& operator=(
      const InputDeviceSettingsControllerImpl&) = delete;
  ~InputDeviceSettingsControllerImpl() override;

  // InputDeviceSettingsController:
  std::vector<mojom::KeyboardPtr> GetConnectedKeyboards() override;
  void SetKeyboardSettings(DeviceId id,
                           const mojom::KeyboardSettings& settings) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

 private:
  void Init();

  void RefreshDeviceLists();
  void RefreshKeyboardList();

  void DispatchKeyboardConnected(DeviceId id);
  void DispatchKeyboardDisconnected(DeviceId id);

  // ui::InputDeviceEventObserver
  void OnInputDeviceConfigurationChanged(uint8_t input_device_type) override;
  void OnDeviceListsComplete() override;

  std::unique_ptr<InputDevicePrefManager> pref_manager_;
  base::flat_map<DeviceId, mojom::KeyboardPtr> keyboards_;
  base::ObserverList<InputDeviceSettingsController::Observer> observers_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_CONTROLLER_IMPL_H_
