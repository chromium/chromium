// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_NOTIFIER_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_NOTIFIER_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/bluetooth_devices_observer.h"
#include "ash/public/cpp/input_device_settings_controller.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/mojom/input_device_settings.mojom-forward.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/export_template.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/input_device_event_observer.h"
#include "ui/events/devices/keyboard_device.h"
#include "ui/events/devices/touchpad_device.h"

namespace ash {

class BluetoothDevicesObserver;

// Calls the given callback every time a device is connected or removed with
// lists of which were added or removed.
template <typename MojomDevicePtr, typename InputDeviceType>
class ASH_EXPORT InputDeviceNotifier : public ui::InputDeviceEventObserver,
                                       public SessionObserver {
 public:
  using DeviceId = InputDeviceSettingsController::DeviceId;
  using InputDeviceListsUpdatedCallback =
      base::RepeatingCallback<void(std::vector<InputDeviceType>,
                                   std::vector<DeviceId>)>;

  // Passed in `connected_devices` must outlive constructed
  // `InputDeviceNotifier`.
  InputDeviceNotifier(
      base::flat_map<DeviceId, MojomDevicePtr>* connected_devices,
      InputDeviceListsUpdatedCallback callback);
  InputDeviceNotifier(const InputDeviceNotifier&) = delete;
  InputDeviceNotifier& operator=(const InputDeviceNotifier&) = delete;
  ~InputDeviceNotifier() override;

  // ui::InputDeviceEventObserver
  void OnInputDeviceConfigurationChanged(uint8_t input_device_type) override;
  void OnDeviceListsComplete() override;

  // SessionObserver
  void OnLoginStatusChanged(LoginStatus login_status) override;

  // Used as callback for `bluetooth_devices_observer_` whenever a bluetooth
  // device state changes.
  void OnBluetoothAdapterOrDeviceChanged(device::BluetoothDevice* device);

 private:
  void RefreshDevices();

  std::vector<InputDeviceType> GetUpdatedDeviceList();
  void HandleImposterPref(
      const std::vector<InputDeviceType>& updated_device_list);

  // `connected_devices_` is owned by `InputDeviceSettingsControllerImpl` which
  // instantiates the `InputDeviceNotifier` as a member. `connected_devices_`
  // will always outlive `InputDeviceNotifier`.
  raw_ptr<base::flat_map<DeviceId, MojomDevicePtr>> connected_devices_;
  InputDeviceListsUpdatedCallback device_lists_updated_callback_;

  std::unique_ptr<BluetoothDevicesObserver> bluetooth_devices_observer_;

  // The set of devices that were keyboard imposters last time devices were
  // refreshed.
  base::flat_set<DeviceId> keyboard_imposter_devices_;
  // The set of devices that were mouse imposters last time devices were
  // refreshed.
  base::flat_set<DeviceId> mouse_imposter_devices_;
  // The set of device keys to add to the prefs.
  base::flat_set<std::string> keyboard_imposter_false_positives_to_add_;
  base::flat_set<std::string> mouse_imposter_false_positives_to_add_;
};

// Below explicit template instantiations needed for all supported types.
template <>
ASH_EXPORT void
InputDeviceNotifier<mojom::KeyboardPtr, ui::KeyboardDevice>::HandleImposterPref(
    const std::vector<ui::KeyboardDevice>& updated_device_list);
template <>
ASH_EXPORT void
InputDeviceNotifier<mojom::MousePtr, ui::InputDevice>::HandleImposterPref(
    const std::vector<ui::InputDevice>& updated_device_list);

template <>
ASH_EXPORT std::vector<ui::KeyboardDevice>
InputDeviceNotifier<mojom::KeyboardPtr,
                    ui::KeyboardDevice>::GetUpdatedDeviceList();
template <>
ASH_EXPORT std::vector<ui::TouchpadDevice>
InputDeviceNotifier<mojom::TouchpadPtr,
                    ui::TouchpadDevice>::GetUpdatedDeviceList();
template <>
ASH_EXPORT std::vector<ui::InputDevice>
InputDeviceNotifier<mojom::MousePtr, ui::InputDevice>::GetUpdatedDeviceList();
template <>
ASH_EXPORT std::vector<ui::InputDevice>
InputDeviceNotifier<mojom::PointingStickPtr,
                    ui::InputDevice>::GetUpdatedDeviceList();
template <>
ASH_EXPORT std::vector<ui::InputDevice>
InputDeviceNotifier<mojom::GraphicsTabletPtr,
                    ui::InputDevice>::GetUpdatedDeviceList();

extern template class EXPORT_TEMPLATE_DECLARE(ASH_EXPORT)
    InputDeviceNotifier<mojom::KeyboardPtr, ui::KeyboardDevice>;
extern template class EXPORT_TEMPLATE_DECLARE(ASH_EXPORT)
    InputDeviceNotifier<mojom::TouchpadPtr, ui::TouchpadDevice>;
extern template class EXPORT_TEMPLATE_DECLARE(ASH_EXPORT)
    InputDeviceNotifier<mojom::MousePtr, ui::InputDevice>;
extern template class EXPORT_TEMPLATE_DECLARE(ASH_EXPORT)
    InputDeviceNotifier<mojom::PointingStickPtr, ui::InputDevice>;
extern template class EXPORT_TEMPLATE_DECLARE(ASH_EXPORT)
    InputDeviceNotifier<mojom::GraphicsTabletPtr, ui::InputDevice>;

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_NOTIFIER_H_
