// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/fwupd/histogram_util.h"

#include <string>

#include "base/metrics/histogram_functions.h"

namespace {

const char kHistogramName[] = "ChromeOS.FirmwareUpdateUi.";

}  // namespace
namespace ash {
namespace firmware_update {
namespace metrics {
void EmitDeviceCount(int num_devices, bool is_startup) {
  base::UmaHistogramCounts100(
      GetSourceStr(is_startup) + std::string(".DeviceCount"), num_devices);
}

std::string GetSourceStr(bool is_startup) {
  return std::string(kHistogramName) +
         std::string(is_startup ? "OnStartup" : "OnRefresh");
}
}  // namespace metrics
}  // namespace firmware_update
}  // namespace ash
