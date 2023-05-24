// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_notifier.h"

#include "ash/bluetooth_devices_observer.h"
#include "ash/public/cpp/input_device_settings_controller.h"
#include "ash/public/mojom/input_device_settings.mojom-forward.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/system/input_device_settings/input_device_settings_utils.h"
#include "base/containers/cxx20_erase_vector.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/flat_map.h"
#include "base/ranges/algorithm.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/bluetooth_device.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/keyboard_device.h"
#include "ui/events/devices/touchpad_device.h"

namespace ash {

namespace {

using DeviceId = InputDeviceSettingsController::DeviceId;

DeviceId ExtractDeviceIdFromInputDevice(const ui::InputDevice& device) {
  return device.id;
}

template <class DeviceMojomPtr>
bool IsDeviceASuspectedImposter(BluetoothDevicesObserver* bluetooth_observer,
                                const ui::InputDevice& device) {
  return false;
}

template <>
bool IsDeviceASuspectedImposter<mojom::KeyboardPtr>(
    BluetoothDevicesObserver* bluetooth_observer,
    const ui::InputDevice& device) {
  // If the device is a keyboard that is known to pretend to have mouse-like
  // functionality, do not use the `suspected_imposter` field.
  if (IsKeyboardPretendingToBeMouse(device)) {
    return false;
  }

  // If the device is bluetooth, check the bluetooth device to see if it is a
  // keyboard or keyboard/mouse combo.
  if (device.type == ui::INPUT_DEVICE_BLUETOOTH) {
    const auto* bluetooth_device =
        bluetooth_observer->GetConnectedBluetoothDevice(device);
    if (!bluetooth_device) {
      return false;
    }

    if (bluetooth_device->GetDeviceType() ==
            device::BluetoothDeviceType::KEYBOARD ||
        bluetooth_device->GetDeviceType() ==
            device::BluetoothDeviceType::KEYBOARD_MOUSE_COMBO) {
      return false;
    }

    return true;
  }

  return device.suspected_imposter;
}

template <>
bool IsDeviceASuspectedImposter<mojom::MousePtr>(
    BluetoothDevicesObserver* bluetooth_observer,
    const ui::InputDevice& device) {
  // If the device is a keyboard that is known to pretend to have mouse-like
  // functionality, the device should always be considered an imposter.
  if (IsKeyboardPretendingToBeMouse(device)) {
    return true;
  }

  // If the device is bluetooth, check the bluetooth device to see if it is a
  // mouse or mouse/keyboard combo.
  if (device.type == ui::INPUT_DEVICE_BLUETOOTH) {
    const auto* bluetooth_device =
        bluetooth_observer->GetConnectedBluetoothDevice(device);
    if (!bluetooth_device) {
      return false;
    }

    if (bluetooth_device->GetDeviceType() ==
            device::BluetoothDeviceType::MOUSE ||
        bluetooth_device->GetDeviceType() ==
            device::BluetoothDeviceType::KEYBOARD_MOUSE_COMBO) {
      return false;
    }

    return true;
  }

  return false;
}

template <typename T>
DeviceId ExtractDeviceIdFromDeviceMapPair(
    const std::pair<DeviceId, T>& id_t_pair) {
  return id_t_pair.first;
}

// Figures out which devices from `connected_devices` have been added/removed
// and stores them in the passed in vectors. `devices_to_add` and
// `devices_to_remove` will be cleared before being filled with the result.
template <class DeviceMojomPtr, typename InputDeviceType>
void GetAddedAndRemovedDevices(
    BluetoothDevicesObserver* bluetooth_observer,
    std::vector<InputDeviceType> updated_device_list,
    const base::flat_map<DeviceId, DeviceMojomPtr>& connected_devices,
    std::vector<InputDeviceType>* devices_to_add,
    std::vector<DeviceId>* devices_to_remove) {
  // Output parameter vectors must be empty to start.
  devices_to_add->clear();
  devices_to_remove->clear();

  // Sort input device list by id as `base::ranges::set_difference` requires
  // input lists are sorted.
  // Remove any devices marked as imposters as well.
  base::ranges::sort(updated_device_list, base::ranges::less(),
                     ExtractDeviceIdFromInputDevice);
  base::EraseIf(updated_device_list, [&](const ui::InputDevice& device) {
    return IsDeviceASuspectedImposter<DeviceMojomPtr>(bluetooth_observer,
                                                      device);
  });

  // Generate a vector with only the device ids from the connected_devices
  // map. Guaranteed to be sorted as flat_map is always in sorted order by
  // key.
  std::vector<DeviceId> connected_devices_ids;
  connected_devices_ids.reserve(connected_devices.size());
  base::ranges::transform(connected_devices,
                          std::back_inserter(connected_devices_ids),
                          ExtractDeviceIdFromDeviceMapPair<DeviceMojomPtr>);
  DCHECK(base::ranges::is_sorted(connected_devices_ids));

  // Compares the `id` field of `updated_device_list` to the ids in
  // `connected_devices_ids`. Devices that are in `updated_device_list` but not
  // in `connected_devices_ids` are inserted into `devices_to_add`.
  // `updated_device_list` and `connected_device_ids` must be sorted.
  base::ranges::set_difference(updated_device_list, connected_devices_ids,
                               std::back_inserter(*devices_to_add),
                               /*Comp=*/base::ranges::less(),
                               /*Proj1=*/ExtractDeviceIdFromInputDevice);

  // Compares the `connected_devices_ids` to the id field of
  // `updated_device_list`. Ids that are in `connected_devices_ids` but not in
  // `updated_device_list` are inserted into `devices_to_remove`.
  // `updated_device_list` and `connected_device_ids` must be sorted.
  base::ranges::set_difference(connected_devices_ids, updated_device_list,
                               std::back_inserter(*devices_to_remove),
                               /*Comp=*/base::ranges::less(),
                               /*Proj1=*/base::identity(),
                               /*Proj2=*/ExtractDeviceIdFromInputDevice);
}

}  // namespace

template <typename MojomDevicePtr, typename InputDeviceType>
InputDeviceNotifier<MojomDevicePtr, InputDeviceType>::InputDeviceNotifier(
    base::flat_map<DeviceId, MojomDevicePtr>* connected_devices,
    InputDeviceListsUpdatedCallback callback)
    : connected_devices_(connected_devices),
      device_lists_updated_callback_(callback) {
  DCHECK(connected_devices_);
  ui::DeviceDataManager::GetInstance()->AddObserver(this);
  bluetooth_devices_observer_ =
      std::make_unique<BluetoothDevicesObserver>(base::BindRepeating(
          &InputDeviceNotifier<MojomDevicePtr, InputDeviceType>::
              OnBluetoothAdapterOrDeviceChanged,
          base::Unretained(this)));
  RefreshDevices();
}

template <typename MojomDevicePtr, typename InputDeviceType>
InputDeviceNotifier<MojomDevicePtr, InputDeviceType>::~InputDeviceNotifier() {
  ui::DeviceDataManager::GetInstance()->RemoveObserver(this);
}

template <typename MojomDevicePtr, typename InputDeviceType>
void InputDeviceNotifier<MojomDevicePtr, InputDeviceType>::RefreshDevices() {
  std::vector<InputDeviceType> devices_to_add;
  std::vector<DeviceId> device_ids_to_remove;

  GetAddedAndRemovedDevices(bluetooth_devices_observer_.get(),
                            GetUpdatedDeviceList(), *connected_devices_,
                            &devices_to_add, &device_ids_to_remove);

  device_lists_updated_callback_.Run(std::move(devices_to_add),
                                     std::move(device_ids_to_remove));
}

template <typename MojomDevicePtr, typename InputDeviceType>
void InputDeviceNotifier<MojomDevicePtr,
                         InputDeviceType>::OnDeviceListsComplete() {
  RefreshDevices();
}

template <typename MojomDevicePtr, typename InputDeviceType>
void InputDeviceNotifier<MojomDevicePtr, InputDeviceType>::
    OnInputDeviceConfigurationChanged(uint8_t input_device_type) {
  RefreshDevices();
}

template <typename MojomDevicePtr, typename InputDeviceType>
void InputDeviceNotifier<MojomDevicePtr, InputDeviceType>::
    OnBluetoothAdapterOrDeviceChanged(device::BluetoothDevice* device) {
  // Do nothing as OnBluetoothAdapterOrDeviceChanged is very noisy and causes
  // updates to happen many times per second. We expect
  // OnInputDeviceConfigurationChanged to include all devices including
  // bluetooth devices, so refreshing devices here is unnecessary.
}

// Template specialization for retrieving the updated device lists for each
// device type.
template <>
std::vector<ui::KeyboardDevice>
InputDeviceNotifier<mojom::KeyboardPtr,
                    ui::KeyboardDevice>::GetUpdatedDeviceList() {
  return ui::DeviceDataManager::GetInstance()->GetKeyboardDevices();
}

template <>
std::vector<ui::TouchpadDevice>
InputDeviceNotifier<mojom::TouchpadPtr,
                    ui::TouchpadDevice>::GetUpdatedDeviceList() {
  return ui::DeviceDataManager::GetInstance()->GetTouchpadDevices();
}

template <>
std::vector<ui::InputDevice>
InputDeviceNotifier<mojom::MousePtr, ui::InputDevice>::GetUpdatedDeviceList() {
  auto mice = ui::DeviceDataManager::GetInstance()->GetMouseDevices();
  base::EraseIf(mice, [](const auto& mouse) {
    // Some I2C touchpads falsely claim to be mice, see b/205272718
    // By filtering out internal mice, i2c touchpads are prevented from being in
    // the "mouse" category in settings.
    return mouse.type == ui::INPUT_DEVICE_INTERNAL;
  });
  return mice;
}

template <>
std::vector<ui::InputDevice>
InputDeviceNotifier<mojom::PointingStickPtr,
                    ui::InputDevice>::GetUpdatedDeviceList() {
  return ui::DeviceDataManager::GetInstance()->GetPointingStickDevices();
}

// Explicit instantiations for each device type.
template class EXPORT_TEMPLATE_DECLARE(ASH_EXPORT)
    InputDeviceNotifier<mojom::KeyboardPtr, ui::KeyboardDevice>;
template class EXPORT_TEMPLATE_DECLARE(ASH_EXPORT)
    InputDeviceNotifier<mojom::TouchpadPtr, ui::TouchpadDevice>;
template class EXPORT_TEMPLATE_DECLARE(ASH_EXPORT)
    InputDeviceNotifier<mojom::MousePtr, ui::InputDevice>;
template class EXPORT_TEMPLATE_DECLARE(ASH_EXPORT)
    InputDeviceNotifier<mojom::PointingStickPtr, ui::InputDevice>;

}  // namespace ash
