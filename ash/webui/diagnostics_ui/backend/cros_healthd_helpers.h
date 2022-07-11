// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_CROS_HEALTHD_HELPERS_H_
#define ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_CROS_HEALTHD_HELPERS_H_

#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_diagnostics.mojom-forward.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom-forward.h"

namespace ash {
namespace diagnostics {

// Extracts BatteryInfo from |info|. Logs and returns a nullptr if
// BatteryInfo in not present.
const chromeos::cros_healthd::mojom::BatteryInfo* GetBatteryInfo(
    const chromeos::cros_healthd::mojom::TelemetryInfo& info);

// Extracts CpuInfo from |info|. Logs and returns a nullptr if CpuInfo
// in not present.
const chromeos::cros_healthd::mojom::CpuInfo* GetCpuInfo(
    const chromeos::cros_healthd::mojom::TelemetryInfo& info);

// Extracts MemoryInfo from |info|. Logs and returns a nullptr if MemoryInfo
// in not present.
const chromeos::cros_healthd::mojom::MemoryInfo* GetMemoryInfo(
    const chromeos::cros_healthd::mojom::TelemetryInfo& info);

// Extracts SystemInfoV2 from |info|. Logs and returns a nullptr if SystemInfoV2
// in not present.
const chromeos::cros_healthd::mojom::SystemInfoV2* GetSystemInfo(
    const chromeos::cros_healthd::mojom::TelemetryInfo& info);

const chromeos::cros_healthd::mojom::NonInteractiveRoutineUpdate*
GetNonInteractiveRoutineUpdate(
    const chromeos::cros_healthd::mojom::RoutineUpdate& update);

}  // namespace diagnostics
}  // namespace ash

#endif  // ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_CROS_HEALTHD_HELPERS_H_
