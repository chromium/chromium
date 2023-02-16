// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_DISPATCHER_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_DISPATCHER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/input_device_settings_controller.h"

namespace ash {

class ASH_EXPORT InputDeviceSettingsDispatcher
    : InputDeviceSettingsController::Observer {
 public:
  InputDeviceSettingsDispatcher();
  InputDeviceSettingsDispatcher(const InputDeviceSettingsDispatcher&) = delete;
  InputDeviceSettingsDispatcher& operator=(
      const InputDeviceSettingsDispatcher&) = delete;
  ~InputDeviceSettingsDispatcher() override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_DISPATCHER_H_
