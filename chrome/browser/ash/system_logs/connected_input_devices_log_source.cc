// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_logs/connected_input_devices_log_source.h"

#include <functional>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "base/containers/fixed_flat_map.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom-shared.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "connected_input_devices_log_source.h"
#include "ui/events/devices/device_data_manager.h"

namespace system_logs {

namespace {

namespace healthd = ::ash::cros_healthd::mojom;
using healthd::TelemetryInfo;
using healthd::TelemetryInfoPtr;
using ProbeCategories = healthd::ProbeCategoryEnum;

constexpr auto vendor_map = base::MakeFixedFlatMap<uint16_t, std::string_view>({
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

ConnectedInputDevicesLogSource::ConnectedInputDevicesLogSource()
    : SystemLogsSource("Input") {}

ConnectedInputDevicesLogSource::~ConnectedInputDevicesLogSource() = default;

void ConnectedInputDevicesLogSource::ProcessDeviceFillResponse(
    const ui::InputDevice dev,
    SystemLogsResponse* response,
    const std::string& vendor_key,
    const std::string& pid_key) {
  DCHECK(response);
  if (dev.type != ui::InputDeviceType::INPUT_DEVICE_INTERNAL)
    return;
  auto it = vendor_map.find(dev.vendor_id);
  std::string vendor_name;
  if (it != vendor_map.end())
    vendor_name.assign(static_cast<std::string>(it->second));
  else
    vendor_name = base::StringPrintf("0x%04x", dev.vendor_id);
  response->emplace(vendor_key, vendor_name);
  response->emplace(pid_key, base::StringPrintf("0x%04x", dev.product_id));
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

    bool has_internal_touchpads =
        (response->find("TOUCHPAD_VENDOR") != response->end()) ||
        (response->find("TOUCHPAD_PID") != response->end());

    if (has_internal_touchpads) {
      base::OnceCallback<void(const std::string&, const std::string&)>
          driver_cb = base::BindOnce(
              [](SysLogsSourceCallback sys_callback,
                 std::unique_ptr<SystemLogsResponse> response,
                 const std::string& driver_names,
                 const std::string& touchpad_library_name) {
                DCHECK(response);
                if (!driver_names.empty()) {
                  response->emplace("TOUCHPAD_DRIVERS", driver_names);
                }
                if (ash::switches::IsRevenBranding() &&
                    !touchpad_library_name.empty()) {
                  response->emplace("TOUCHPAD_LIBRARY", touchpad_library_name);
                }
                std::move(sys_callback).Run(std::move(response));
              },
              std::move(callback), std::move(response));

      GetCrosHealthdService()->ProbeTelemetryInfo(
          {ProbeCategories::kInput},
          base::BindOnce(
              &ConnectedInputDevicesLogSource::OnTelemetryInfoProbeResponse,
              weak_ptr_factory_.GetWeakPtr(), std::move(driver_cb)));
    } else {
      std::move(callback).Run(std::move(response));
    }
  }
}

ash::cros_healthd::mojom::CrosHealthdProbeService*
ConnectedInputDevicesLogSource::GetCrosHealthdService() {
  if (!probe_service_ || !probe_service_.is_connected()) {
    ash::cros_healthd::ServiceConnection::GetInstance()->BindProbeService(
        probe_service_.BindNewPipeAndPassReceiver());
    probe_service_.set_disconnect_handler(
        base::BindOnce(&ConnectedInputDevicesLogSource::OnDisconnect,
                       weak_ptr_factory_.GetWeakPtr()));
  }
  return probe_service_.get();
}

void ConnectedInputDevicesLogSource::OnDisconnect() {
  probe_service_.reset();
}

void ConnectedInputDevicesLogSource::OnTelemetryInfoProbeResponse(
    base::OnceCallback<void(const std::string&, const std::string&)> callback,
    TelemetryInfoPtr info_ptr) {
  std::vector<std::string> drivers = {};
  std::string touchpad_library;
  if (!info_ptr->input_result.is_null()) {
    const auto& input_info = info_ptr->input_result->get_input_info();

    if (ash::switches::IsRevenBranding()) {
      touchpad_library = input_info->touchpad_library_name;
    }

    for (const auto& touchpad_device : input_info->touchpad_devices.value()) {
      drivers.push_back(touchpad_device->driver_name);
    }
  } else {
    DVLOG(1) << "InputResult not found in croshealthd response";
  }

  std::move(callback).Run(base::JoinString(drivers, ","), touchpad_library);
}

}  // namespace system_logs
