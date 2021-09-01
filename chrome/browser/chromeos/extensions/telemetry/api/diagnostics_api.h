// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_DIAGNOSTICS_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_DIAGNOSTICS_API_H_

#include "ash/webui/telemetry_extension_ui/mojom/diagnostics_service.mojom.h"
#include "ash/webui/telemetry_extension_ui/services/diagnostics_service.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

class DiagnosticsApiFunctionBase : public ExtensionFunction {
 public:
  DiagnosticsApiFunctionBase();

  DiagnosticsApiFunctionBase(const DiagnosticsApiFunctionBase&) = delete;
  DiagnosticsApiFunctionBase& operator=(const DiagnosticsApiFunctionBase&) =
      delete;

 protected:
  ~DiagnosticsApiFunctionBase() override;

  mojo::Remote<ash::health::mojom::DiagnosticsService>
      remote_diagnostics_service_;

 private:
  DiagnosticsService diagnostics_service_;
};

class OsDiagnosticsGetAvailableRoutinesFunction
    : public DiagnosticsApiFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.getAvailableRoutines",
                             OS_DIAGNOSTICS_GETAVAILABLEROUTINES)

  OsDiagnosticsGetAvailableRoutinesFunction();
  OsDiagnosticsGetAvailableRoutinesFunction(
      const OsDiagnosticsGetAvailableRoutinesFunction&) = delete;
  OsDiagnosticsGetAvailableRoutinesFunction& operator=(
      const OsDiagnosticsGetAvailableRoutinesFunction&) = delete;

 private:
  ~OsDiagnosticsGetAvailableRoutinesFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnResult(
      const std::vector<ash::health::mojom::DiagnosticRoutineEnum>& routines);
};

class OsDiagnosticsRunBatteryCapacityRoutineFunction
    : public DiagnosticsApiFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.runBatteryCapacityRoutine",
                             OS_DIAGNOSTICS_RUNBATTERYCAPACITYROUTINE)

  OsDiagnosticsRunBatteryCapacityRoutineFunction();
  OsDiagnosticsRunBatteryCapacityRoutineFunction(
      const OsDiagnosticsRunBatteryCapacityRoutineFunction&) = delete;
  OsDiagnosticsRunBatteryCapacityRoutineFunction& operator=(
      const OsDiagnosticsRunBatteryCapacityRoutineFunction&) = delete;

 private:
  ~OsDiagnosticsRunBatteryCapacityRoutineFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnResult(ash::health::mojom::RunRoutineResponsePtr ptr);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_DIAGNOSTICS_API_H_
