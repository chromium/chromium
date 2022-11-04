// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_SYSTEM_CROS_HEALTHD_HELPERS_H_
#define ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_SYSTEM_CROS_HEALTHD_HELPERS_H_

#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_diagnostics.mojom-forward.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom-forward.h"

namespace ash::diagnostics {

// Extracts BatteryInfo from |info|. Logs and returns a nullptr if
// BatteryInfo in not present.
const cros_healthd::mojom::BatteryInfo* GetBatteryInfo(
    const cros_healthd::mojom::TelemetryInfo& info);

// Extracts CpuInfo from |info|. Logs and returns a nullptr if CpuInfo
// in not present.
const cros_healthd::mojom::CpuInfo* GetCpuInfo(
    const cros_healthd::mojom::TelemetryInfo& info);

// Extracts MemoryInfo from |info|. Logs and returns a nullptr if MemoryInfo
// in not present.
const cros_healthd::mojom::MemoryInfo* GetMemoryInfo(
    const cros_healthd::mojom::TelemetryInfo& info);

// Extracts SystemInfo from |info|. Logs and returns a nullptr if SystemInfo
// in not present.
const cros_healthd::mojom::SystemInfo* GetSystemInfo(
    const cros_healthd::mojom::TelemetryInfo& info);

const cros_healthd::mojom::NonInteractiveRoutineUpdate*
GetNonInteractiveRoutineUpdate(
    const cros_healthd::mojom::RoutineUpdate& update);

}  // namespace ash::diagnostics

#endif  // ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_SYSTEM_CROS_HEALTHD_HELPERS_H_
