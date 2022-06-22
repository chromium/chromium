// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_TELEMETRY_EXTENSION_UI_SERVICES_DIAGNOSTICS_SERVICE_CONVERTERS_H_
#define ASH_WEBUI_TELEMETRY_EXTENSION_UI_SERVICES_DIAGNOSTICS_SERVICE_CONVERTERS_H_

#include <string>
#include <utility>
#include <vector>

#include "ash/webui/telemetry_extension_ui/mojom/diagnostics_service.mojom-forward.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_diagnostics.mojom.h"
#include "mojo/public/cpp/system/handle.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

// TODO(https://crbug.com/1164001): Remove if cros_healthd::mojom moved to ash.
namespace cros_healthd {
namespace mojom = ::chromeos::cros_healthd::mojom;
}  // namespace cros_healthd

namespace converters {

// This file contains helper functions used by DiagnosticsService to convert its
// types to/from cros_healthd DiagnosticsService types.

namespace unchecked {

health::mojom::RoutineUpdatePtr UncheckedConvertPtr(
    cros_healthd::mojom::RoutineUpdatePtr input);

health::mojom::RoutineUpdateUnionPtr UncheckedConvertPtr(
    cros_healthd::mojom::RoutineUpdateUnionPtr input);

health::mojom::InteractiveRoutineUpdatePtr UncheckedConvertPtr(
    cros_healthd::mojom::InteractiveRoutineUpdatePtr input);

health::mojom::NonInteractiveRoutineUpdatePtr UncheckedConvertPtr(
    cros_healthd::mojom::NonInteractiveRoutineUpdatePtr input);

health::mojom::RunRoutineResponsePtr UncheckedConvertPtr(
    cros_healthd::mojom::RunRoutineResponsePtr input);

}  // namespace unchecked

absl::optional<health::mojom::DiagnosticRoutineEnum> Convert(
    cros_healthd::mojom::DiagnosticRoutineEnum input);

std::vector<health::mojom::DiagnosticRoutineEnum> Convert(
    const std::vector<cros_healthd::mojom::DiagnosticRoutineEnum>& input);

health::mojom::DiagnosticRoutineUserMessageEnum Convert(
    cros_healthd::mojom::DiagnosticRoutineUserMessageEnum input);

health::mojom::DiagnosticRoutineStatusEnum Convert(
    cros_healthd::mojom::DiagnosticRoutineStatusEnum input);

cros_healthd::mojom::DiagnosticRoutineCommandEnum Convert(
    health::mojom::DiagnosticRoutineCommandEnum input);

cros_healthd::mojom::AcPowerStatusEnum Convert(
    health::mojom::AcPowerStatusEnum input);

cros_healthd::mojom::NvmeSelfTestTypeEnum Convert(
    health::mojom::NvmeSelfTestTypeEnum input);

cros_healthd::mojom::DiskReadRoutineTypeEnum Convert(
    health::mojom::DiskReadRoutineTypeEnum input);

template <class InputT>
auto ConvertDiagnosticsPtr(InputT input) {
  return (!input.is_null()) ? unchecked::UncheckedConvertPtr(std::move(input))
                            : nullptr;
}

}  // namespace converters
}  // namespace ash

#endif  // ASH_WEBUI_TELEMETRY_EXTENSION_UI_SERVICES_DIAGNOSTICS_SERVICE_CONVERTERS_H_
