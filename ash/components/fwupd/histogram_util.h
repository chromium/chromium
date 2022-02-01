// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_FWUPD_HISTOGRAM_UTIL_H_
#define ASH_COMPONENTS_FWUPD_HISTOGRAM_UTIL_H_

#include <cstdint>
#include <string>

namespace ash {
namespace firmware_update {
namespace metrics {

void EmitDeviceCount(int num_devices, bool is_startup);

void EmitUpdateCount(int num_updates,
                     int num_critical_updates,
                     bool is_startup);

std::string GetSourceStr(bool is_startup);

}  // namespace metrics
}  // namespace firmware_update
}  // namespace ash

#endif  // ASH_COMPONENTS_FWUPD_HISTOGRAM_UTIL_H_
