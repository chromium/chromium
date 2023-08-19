// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_DIAGNOSTICS_DIAGNOSTICS_API_CONVERTERS_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_DIAGNOSTICS_DIAGNOSTICS_API_CONVERTERS_H_

#include "chrome/common/chromeos/extensions/api/diagnostics.h"
#include "chromeos/crosapi/mojom/diagnostics_service.mojom.h"

namespace chromeos::converters::diagnostics {

bool ConvertMojoRoutine(crosapi::mojom::DiagnosticsRoutineEnum in,
                        chromeos::api::os_diagnostics::RoutineType* out);

chromeos::api::os_diagnostics::RoutineStatus ConvertRoutineStatus(
    crosapi::mojom::DiagnosticsRoutineStatusEnum status);

crosapi::mojom::DiagnosticsRoutineCommandEnum ConvertRoutineCommand(
    chromeos::api::os_diagnostics::RoutineCommandType commandType);

crosapi::mojom::DiagnosticsAcPowerStatusEnum ConvertAcPowerStatusRoutineType(
    chromeos::api::os_diagnostics::AcPowerStatus routineType);

chromeos::api::os_diagnostics::UserMessageType ConvertRoutineUserMessage(
    crosapi::mojom::DiagnosticsRoutineUserMessageEnum userMessage);

crosapi::mojom::DiagnosticsDiskReadRoutineTypeEnum ConvertDiskReadRoutineType(
    chromeos::api::os_diagnostics::DiskReadRoutineType routineType);

crosapi::mojom::DiagnosticsNvmeSelfTestTypeEnum ConvertNvmeSelfTestRoutineType(
    chromeos::api::os_diagnostics::RunNvmeSelfTestRequest routineType);

}  // namespace chromeos::converters::diagnostics

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_DIAGNOSTICS_DIAGNOSTICS_API_CONVERTERS_H_
