// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/devicetype.h"

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "chromeos/constants/devicetype.h"

namespace ash {
namespace {
constexpr char kDefaultDeviceTypeName[] = "Chromebook";
}  // namespace

std::string GetDeviceBluetoothName(const std::string& bluetooth_address) {
  const std::string device_name = DeviceTypeToString(chromeos::GetDeviceType());

  // Take the lower 2 bytes of hashed |bluetooth_address| and combine it with
  // the device type to create a more identifiable device name.
  return base::StringPrintf(
      "%s_%04X",
      device_name.empty() ? kDefaultDeviceTypeName : device_name.c_str(),
      base::PersistentHash(bluetooth_address) & 0xFFFF);
}

std::string DeviceTypeToString(chromeos::DeviceType device_type) {
  switch (device_type) {
    case chromeos::DeviceType::kChromebase:
      return "Chromebase";
    case chromeos::DeviceType::kChromebit:
      return "Chromebit";
    case chromeos::DeviceType::kChromebook:
      return "Chromebook";
    case chromeos::DeviceType::kChromebox:
      return "Chromebox";
    case chromeos::DeviceType::kUnknown:
    default:
      return "";
  }
}

bool IsGoogleBrandedDevice() {
  // Refer to comment describing base::SysInfo::GetLsbReleaseBoard for why
  // splitting the Lsb Release Board string is needed.
  std::vector<std::string> board =
      base::SplitString(base::SysInfo::GetLsbReleaseBoard(), "-",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (board.empty())
    return false;

  // TODO(crbug/966108): This method of determining board names will leak
  // hardware codenames.  Unreleased boards should NOT be included in this list.
  return board[0] == "nocturne" || board[0] == "eve" || board[0] == "atlas" ||
         board[0] == "samus";
}

}  // namespace ash
