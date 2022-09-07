// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/power/hid_battery_util.h"

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "third_party/re2/src/re2/re2.h"

namespace ash {

namespace {

// HID device's battery sysfs entry path looks like
// /sys/class/power_supply/hid-{AA:BB:CC:DD:EE:FF|AAAA:BBBB:CCCC.DDDD}-battery.
constexpr char kHIDBatteryPrefix[] = "/sys/class/power_supply/hid-";
constexpr char kHIDBatterySuffix[] = "-battery";
constexpr size_t kPrefixLen = std::size(kHIDBatteryPrefix) - 1;
constexpr size_t kPrefixAndSuffixLen =
    (std::size(kHIDBatteryPrefix) - 1) + (std::size(kHIDBatterySuffix) - 1);

// Regex to check for valid bluetooth addresses.
constexpr char kBluetoothAddressRegex[] =
    "^([0-9A-Fa-f]{2}:){5}([0-9A-Fa-f]{2})$";

}  // namespace

bool IsHIDBattery(const std::string& path) {
  if (!base::StartsWith(path, kHIDBatteryPrefix,
                        base::CompareCase::INSENSITIVE_ASCII) ||
      !base::EndsWith(path, kHIDBatterySuffix,
                      base::CompareCase::INSENSITIVE_ASCII)) {
    return false;
  }

  return path.size() > kPrefixAndSuffixLen;
}

std::string ExtractHIDBatteryIdentifier(const std::string& path) {
  if (path.size() <= kPrefixAndSuffixLen)
    return std::string();

  size_t id_len = path.size() - kPrefixAndSuffixLen;
  return path.substr(kPrefixLen, id_len);
}

std::string ExtractBluetoothAddressFromHIDBatteryPath(const std::string& path) {
  if (!IsHIDBattery(path))
    return std::string();

  std::string identifier = ExtractHIDBatteryIdentifier(path);
  if (!RE2::FullMatch(identifier, kBluetoothAddressRegex))
    return std::string();

  return base::ToLowerASCII(identifier);
}

}  // namespace ash
