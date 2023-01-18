// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_NOTIFIER_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_NOTIFIER_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/input_device_settings_controller.h"
#include "ash/public/mojom/input_device_settings.mojom-forward.h"
#include "base/containers/flat_map.h"
#include "base/export_template.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/input_device_event_observer.h"

namespace ash {

// Calls the given callback every time a device is connected or removed with
// lists of which were added or removed.
template <typename MojomDevicePtr>
class ASH_EXPORT InputDeviceNotifier : public ui::InputDeviceEventObserver {
 public:
  using DeviceId = InputDeviceSettingsController::DeviceId;
  using InputDeviceListsUpdatedCallback =
      base::RepeatingCallback<void(std::vector<ui::InputDevice>,
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

 private:
  void RefreshDevices();

  std::vector<ui::InputDevice> GetUpdatedDeviceList();

  // `connected_devices_` is owned by `InputDeviceSettingsControllerImpl` which
  // instantiates the `InputDeviceNotifier` as a member. `connected_devices_`
  // will always outlive `InputDeviceNotifier`.
  base::raw_ptr<base::flat_map<DeviceId, MojomDevicePtr>> connected_devices_;
  InputDeviceListsUpdatedCallback device_lists_updated_callback_;
};

// Below explicit template instantiations needed for all supported types.
template <>
ASH_EXPORT std::vector<ui::InputDevice>
InputDeviceNotifier<mojom::KeyboardPtr>::GetUpdatedDeviceList();

extern template class EXPORT_TEMPLATE_DECLARE(ASH_EXPORT)
    InputDeviceNotifier<mojom::KeyboardPtr>;

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_NOTIFIER_H_
