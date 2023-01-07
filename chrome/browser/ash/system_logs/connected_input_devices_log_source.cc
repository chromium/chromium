// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_logs/connected_input_devices_log_source.h"
#include "base/containers/fixed_flat_map.h"
#include "base/strings/stringprintf.h"
#include "ui/events/devices/device_data_manager.h"

namespace system_logs {

namespace {
constexpr auto vendor_map =
    base::MakeFixedFlatMap<uint16_t, base::StringPiece>({
        {0x03eb, "Atmel"},
        {0x0457, "Silicon Integrated Systems"},
        {0x04b4, "Cypress"},
        {0x04f3, "Elan"},
        {0x056a, "Wacom"},
        {0x0603, "Novatek"},
        {0x06cb, "Synaptics"},
        {0x093a, "Pixart"},
        {0x14e5, "Zinitix"},
        {0x18d1, "Google"},
        {0x1fd2, "Melfas"},
        {0x22c5, "Himax"},
        {0x2386, "Raydium"},
        {0x2575, "Weida"},
        {0x27c6, "Goodix"},
        {0x2a94, "G2 Touch"},
        {0x2c68, "EMRight"},
        {0x2d1f, "Wacom Taiwan"},
    });
}  // namespace

void ConnectedInputDevicesLogSource::ProcessDeviceFillResponse(
    const ui::InputDevice dev,
    SystemLogsResponse* response,
    const std::string& vendor_key,
    const std::string& pid_key) {
  DCHECK(response);
  if (dev.type != ui::InputDeviceType::INPUT_DEVICE_INTERNAL)
    return;
  auto* it = vendor_map.find(dev.vendor_id);
  std::string vendor_name;
  if (it != vendor_map.end())
    vendor_name.assign(static_cast<std::string>(it->second));
  else
    vendor_name = base::StringPrintf("%#06x", dev.vendor_id);
  response->emplace(vendor_key, vendor_name);
  response->emplace(pid_key, base::StringPrintf("%#06x", dev.product_id));
}

void ConnectedInputDevicesLogSource::Fetch(SysLogsSourceCallback callback) {
  DCHECK(!callback.is_null());

  auto response = std::make_unique<SystemLogsResponse>();

  if (ui::DeviceDataManager::HasInstance()) {
    auto* ddm = ui::DeviceDataManager::GetInstance();
    for (auto t : ddm->GetTouchpadDevices())
      ProcessDeviceFillResponse(t, response.get(), "TOUCHPAD_VENDOR",
                                "TOUCHPAD_PID");
    for (auto t : ddm->GetTouchscreenDevices())
      ProcessDeviceFillResponse(t, response.get(), "TOUCHSCREEN_VENDOR",
                                "TOUCHSCREEN_PID");
  }

  std::move(callback).Run(std::move(response));
}

}  // namespace system_logs
