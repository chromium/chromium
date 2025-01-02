// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_INPUT_INPUT_DEVICE_INFORMATION_H_
#define ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_INPUT_INPUT_DEVICE_INFORMATION_H_

#include <vector>

#include "ash/webui/diagnostics_ui/mojom/input_data_provider.mojom.h"
#include "ui/events/ash/keyboard_capability.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/ozone/evdev/event_device_info.h"

namespace ash::diagnostics {

// Wrapper for tracking several pieces of information about an evdev-backed
// device.
class InputDeviceInformation {
 public:
  InputDeviceInformation();
  InputDeviceInformation(const InputDeviceInformation& other) = delete;
  InputDeviceInformation& operator=(const InputDeviceInformation& other) =
      delete;
  ~InputDeviceInformation();

  int evdev_id;
  ui::EventDeviceInfo event_device_info;
  ui::InputDevice input_device;
  mojom::ConnectionType connection_type;
  base::FilePath path;

  // Keyboard-only fields:
  ui::KeyboardCapability::DeviceType keyboard_type;
  ui::KeyboardCapability::KeyboardTopRowLayout keyboard_top_row_layout;
  std::vector<uint32_t> keyboard_scan_codes;
};

// Class for running GetDeviceInfo in its own sequence, to allow it to block.
class InputDeviceInfoHelper {
 public:
  virtual ~InputDeviceInfoHelper() = default;

  virtual std::unique_ptr<InputDeviceInformation> GetDeviceInfo(
      int evdev_id,
      base::FilePath path);
};

}  // namespace ash::diagnostics

#endif  // ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_INPUT_INPUT_DEVICE_INFORMATION_H_
