// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/on_device_controls/on_device_utils.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/strings/string_util.h"
#include "chromeos/ash/components/system/statistics_provider.h"

namespace {

constexpr char const* kOnDeviceControlsRegions[] = {
    "GF", "GP", "MF", "MQ", "RE", "YT", "FR",
    "BL", "NC", "PF", "PM", "TF", "WF"};

}  // namespace

namespace ash::on_device_controls {

std::string GetDeviceRegionCode() {
  const std::optional<std::string_view> region_code =
      system::StatisticsProvider::GetInstance()->GetMachineStatistic(
          system::kRegionKey);
  if (region_code) {
    std::string region_code_upper_case =
        base::ToUpperASCII(region_code.value());
    std::string region_upper_case =
        region_code_upper_case.substr(0, region_code_upper_case.find("."));
    return region_upper_case.length() == 2 ? region_upper_case : std::string();
  }

  return std::string();
}

bool IsOnDeviceControlsRegion(const std::string& region_code) {
  if (region_code.empty()) {
    return false;
  }

  for (const char* region : kOnDeviceControlsRegions) {
    if (region_code == region) {
      return true;
    }
  }

  return false;
}

}  // namespace ash::on_device_controls
