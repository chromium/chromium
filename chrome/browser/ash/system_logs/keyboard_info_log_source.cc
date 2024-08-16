// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_logs/keyboard_info_log_source.h"

#include "ash/shell.h"
#include "base/notreached.h"
#include "ui/events/ash/keyboard_capability.h"
#include "ui/events/devices/device_data_manager.h"

namespace system_logs {

namespace {

using DeviceType = ui::KeyboardCapability::DeviceType;
using KeyboardTopRowLayout = ui::KeyboardCapability::KeyboardTopRowLayout;

const char* GetDeviceTypeString(DeviceType device_type) {
  switch (device_type) {
    case DeviceType::kDeviceUnknown:
      return "Unknown";
    case DeviceType::kDeviceInternalKeyboard:
      return "Internal Keyboard";
    case DeviceType::kDeviceInternalRevenKeyboard:
      return "Internal Reven Keyboard";
    case DeviceType::kDeviceExternalAppleKeyboard:
      return "External Apple Keyboard";
    case DeviceType::kDeviceExternalChromeOsKeyboard:
      return "External ChromeOs Keyboard";
    case DeviceType::kDeviceExternalNullTopRowChromeOsKeyboard:
      return "External Null Top Row ChromeOs Keyboard";
    case DeviceType::kDeviceExternalGenericKeyboard:
      return "External Generic Keyboard";
    case DeviceType::kDeviceExternalUnknown:
      return "External Unknown";
    case DeviceType::kDeviceHotrodRemote:
      return "Hotrod Remote";
    case DeviceType::kDeviceVirtualCoreKeyboard:
      return "Virtual Core Keyboard";
    default:
      NOTREACHED();
  }
}

const char* GetTopRowLayoutString(KeyboardTopRowLayout top_row_layout) {
  switch (top_row_layout) {
    case KeyboardTopRowLayout::kKbdTopRowLayout1:
      return "Layout1";
    case KeyboardTopRowLayout::kKbdTopRowLayout2:
      return "Layout2";
    case KeyboardTopRowLayout::kKbdTopRowLayoutWilco:
      return "Wilco";
    case KeyboardTopRowLayout::kKbdTopRowLayoutDrallion:
      return "Drallion";
    case KeyboardTopRowLayout::kKbdTopRowLayoutCustom:
      return "Custom";
  }
}

}  // namespace

KeyboardInfoLogSource::KeyboardInfoLogSource()
    : SystemLogsSource("KeyboardInfo") {}

void KeyboardInfoLogSource::Fetch(SysLogsSourceCallback callback) {
  CHECK(!callback.is_null());

  auto response = std::make_unique<SystemLogsResponse>();

  ui::KeyboardCapability* keyboard_capability =
      ash::Shell::Get()->keyboard_capability();
  const auto& keyboards =
      ui::DeviceDataManager::GetInstance()->GetKeyboardDevices();
  std::ostringstream output;

  for (uint index = 0; index < keyboards.size(); ++index) {
    const auto keyboard = keyboards[index];
    // Print a new line before each keyboard info if it's not the first one.
    if (index != 0) {
      output << "\n";
    }

    const auto device_type = keyboard_capability->GetDeviceType(keyboard);
    const auto top_row_layout = keyboard_capability->GetTopRowLayout(keyboard);
    const auto* top_row_action_keys =
        keyboard_capability->GetTopRowActionKeys(keyboard);
    const auto* top_row_scan_code =
        keyboard_capability->GetTopRowScanCodes(keyboard);

    output << "keyboard name: " << keyboard.name << "\n";
    output << "device type: " << GetDeviceTypeString(device_type) << "\n";
    output << "top_row_layout: " << GetTopRowLayoutString(top_row_layout)
           << "\n";
    output << "top_row_action_keys: ";
    if (top_row_action_keys != nullptr) {
      for (const auto action_key : *top_row_action_keys) {
        output << static_cast<uint32_t>(action_key) << " ";
      }
    }
    output << "\n";
    output << "top_row_scan_code: ";
    if (top_row_scan_code != nullptr) {
      for (const auto code : *top_row_scan_code) {
        output << code << " ";
      }
    }
    output << "\n";
  }

  response->emplace("keyboard_info", output.str());

  std::move(callback).Run(std::move(response));
}

}  // namespace system_logs
