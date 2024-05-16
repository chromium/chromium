// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/common/base_telemetry_extension_browser_test.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/diagnostics/fake_diagnostics_service.h"
#include "chromeos/crosapi/mojom/diagnostics_service.mojom.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension_features.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/chromeos/extensions/telemetry/api/diagnostics/fake_diagnostics_service_factory.h"
#include "chromeos/ash/components/telemetry_extension/diagnostics/diagnostics_service_ash.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/startup/browser_init_params.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace chromeos {

namespace {

namespace crosapi = ::crosapi::mojom;

}  // namespace

class TelemetryExtensionDiagnosticsApiBrowserTest
    : public BaseTelemetryExtensionBrowserTest {
 public:
  TelemetryExtensionDiagnosticsApiBrowserTest() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    ash::DiagnosticsServiceAsh::Factory::SetForTesting(
        &fake_diagnostics_service_factory_);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }

  ~TelemetryExtensionDiagnosticsApiBrowserTest() override = default;

  TelemetryExtensionDiagnosticsApiBrowserTest(
      const TelemetryExtensionDiagnosticsApiBrowserTest&) = delete;
  TelemetryExtensionDiagnosticsApiBrowserTest& operator=(
      const TelemetryExtensionDiagnosticsApiBrowserTest&) = delete;

 protected:
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  template <typename Interface>
  bool InterfaceVersionHigherOrEqual(int version) {
    auto* lacros_service = chromeos::LacrosService::Get();

    return lacros_service &&
           lacros_service->GetInterfaceVersion<Interface>() >= version;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  void SetServiceForTesting(
      std::unique_ptr<FakeDiagnosticsService> fake_diagnostics_service_impl) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    fake_diagnostics_service_factory_.SetCreateInstanceResponse(
        std::move(fake_diagnostics_service_impl));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
    fake_diagnostics_service_impl_ = std::move(fake_diagnostics_service_impl);
    // Replace the production DiagnosticsService with a fake for testing.
    chromeos::LacrosService::Get()->InjectRemoteForTesting(
        fake_diagnostics_service_impl_->BindNewPipeAndPassRemote());
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  FakeDiagnosticsServiceFactory fake_diagnostics_service_factory_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  std::unique_ptr<FakeDiagnosticsService> fake_diagnostics_service_impl_;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
};

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       GetAvailableRoutinesSuccess) {
  {
    auto fake_service_impl = std::make_unique<FakeDiagnosticsService>();
    fake_service_impl->SetAvailableRoutines({
        crosapi::DiagnosticsRoutineEnum::kAcPower,
        crosapi::DiagnosticsRoutineEnum::kBatteryCapacity,
        crosapi::DiagnosticsRoutineEnum::kBatteryCharge,
        crosapi::DiagnosticsRoutineEnum::kBatteryDischarge,
        crosapi::DiagnosticsRoutineEnum::kBatteryHealth,
        crosapi::DiagnosticsRoutineEnum::kCpuCache,
        crosapi::DiagnosticsRoutineEnum::kFloatingPointAccuracy,
        crosapi::DiagnosticsRoutineEnum::kPrimeSearch,
        crosapi::DiagnosticsRoutineEnum::kCpuStress,
        crosapi::DiagnosticsRoutineEnum::kDiskRead,
        crosapi::DiagnosticsRoutineEnum::kDnsResolution,
        crosapi::DiagnosticsRoutineEnum::kDnsResolverPresent,
        crosapi::DiagnosticsRoutineEnum::kLanConnectivity,
        crosapi::DiagnosticsRoutineEnum::kMemory,
        crosapi::DiagnosticsRoutineEnum::kSignalStrength,
        crosapi::DiagnosticsRoutineEnum::kGatewayCanBePinged,
        crosapi::DiagnosticsRoutineEnum::kSmartctlCheck,
        crosapi::DiagnosticsRoutineEnum::kSensitiveSensor,
        crosapi::DiagnosticsRoutineEnum::kNvmeSelfTest,
        crosapi::DiagnosticsRoutineEnum::kFingerprintAlive,
        crosapi::DiagnosticsRoutineEnum::kSmartctlCheckWithPercentageUsed,
        crosapi::DiagnosticsRoutineEnum::kEmmcLifetime,
        crosapi::DiagnosticsRoutineEnum::kBluetoothPower,
        crosapi::DiagnosticsRoutineEnum::kUfsLifetime,
        crosapi::DiagnosticsRoutineEnum::kPowerButton,
        crosapi::DiagnosticsRoutineEnum::kAudioDriver,
        crosapi::DiagnosticsRoutineEnum::kBluetoothDiscovery,
        crosapi::DiagnosticsRoutineEnum::kBluetoothScanning,
        crosapi::DiagnosticsRoutineEnum::kBluetoothPairing,
        crosapi::DiagnosticsRoutineEnum::kFan,
    });

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function getAvailableRoutines() {
        const response =
          await chrome.os.diagnostics.getAvailableRoutines();
        chrome.test.assertEq(
          {
            routines: [
              "ac_power",
              "battery_capacity",
              "battery_charge",
              "battery_discharge",
              "battery_health",
              "cpu_cache",
              "cpu_floating_point_accuracy",
              "cpu_prime_search",
              "cpu_stress",
              "disk_read",
              "dns_resolution",
              "dns_resolver_present",
              "lan_connectivity",
              "memory",
              "signal_strength",
              "gateway_can_be_pinged",
              "smartctl_check",
              "sensitive_sensor",
              "nvme_self_test",
              "fingerprint_alive",
              "smartctl_check_with_percentage_used",
              "emmc_lifetime",
              "bluetooth_power",
              "ufs_lifetime",
              "power_button",
              "audio_driver",
              "bluetooth_discovery",
              "bluetooth_scanning",
              "bluetooth_pairing",
              "fan"
            ]
          }, response);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       GetRoutineUpdateNonInteractiveSuccess) {
  // Configure FakeDiagnosticsService.
  {
    auto nonInteractiveRoutineUpdate =
        crosapi::DiagnosticsNonInteractiveRoutineUpdate::New();
    nonInteractiveRoutineUpdate->status =
        crosapi::DiagnosticsRoutineStatusEnum::kReady;
    nonInteractiveRoutineUpdate->status_message = "Routine ran by Google.";

    auto routineUpdateUnion =
        crosapi::DiagnosticsRoutineUpdateUnion::NewNoninteractiveUpdate(
            std::move(nonInteractiveRoutineUpdate));

    auto response = crosapi::DiagnosticsRoutineUpdate::New();
    response->progress_percent = 87;
    response->routine_update_union = std::move(routineUpdateUnion);

    // Set the return value for a call to GetAvailableRoutines.
    auto fake_service_impl = std::make_unique<FakeDiagnosticsService>();
    fake_service_impl->SetRoutineUpdateResponse(std::move(response));

    // Set the expected passed parameters.
    fake_service_impl->SetExpectedLastPassedParameters(
        base::Value::Dict()
            .Set("id", 123456)
            .Set("command",
                 static_cast<int32_t>(
                     crosapi::DiagnosticsRoutineCommandEnum::kGetStatus))
            .Set("include_output", true));

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function getRoutineUpdate() {
        const response =
          await chrome.os.diagnostics.getRoutineUpdate(
            {
              id: 123456,
              command: "status"
            }
          );
        chrome.test.assertEq(
          {
            progress_percent: 87,
            status: "ready",
            status_message: "Routine ran by Google."
          },
          response);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       GetRoutineUpdateInteractiveSuccess) {
  // Configure FakeDiagnosticsService.
  {
    auto interactiveRoutineUpdate =
        crosapi::DiagnosticsInteractiveRoutineUpdate::New();
    interactiveRoutineUpdate->user_message =
        crosapi::DiagnosticsRoutineUserMessageEnum::kUnplugACPower;

    auto routineUpdateUnion =
        crosapi::DiagnosticsRoutineUpdateUnion::NewInteractiveUpdate(
            std::move(interactiveRoutineUpdate));

    auto response = crosapi::DiagnosticsRoutineUpdate::New();
    response->progress_percent = 50;
    response->output = "routine is running...";
    response->routine_update_union = std::move(routineUpdateUnion);

    // Set the return value for a call to GetAvailableRoutines.
    auto fake_service_impl = std::make_unique<FakeDiagnosticsService>();
    fake_service_impl->SetRoutineUpdateResponse(std::move(response));

    // Set the expected passed parameters.
    fake_service_impl->SetExpectedLastPassedParameters(
        base::Value::Dict()
            .Set("id", 654321)
            .Set("command",
                 static_cast<int32_t>(
                     crosapi::DiagnosticsRoutineCommandEnum::kRemove))
            .Set("include_output", true));

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function getRoutineUpdate() {
        const response =
          await chrome.os.diagnostics.getRoutineUpdate(
            {
              id: 654321,
              command: "remove",
            }
          );
        chrome.test.assertEq(
          {
            progress_percent: 50,
            output: "routine is running...",
            status: "waiting_user_action",
            status_message: "Waiting for user action. See user_message",
            user_message: "unplug_ac_power"
          },
          response);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunAcPowerRoutineSuccess) {
  // Configure FakeDiagnosticsService.
  {
    auto expected_response = crosapi::DiagnosticsRunRoutineResponse::New();
    expected_response->id = 0;
    expected_response->status = crosapi::DiagnosticsRoutineStatusEnum::kReady;

    // Set the return value for a call to RunAcPowerRoutine.
    auto fake_service_impl = std::make_unique<FakeDiagnosticsService>();
    fake_service_impl->SetRunRoutineResponse(std::move(expected_response));

    // Set the expected runtime actions.
    fake_service_impl->SetExpectedLastPassedParameters(
        base::Value::Dict()
            .Set("expected_status",
                 static_cast<int32_t>(
                     crosapi::DiagnosticsAcPowerStatusEnum::kConnected))
            .Set("expected_power_type", "ac_power"));
    fake_service_impl->SetExpectedLastCalledRoutine(
        crosapi::DiagnosticsRoutineEnum::kAcPower);

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function runAcPowerRoutine() {
        const response =
          await chrome.os.diagnostics.runAcPowerRoutine(
            {
              expected_status: "connected",
              expected_power_type: "ac_power",
            }
          );
        chrome.test.assertEq({id: 0, status: "ready"}, response);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunBatteryCapacityRoutineSuccess) {
  // Configure FakeDiagnosticsService.
  {
    auto expected_response = crosapi::DiagnosticsRunRoutineResponse::New();
    expected_response->id = 0;
    expected_response->status = crosapi::DiagnosticsRoutineStatusEnum::kReady;

    // Set the return value for a call to RunBatteryCapacityRoutine.
    auto fake_service_impl = std::make_unique<FakeDiagnosticsService>();
    fake_service_impl->SetRunRoutineResponse(std::move(expected_response));

    // Set the expected called routine.
    fake_service_impl->SetExpectedLastCalledRoutine(
        crosapi::DiagnosticsRoutineEnum::kBatteryCapacity);

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function runBatteryCapacityRoutine() {
        const response =
          await chrome.os.diagnostics.runBatteryCapacityRoutine();
        chrome.test.assertEq({id: 0, status: "ready"}, response);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunBatteryChargeRoutineSuccess) {
  // Configure FakeDiagnosticsService.
  {
    auto expected_response = crosapi::DiagnosticsRunRoutineResponse::New();
    expected_response->id = 0;
    expected_response->status = crosapi::DiagnosticsRoutineStatusEnum::kReady;

    // Set the return value for a call to RunBatteryChargeRoutine.
    auto fake_service_impl = std::make_unique<FakeDiagnosticsService>();
    fake_service_impl->SetRunRoutineResponse(std::move(expected_response));

    // Set the expected runtime actions.
    fake_service_impl->SetExpectedLastPassedParameters(
        base::Value::Dict()
            .Set("length_seconds", 1000)
            .Set("minimum_charge_percent_required", 1));
    fake_service_impl->SetExpectedLastCalledRoutine(
        crosapi::DiagnosticsRoutineEnum::kBatteryCharge);

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function runBatteryChargeRoutine() {
        const response =
          await chrome.os.diagnostics.runBatteryChargeRoutine(
            {
              length_seconds: 1000,
              minimum_charge_percent_required: 1
            }
          );
        chrome.test.assertEq({id: 0, status: "ready"}, response);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunBatteryDischargeRoutineSuccess) {
  // Configure FakeDiagnosticsService.
  {
    auto expected_response = crosapi::DiagnosticsRunRoutineResponse::New();
    expected_response->id = 0;
    expected_response->status = crosapi::DiagnosticsRoutineStatusEnum::kReady;

    // Set the return value for a call to RunBatteryDischargeRoutine.
    auto fake_service_impl = std::make_unique<FakeDiagnosticsService>();
    fake_service_impl->SetRunRoutineResponse(std::move(expected_response));

    // Set the expected runtime actions.
    fake_service_impl->SetExpectedLastPassedParameters(
        base::Value::Dict()
            .Set("length_seconds", 10)
            .Set("maximum_discharge_percent_allowed", 15));
    fake_service_impl->SetExpectedLastCalledRoutine(
        crosapi::DiagnosticsRoutineEnum::kBatteryDischarge);

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function runBatteryDischargeRoutine() {
        const response =
          await chrome.os.diagnostics.runBatteryDischargeRoutine(
            {
              length_seconds: 10,
              maximum_discharge_percent_allowed: 15
            }
          );
        chrome.test.assertEq({id: 0, status: "ready"}, response);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunBatteryHealthRoutineSuccess) {
  // Configure FakeDiagnosticsService.
  {
    auto expected_response = crosapi::DiagnosticsRunRoutineResponse::New();
    expected_response->id = 0;
    expected_response->status = crosapi::DiagnosticsRoutineStatusEnum::kReady;

    // Set the return value for a call to RunBatteryHealthRoutine.
    auto fake_service_impl = std::make_unique<FakeDiagnosticsService>();
    fake_service_impl->SetRunRoutineResponse(std::move(expected_response));

    // Set the expected called routine.
    fake_service_impl->SetExpectedLastCalledRoutine(
        crosapi::DiagnosticsRoutineEnum::kBatteryHealth);

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function runBatteryHealthRoutine() {
        const response =
          await chrome.os.diagnostics.runBatteryHealthRoutine();
        chrome.test.assertEq({id: 0, status: "ready"}, response);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunBluetoothDiscoveryRoutineSuccess) {
  // Configure FakeDiagnosticsService.
  {
    auto expected_response = crosapi::DiagnosticsRunRoutineResponse::New();
    expected_response->id = 0;
    expected_response->status = crosapi::DiagnosticsRoutineStatusEnum::kReady;

    // Set the return value for a call to RunBluetoothDiscoveryRoutine.
    auto fake_service_impl = std::make_unique<FakeDiagnosticsService>();
    fake_service_impl->SetRunRoutineResponse(std::move(expected_response));

    // Set the expected called routine.
    fake_service_impl->SetExpectedLastCalledRoutine(
        crosapi::DiagnosticsRoutineEnum::kBluetoothDiscovery);

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function runBluetoothDiscoveryRoutine() {
        const response =
          await chrome.os.diagnostics.runBluetoothDiscoveryRoutine();
        chrome.test.assertEq({id: 0, status: "ready"}, response);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunBluetoothScanningRoutineSuccess) {
  // Configure FakeDiagnosticsService.
  {
    auto expected_response = crosapi::DiagnosticsRunRoutineResponse::New();
    expected_response->id = 0;
    expected_response->status = crosapi::DiagnosticsRoutineStatusEnum::kReady;

    // Set the return value for a call to RunBluetoothScanningRoutine.
    auto fake_service_impl = std::make_unique<FakeDiagnosticsService>();
    fake_service_impl->SetRunRoutineResponse(std::move(expected_response));

    // Set the expected called routine.
    fake_service_impl->SetExpectedLastCalledRoutine(
        crosapi::DiagnosticsRoutineEnum::kBluetoothScanning);

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function runBluetoothScanningRoutine() {
        const response =
          await chrome.os.diagnostics.runBluetoothScanningRoutine(
            {
              length_seconds: 10
            });
        chrome.test.assertEq({id: 0, status: "ready"}, response);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunBluetoothPairingRoutineSuccess) {
  // Configure FakeDiagnosticsService.
  {
    auto expected_response = crosapi::DiagnosticsRunRoutineResponse::New();
    expected_response->id = 0;
    expected_response->status = crosapi::DiagnosticsRoutineStatusEnum::kReady;

    // Set the return value for a call to RunBluetoothPairingRoutine.
    auto fake_service_impl = std::make_unique<FakeDiagnosticsService>();
    fake_service_impl->SetRunRoutineResponse(std::move(expected_response));

    // Set the expected called routine.
    fake_service_impl->SetExpectedLastCalledRoutine(
        crosapi::DiagnosticsRoutineEnum::kBluetoothPairing);

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function runBluetoothPairingRoutine() {
        const response =
          await chrome.os.diagnostics.runBluetoothPairingRoutine(
            {
              peripheral_id: "HEALTHD_TEST_ID"
            }
          );
        chrome.test.assertEq({id: 0, status: "ready"}, response);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunBluetoothPowerRoutineSuccess) {
  // Configure FakeDiagnosticsService.
  {
    auto expected_response = crosapi::DiagnosticsRunRoutineResponse::New();
    expected_response->id = 0;
    expected_response->status = crosapi::DiagnosticsRoutineStatusEnum::kReady;

    // Set the return value for a call to RunBluetoothPowerRoutine.
    auto fake_service_impl = std::make_unique<FakeDiagnosticsService>();
    fake_service_impl->SetRunRoutineResponse(std::move(expected_response));

    // Set the expected called routine.
    fake_service_impl->SetExpectedLastCalledRoutine(
        crosapi::DiagnosticsRoutineEnum::kBluetoothPower);

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function runBluetoothPowerRoutine() {
        const response =
          await chrome.os.diagnostics.runBluetoothPowerRoutine();
        chrome.test.assertEq({id: 0, status: "ready"}, response);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunCpuCacheRoutineSuccess) {
  // Configure FakeDiagnosticsService.
  {
    auto expected_response = crosapi::DiagnosticsRunRoutineResponse::New();
    expected_response->id = 0;
    expected_response->status = crosapi::DiagnosticsRoutineStatusEnum::kReady;

    // Set the return value for a call to RunCpuCacheRoutine.
    auto fake_service_impl = std::make_unique<FakeDiagnosticsService>();
    fake_service_impl->SetRunRoutineResponse(std::move(expected_response));

    // Set the expected runtime actions.
    fake_service_impl->SetExpectedLastPassedParameters(
        base::Value::Dict().Set("length_seconds", 120));
    fake_service_impl->SetExpectedLastCalledRoutine(
        crosapi::DiagnosticsRoutineEnum::kCpuCache);

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function runCpuCacheRoutine() {
        const response =
          await chrome.os.diagnostics.runCpuCacheRoutine(
            {
              length_seconds: 120
            }
          );
        chrome.test.assertEq({id: 0, status: "ready"}, response);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunCpuFloatingPointAccuracyRoutineSuccess) {
  // Configure FakeDiagnosticsService.
  {
    auto expected_response = crosapi::DiagnosticsRunRoutineResponse::New();
    expected_response->id = 0;
    expected_response->status = crosapi::DiagnosticsRoutineStatusEnum::kReady;

    // Set the return value for a call to RunCpuFloatingPointAccuracyRoutine.
    auto fake_service_impl = std::make_unique<FakeDiagnosticsService>();
    fake_service_impl->SetRunRoutineResponse(std::move(expected_response));

    // Set the expected runtime actions.
    fake_service_impl->SetExpectedLastPassedParameters(
        base::Value::Dict().Set("length_seconds", 120));
    fake_service_impl->SetExpectedLastCalledRoutine(
        crosapi::DiagnosticsRoutineEnum::kFloatingPointAccuracy);

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function runCpuFloatingPointAccuracyRoutine() {
        const response =
          await chrome.os.diagnostics.runCpuFloatingPointAccuracyRoutine(
            {
              length_seconds: 120
            }
          );
        chrome.test.assertEq({id: 0, status: "ready"}, response);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunCpuPrimeSearchRoutineSuccess) {
  // Configure FakeDiagnosticsService.
  {
    auto expected_response = crosapi::DiagnosticsRunRoutineResponse::New();
    expected_response->id = 0;
    expected_response->status = crosapi::DiagnosticsRoutineStatusEnum::kReady;

    // Set the return value for a call to RunCpuPrimeSearchRoutine.
    auto fake_service_impl = std::make_unique<FakeDiagnosticsService>();
    fake_service_impl->SetRunRoutineResponse(std::move(expected_response));

    // Set the expected runtime actions.
    fake_service_impl->SetExpectedLastPassedParameters(
        base::Value::Dict().Set("length_seconds", 120));
    fake_service_impl->SetExpectedLastCalledRoutine(
        crosapi::DiagnosticsRoutineEnum::kPrimeSearch);

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function runCpuPrimeSearchRoutine() {
        const response =
          await chrome.os.diagnostics.runCpuPrimeSearchRoutine(
            {
              length_seconds: 120
            }
          );
        chrome.test.assertEq({id: 0, status: "ready"}, response);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunCpuStressRoutineSuccess) {
  // Configure FakeDiagnosticsService.
  {
    auto expected_response = crosapi::DiagnosticsRunRoutineResponse::New();
    expected_response->id = 0;
    expected_response->status = crosapi::DiagnosticsRoutineStatusEnum::kReady;

    // Set the return value for a call to RunCpuStressRoutine.
    auto fake_service_impl = std::make_unique<FakeDiagnosticsService>();
    fake_service_impl->SetRunRoutineResponse(std::move(expected_response));

    // Set the expected runtime actions.
    fake_service_impl->SetExpectedLastPassedParameters(
        base::Value::Dict().Set("length_seconds", 120));
    fake_service_impl->SetExpectedLastCalledRoutine(
        crosapi::DiagnosticsRoutineEnum::kCpuStress);

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function runCpuStressRoutine() {
        const response =
          await chrome.os.diagnostics.runCpuStressRoutine(
            {
              length_seconds: 120
            }
          );
        chrome.test.assertEq({id: 0, status: "ready"}, response);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunDiskReadRoutineSuccess) {
  // Configure FakeDiagnosticsService.
  {
    auto expected_response = crosapi::DiagnosticsRunRoutineResponse::New();
    expected_response->id = 0;
    expected_response->status = crosapi::DiagnosticsRoutineStatusEnum::kReady;

    // Set the return value for a call to RunDiskReadRoutine.
    auto fake_service_impl = std::make_unique<FakeDiagnosticsService>();
    fake_service_impl->SetRunRoutineResponse(std::move(expected_response));

    // Set the expected runtime actions.
    fake_service_impl->SetExpectedLastPassedParameters(
        base::Value::Dict()
            .Set("type",
                 static_cast<int32_t>(
                     crosapi::DiagnosticsDiskReadRoutineTypeEnum::kLinearRead))
            .Set("length_seconds", 20)
            .Set("file_size_mb", 1000));
    fake_service_impl->SetExpectedLastCalledRoutine(
        crosapi::DiagnosticsRoutineEnum::kDiskRead);

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function runDiskReadRoutine() {
        const response =
          await chrome.os.diagnostics.runDiskReadRoutine(
            {
                type: "linear",
                length_seconds: 20,
                file_size_mb: 1000
            }
          );
        chrome.test.assertEq({id: 0, status: "ready"}, response);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunDnsResolutionRoutineSuccess) {
  // Configure FakeDiagnosticsService.
  {
    auto expected_response = crosapi::DiagnosticsRunRoutineResponse::New();
    expected_response->id = 0;
    expected_response->status = crosapi::DiagnosticsRoutineStatusEnum::kReady;

    // Set the return value for a call to RunDiskReadRoutine.
    auto fake_service_impl = std::make_unique<FakeDiagnosticsService>();
    fake_service_impl->SetRunRoutineResponse(std::move(expected_response));

    fake_service_impl->SetExpectedLastCalledRoutine(
        crosapi::DiagnosticsRoutineEnum::kDnsResolution);

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function runDnsResolutionRoutine() {
        const response =
          await chrome.os.diagnostics.runDnsResolutionRoutine();
        chrome.test.assertEq({id: 0, status: "ready"}, response);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunDnsResolverPresentRoutineSuccess) {
  // Configure FakeDiagnosticsService.
  {
    auto expected_response = crosapi::DiagnosticsRunRoutineResponse::New();
    expected_response->id = 0;
    expected_response->status = crosapi::DiagnosticsRoutineStatusEnum::kReady;

    // Set the return value for a call to RunDiskReadRoutine.
    auto fake_service_impl = std::make_unique<FakeDiagnosticsService>();
    fake_service_impl->SetRunRoutineResponse(std::move(expected_response));

    fake_service_impl->SetExpectedLastCalledRoutine(
        crosapi::DiagnosticsRoutineEnum::kDnsResolverPresent);

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function runDiskReadRoutine() {
        const response =
          await chrome.os.diagnostics.runDnsResolverPresentRoutine();
        chrome.test.assertEq({id: 0, status: "ready"}, response);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunEmmcLifetimeRoutineSuccess) {
  // Configure FakeDiagnosticsService.
  {
    auto expected_response = crosapi::DiagnosticsRunRoutineResponse::New();
    expected_response->id = 0;
    expected_response->status = crosapi::DiagnosticsRoutineStatusEnum::kReady;

    // Set the return value for a call to RunEmmcLifetimeRoutine.
    auto fake_service_impl = std::make_unique<FakeDiagnosticsService>();
    fake_service_impl->SetRunRoutineResponse(std::move(expected_response));

    fake_service_impl->SetExpectedLastCalledRoutine(
        crosapi::DiagnosticsRoutineEnum::kEmmcLifetime);
    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function runEmmcLifetimeRoutine() {
        const response =
          await chrome.os.diagnostics.runEmmcLifetimeRoutine();
          chrome.test.assertEq({id: 0, status: "ready"}, response);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunFingerprintAliveRoutineSuccess) {
  // Configure FakeDiagnosticsService.
  {
    auto expected_response = crosapi::DiagnosticsRunRoutineResponse::New();
    expected_response->id = 0;
    expected_response->status = crosapi::DiagnosticsRoutineStatusEnum::kReady;

    // Set the return value for a call to RunFingerprintAliveRoutine.
    auto fake_service_impl = std::make_unique<FakeDiagnosticsService>();
    fake_service_impl->SetRunRoutineResponse(std::move(expected_response));

    fake_service_impl->SetExpectedLastCalledRoutine(
        crosapi::DiagnosticsRoutineEnum::kFingerprintAlive);

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function runFingerprintAliveRoutine() {
        const response =
          await chrome.os.diagnostics.runFingerprintAliveRoutine();
        chrome.test.assertEq({id: 0, status: "ready"}, response);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunGatewayCanBePingedRoutineSuccess) {
  // Configure FakeDiagnosticsService.
  {
    auto expected_response = crosapi::DiagnosticsRunRoutineResponse::New();
    expected_response->id = 0;
    expected_response->status = crosapi::DiagnosticsRoutineStatusEnum::kReady;

    // Set the return value for a call to RunDiskReadRoutine.
    auto fake_service_impl = std::make_unique<FakeDiagnosticsService>();
    fake_service_impl->SetRunRoutineResponse(std::move(expected_response));

    fake_service_impl->SetExpectedLastCalledRoutine(
        crosapi::DiagnosticsRoutineEnum::kGatewayCanBePinged);

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function runGatewayCanBePingedRoutine() {
        const response =
          await chrome.os.diagnostics.runGatewayCanBePingedRoutine();
        chrome.test.assertEq({id: 0, status: "ready"}, response);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunLanConnectivityRoutineSuccess) {
  // Configure FakeDiagnosticsService.
  {
    auto expected_response = crosapi::DiagnosticsRunRoutineResponse::New();
    expected_response->id = 0;
    expected_response->status = crosapi::DiagnosticsRoutineStatusEnum::kReady;

    // Set the return value for a call to RunLanConnectivityRoutine.
    auto fake_service_impl = std::make_unique<FakeDiagnosticsService>();
    fake_service_impl->SetRunRoutineResponse(std::move(expected_response));

    // Set the expected called routine.
    fake_service_impl->SetExpectedLastCalledRoutine(
        crosapi::DiagnosticsRoutineEnum::kLanConnectivity);

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function runLanConnectivityRoutine() {
        const response =
          await chrome.os.diagnostics.runLanConnectivityRoutine();
        chrome.test.assertEq({id: 0, status: "ready"}, response);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunMemoryRoutineSuccess) {
  // Configure FakeDiagnosticsService.
  {
    auto expected_response = crosapi::DiagnosticsRunRoutineResponse::New();
    expected_response->id = 0;
    expected_response->status = crosapi::DiagnosticsRoutineStatusEnum::kReady;

    // Set the return value for a call to RunMemoryRoutine.
    auto fake_service_impl = std::make_unique<FakeDiagnosticsService>();
    fake_service_impl->SetRunRoutineResponse(std::move(expected_response));

    // Set the expected called routine.
    fake_service_impl->SetExpectedLastCalledRoutine(
        crosapi::DiagnosticsRoutineEnum::kMemory);

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function runMemoryRoutine() {
        const response =
          await chrome.os.diagnostics.runMemoryRoutine();
        chrome.test.assertEq({id: 0, status: "ready"}, response);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunNvmeSelfTestRoutineSuccess) {
  // Configure FakeDiagnosticsService.
  {
    auto expected_response = crosapi::DiagnosticsRunRoutineResponse::New();
    expected_response->id = 0;
    expected_response->status = crosapi::DiagnosticsRoutineStatusEnum::kReady;

    // Set the return value for a call to RunNvmeSelfTestRoutine.
    auto fake_service_impl = std::make_unique<FakeDiagnosticsService>();
    fake_service_impl->SetRunRoutineResponse(std::move(expected_response));

    // Set the expected runtime actions.
    fake_service_impl->SetExpectedLastPassedParameters(base::Value::Dict().Set(
        "test_type",
        static_cast<int32_t>(
            crosapi::DiagnosticsNvmeSelfTestTypeEnum::kShortSelfTest)));
    fake_service_impl->SetExpectedLastCalledRoutine(
        crosapi::DiagnosticsRoutineEnum::kNvmeSelfTest);

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function runNvmeSelfTestRoutine() {
        const response =
          await chrome.os.diagnostics.runNvmeSelfTestRoutine(
            {
              test_type: 'short_test'
            }
          );
        chrome.test.assertEq({id: 0, status: "ready"}, response);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunSensitiveSensorRoutineSuccess) {
  // Configure FakeDiagnosticsService.
  {
    auto expected_response = crosapi::DiagnosticsRunRoutineResponse::New();
    expected_response->id = 0;
    expected_response->status = crosapi::DiagnosticsRoutineStatusEnum::kReady;

    // Set the return value for a call to RunSmartctlCheckRoutine.
    auto fake_service_impl = std::make_unique<FakeDiagnosticsService>();
    fake_service_impl->SetRunRoutineResponse(std::move(expected_response));

    // Set the expected called routine.
    fake_service_impl->SetExpectedLastCalledRoutine(
        crosapi::DiagnosticsRoutineEnum::kSensitiveSensor);

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function runSensitiveSensorRoutine() {
        const response =
          await chrome.os.diagnostics.runSensitiveSensorRoutine();
        chrome.test.assertEq({id: 0, status: "ready"}, response);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunSignalStrengthRoutineSuccess) {
  // Configure FakeDiagnosticsService.
  {
    auto expected_response = crosapi::DiagnosticsRunRoutineResponse::New();
    expected_response->id = 0;
    expected_response->status = crosapi::DiagnosticsRoutineStatusEnum::kReady;

    // Set the return value for a call to RunSmartctlCheckRoutine.
    auto fake_service_impl = std::make_unique<FakeDiagnosticsService>();
    fake_service_impl->SetRunRoutineResponse(std::move(expected_response));

    // Set the expected called routine.
    fake_service_impl->SetExpectedLastCalledRoutine(
        crosapi::DiagnosticsRoutineEnum::kSignalStrength);

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function runSignalStrengthRoutine() {
        const response =
          await chrome.os.diagnostics.runSignalStrengthRoutine();
        chrome.test.assertEq({id: 0, status: "ready"}, response);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunSmartctlCheckRoutineSuccess) {
  // Configure FakeDiagnosticsService.
  {
    auto expected_response = crosapi::DiagnosticsRunRoutineResponse::New();
    expected_response->id = 0;
    expected_response->status = crosapi::DiagnosticsRoutineStatusEnum::kReady;

    // Set the return value for a call to RunSmartctlCheckRoutine.
    auto fake_service_impl = std::make_unique<FakeDiagnosticsService>();
    fake_service_impl->SetRunRoutineResponse(std::move(expected_response));

    // Set the expected called routine.
    fake_service_impl->SetExpectedLastCalledRoutine(
        crosapi::DiagnosticsRoutineEnum::kSmartctlCheck);

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function runSmartctlCheckRoutine() {
        const response =
          await chrome.os.diagnostics.runSmartctlCheckRoutine();
        chrome.test.assertEq({id: 0, status: "ready"}, response);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunSmartctlCheckRoutineWithPercentageUsedSuccess) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Only run this tests when Ash does support the new parameter for
  // SmartctlCheck. The parameter is supported from version 1 onwards.
  if (!InterfaceVersionHigherOrEqual<crosapi::DiagnosticsService>(1)) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Configure FakeDiagnosticsService.
  {
    auto expected_response = crosapi::DiagnosticsRunRoutineResponse::New();
    expected_response->id = 0;
    expected_response->status = crosapi::DiagnosticsRoutineStatusEnum::kReady;

    // Set the return value for a call to RunSmartctlCheckRoutine.
    auto fake_service_impl = std::make_unique<FakeDiagnosticsService>();
    fake_service_impl->SetRunRoutineResponse(std::move(expected_response));

    fake_service_impl->SetExpectedLastPassedParameters(
        base::Value::Dict().Set("percentage_used_threshold", 42));

    // Set the expected called routine.
    fake_service_impl->SetExpectedLastCalledRoutine(
        crosapi::DiagnosticsRoutineEnum::kSmartctlCheckWithPercentageUsed);

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function runSmartctlCheckRoutine() {
        const response =
          await chrome.os.diagnostics.runSmartctlCheckRoutine(
            {
              percentage_used_threshold: 42
            }
          );
        chrome.test.assertEq({id: 0, status: "ready"}, response);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunUfsLifetimeRoutineSuccess) {
  // Configure FakeDiagnosticsService.
  {
    auto expected_response = crosapi::DiagnosticsRunRoutineResponse::New();
    expected_response->id = 0;
    expected_response->status = crosapi::DiagnosticsRoutineStatusEnum::kReady;

    // Set the return value for a call to RunUfsLifetimeRoutine.
    auto fake_service_impl = std::make_unique<FakeDiagnosticsService>();
    fake_service_impl->SetRunRoutineResponse(std::move(expected_response));

    // Set the expected called routine.
    fake_service_impl->SetExpectedLastCalledRoutine(
        crosapi::DiagnosticsRoutineEnum::kUfsLifetime);

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function runUfsLifetimeRoutine() {
        const response =
          await chrome.os.diagnostics.runUfsLifetimeRoutine();
        chrome.test.assertEq({id: 0, status: "ready"}, response);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunPowerButtonRoutineSuccess) {
  // Configure FakeDiagnosticsService.
  {
    auto expected_response = crosapi::DiagnosticsRunRoutineResponse::New();
    expected_response->id = 0;
    expected_response->status = crosapi::DiagnosticsRoutineStatusEnum::kReady;

    // Set the return value for a call to RunPowerButtonRoutine.
    auto fake_service_impl = std::make_unique<FakeDiagnosticsService>();
    fake_service_impl->SetRunRoutineResponse(std::move(expected_response));

    fake_service_impl->SetExpectedLastPassedParameters(
        base::Value::Dict().Set("timeout_seconds", 10));

    // Set the expected called routine.
    fake_service_impl->SetExpectedLastCalledRoutine(
        crosapi::DiagnosticsRoutineEnum::kPowerButton);

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function runPowerButtonRoutine() {
        const response =
          await chrome.os.diagnostics.runPowerButtonRoutine(
            {
              timeout_seconds: 10
            }
          );
        chrome.test.assertEq({id: 0, status: "ready"}, response);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunAudioDriverRoutineSuccess) {
  // Configure FakeDiagnosticsService.
  {
    auto expected_response = crosapi::DiagnosticsRunRoutineResponse::New();
    expected_response->id = 0;
    expected_response->status = crosapi::DiagnosticsRoutineStatusEnum::kReady;

    // Set the return value for a call to RunAudioDriverRoutine.
    auto fake_service_impl = std::make_unique<FakeDiagnosticsService>();
    fake_service_impl->SetRunRoutineResponse(std::move(expected_response));

    // Set the expected called routine.
    fake_service_impl->SetExpectedLastCalledRoutine(
        crosapi::DiagnosticsRoutineEnum::kAudioDriver);

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function runAudioDriverRoutine() {
        const response =
          await chrome.os.diagnostics.runAudioDriverRoutine();
        chrome.test.assertEq({id: 0, status: "ready"}, response);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunFanRoutineSuccess) {
  // Configure FakeDiagnosticsService.
  {
    auto expected_response = crosapi::DiagnosticsRunRoutineResponse::New();
    expected_response->id = 0;
    expected_response->status = crosapi::DiagnosticsRoutineStatusEnum::kReady;

    // Set the return value for a call to RunFanRoutine.
    auto fake_service_impl = std::make_unique<FakeDiagnosticsService>();
    fake_service_impl->SetRunRoutineResponse(std::move(expected_response));

    // Set the expected called routine.
    fake_service_impl->SetExpectedLastCalledRoutine(
        crosapi::DiagnosticsRoutineEnum::kFan);

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function runFanRoutine() {
        const response =
          await chrome.os.diagnostics.runFanRoutine();
        chrome.test.assertEq({id: 0, status: "ready"}, response);
        chrome.test.succeed();
      }
    ]);
  )");
}

class NoExtraPermissionTelemetryExtensionDiagnosticsApiBrowserTest
    : public TelemetryExtensionDiagnosticsApiBrowserTest {
 public:
  NoExtraPermissionTelemetryExtensionDiagnosticsApiBrowserTest() = default;

 protected:
  std::string GetManifestFile(const std::string& manifest_key,
                              const std::string& matches_origin) override {
    return base::StringPrintf(R"(
      {
        "key": "%s",
        "name": "Test Telemetry Extension",
        "version": "1",
        "manifest_version": 3,
        "chromeos_system_extension": {},
        "background": {
          "service_worker": "sw.js"
        },
        "permissions": [ "os.diagnostics" ],
        "externally_connectable": {
          "matches": [
            "%s"
          ]
        },
        "options_page": "options.html"
      }
    )",
                              manifest_key.c_str(), matches_origin.c_str());
  }
};

IN_PROC_BROWSER_TEST_F(
    NoExtraPermissionTelemetryExtensionDiagnosticsApiBrowserTest,
    RunBluetoothScanningRoutineWithoutPermissionFail) {
  // Configure FakeDiagnosticsService.
  {
    auto fake_service_impl = std::make_unique<FakeDiagnosticsService>();
    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function runBluetoothScanningRoutineNotWorking() {
        await chrome.test.assertPromiseRejects(
          chrome.os.diagnostics.runBluetoothScanningRoutine({
            length_seconds: 10
          }),
          'Error: Unauthorized access to ' +
          'chrome.os.diagnostics.runBluetoothScanningRoutine. Extension ' +
          'doesn\'t have the permission.'
        );
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(
    NoExtraPermissionTelemetryExtensionDiagnosticsApiBrowserTest,
    RunBluetoothPairingRoutineWithoutPermissionFail) {
  // Configure FakeDiagnosticsService.
  {
    auto fake_service_impl = std::make_unique<FakeDiagnosticsService>();
    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function runBluetoothPairingRoutineNotWorking() {
        await chrome.test.assertPromiseRejects(
          chrome.os.diagnostics.runBluetoothPairingRoutine({
            peripheral_id: "HEALTHD_TEST_ID"
          }),
          'Error: Unauthorized access to ' +
          'chrome.os.diagnostics.runBluetoothPairingRoutine. Extension ' +
          'doesn\'t have the permission.'
        );
        chrome.test.succeed();
      }
    ]);
  )");
}

}  // namespace chromeos
