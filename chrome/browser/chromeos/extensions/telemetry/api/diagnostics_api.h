// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_DIAGNOSTICS_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_DIAGNOSTICS_API_H_

#include "ash/webui/telemetry_extension_ui/mojom/diagnostics_service.mojom.h"
#include "ash/webui/telemetry_extension_ui/services/diagnostics_service.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/base_telemetry_extension_api_guard_function.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

class DiagnosticsApiFunctionBase
    : public BaseTelemetryExtensionApiGuardFunction {
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

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;

  void OnResult(
      const std::vector<ash::health::mojom::DiagnosticRoutineEnum>& routines);
};

class OsDiagnosticsGetRoutineUpdateFunction
    : public DiagnosticsApiFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.getRoutineUpdate",
                             OS_DIAGNOSTICS_GETROUTINEUPDATE)

  OsDiagnosticsGetRoutineUpdateFunction();
  OsDiagnosticsGetRoutineUpdateFunction(
      const OsDiagnosticsGetRoutineUpdateFunction&) = delete;
  OsDiagnosticsGetRoutineUpdateFunction& operator=(
      const OsDiagnosticsGetRoutineUpdateFunction&) = delete;

 private:
  ~OsDiagnosticsGetRoutineUpdateFunction() override;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;

  void OnResult(ash::health::mojom::RoutineUpdatePtr ptr);
};

class DiagnosticsApiRunRoutineFunctionBase : public DiagnosticsApiFunctionBase {
 public:
  DiagnosticsApiRunRoutineFunctionBase();

  DiagnosticsApiRunRoutineFunctionBase(
      const DiagnosticsApiRunRoutineFunctionBase&) = delete;
  DiagnosticsApiRunRoutineFunctionBase& operator=(
      const DiagnosticsApiRunRoutineFunctionBase&) = delete;

  void OnResult(ash::health::mojom::RunRoutineResponsePtr ptr);

 protected:
  ~DiagnosticsApiRunRoutineFunctionBase() override;
};

class OsDiagnosticsRunBatteryCapacityRoutineFunction
    : public DiagnosticsApiRunRoutineFunctionBase {
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

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;
};

class OsDiagnosticsRunBatteryChargeRoutineFunction
    : public DiagnosticsApiRunRoutineFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.runBatteryChargeRoutine",
                             OS_DIAGNOSTICS_RUNBATTERYCHARGEROUTINE)

  OsDiagnosticsRunBatteryChargeRoutineFunction();
  OsDiagnosticsRunBatteryChargeRoutineFunction(
      const OsDiagnosticsRunBatteryChargeRoutineFunction&) = delete;
  OsDiagnosticsRunBatteryChargeRoutineFunction& operator=(
      const OsDiagnosticsRunBatteryChargeRoutineFunction&) = delete;

 private:
  ~OsDiagnosticsRunBatteryChargeRoutineFunction() override;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;
};

class OsDiagnosticsRunBatteryDischargeRoutineFunction
    : public DiagnosticsApiRunRoutineFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.runBatteryDischargeRoutine",
                             OS_DIAGNOSTICS_RUNBATTERYDISCHARGEROUTINE)

  OsDiagnosticsRunBatteryDischargeRoutineFunction();
  OsDiagnosticsRunBatteryDischargeRoutineFunction(
      const OsDiagnosticsRunBatteryDischargeRoutineFunction&) = delete;
  OsDiagnosticsRunBatteryDischargeRoutineFunction& operator=(
      const OsDiagnosticsRunBatteryDischargeRoutineFunction&) = delete;

 private:
  ~OsDiagnosticsRunBatteryDischargeRoutineFunction() override;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;
};

class OsDiagnosticsRunBatteryHealthRoutineFunction
    : public DiagnosticsApiRunRoutineFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.runBatteryHealthRoutine",
                             OS_DIAGNOSTICS_RUNBATTERYHEALTHROUTINE)

  OsDiagnosticsRunBatteryHealthRoutineFunction();
  OsDiagnosticsRunBatteryHealthRoutineFunction(
      const OsDiagnosticsRunBatteryHealthRoutineFunction&) = delete;
  OsDiagnosticsRunBatteryHealthRoutineFunction& operator=(
      const OsDiagnosticsRunBatteryHealthRoutineFunction&) = delete;

 private:
  ~OsDiagnosticsRunBatteryHealthRoutineFunction() override;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;
};

class OsDiagnosticsRunCpuCacheRoutineFunction
    : public DiagnosticsApiRunRoutineFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.runCpuCacheRoutine",
                             OS_DIAGNOSTICS_RUNCPUCACHEROUTINE)

  OsDiagnosticsRunCpuCacheRoutineFunction();
  OsDiagnosticsRunCpuCacheRoutineFunction(
      const OsDiagnosticsRunCpuCacheRoutineFunction&) = delete;
  OsDiagnosticsRunCpuCacheRoutineFunction& operator=(
      const OsDiagnosticsRunCpuCacheRoutineFunction&) = delete;

 private:
  ~OsDiagnosticsRunCpuCacheRoutineFunction() override;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;
};

class OsDiagnosticsRunCpuFloatingPointAccuracyRoutineFunction
    : public DiagnosticsApiRunRoutineFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "os.diagnostics.runCpuFloatingPointAccuracyRoutine",
      OS_DIAGNOSTICS_RUNCPUFLOATINGPOINTACCURACYROUTINE)

  OsDiagnosticsRunCpuFloatingPointAccuracyRoutineFunction();
  OsDiagnosticsRunCpuFloatingPointAccuracyRoutineFunction(
      const OsDiagnosticsRunCpuFloatingPointAccuracyRoutineFunction&) = delete;
  OsDiagnosticsRunCpuFloatingPointAccuracyRoutineFunction& operator=(
      const OsDiagnosticsRunCpuFloatingPointAccuracyRoutineFunction&) = delete;

 private:
  ~OsDiagnosticsRunCpuFloatingPointAccuracyRoutineFunction() override;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;
};

class OsDiagnosticsRunCpuPrimeSearchRoutineFunction
    : public DiagnosticsApiRunRoutineFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.runCpuPrimeSearchRoutine",
                             OS_DIAGNOSTICS_RUNCPUPRIMESEARCHROUTINE)

  OsDiagnosticsRunCpuPrimeSearchRoutineFunction();
  OsDiagnosticsRunCpuPrimeSearchRoutineFunction(
      const OsDiagnosticsRunCpuPrimeSearchRoutineFunction&) = delete;
  OsDiagnosticsRunCpuPrimeSearchRoutineFunction& operator=(
      const OsDiagnosticsRunCpuPrimeSearchRoutineFunction&) = delete;

 private:
  ~OsDiagnosticsRunCpuPrimeSearchRoutineFunction() override;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;
};

class OsDiagnosticsRunCpuStressRoutineFunction
    : public DiagnosticsApiRunRoutineFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.runCpuStressRoutine",
                             OS_DIAGNOSTICS_RUNCPUSTRESSROUTINE)

  OsDiagnosticsRunCpuStressRoutineFunction();
  OsDiagnosticsRunCpuStressRoutineFunction(
      const OsDiagnosticsRunCpuStressRoutineFunction&) = delete;
  OsDiagnosticsRunCpuStressRoutineFunction& operator=(
      const OsDiagnosticsRunCpuStressRoutineFunction&) = delete;

 private:
  ~OsDiagnosticsRunCpuStressRoutineFunction() override;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;
};

class OsDiagnosticsRunDiskReadRoutineFunction
    : public DiagnosticsApiRunRoutineFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.runDiskReadRoutine",
                             OS_DIAGNOSTICS_RUNDISKREADROUTINE)

  OsDiagnosticsRunDiskReadRoutineFunction();
  OsDiagnosticsRunDiskReadRoutineFunction(
      const OsDiagnosticsRunDiskReadRoutineFunction&) = delete;
  OsDiagnosticsRunDiskReadRoutineFunction& operator=(
      const OsDiagnosticsRunDiskReadRoutineFunction&) = delete;

 private:
  ~OsDiagnosticsRunDiskReadRoutineFunction() override;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;
};

class OsDiagnosticsRunMemoryRoutineFunction
    : public DiagnosticsApiRunRoutineFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.runMemoryRoutine",
                             OS_DIAGNOSTICS_RUNMEMORYROUTINE)

  OsDiagnosticsRunMemoryRoutineFunction();
  OsDiagnosticsRunMemoryRoutineFunction(
      const OsDiagnosticsRunMemoryRoutineFunction&) = delete;
  OsDiagnosticsRunMemoryRoutineFunction& operator=(
      const OsDiagnosticsRunMemoryRoutineFunction&) = delete;

 private:
  ~OsDiagnosticsRunMemoryRoutineFunction() override;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;
};

class OsDiagnosticsRunNvmeWearLevelRoutineFunction
    : public DiagnosticsApiRunRoutineFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.runNvmeWearLevelRoutine",
                             OS_DIAGNOSTICS_RUNNVMEWEARLEVELROUTINE)

  OsDiagnosticsRunNvmeWearLevelRoutineFunction();
  OsDiagnosticsRunNvmeWearLevelRoutineFunction(
      const OsDiagnosticsRunNvmeWearLevelRoutineFunction&) = delete;
  OsDiagnosticsRunNvmeWearLevelRoutineFunction& operator=(
      const OsDiagnosticsRunNvmeWearLevelRoutineFunction&) = delete;

 private:
  ~OsDiagnosticsRunNvmeWearLevelRoutineFunction() override;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;
};

class OsDiagnosticsRunSmartctlCheckRoutineFunction
    : public DiagnosticsApiRunRoutineFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.runSmartctlCheckRoutine",
                             OS_DIAGNOSTICS_RUNSMARTCTLCHECKROUTINE)

  OsDiagnosticsRunSmartctlCheckRoutineFunction();
  OsDiagnosticsRunSmartctlCheckRoutineFunction(
      const OsDiagnosticsRunSmartctlCheckRoutineFunction&) = delete;
  OsDiagnosticsRunSmartctlCheckRoutineFunction& operator=(
      const OsDiagnosticsRunSmartctlCheckRoutineFunction&) = delete;

 private:
  ~OsDiagnosticsRunSmartctlCheckRoutineFunction() override;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_DIAGNOSTICS_API_H_
