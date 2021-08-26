// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_TELEMETRY_EXTENSION_UI_SERVICES_DIAGNOSTICS_SERVICE_CONVERTERS_H_
#define ASH_WEBUI_TELEMETRY_EXTENSION_UI_SERVICES_DIAGNOSTICS_SERVICE_CONVERTERS_H_

#include <string>
#include <utility>
#include <vector>

#include "ash/webui/telemetry_extension_ui/mojom/diagnostics_service.mojom-forward.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_diagnostics.mojom.h"
#include "mojo/public/cpp/system/handle.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {
namespace converters {

// This file contains helper functions used by DiagnosticsService to convert its
// types to/from cros_healthd DiagnosticsService types.

namespace unchecked {

ash::health::mojom::RoutineUpdatePtr UncheckedConvertPtr(
    cros_healthd::mojom::RoutineUpdatePtr input);

ash::health::mojom::RoutineUpdateUnionPtr UncheckedConvertPtr(
    cros_healthd::mojom::RoutineUpdateUnionPtr input);

ash::health::mojom::InteractiveRoutineUpdatePtr UncheckedConvertPtr(
    cros_healthd::mojom::InteractiveRoutineUpdatePtr input);

ash::health::mojom::NonInteractiveRoutineUpdatePtr UncheckedConvertPtr(
    cros_healthd::mojom::NonInteractiveRoutineUpdatePtr input);

ash::health::mojom::RunRoutineResponsePtr UncheckedConvertPtr(
    cros_healthd::mojom::RunRoutineResponsePtr input);

}  // namespace unchecked

absl::optional<ash::health::mojom::DiagnosticRoutineEnum> Convert(
    cros_healthd::mojom::DiagnosticRoutineEnum input);

std::vector<ash::health::mojom::DiagnosticRoutineEnum> Convert(
    const std::vector<cros_healthd::mojom::DiagnosticRoutineEnum>& input);

ash::health::mojom::DiagnosticRoutineUserMessageEnum Convert(
    cros_healthd::mojom::DiagnosticRoutineUserMessageEnum input);

ash::health::mojom::DiagnosticRoutineStatusEnum Convert(
    cros_healthd::mojom::DiagnosticRoutineStatusEnum input);

cros_healthd::mojom::DiagnosticRoutineCommandEnum Convert(
    ash::health::mojom::DiagnosticRoutineCommandEnum input);

cros_healthd::mojom::AcPowerStatusEnum Convert(
    ash::health::mojom::AcPowerStatusEnum input);

cros_healthd::mojom::NvmeSelfTestTypeEnum Convert(
    ash::health::mojom::NvmeSelfTestTypeEnum input);

cros_healthd::mojom::DiskReadRoutineTypeEnum Convert(
    ash::health::mojom::DiskReadRoutineTypeEnum input);

}  // namespace converters
}  // namespace chromeos

#endif  // ASH_WEBUI_TELEMETRY_EXTENSION_UI_SERVICES_DIAGNOSTICS_SERVICE_CONVERTERS_H_
