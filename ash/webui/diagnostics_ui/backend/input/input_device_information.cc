// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/input/input_device_information.h"

#include <fcntl.h>
#include <linux/input-event-codes.h>

#include "ash/shell.h"
#include "ash/webui/diagnostics_ui/backend/input/input_data_provider.h"
#include "ui/events/ash/event_rewriter_ash.h"
#include "ui/events/ash/keyboard_capability.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/device_util_linux.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/keyboard_device.h"
#include "ui/events/ozone/evdev/event_device_info.h"

namespace ash::diagnostics {

InputDeviceInformation::InputDeviceInformation() = default;
InputDeviceInformation::~InputDeviceInformation() = default;

// All blockings calls for identifying hardware need to go here: both
// EventDeviceInfo::Initialize and ui::GetInputPathInSys can block in
// base::MakeAbsoluteFilePath.
std::unique_ptr<InputDeviceInformation> InputDeviceInfoHelper::GetDeviceInfo(
    int id,
    base::FilePath path) {
  base::ScopedFD fd(open(path.value().c_str(), O_RDWR | O_NONBLOCK));
  if (fd.get() < 0) {
    PLOG(ERROR) << "Couldn't open device path " << path << ".";
    return nullptr;
  }

  auto info = std::make_unique<InputDeviceInformation>();

  if (!info->event_device_info.Initialize(fd.get(), path)) {
    LOG(ERROR) << "Failed to get device info for " << path << ".";
    return nullptr;
  }

  const base::FilePath sys_path = ui::GetInputPathInSys(path);

  bool find_keyboard = false;
  int keyboard_id;
  const auto& keyboards =
      ui::DeviceDataManager::GetInstance()->GetKeyboardDevices();

  // Find the keyboard that correlates to the device name, vid and pid.
  for (const auto& keyboard : keyboards) {
    if (keyboard.name == info->event_device_info.name() &&
        keyboard.vendor_id == info->event_device_info.vendor_id() &&
        keyboard.product_id == info->event_device_info.product_id()) {
      keyboard_id = keyboard.id;
      find_keyboard = true;
    }
  }
  info->path = path;
  info->evdev_id = id;
  info->connection_type = InputDataProvider::ConnectionTypeFromInputDeviceType(
      info->event_device_info.device_type());
  info->input_device = ui::InputDevice(
      find_keyboard ? keyboard_id : id, info->event_device_info.device_type(),
      info->event_device_info.name(), info->event_device_info.phys(), sys_path,
      info->event_device_info.vendor_id(), info->event_device_info.product_id(),
      info->event_device_info.version());

  if (info->event_device_info.HasKeyboard()) {
    const ui::KeyboardDevice keyboard = ui::KeyboardDevice(info->input_device);
    const auto* keyboard_capability = Shell::Get()->keyboard_capability();
    info->keyboard_type = keyboard_capability->GetDeviceType(keyboard);
    info->keyboard_top_row_layout =
        keyboard_capability->GetTopRowLayout(keyboard);
    const auto* keyboard_scan_codes =
        keyboard_capability->GetTopRowScanCodes(keyboard);
    info->keyboard_scan_codes =
        keyboard_scan_codes ? *keyboard_scan_codes : std::vector<uint32_t>();
  }

  return info;
}

}  // namespace ash::diagnostics
