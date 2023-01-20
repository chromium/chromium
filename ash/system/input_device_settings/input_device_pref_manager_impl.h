// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_PREF_MANAGER_IMPL_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_PREF_MANAGER_IMPL_H_

#include "ash/ash_export.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/system/input_device_settings/input_device_pref_manager.h"
#include "ui/events/devices/input_device.h"

namespace ash {

class ASH_EXPORT InputDevicePrefManagerImpl : public InputDevicePrefManager {
 public:
  InputDevicePrefManagerImpl();
  InputDevicePrefManagerImpl(const InputDevicePrefManagerImpl&) = delete;
  InputDevicePrefManagerImpl& operator=(const InputDevicePrefManagerImpl&) =
      delete;
  ~InputDevicePrefManagerImpl() override;

  // Builds `device_key` for use in storing device settings in prefs.
  static std::string BuildDeviceKey(const ui::InputDevice& device);

  // InputDevicePrefManager:
  void InitializeKeyboardSettings(mojom::Keyboard* keyboard) override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_PREF_MANAGER_IMPL_H_
