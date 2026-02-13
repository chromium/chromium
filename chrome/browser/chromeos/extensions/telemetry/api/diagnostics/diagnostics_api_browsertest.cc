// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/common/base_telemetry_extension_browser_test.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/diagnostics/fake_diagnostics_service.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/diagnostics/fake_diagnostics_service_factory.h"
#include "chromeos/ash/components/telemetry_extension/diagnostics/diagnostics_service_ash.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_diagnostics.mojom-shared.h"
#include "chromeos/crosapi/mojom/diagnostics_service.mojom.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {
namespace crosapi = ::crosapi::mojom;
}  // namespace

class TelemetryExtensionDiagnosticsApiBrowserTest
    : public BaseTelemetryExtensionBrowserTest {
 public:
  TelemetryExtensionDiagnosticsApiBrowserTest() {
    ash::DiagnosticsServiceAsh::Factory::SetForTesting(
        &fake_diagnostics_service_factory_);
  }

  ~TelemetryExtensionDiagnosticsApiBrowserTest() override = default;

  TelemetryExtensionDiagnosticsApiBrowserTest(
      const TelemetryExtensionDiagnosticsApiBrowserTest&) = delete;
  TelemetryExtensionDiagnosticsApiBrowserTest& operator=(
      const TelemetryExtensionDiagnosticsApiBrowserTest&) = delete;

 protected:
  void SetServiceForTesting(
      std::unique_ptr<FakeDiagnosticsService> fake_diagnostics_service_impl) {
    fake_diagnostics_service_factory_.SetCreateInstanceResponse(
        std::move(fake_diagnostics_service_impl));
  }

  // Set up FakeCrosHealthd's response to RunRoutine requests.
  void SetRunRoutineResponse(
      int id,
      ash::cros_healthd::mojom::DiagnosticRoutineStatusEnum status) {
    auto response =
        ash::cros_healthd::mojom::RunRoutineResponse::New(id, status);
    ash::cros_healthd::FakeCrosHealthd::Get()->SetRunRoutineResponseForTesting(
        response);
  }

  void SetExpectedLastPassedParameters(base::DictValue dict) {
    ash::cros_healthd::FakeCrosHealthd::Get()
        ->SetExpectedLastPassedDiagnosticsParametersForTesting(std::move(dict));
  }

  FakeDiagnosticsServiceFactory fake_diagnostics_service_factory_;
};

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       GetAvailableRoutinesSuccess) {
  SetServiceForTesting(std::make_unique<FakeDiagnosticsService>());
  ash::cros_healthd::FakeCrosHealthd::Get()->SetAvailableRoutinesForTesting({
      ash::cros_healthd::mojom::DiagnosticRoutineEnum::kAcPower,
      ash::cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryCapacity,
      ash::cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryCharge,
      ash::cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryDischarge,
      ash::cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryHealth,
      ash::cros_healthd::mojom::DiagnosticRoutineEnum::kCpuCache,
      ash::cros_healthd::mojom::DiagnosticRoutineEnum::kFloatingPointAccuracy,
      ash::cros_healthd::mojom::DiagnosticRoutineEnum::kPrimeSearch,
      ash::cros_healthd::mojom::DiagnosticRoutineEnum::kCpuStress,
      ash::cros_healthd::mojom::DiagnosticRoutineEnum::kDiskRead,
      ash::cros_healthd::mojom::DiagnosticRoutineEnum::kDnsResolution,
      ash::cros_healthd::mojom::DiagnosticRoutineEnum::kDnsResolverPresent,
      ash::cros_healthd::mojom::DiagnosticRoutineEnum::kLanConnectivity,
      ash::cros_healthd::mojom::DiagnosticRoutineEnum::kMemory,
      ash::cros_healthd::mojom::DiagnosticRoutineEnum::kSignalStrength,
      ash::cros_healthd::mojom::DiagnosticRoutineEnum::kGatewayCanBePinged,
      ash::cros_healthd::mojom::DiagnosticRoutineEnum::kSmartctlCheck,
      ash::cros_healthd::mojom::DiagnosticRoutineEnum::kSensitiveSensor,
      ash::cros_healthd::mojom::DiagnosticRoutineEnum::kNvmeSelfTest,
      ash::cros_healthd::mojom::DiagnosticRoutineEnum::kFingerprintAlive,
      ash::cros_healthd::mojom::DiagnosticRoutineEnum::
          kSmartctlCheckWithPercentageUsed,
      ash::cros_healthd::mojom::DiagnosticRoutineEnum::kEmmcLifetime,
      ash::cros_healthd::mojom::DiagnosticRoutineEnum::kBluetoothPower,
      ash::cros_healthd::mojom::DiagnosticRoutineEnum::kUfsLifetime,
      ash::cros_healthd::mojom::DiagnosticRoutineEnum::kPowerButton,
      ash::cros_healthd::mojom::DiagnosticRoutineEnum::kAudioDriver,
      ash::cros_healthd::mojom::DiagnosticRoutineEnum::kBluetoothDiscovery,
      ash::cros_healthd::mojom::DiagnosticRoutineEnum::kBluetoothScanning,
      ash::cros_healthd::mojom::DiagnosticRoutineEnum::kBluetoothPairing,
      ash::cros_healthd::mojom::DiagnosticRoutineEnum::kFan,
  });

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
        base::DictValue()
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
        base::DictValue()
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
  SetServiceForTesting(std::make_unique<FakeDiagnosticsService>());

  base::DictValue expected_parameters;
  expected_parameters.Set(
      "expected_status",
      static_cast<int32_t>(
          ash::cros_healthd::mojom::AcPowerStatusEnum::kConnected));
  expected_parameters.Set("expected_power_type", "ac_power");
  SetExpectedLastPassedParameters(std::move(expected_parameters));
  SetRunRoutineResponse(
      0, ash::cros_healthd::mojom::DiagnosticRoutineStatusEnum::kReady);

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

  EXPECT_EQ(ash::cros_healthd::FakeCrosHealthd::Get()->GetLastRunRoutine(),
            ash::cros_healthd::mojom::DiagnosticRoutineEnum::kAcPower);
  EXPECT_TRUE(ash::cros_healthd::FakeCrosHealthd::Get()
                  ->DidExpectedDiagnosticsParametersMatch());
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunBatteryCapacityRoutineSuccess) {
  SetServiceForTesting(std::make_unique<FakeDiagnosticsService>());
  SetRunRoutineResponse(
      0, ash::cros_healthd::mojom::DiagnosticRoutineStatusEnum::kReady);

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

  EXPECT_EQ(ash::cros_healthd::FakeCrosHealthd::Get()->GetLastRunRoutine(),
            ash::cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryCapacity);
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunBatteryChargeRoutineSuccess) {
  SetServiceForTesting(std::make_unique<FakeDiagnosticsService>());

  base::DictValue expected_parameters;
  expected_parameters.Set("length_seconds", 1000);
  expected_parameters.Set("minimum_charge_percent_required", 1);
  SetExpectedLastPassedParameters(std::move(expected_parameters));
  SetRunRoutineResponse(
      0, ash::cros_healthd::mojom::DiagnosticRoutineStatusEnum::kReady);

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

  EXPECT_EQ(ash::cros_healthd::FakeCrosHealthd::Get()->GetLastRunRoutine(),
            ash::cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryCharge);
  EXPECT_TRUE(ash::cros_healthd::FakeCrosHealthd::Get()
                  ->DidExpectedDiagnosticsParametersMatch());
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunBatteryDischargeRoutineSuccess) {
  SetServiceForTesting(std::make_unique<FakeDiagnosticsService>());

  base::DictValue expected_parameters;
  expected_parameters.Set("length_seconds", 10);
  expected_parameters.Set("maximum_discharge_percent_allowed", 15);
  SetExpectedLastPassedParameters(std::move(expected_parameters));
  SetRunRoutineResponse(
      0, ash::cros_healthd::mojom::DiagnosticRoutineStatusEnum::kReady);

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

  EXPECT_EQ(ash::cros_healthd::FakeCrosHealthd::Get()->GetLastRunRoutine(),
            ash::cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryDischarge);
  EXPECT_TRUE(ash::cros_healthd::FakeCrosHealthd::Get()
                  ->DidExpectedDiagnosticsParametersMatch());
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunBatteryHealthRoutineSuccess) {
  SetServiceForTesting(std::make_unique<FakeDiagnosticsService>());
  SetRunRoutineResponse(
      0, ash::cros_healthd::mojom::DiagnosticRoutineStatusEnum::kReady);

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

  EXPECT_EQ(ash::cros_healthd::FakeCrosHealthd::Get()->GetLastRunRoutine(),
            ash::cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryHealth);
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunBluetoothDiscoveryRoutineSuccess) {
  SetServiceForTesting(std::make_unique<FakeDiagnosticsService>());
  SetRunRoutineResponse(
      0, ash::cros_healthd::mojom::DiagnosticRoutineStatusEnum::kReady);

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

  EXPECT_EQ(
      ash::cros_healthd::FakeCrosHealthd::Get()->GetLastRunRoutine(),
      ash::cros_healthd::mojom::DiagnosticRoutineEnum::kBluetoothDiscovery);
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunBluetoothScanningRoutineSuccess) {
  SetServiceForTesting(std::make_unique<FakeDiagnosticsService>());
  SetRunRoutineResponse(
      0, ash::cros_healthd::mojom::DiagnosticRoutineStatusEnum::kReady);

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

  EXPECT_EQ(
      ash::cros_healthd::FakeCrosHealthd::Get()->GetLastRunRoutine(),
      ash::cros_healthd::mojom::DiagnosticRoutineEnum::kBluetoothScanning);
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunBluetoothPairingRoutineSuccess) {
  SetServiceForTesting(std::make_unique<FakeDiagnosticsService>());
  SetRunRoutineResponse(
      0, ash::cros_healthd::mojom::DiagnosticRoutineStatusEnum::kReady);

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

  EXPECT_EQ(ash::cros_healthd::FakeCrosHealthd::Get()->GetLastRunRoutine(),
            ash::cros_healthd::mojom::DiagnosticRoutineEnum::kBluetoothPairing);
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunBluetoothPowerRoutineSuccess) {
  SetServiceForTesting(std::make_unique<FakeDiagnosticsService>());
  SetRunRoutineResponse(
      0, ash::cros_healthd::mojom::DiagnosticRoutineStatusEnum::kReady);

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

  EXPECT_EQ(ash::cros_healthd::FakeCrosHealthd::Get()->GetLastRunRoutine(),
            ash::cros_healthd::mojom::DiagnosticRoutineEnum::kBluetoothPower);
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunCpuCacheRoutineSuccess) {
  SetServiceForTesting(std::make_unique<FakeDiagnosticsService>());

  base::DictValue expected_parameters;
  expected_parameters.Set("length_seconds", 120);
  SetExpectedLastPassedParameters(std::move(expected_parameters));
  SetRunRoutineResponse(
      0, ash::cros_healthd::mojom::DiagnosticRoutineStatusEnum::kReady);

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

  EXPECT_EQ(ash::cros_healthd::FakeCrosHealthd::Get()->GetLastRunRoutine(),
            ash::cros_healthd::mojom::DiagnosticRoutineEnum::kCpuCache);
  EXPECT_TRUE(ash::cros_healthd::FakeCrosHealthd::Get()
                  ->DidExpectedDiagnosticsParametersMatch());
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunCpuFloatingPointAccuracyRoutineSuccess) {
  SetServiceForTesting(std::make_unique<FakeDiagnosticsService>());

  base::DictValue expected_parameters;
  expected_parameters.Set("length_seconds", 120);
  SetExpectedLastPassedParameters(std::move(expected_parameters));
  SetRunRoutineResponse(
      0, ash::cros_healthd::mojom::DiagnosticRoutineStatusEnum::kReady);

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

  EXPECT_EQ(
      ash::cros_healthd::FakeCrosHealthd::Get()->GetLastRunRoutine(),
      ash::cros_healthd::mojom::DiagnosticRoutineEnum::kFloatingPointAccuracy);
  EXPECT_TRUE(ash::cros_healthd::FakeCrosHealthd::Get()
                  ->DidExpectedDiagnosticsParametersMatch());
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunCpuPrimeSearchRoutineSuccess) {
  SetServiceForTesting(std::make_unique<FakeDiagnosticsService>());

  base::DictValue expected_parameters;
  expected_parameters.Set("length_seconds", 120);
  SetExpectedLastPassedParameters(std::move(expected_parameters));
  SetRunRoutineResponse(
      0, ash::cros_healthd::mojom::DiagnosticRoutineStatusEnum::kReady);

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

  EXPECT_EQ(ash::cros_healthd::FakeCrosHealthd::Get()->GetLastRunRoutine(),
            ash::cros_healthd::mojom::DiagnosticRoutineEnum::kPrimeSearch);
  EXPECT_TRUE(ash::cros_healthd::FakeCrosHealthd::Get()
                  ->DidExpectedDiagnosticsParametersMatch());
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunCpuStressRoutineSuccess) {
  SetServiceForTesting(std::make_unique<FakeDiagnosticsService>());

  base::DictValue expected_parameters;
  expected_parameters.Set("length_seconds", 120);
  SetExpectedLastPassedParameters(std::move(expected_parameters));
  SetRunRoutineResponse(
      0, ash::cros_healthd::mojom::DiagnosticRoutineStatusEnum::kReady);

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

  EXPECT_EQ(ash::cros_healthd::FakeCrosHealthd::Get()->GetLastRunRoutine(),
            ash::cros_healthd::mojom::DiagnosticRoutineEnum::kCpuStress);
  EXPECT_TRUE(ash::cros_healthd::FakeCrosHealthd::Get()
                  ->DidExpectedDiagnosticsParametersMatch());
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunDiskReadRoutineSuccess) {
  SetServiceForTesting(std::make_unique<FakeDiagnosticsService>());

  base::DictValue expected_parameters;
  expected_parameters.Set(
      "type",
      static_cast<int32_t>(
          ash::cros_healthd::mojom::DiskReadRoutineTypeEnum::kLinearRead));
  expected_parameters.Set("length_seconds", 20);
  expected_parameters.Set("file_size_mb", 1000);
  SetExpectedLastPassedParameters(std::move(expected_parameters));
  SetRunRoutineResponse(
      0, ash::cros_healthd::mojom::DiagnosticRoutineStatusEnum::kReady);

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

  EXPECT_EQ(ash::cros_healthd::FakeCrosHealthd::Get()->GetLastRunRoutine(),
            ash::cros_healthd::mojom::DiagnosticRoutineEnum::kDiskRead);
  EXPECT_TRUE(ash::cros_healthd::FakeCrosHealthd::Get()
                  ->DidExpectedDiagnosticsParametersMatch());
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunDnsResolutionRoutineSuccess) {
  SetServiceForTesting(std::make_unique<FakeDiagnosticsService>());
  SetRunRoutineResponse(
      0, ash::cros_healthd::mojom::DiagnosticRoutineStatusEnum::kReady);

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

  EXPECT_EQ(ash::cros_healthd::FakeCrosHealthd::Get()->GetLastRunRoutine(),
            ash::cros_healthd::mojom::DiagnosticRoutineEnum::kDnsResolution);
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunDnsResolverPresentRoutineSuccess) {
  SetServiceForTesting(std::make_unique<FakeDiagnosticsService>());
  SetRunRoutineResponse(
      0, ash::cros_healthd::mojom::DiagnosticRoutineStatusEnum::kReady);

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

  EXPECT_EQ(
      ash::cros_healthd::FakeCrosHealthd::Get()->GetLastRunRoutine(),
      ash::cros_healthd::mojom::DiagnosticRoutineEnum::kDnsResolverPresent);
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunEmmcLifetimeRoutineSuccess) {
  SetServiceForTesting(std::make_unique<FakeDiagnosticsService>());
  SetRunRoutineResponse(
      0, ash::cros_healthd::mojom::DiagnosticRoutineStatusEnum::kReady);

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

  EXPECT_EQ(ash::cros_healthd::FakeCrosHealthd::Get()->GetLastRunRoutine(),
            ash::cros_healthd::mojom::DiagnosticRoutineEnum::kEmmcLifetime);
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunFingerprintAliveRoutineSuccess) {
  SetServiceForTesting(std::make_unique<FakeDiagnosticsService>());
  SetRunRoutineResponse(
      0, ash::cros_healthd::mojom::DiagnosticRoutineStatusEnum::kReady);

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

  EXPECT_EQ(ash::cros_healthd::FakeCrosHealthd::Get()->GetLastRunRoutine(),
            ash::cros_healthd::mojom::DiagnosticRoutineEnum::kFingerprintAlive);
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunGatewayCanBePingedRoutineSuccess) {
  SetServiceForTesting(std::make_unique<FakeDiagnosticsService>());
  SetRunRoutineResponse(
      0, ash::cros_healthd::mojom::DiagnosticRoutineStatusEnum::kReady);

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

  EXPECT_EQ(
      ash::cros_healthd::FakeCrosHealthd::Get()->GetLastRunRoutine(),
      ash::cros_healthd::mojom::DiagnosticRoutineEnum::kGatewayCanBePinged);
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunLanConnectivityRoutineSuccess) {
  SetServiceForTesting(std::make_unique<FakeDiagnosticsService>());
  SetRunRoutineResponse(
      0, ash::cros_healthd::mojom::DiagnosticRoutineStatusEnum::kReady);

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

  EXPECT_EQ(ash::cros_healthd::FakeCrosHealthd::Get()->GetLastRunRoutine(),
            ash::cros_healthd::mojom::DiagnosticRoutineEnum::kLanConnectivity);
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunMemoryRoutineSuccess) {
  SetServiceForTesting(std::make_unique<FakeDiagnosticsService>());
  SetRunRoutineResponse(
      0, ash::cros_healthd::mojom::DiagnosticRoutineStatusEnum::kReady);

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

  EXPECT_EQ(ash::cros_healthd::FakeCrosHealthd::Get()->GetLastRunRoutine(),
            ash::cros_healthd::mojom::DiagnosticRoutineEnum::kMemory);
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunNvmeSelfTestRoutineSuccess) {
  SetServiceForTesting(std::make_unique<FakeDiagnosticsService>());

  base::DictValue expected_parameters;
  expected_parameters.Set(
      "nvme_self_test_type",
      static_cast<int32_t>(
          ash::cros_healthd::mojom::NvmeSelfTestTypeEnum::kShortSelfTest));
  SetExpectedLastPassedParameters(std::move(expected_parameters));
  SetRunRoutineResponse(
      0, ash::cros_healthd::mojom::DiagnosticRoutineStatusEnum::kReady);

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

  EXPECT_EQ(ash::cros_healthd::FakeCrosHealthd::Get()->GetLastRunRoutine(),
            ash::cros_healthd::mojom::DiagnosticRoutineEnum::kNvmeSelfTest);
  EXPECT_TRUE(ash::cros_healthd::FakeCrosHealthd::Get()
                  ->DidExpectedDiagnosticsParametersMatch());
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunSensitiveSensorRoutineSuccess) {
  SetServiceForTesting(std::make_unique<FakeDiagnosticsService>());
  SetRunRoutineResponse(
      0, ash::cros_healthd::mojom::DiagnosticRoutineStatusEnum::kReady);

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

  EXPECT_EQ(ash::cros_healthd::FakeCrosHealthd::Get()->GetLastRunRoutine(),
            ash::cros_healthd::mojom::DiagnosticRoutineEnum::kSensitiveSensor);
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunSignalStrengthRoutineSuccess) {
  SetServiceForTesting(std::make_unique<FakeDiagnosticsService>());
  SetRunRoutineResponse(
      0, ash::cros_healthd::mojom::DiagnosticRoutineStatusEnum::kReady);

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

  EXPECT_EQ(ash::cros_healthd::FakeCrosHealthd::Get()->GetLastRunRoutine(),
            ash::cros_healthd::mojom::DiagnosticRoutineEnum::kSignalStrength);
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunSmartctlCheckRoutineSuccess) {
  SetServiceForTesting(std::make_unique<FakeDiagnosticsService>());
  SetRunRoutineResponse(
      0, ash::cros_healthd::mojom::DiagnosticRoutineStatusEnum::kReady);

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

  EXPECT_EQ(ash::cros_healthd::FakeCrosHealthd::Get()->GetLastRunRoutine(),
            ash::cros_healthd::mojom::DiagnosticRoutineEnum::
                kSmartctlCheckWithPercentageUsed);
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunSmartctlCheckRoutineWithPercentageUsedSuccess) {
  SetServiceForTesting(std::make_unique<FakeDiagnosticsService>());

  base::DictValue expected_parameters;
  expected_parameters.Set("percentage_used_threshold", 42);
  SetExpectedLastPassedParameters(std::move(expected_parameters));
  SetRunRoutineResponse(
      0, ash::cros_healthd::mojom::DiagnosticRoutineStatusEnum::kReady);

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

  EXPECT_EQ(ash::cros_healthd::FakeCrosHealthd::Get()->GetLastRunRoutine(),
            ash::cros_healthd::mojom::DiagnosticRoutineEnum::
                kSmartctlCheckWithPercentageUsed);
  EXPECT_TRUE(ash::cros_healthd::FakeCrosHealthd::Get()
                  ->DidExpectedDiagnosticsParametersMatch());
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunUfsLifetimeRoutineSuccess) {
  SetServiceForTesting(std::make_unique<FakeDiagnosticsService>());
  SetRunRoutineResponse(
      0, ash::cros_healthd::mojom::DiagnosticRoutineStatusEnum::kReady);

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

  EXPECT_EQ(ash::cros_healthd::FakeCrosHealthd::Get()->GetLastRunRoutine(),
            ash::cros_healthd::mojom::DiagnosticRoutineEnum::kUfsLifetime);
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunPowerButtonRoutineSuccess) {
  SetServiceForTesting(std::make_unique<FakeDiagnosticsService>());

  base::DictValue expected_parameters;
  expected_parameters.Set("timeout_seconds", 10);
  SetExpectedLastPassedParameters(std::move(expected_parameters));
  SetRunRoutineResponse(
      0, ash::cros_healthd::mojom::DiagnosticRoutineStatusEnum::kReady);

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

  EXPECT_EQ(ash::cros_healthd::FakeCrosHealthd::Get()->GetLastRunRoutine(),
            ash::cros_healthd::mojom::DiagnosticRoutineEnum::kPowerButton);
  EXPECT_TRUE(ash::cros_healthd::FakeCrosHealthd::Get()
                  ->DidExpectedDiagnosticsParametersMatch());
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunAudioDriverRoutineSuccess) {
  SetServiceForTesting(std::make_unique<FakeDiagnosticsService>());
  SetRunRoutineResponse(
      0, ash::cros_healthd::mojom::DiagnosticRoutineStatusEnum::kReady);

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

  EXPECT_EQ(ash::cros_healthd::FakeCrosHealthd::Get()->GetLastRunRoutine(),
            ash::cros_healthd::mojom::DiagnosticRoutineEnum::kAudioDriver);
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunFanRoutineSuccess) {
  SetServiceForTesting(std::make_unique<FakeDiagnosticsService>());
  SetRunRoutineResponse(
      0, ash::cros_healthd::mojom::DiagnosticRoutineStatusEnum::kReady);

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

  EXPECT_EQ(ash::cros_healthd::FakeCrosHealthd::Get()->GetLastRunRoutine(),
            ash::cros_healthd::mojom::DiagnosticRoutineEnum::kFan);
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
