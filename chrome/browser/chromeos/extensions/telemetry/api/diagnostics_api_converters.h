// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_DIAGNOSTICS_API_CONVERTERS_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_DIAGNOSTICS_API_CONVERTERS_H_

#include "ash/webui/telemetry_extension_ui/mojom/diagnostics_service.mojom.h"
#include "chrome/common/chromeos/extensions/api/diagnostics.h"

namespace chromeos {
namespace converters {

bool ConvertMojoRoutine(ash::health::mojom::DiagnosticRoutineEnum in,
                        chromeos::api::os_diagnostics::RoutineType* out);

chromeos::api::os_diagnostics::RoutineStatus ConvertRoutineStatus(
    ash::health::mojom::DiagnosticRoutineStatusEnum status);

ash::health::mojom::DiagnosticRoutineCommandEnum ConvertRoutineCommand(
    chromeos::api::os_diagnostics::RoutineCommandType commandType);

chromeos::api::os_diagnostics::UserMessageType ConvertRoutineUserMessage(
    ash::health::mojom::DiagnosticRoutineUserMessageEnum userMessage);

ash::health::mojom::DiskReadRoutineTypeEnum ConvertDiskReadRoutineType(
    chromeos::api::os_diagnostics::DiskReadRoutineType routineType);

}  // namespace converters
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_DIAGNOSTICS_API_CONVERTERS_H_
