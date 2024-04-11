// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_notifier.h"

#include <functional>
#include <vector>

#include "ash/bluetooth_devices_observer.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/input_device_settings_controller.h"
#include "ash/public/mojom/input_device_settings.mojom-forward.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/input_device_settings/input_device_settings_metadata.h"
#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "ash/system/input_device_settings/input_device_settings_utils.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/values.h"
#include "components/prefs/pref_service.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/bluetooth_device.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/keyboard_device.h"
#include "ui/events/devices/touchpad_device.h"

namespace ash {

namespace {

using DeviceId = InputDeviceSettingsController::DeviceId;

bool AreOnLoginScreen() {
  auto status = Shell::Get()->session_controller()->login_status();
  return status == LoginStatus::NOT_LOGGED_IN;
}

DeviceId ExtractDeviceIdFromInputDevice(const ui::InputDevice& device) {
  return device.id;
}

template <class DeviceMojomPtr>
bool IsDeviceASuspectedImposter(BluetoothDevicesObserver* bluetooth_observer,
                                const ui::InputDevice& device) {
  return false;
}

// Imposter here means a device that has a virtual keyboard device as well as a
// virtual mouse device presented to evdev and the keyboard device is "fake".
bool IsKeyboardAKnownImposterFalsePositive(const ui::InputDevice& device) {
  if (!Shell::Get()->session_controller()->IsActiveUserSessionStarted()) {
    return false;
  }

  PrefService* prefs =
      Shell::Get()->session_controller()->GetActivePrefService();
  if (!prefs) {
    return false;
  }

  const auto& imposters =
      prefs->GetList(prefs::kKeyboardDeviceImpostersListPref);
  const std::string device_key = BuildDeviceKey(device);
  return base::Contains(imposters, device_key);
}

// Imposter here means a device that has a virtual keyboard device as well as a
// virtual mouse device presented to evdev and the mouse device is "fake".
bool IsMouseAKnownImposterFalsePositive(const ui::InputDevice& device) {
  if (!features::IsMouseImposterCheckEnabled()) {
    return false;
  }

  if (!Shell::Get()->session_controller()->IsActiveUserSessionStarted()) {
    return false;
  }

  PrefService* prefs =
      Shell::Get()->session_controller()->GetActivePrefService();
  if (!prefs) {
    return false;
  }

  const auto& imposters = prefs->GetList(prefs::kMouseDeviceImpostersListPref);
  const std::string device_key = BuildDeviceKey(device);
  return base::Contains(imposters, device_key);
}

// Saves `imposter_false_positives_to_add` to the known list of imposters in
// prefs. Clears the list if it successfully adds the devices to prefs.
void SaveKeyboardsToImposterPref(
    base::flat_set<std::string>& imposter_false_positives_to_add) {
  if (!Shell::Get()->session_controller()->IsActiveUserSessionStarted()) {
    return;
  }

  PrefService* prefs =
      Shell::Get()->session_controller()->GetActivePrefService();
  if (!prefs) {
    return;
  }

  auto updated_imposters =
      prefs->GetList(prefs::kKeyboardDeviceImpostersListPref).Clone();
  for (const auto& device_key : imposter_false_positives_to_add) {
    if (base::Contains(updated_imposters, device_key)) {
      continue;
    }

    updated_imposters.Append(device_key);
  }

  prefs->SetList(prefs::kKeyboardDeviceImpostersListPref,
                 std::move(updated_imposters));
  imposter_false_positives_to_add.clear();
}

// Saves `imposter_false_positives_to_add` to the known list of mouse imposters
// in prefs. Clears the list if it successfully adds the devices to prefs.
void SaveMiceToImposterPref(
    base::flat_set<std::string>& imposter_false_positives_to_add) {
  if (!Shell::Get()->session_controller()->IsActiveUserSessionStarted()) {
    return;
  }

  PrefService* prefs =
      Shell::Get()->session_controller()->GetActivePrefService();
  if (!prefs) {
    return;
  }

  auto updated_imposters =
      prefs->GetList(prefs::kMouseDeviceImpostersListPref).Clone();
  for (const auto& device_key : imposter_false_positives_to_add) {
    if (base::Contains(updated_imposters, device_key)) {
      continue;
    }

    updated_imposters.Append(device_key);
  }

  prefs->SetList(prefs::kMouseDeviceImpostersListPref,
                 std::move(updated_imposters));
  imposter_false_positives_to_add.clear();
}

template <>
bool IsDeviceASuspectedImposter<mojom::KeyboardPtr>(
    BluetoothDevicesObserver* bluetooth_observer,
    const ui::InputDevice& device) {
  if (AreOnLoginScreen()) {
    return false;
  }

  // If the device type is keyboard or keyboard mouse combo, it should not be
  // considered an imposter.
  const auto device_type = GetDeviceType(device);
  switch (device_type) {
    case DeviceType::kKeyboard:
    case DeviceType::kKeyboardMouseCombo:
      return false;
    case DeviceType::kMouse:
      return true;
    case DeviceType::kUnknown:
      break;
  }

  if (IsKeyboardAKnownImposterFalsePositive(device)) {
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

  return device.suspected_keyboard_imposter;
}

template <>
bool IsDeviceASuspectedImposter<mojom::MousePtr>(
    BluetoothDevicesObserver* bluetooth_observer,
    const ui::InputDevice& device) {
  if (AreOnLoginScreen()) {
    return false;
  }

  // If the device type is keyboard, the device should
  // always be considered an imposter.
  const auto device_type = GetDeviceType(device);
  switch (device_type) {
    case DeviceType::kKeyboard:
      return true;
    case DeviceType::kKeyboardMouseCombo:
    case DeviceType::kMouse:
      return false;
    case DeviceType::kUnknown:
      break;
  }

  if (IsMouseAKnownImposterFalsePositive(device)) {
    return false;
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

  if (!features::IsMouseImposterCheckEnabled()) {
    return false;
  }

  return device.suspected_mouse_imposter;
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
  std::erase_if(updated_device_list, [&](const ui::InputDevice& device) {
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
                               /*Proj1=*/std::identity(),
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
  Shell::Get()->session_controller()->AddObserver(this);
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
  Shell::Get()->session_controller()->RemoveObserver(this);
}

template <typename MojomDevicePtr, typename InputDeviceType>
void InputDeviceNotifier<MojomDevicePtr, InputDeviceType>::RefreshDevices() {
  std::vector<InputDeviceType> devices_to_add;
  std::vector<DeviceId> device_ids_to_remove;

  std::vector<InputDeviceType> updated_device_list = GetUpdatedDeviceList();
  HandleImposterPref(updated_device_list);
  GetAddedAndRemovedDevices(bluetooth_devices_observer_.get(),
                            updated_device_list, *connected_devices_,
                            &devices_to_add, &device_ids_to_remove);

  device_lists_updated_callback_.Run(std::move(devices_to_add),
                                     std::move(device_ids_to_remove));
}

template <typename MojomDevicePtr, typename InputDeviceType>
void InputDeviceNotifier<MojomDevicePtr, InputDeviceType>::HandleImposterPref(
    const std::vector<InputDeviceType>& updated_device_list) {
  return;
}

template <>
void InputDeviceNotifier<mojom::MousePtr, ui::InputDevice>::HandleImposterPref(
    const std::vector<ui::InputDevice>& updated_device_list) {
  if (!features::IsMouseImposterCheckEnabled()) {
    return;
  }

  // Use a temporary set to store the device ids of imposter devices so devices
  // get removed upon device disconnect.
  base::flat_set<DeviceId> updated_imposter_devices;
  for (const ui::InputDevice& device : updated_device_list) {
    if (device.suspected_mouse_imposter) {
      updated_imposter_devices.insert(device.id);
      continue;
    }

    // If the device is no longer an imposter and once was (which means it was
    // in `mouse_imposter_devices_`) add it to our list of device keys to add to
    // the known imposter list.
    if (mouse_imposter_devices_.contains(device.id)) {
      mouse_imposter_false_positives_to_add_.insert(BuildDeviceKey(device));
    }
  }
  mouse_imposter_devices_ = std::move(updated_imposter_devices);

  // Always try to add additional devices to the imposter pref list.
  SaveMiceToImposterPref(mouse_imposter_false_positives_to_add_);
}

template <>
void InputDeviceNotifier<mojom::KeyboardPtr, ui::KeyboardDevice>::
    HandleImposterPref(
        const std::vector<ui::KeyboardDevice>& updated_device_list) {
  // Use a temporary set to store the device ids of imposter devices so devices
  // get removed upon device disconnect.
  base::flat_set<DeviceId> updated_imposter_devices;
  for (const ui::KeyboardDevice& device : updated_device_list) {
    if (device.suspected_keyboard_imposter) {
      updated_imposter_devices.insert(device.id);
      continue;
    }

    // If the device is no longer an imposter and once was (which means it was
    // in `keyboard_imposter_devices_`) add it to our list of device keys to add
    // to the known imposter list.
    if (keyboard_imposter_devices_.contains(device.id)) {
      keyboard_imposter_false_positives_to_add_.insert(BuildDeviceKey(device));
    }
  }
  keyboard_imposter_devices_ = std::move(updated_imposter_devices);

  // Always try to add additional devices to the imposter pref list.
  SaveKeyboardsToImposterPref(keyboard_imposter_false_positives_to_add_);
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
void InputDeviceNotifier<MojomDevicePtr, InputDeviceType>::OnLoginStatusChanged(
    LoginStatus login_status) {
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
  std::erase_if(mice, [](const auto& mouse) {
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

template <>
std::vector<ui::InputDevice>
InputDeviceNotifier<mojom::GraphicsTabletPtr,
                    ui::InputDevice>::GetUpdatedDeviceList() {
  DCHECK(ash::features::IsPeripheralCustomizationEnabled());
  return ui::DeviceDataManager::GetInstance()->GetGraphicsTabletDevices();
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
template class EXPORT_TEMPLATE_DECLARE(ASH_EXPORT)
    InputDeviceNotifier<mojom::GraphicsTabletPtr, ui::InputDevice>;

}  // namespace ash
