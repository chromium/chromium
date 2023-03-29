// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_POLICY_HANDLER_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_POLICY_HANDLER_H_

#include "ash/ash_export.h"

namespace ash {

class ASH_EXPORT InputDeviceSettingsPolicyHandler {
 public:
  InputDeviceSettingsPolicyHandler();
  InputDeviceSettingsPolicyHandler(const InputDeviceSettingsPolicyHandler&) =
      delete;
  InputDeviceSettingsPolicyHandler& operator=(
      const InputDeviceSettingsPolicyHandler&) = delete;
  ~InputDeviceSettingsPolicyHandler();
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_POLICY_HANDLER_H_
