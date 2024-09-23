// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_DIAGNOSTICS_DIAGNOSTICS_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_DIAGNOSTICS_DIAGNOSTICS_API_H_

#include <memory>
#include <optional>

#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/common/base_telemetry_extension_api_guard_function.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/diagnostics/remote_diagnostics_service_strategy.h"
#include "chromeos/crosapi/mojom/diagnostics_service.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_extension_exception.mojom.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

class DiagnosticsApiFunctionV1AndV2Base
    : public BaseTelemetryExtensionApiGuardFunction {
 protected:
  ~DiagnosticsApiFunctionV1AndV2Base() override = default;

  // Gets the parameters passed to the JavaScript call and tries to convert it
  // to the `Params` type. If the `Params` can't be created, this resolves the
  // corresponding JavaScript call with an error and returns `nullptr`.
  template <class Params>
  std::optional<Params> GetParams();
};

class DiagnosticsApiFunctionBase : public DiagnosticsApiFunctionV1AndV2Base {
 public:
  DiagnosticsApiFunctionBase();

 protected:
  ~DiagnosticsApiFunctionBase() override;

  mojo::Remote<crosapi::mojom::DiagnosticsService>& GetRemoteService();

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  bool IsCrosApiAvailable() override;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

 private:
  std::unique_ptr<RemoteDiagnosticsServiceStrategy>
      remote_diagnostics_service_strategy_;
};

class DiagnosticsApiFunctionBaseV2 : public DiagnosticsApiFunctionV1AndV2Base {
 public:
  DiagnosticsApiFunctionBaseV2() = default;

 protected:
  ~DiagnosticsApiFunctionBaseV2() override = default;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  bool IsCrosApiAvailable() override;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
};

/****************** DIAGNOSTICS API V1 ******************/

class OsDiagnosticsGetAvailableRoutinesFunction
    : public DiagnosticsApiFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.getAvailableRoutines",
                             OS_DIAGNOSTICS_GETAVAILABLEROUTINES)
 private:
  ~OsDiagnosticsGetAvailableRoutinesFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;

  void OnResult(
      const std::vector<crosapi::mojom::DiagnosticsRoutineEnum>& routines);
};

class OsDiagnosticsGetRoutineUpdateFunction
    : public DiagnosticsApiFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.getRoutineUpdate",
                             OS_DIAGNOSTICS_GETROUTINEUPDATE)
 private:
  ~OsDiagnosticsGetRoutineUpdateFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;

  void OnResult(crosapi::mojom::DiagnosticsRoutineUpdatePtr ptr);
};

class DiagnosticsApiRunRoutineFunctionBase : public DiagnosticsApiFunctionBase {
 public:
  void OnResult(crosapi::mojom::DiagnosticsRunRoutineResponsePtr ptr);

 protected:
  ~DiagnosticsApiRunRoutineFunctionBase() override = default;

  // Returns a callback that resolves the corresponding JavaScript call with
  // the response passed to the callback.
  base::OnceCallback<void(crosapi::mojom::DiagnosticsRunRoutineResponsePtr)>
  GetOnResult();
};

class OsDiagnosticsRunAcPowerRoutineFunction
    : public DiagnosticsApiRunRoutineFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.runAcPowerRoutine",
                             OS_DIAGNOSTICS_RUNACPOWERROUTINE)
 private:
  ~OsDiagnosticsRunAcPowerRoutineFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;
};

class OsDiagnosticsRunBatteryCapacityRoutineFunction
    : public DiagnosticsApiRunRoutineFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.runBatteryCapacityRoutine",
                             OS_DIAGNOSTICS_RUNBATTERYCAPACITYROUTINE)
 private:
  ~OsDiagnosticsRunBatteryCapacityRoutineFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;
};

class OsDiagnosticsRunBatteryChargeRoutineFunction
    : public DiagnosticsApiRunRoutineFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.runBatteryChargeRoutine",
                             OS_DIAGNOSTICS_RUNBATTERYCHARGEROUTINE)
 private:
  ~OsDiagnosticsRunBatteryChargeRoutineFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;
};

class OsDiagnosticsRunBatteryDischargeRoutineFunction
    : public DiagnosticsApiRunRoutineFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.runBatteryDischargeRoutine",
                             OS_DIAGNOSTICS_RUNBATTERYDISCHARGEROUTINE)
 private:
  ~OsDiagnosticsRunBatteryDischargeRoutineFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;
};

class OsDiagnosticsRunBatteryHealthRoutineFunction
    : public DiagnosticsApiRunRoutineFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.runBatteryHealthRoutine",
                             OS_DIAGNOSTICS_RUNBATTERYHEALTHROUTINE)
 private:
  ~OsDiagnosticsRunBatteryHealthRoutineFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;
};

class OsDiagnosticsRunBluetoothDiscoveryRoutineFunction
    : public DiagnosticsApiRunRoutineFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.runBluetoothDiscoveryRoutine",
                             OS_DIAGNOSTICS_RUNBLUETOOTHDISCOVERYROUTINE)
 private:
  ~OsDiagnosticsRunBluetoothDiscoveryRoutineFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;
};

class OsDiagnosticsRunBluetoothPairingRoutineFunction
    : public DiagnosticsApiRunRoutineFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.runBluetoothPairingRoutine",
                             OS_DIAGNOSTICS_RUNBLUETOOTHPAIRINGROUTINE)
 private:
  ~OsDiagnosticsRunBluetoothPairingRoutineFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;
};

class OsDiagnosticsRunBluetoothPowerRoutineFunction
    : public DiagnosticsApiRunRoutineFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.runBluetoothPowerRoutine",
                             OS_DIAGNOSTICS_RUNBLUETOOTHPOWERROUTINE)
 private:
  ~OsDiagnosticsRunBluetoothPowerRoutineFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;
};

class OsDiagnosticsRunBluetoothScanningRoutineFunction
    : public DiagnosticsApiRunRoutineFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.runBluetoothScanningRoutine",
                             OS_DIAGNOSTICS_RUNBLUETOOTHSCANNINGROUTINE)
 private:
  ~OsDiagnosticsRunBluetoothScanningRoutineFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;
};

class OsDiagnosticsRunCpuCacheRoutineFunction
    : public DiagnosticsApiRunRoutineFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.runCpuCacheRoutine",
                             OS_DIAGNOSTICS_RUNCPUCACHEROUTINE)
 private:
  ~OsDiagnosticsRunCpuCacheRoutineFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;
};

class OsDiagnosticsRunCpuFloatingPointAccuracyRoutineFunction
    : public DiagnosticsApiRunRoutineFunctionBase {
  DECLARE_EXTENSION_FUNCTION(
      "os.diagnostics.runCpuFloatingPointAccuracyRoutine",
      OS_DIAGNOSTICS_RUNCPUFLOATINGPOINTACCURACYROUTINE)
 private:
  ~OsDiagnosticsRunCpuFloatingPointAccuracyRoutineFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;
};

class OsDiagnosticsRunCpuPrimeSearchRoutineFunction
    : public DiagnosticsApiRunRoutineFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.runCpuPrimeSearchRoutine",
                             OS_DIAGNOSTICS_RUNCPUPRIMESEARCHROUTINE)
 private:
  ~OsDiagnosticsRunCpuPrimeSearchRoutineFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;
};

class OsDiagnosticsRunCpuStressRoutineFunction
    : public DiagnosticsApiRunRoutineFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.runCpuStressRoutine",
                             OS_DIAGNOSTICS_RUNCPUSTRESSROUTINE)
 private:
  ~OsDiagnosticsRunCpuStressRoutineFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;
};

class OsDiagnosticsRunDiskReadRoutineFunction
    : public DiagnosticsApiRunRoutineFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.runDiskReadRoutine",
                             OS_DIAGNOSTICS_RUNDISKREADROUTINE)
 private:
  ~OsDiagnosticsRunDiskReadRoutineFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;
};

class OsDiagnosticsRunDnsResolutionRoutineFunction
    : public DiagnosticsApiRunRoutineFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.runDnsResolutionRoutine",
                             OS_DIAGNOSTICS_RUNDNSRESOLUTIONROUTINE)
 private:
  ~OsDiagnosticsRunDnsResolutionRoutineFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;
};

class OsDiagnosticsRunDnsResolverPresentRoutineFunction
    : public DiagnosticsApiRunRoutineFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.runDnsResolverPresentRoutine",
                             OS_DIAGNOSTICS_RUNDNSRESOLVERPRESENTROUTINE)
 private:
  ~OsDiagnosticsRunDnsResolverPresentRoutineFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;
};

class OsDiagnosticsRunFingerprintAliveRoutineFunction
    : public DiagnosticsApiRunRoutineFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.runFingerprintAliveRoutine",
                             OS_DIAGNOSTICS_RUNFINGERPRINTALIVEROUTINE)
 private:
  ~OsDiagnosticsRunFingerprintAliveRoutineFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;
};

class OsDiagnosticsRunEmmcLifetimeRoutineFunction
    : public DiagnosticsApiRunRoutineFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.runEmmcLifetimeRoutine",
                             OS_DIAGNOSTICS_RUNEMMCLIFETIMEROUTINE)
 private:
  ~OsDiagnosticsRunEmmcLifetimeRoutineFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;
};

class OsDiagnosticsRunGatewayCanBePingedRoutineFunction
    : public DiagnosticsApiRunRoutineFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.runGatewayCanBePingedRoutine",
                             OS_DIAGNOSTICS_RUNGATEWAYCANBEPINGEDROUTINE)
 private:
  ~OsDiagnosticsRunGatewayCanBePingedRoutineFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;
};

class OsDiagnosticsRunLanConnectivityRoutineFunction
    : public DiagnosticsApiRunRoutineFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.runLanConnectivityRoutine",
                             OS_DIAGNOSTICS_RUNLANCONNECTIVITYROUTINE)
 private:
  ~OsDiagnosticsRunLanConnectivityRoutineFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;
};

class OsDiagnosticsRunMemoryRoutineFunction
    : public DiagnosticsApiRunRoutineFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.runMemoryRoutine",
                             OS_DIAGNOSTICS_RUNMEMORYROUTINE)
 private:
  ~OsDiagnosticsRunMemoryRoutineFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;
};

class OsDiagnosticsRunNvmeSelfTestRoutineFunction
    : public DiagnosticsApiRunRoutineFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.runNvmeSelfTestRoutine",
                             OS_DIAGNOSTICS_RUNNVMESELFTESTROUTINE)
 private:
  ~OsDiagnosticsRunNvmeSelfTestRoutineFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;
};

class OsDiagnosticsRunSensitiveSensorRoutineFunction
    : public DiagnosticsApiRunRoutineFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.runSensitiveSensorRoutine",
                             OS_DIAGNOSTICS_RUNSENSITIVESENSORROUTINE)
 private:
  ~OsDiagnosticsRunSensitiveSensorRoutineFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;
};

class OsDiagnosticsRunSignalStrengthRoutineFunction
    : public DiagnosticsApiRunRoutineFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.runSignalStrengthRoutine",
                             OS_DIAGNOSTICS_RUNSIGNALSTRENGTHROUTINE)
 private:
  ~OsDiagnosticsRunSignalStrengthRoutineFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;
};

class OsDiagnosticsRunSmartctlCheckRoutineFunction
    : public DiagnosticsApiRunRoutineFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.runSmartctlCheckRoutine",
                             OS_DIAGNOSTICS_RUNSMARTCTLCHECKROUTINE)
 private:
  ~OsDiagnosticsRunSmartctlCheckRoutineFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;
};

class OsDiagnosticsRunUfsLifetimeRoutineFunction
    : public DiagnosticsApiRunRoutineFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.runUfsLifetimeRoutine",
                             OS_DIAGNOSTICS_RUNUFSLIFETIMEROUTINE)
 private:
  ~OsDiagnosticsRunUfsLifetimeRoutineFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;
};

class OsDiagnosticsRunPowerButtonRoutineFunction
    : public DiagnosticsApiRunRoutineFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.runPowerButtonRoutine",
                             OS_DIAGNOSTICS_RUNPOWERBUTTONROUTINE)
 private:
  ~OsDiagnosticsRunPowerButtonRoutineFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;
};

class OsDiagnosticsRunAudioDriverRoutineFunction
    : public DiagnosticsApiRunRoutineFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.runAudioDriverRoutine",
                             OS_DIAGNOSTICS_RUNAUDIODRIVERROUTINE)
 private:
  ~OsDiagnosticsRunAudioDriverRoutineFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;
};

class OsDiagnosticsRunFanRoutineFunction
    : public DiagnosticsApiRunRoutineFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.runFanRoutine",
                             OS_DIAGNOSTICS_RUNFANROUTINE)
 private:
  ~OsDiagnosticsRunFanRoutineFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;
};

/****************** DIAGNOSTICS API V2 ******************/

class OsDiagnosticsCreateRoutineFunction : public DiagnosticsApiFunctionBaseV2 {
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.createRoutine",
                             OS_DIAGNOSTICS_CREATEROUTINE)
 private:
  ~OsDiagnosticsCreateRoutineFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;
};

class OsDiagnosticsCreateMemoryRoutineFunction
    : public DiagnosticsApiFunctionBaseV2 {
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.createMemoryRoutine",
                             OS_DIAGNOSTICS_CREATEMEMORYROUTINE)
 private:
  ~OsDiagnosticsCreateMemoryRoutineFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;
};

class OsDiagnosticsCreateVolumeButtonRoutineFunction
    : public DiagnosticsApiFunctionBaseV2 {
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.createVolumeButtonRoutine",
                             OS_DIAGNOSTICS_CREATEVOLUMEBUTTONROUTINE)
 private:
  ~OsDiagnosticsCreateVolumeButtonRoutineFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;
};

class OsDiagnosticsCreateFanRoutineFunction
    : public DiagnosticsApiFunctionBaseV2 {
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.createFanRoutine",
                             OS_DIAGNOSTICS_CREATEFANROUTINE)
 private:
  ~OsDiagnosticsCreateFanRoutineFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;
};

class OsDiagnosticsStartRoutineFunction : public DiagnosticsApiFunctionBaseV2 {
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.startRoutine",
                             OS_DIAGNOSTICS_STARTROUTINE)
 private:
  ~OsDiagnosticsStartRoutineFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;
};

class OsDiagnosticsCancelRoutineFunction : public DiagnosticsApiFunctionBaseV2 {
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.cancelRoutine",
                             OS_DIAGNOSTICS_CANCELROUTINE)
 private:
  ~OsDiagnosticsCancelRoutineFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;
};

class OsDiagnosticsReplyToRoutineInquiryFunction
    : public DiagnosticsApiFunctionBaseV2 {
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.replyToRoutineInquiry",
                             OS_DIAGNOSTICS_REPLYTOROUTINEINQUIRY)
 private:
  ~OsDiagnosticsReplyToRoutineInquiryFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;
};

class OsDiagnosticsIsRoutineArgumentSupportedFunction
    : public DiagnosticsApiFunctionBaseV2 {
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.isRoutineArgumentSupported",
                             OS_DIAGNOSTICS_ISROUTINEARGUMENTSUPPORTED)
 private:
  ~OsDiagnosticsIsRoutineArgumentSupportedFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;

  void OnResult(crosapi::mojom::TelemetryExtensionSupportStatusPtr result);
};

class OsDiagnosticsIsMemoryRoutineArgumentSupportedFunction
    : public DiagnosticsApiFunctionBaseV2 {
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.isMemoryRoutineArgumentSupported",
                             OS_DIAGNOSTICS_ISMEMORYROUTINEARGUMENTSUPPORTED)
 private:
  ~OsDiagnosticsIsMemoryRoutineArgumentSupportedFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;

  void OnResult(crosapi::mojom::TelemetryExtensionSupportStatusPtr result);
};

class OsDiagnosticsIsVolumeButtonRoutineArgumentSupportedFunction
    : public DiagnosticsApiFunctionBaseV2 {
  DECLARE_EXTENSION_FUNCTION(
      "os.diagnostics.isVolumeButtonRoutineArgumentSupported",
      OS_DIAGNOSTICS_ISVOLUMEBUTTONROUTINEARGUMENTSUPPORTED)
 private:
  ~OsDiagnosticsIsVolumeButtonRoutineArgumentSupportedFunction() override =
      default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;

  void OnResult(crosapi::mojom::TelemetryExtensionSupportStatusPtr result);
};

class OsDiagnosticsIsFanRoutineArgumentSupportedFunction
    : public DiagnosticsApiFunctionBaseV2 {
  DECLARE_EXTENSION_FUNCTION("os.diagnostics.isFanRoutineArgumentSupported",
                             OS_DIAGNOSTICS_ISFANROUTINEARGUMENTSUPPORTED)
 private:
  ~OsDiagnosticsIsFanRoutineArgumentSupportedFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;

  void OnResult(crosapi::mojom::TelemetryExtensionSupportStatusPtr result);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_DIAGNOSTICS_DIAGNOSTICS_API_H_
