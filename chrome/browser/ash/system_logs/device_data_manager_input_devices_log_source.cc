// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_logs/device_data_manager_input_devices_log_source.h"

#include "ash/shell.h"
#include "base/containers/flat_map.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/touch_device_manager.h"
#include "ui/events/devices/device_data_manager.h"

namespace system_logs {

namespace {

constexpr char kUiDeviceDataManagerDevicesLogEntry[] =
    "ui_device_data_manager_devices";
constexpr char kUiDeviceDataManagerDeviceCountsLogEntry[] =
    "ui_device_data_manager_device_counts";
constexpr char kDeviceDataManagerDevicePrefix[] = "devicedatamanager_";

constexpr std::string FormatTypeLower(ui::InputDeviceType value) {
  switch (value) {
    case ui::INPUT_DEVICE_INTERNAL:
      return "internal";
    case ui::INPUT_DEVICE_USB:
      return "usb";
    case ui::INPUT_DEVICE_BLUETOOTH:
      return "bluetooth";
    case ui::INPUT_DEVICE_UNKNOWN:
      return "unknown";
  }
  NOTREACHED();
}

constexpr ui::InputDeviceType kDeviceTypeList[] = {
    ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
    ui::InputDeviceType::INPUT_DEVICE_USB,
    ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH,
    ui::InputDeviceType::INPUT_DEVICE_UNKNOWN};

enum DeviceCategory {
  kTouchscreenDevices = 1,
  kKeyboardDevices,
  kMouseDevices,
  kTouchpadDevices,
  kUncategorizedDevices
};

constexpr DeviceCategory kDeviceCategoryList[] = {
    kTouchscreenDevices, kKeyboardDevices, kMouseDevices, kTouchpadDevices,
    kUncategorizedDevices};

std::ostream& operator<<(std::ostream& os, DeviceCategory value) {
  switch (value) {
    case kTouchscreenDevices:
      return os << "touchscreen";
    case kKeyboardDevices:
      return os << "keyboard";
    case kMouseDevices:
      return os << "mouse";
    case kTouchpadDevices:
      return os << "touchpad";
    case kUncategorizedDevices:
      return os << "uncategorized";
  }
  NOTREACHED();
}

std::string DescribeDisplayCalibrationPoint(
    std::pair<gfx::Point, gfx::Point> value) {
  return base::StringPrintf("[%d,%d]->[%d,%d]", value.first.x(),
                            value.first.y(), value.second.x(),
                            value.second.y());
}

display::DisplayManager* GetDisplayManager() {
  return ash::Shell::HasInstance() ? ash::Shell::Get()->display_manager()
                                   : nullptr;
}

// Lists the target display's id, if this touch device is mapped to one,
// and the calibration data, if there is any.
void DescribeDisplayInformation(ui::TouchscreenDevice* dev, std::ostream& str) {
  // TODO(b/265986652): ensure there is enough info logged to make sense of
  // display IDs.
  str << " target_display_id=";
  if (dev->target_display_id == display::kInvalidDisplayId) {
    str << "kInvalidDisplayId";
  } else {
    str << dev->target_display_id;
  }
  str << std::endl;

  auto touch_id = display::TouchDeviceIdentifier::FromDevice(*dev);
  str << " touch_device_manager_id=" << touch_id.ToString() << ":"
      << touch_id.SecondaryIdToString() << std::endl;

  auto* display_manager = GetDisplayManager();
  if (!display_manager) {
    str << " no display manager" << std::endl;
    return;
  }

  auto* touch_display_manager = display_manager->touch_device_manager();
  if (!touch_display_manager) {
    str << " no touch device manager" << std::endl;
    return;
  }

  auto calibration_data = touch_display_manager->GetCalibrationData(*dev);
  if (!calibration_data.IsEmpty()) {
    str << " display->touch calibration={"
        << DescribeDisplayCalibrationPoint(calibration_data.point_pairs[0])
        << ", "
        << DescribeDisplayCalibrationPoint(calibration_data.point_pairs[1])
        << ", "
        << DescribeDisplayCalibrationPoint(calibration_data.point_pairs[2])
        << ", "
        << DescribeDisplayCalibrationPoint(calibration_data.point_pairs[3])
        << "}" << std::endl
        << " touch calibration bounds=" << calibration_data.bounds.ToString()
        << std::endl;
  } else {
    str << " no touch calibration" << std::endl;
  }
}

void DescribeAndCountAllInputDevices(ui::DeviceDataManager* device_data_manager,
                                     SystemLogsResponse* response) {
  std::stringstream str;

  DCHECK(response);

  if (!device_data_manager) {
    constexpr char kFailureMessage[] = "No DeviceDataManager instance";
    response->emplace(kUiDeviceDataManagerDevicesLogEntry, kFailureMessage);
    response->emplace(kUiDeviceDataManagerDeviceCountsLogEntry,
                      kFailureMessage);
    return;
  }

  str << "AreDeviceListsComplete="
      << device_data_manager->AreDeviceListsComplete() << std::endl
      << "AreTouchscreensEnabled="
      << device_data_manager->AreTouchscreensEnabled() << std::endl
      << "AreTouchscreenTargetDisplaysValid="
      << device_data_manager->AreTouchscreenTargetDisplaysValid() << std::endl
      << std::endl;

  base::flat_map<ui::InputDeviceType,
                 base::flat_map<DeviceCategory, std::uint32_t>>
      count;

  // Note that the same device may belong to more than one category, can
  // be shown more than once, and will be listed in effectively arbitrary order.

  for (auto device : device_data_manager->GetTouchscreenDevices()) {
    str << kDeviceDataManagerDevicePrefix << kTouchscreenDevices << ": ";
    device.DescribeForLog(str);
    DescribeDisplayInformation(&device, str);
    str << std::endl;
    count[device.type][kTouchscreenDevices]++;
  }

  for (auto device : device_data_manager->GetKeyboardDevices()) {
    str << kDeviceDataManagerDevicePrefix << kKeyboardDevices << ": ";
    device.DescribeForLog(str);
    str << std::endl;
    count[device.type][kKeyboardDevices]++;
  }

  for (auto device : device_data_manager->GetMouseDevices()) {
    str << kDeviceDataManagerDevicePrefix << kMouseDevices << ": ";
    device.DescribeForLog(str);
    str << std::endl;
    count[device.type][kMouseDevices]++;
  }

  for (auto device : device_data_manager->GetTouchpadDevices()) {
    str << kDeviceDataManagerDevicePrefix << kTouchpadDevices << ": ";
    device.DescribeForLog(str);
    str << std::endl;
    count[device.type][kTouchpadDevices]++;
  }

  for (auto device : device_data_manager->GetUncategorizedDevices()) {
    str << kDeviceDataManagerDevicePrefix << kUncategorizedDevices << ": ";
    device.DescribeForLog(str);
    str << std::endl;
    count[device.type][kUncategorizedDevices]++;
  }

  response->emplace(kUiDeviceDataManagerDevicesLogEntry, str.str());

  str = std::stringstream();

  // Emit counts for each possible combination of device type and connection
  // category, including those that are zero.
  for (auto type : kDeviceTypeList) {
    for (auto category : kDeviceCategoryList) {
      str << "count_" << FormatTypeLower(type) << "_" << category
          << "_devices=" << count[type][category] << std::endl;
    }
  }

  response->emplace(kUiDeviceDataManagerDeviceCountsLogEntry, str.str());
}

}  // namespace

DeviceDataManagerInputDevicesLogSource::DeviceDataManagerInputDevicesLogSource()
    : SystemLogsSource("DeviceDataManagerInput") {}

DeviceDataManagerInputDevicesLogSource::
    ~DeviceDataManagerInputDevicesLogSource() = default;

void DeviceDataManagerInputDevicesLogSource::Fetch(
    SysLogsSourceCallback callback) {
  DCHECK(!callback.is_null());

  auto response = std::make_unique<SystemLogsResponse>();
  ui::DeviceDataManager* device_data_manager =
      ui::DeviceDataManager::HasInstance()
          ? ui::DeviceDataManager::GetInstance()
          : nullptr;

  DescribeAndCountAllInputDevices(device_data_manager, response.get());

  std::move(callback).Run(std::move(response));
}

}  // namespace system_logs
