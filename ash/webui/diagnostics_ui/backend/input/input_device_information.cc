// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/input/input_device_information.h"

#include <fcntl.h>

#include "ash/webui/diagnostics_ui/backend/input/input_data_provider.h"
#include "ui/chromeos/events/event_rewriter_chromeos.h"
#include "ui/events/devices/device_util_linux.h"
#include "ui/events/devices/input_device.h"
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

  info->path = path;
  info->evdev_id = id;
  info->connection_type = InputDataProvider::ConnectionTypeFromInputDeviceType(
      info->event_device_info.device_type());
  info->input_device = ui::InputDevice(
      id, info->event_device_info.device_type(), info->event_device_info.name(),
      info->event_device_info.phys(), sys_path,
      info->event_device_info.vendor_id(), info->event_device_info.product_id(),
      info->event_device_info.version());

  if (info->event_device_info.HasKeyboard()) {
    ui::EventRewriterChromeOS::IdentifyKeyboard(
        info->input_device, &info->keyboard_type,
        &info->keyboard_top_row_layout, &info->keyboard_scan_code_map);
  }

  return info;
}

}  // namespace ash::diagnostics
