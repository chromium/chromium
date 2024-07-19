// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_DIAGNOSTICS_DIAGNOSTICS_API_CONVERTERS_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_DIAGNOSTICS_DIAGNOSTICS_API_CONVERTERS_H_

#include <optional>

#include "chrome/common/chromeos/extensions/api/diagnostics.h"
#include "chromeos/crosapi/mojom/diagnostics_service.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"

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

crosapi::mojom::TelemetryDiagnosticVolumeButtonRoutineArgument::ButtonType
ConvertVolumeButtonRoutineButtonType(
    chromeos::api::os_diagnostics::VolumeButtonType volume_button_type);

crosapi::mojom::TelemetryDiagnosticLedName ConvertLedName(
    chromeos::api::os_diagnostics::LedName led_name);

crosapi::mojom::TelemetryDiagnosticLedColor ConvertLedColor(
    chromeos::api::os_diagnostics::LedColor led_color);

crosapi::mojom::TelemetryDiagnosticCheckLedLitUpStateReply::State
ConvertLedLitUpState(
    chromeos::api::os_diagnostics::LedLitUpState led_lit_up_state);

crosapi::mojom::TelemetryDiagnosticCheckKeyboardBacklightStateReply::State
ConvertKeyboardBacklightState(
    chromeos::api::os_diagnostics::KeyboardBacklightState
        keyboard_backlight_state);

// Converts the web IDL union to the Mojo union type. Returns std::nullopt when
// the conversion fails. Returns an `unrecognizedArgument` if all fields in
// `extension_union` are null to handle the case when extension is newer than
// the browser.
std::optional<crosapi::mojom::TelemetryDiagnosticRoutineArgumentPtr>
ConvertRoutineArgumentsUnion(
    chromeos::api::os_diagnostics::CreateRoutineArgumentsUnion extension_union);

// Converts the web IDL union to the Mojo union type. Returns std::nullopt when
// the conversion fails. Returns an `unrecognizedReply` if all fields in
// `extension_union` are null to handle the case when extension is newer than
// the browser.
std::optional<crosapi::mojom::TelemetryDiagnosticRoutineInquiryReplyPtr>
ConvertRoutineInquiryReplyUnion(
    chromeos::api::os_diagnostics::RoutineInquiryReplyUnion extension_union);

}  // namespace chromeos::converters::diagnostics

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_DIAGNOSTICS_DIAGNOSTICS_API_CONVERTERS_H_
