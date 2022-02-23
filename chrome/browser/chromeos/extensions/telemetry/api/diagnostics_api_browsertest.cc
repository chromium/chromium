// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/wilco_dtc_supportd/mojo_utils.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/base_telemetry_extension_browser_test.h"
#include "chromeos/dbus/cros_healthd/fake_cros_healthd_client.h"
#include "chromeos/dbus/cros_healthd/fake_cros_healthd_service.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_diagnostics.mojom.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {

using TelemetryExtensionDiagnosticsApiBrowserTest =
    BaseTelemetryExtensionBrowserTest;

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       GetAvailableRoutinesSuccess) {
  cros_healthd::FakeCrosHealthdClient::Get()->SetAvailableRoutinesForTesting({
      cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryCapacity,
      cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryCharge,
      cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryDischarge,
      cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryHealth,
      cros_healthd::mojom::DiagnosticRoutineEnum::kCpuCache,
      cros_healthd::mojom::DiagnosticRoutineEnum::kFloatingPointAccuracy,
      cros_healthd::mojom::DiagnosticRoutineEnum::kPrimeSearch,
      cros_healthd::mojom::DiagnosticRoutineEnum::kCpuStress,
      cros_healthd::mojom::DiagnosticRoutineEnum::kDiskRead,
      cros_healthd::mojom::DiagnosticRoutineEnum::kMemory,
      cros_healthd::mojom::DiagnosticRoutineEnum::kNvmeWearLevel,
      cros_healthd::mojom::DiagnosticRoutineEnum::kSmartctlCheck,
  });

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function getAvailableRoutines() {
        const response =
          await chrome.os.diagnostics.getAvailableRoutines();
        chrome.test.assertEq(
          {
            routines: [
              "battery_capacity",
              "battery_charge",
              "battery_discharge",
              "battery_health",
              "cpu_cache",
              "cpu_floating_point_accuracy",
              "cpu_prime_search",
              "cpu_stress",
              "disk_read",
              "memory",
              "nvme_wear_level",
              "smartctl_check"
            ]
          }, response);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       GetRoutineUpdateNonInteractiveSuccess) {
  // Configure FakeCrosHealthd to return noninteractive response for
  // GetRoutineUpdate().
  auto nonInteractiveRoutineUpdate =
      cros_healthd::mojom::NonInteractiveRoutineUpdate::New();
  nonInteractiveRoutineUpdate->status =
      cros_healthd::mojom::DiagnosticRoutineStatusEnum::kReady;
  nonInteractiveRoutineUpdate->status_message = "Routine ran by Google.";

  auto routineUpdateUnion = cros_healthd::mojom::RoutineUpdateUnion::New();
  routineUpdateUnion->set_noninteractive_update(
      std::move(nonInteractiveRoutineUpdate));

  auto response = cros_healthd::mojom::RoutineUpdate::New();
  response->progress_percent = 87;
  response->routine_update_union = std::move(routineUpdateUnion);

  cros_healthd::FakeCrosHealthdClient::Get()
      ->SetGetRoutineUpdateResponseForTesting(response);

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

  // Verify that CrosHealthd is called with the correct parameters.
  absl::optional<cros_healthd::FakeCrosHealthdService::RoutineUpdateParams>
      update_params =
          cros_healthd::FakeCrosHealthdClient::Get()->GetRoutineUpdateParams();

  ASSERT_TRUE(update_params.has_value());
  EXPECT_EQ(123456, update_params->id);
  EXPECT_EQ(cros_healthd::mojom::DiagnosticRoutineCommandEnum::kGetStatus,
            update_params->command);
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       GetRoutineUpdateInteractiveSuccess) {
  // Configure FakeCrosHealthd to return interactive response for
  // GetRoutineUpdate().
  auto interactiveRoutineUpdate =
      cros_healthd::mojom::InteractiveRoutineUpdate::New();
  interactiveRoutineUpdate->user_message =
      cros_healthd::mojom::DiagnosticRoutineUserMessageEnum::kUnplugACPower;

  auto routineUpdateUnion = cros_healthd::mojom::RoutineUpdateUnion::New();
  routineUpdateUnion->set_interactive_update(
      std::move(interactiveRoutineUpdate));

  auto response = cros_healthd::mojom::RoutineUpdate::New();
  response->progress_percent = 50;
  response->output = ash::MojoUtils::CreateReadOnlySharedMemoryMojoHandle(
      "routine is running...");
  response->routine_update_union = std::move(routineUpdateUnion);

  cros_healthd::FakeCrosHealthdClient::Get()
      ->SetGetRoutineUpdateResponseForTesting(response);

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

  // Verify that CrosHealthd is called with the correct parameters.
  absl::optional<cros_healthd::FakeCrosHealthdService::RoutineUpdateParams>
      update_params =
          cros_healthd::FakeCrosHealthdClient::Get()->GetRoutineUpdateParams();

  ASSERT_TRUE(update_params.has_value());
  EXPECT_EQ(654321, update_params->id);
  EXPECT_EQ(cros_healthd::mojom::DiagnosticRoutineCommandEnum::kRemove,
            update_params->command);
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunBatteryCapacityRoutineSuccess) {
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
  EXPECT_EQ(cros_healthd::FakeCrosHealthdClient::Get()->GetLastRunRoutine(),
            cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryCapacity);
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunBatteryChargeRoutineSuccess) {
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
  EXPECT_EQ(cros_healthd::FakeCrosHealthdClient::Get()->GetLastRunRoutine(),
            cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryCharge);
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunBatteryDischargeRoutineSuccess) {
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
  EXPECT_EQ(cros_healthd::FakeCrosHealthdClient::Get()->GetLastRunRoutine(),
            cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryDischarge);
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunBatteryHealthRoutineSuccess) {
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
  EXPECT_EQ(cros_healthd::FakeCrosHealthdClient::Get()->GetLastRunRoutine(),
            cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryHealth);
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunCpuCacheRoutineSuccess) {
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
  EXPECT_EQ(cros_healthd::FakeCrosHealthdClient::Get()->GetLastRunRoutine(),
            cros_healthd::mojom::DiagnosticRoutineEnum::kCpuCache);
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunCpuFloatingPointAccuracyRoutineSuccess) {
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
  EXPECT_EQ(cros_healthd::FakeCrosHealthdClient::Get()->GetLastRunRoutine(),
            cros_healthd::mojom::DiagnosticRoutineEnum::kFloatingPointAccuracy);
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunCpuPrimeSearchRoutineSuccess) {
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
  EXPECT_EQ(cros_healthd::FakeCrosHealthdClient::Get()->GetLastRunRoutine(),
            cros_healthd::mojom::DiagnosticRoutineEnum::kPrimeSearch);
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunCpuStressRoutineSuccess) {
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
  EXPECT_EQ(cros_healthd::FakeCrosHealthdClient::Get()->GetLastRunRoutine(),
            cros_healthd::mojom::DiagnosticRoutineEnum::kCpuStress);
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunDiskReadRoutineSuccess) {
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
  EXPECT_EQ(cros_healthd::FakeCrosHealthdClient::Get()->GetLastRunRoutine(),
            cros_healthd::mojom::DiagnosticRoutineEnum::kDiskRead);
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunMemoryRoutineSuccess) {
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
  EXPECT_EQ(cros_healthd::FakeCrosHealthdClient::Get()->GetLastRunRoutine(),
            cros_healthd::mojom::DiagnosticRoutineEnum::kMemory);
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunNvmeWearLevelRoutineSuccess) {
  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function runNvmeWearLevelRoutine() {
        const response =
          await chrome.os.diagnostics.runNvmeWearLevelRoutine(
            {
              wear_level_threshold: 80
            }
          );
        chrome.test.assertEq({id: 0, status: "ready"}, response);
        chrome.test.succeed();
      }
    ]);
  )");
  EXPECT_EQ(cros_healthd::FakeCrosHealthdClient::Get()->GetLastRunRoutine(),
            cros_healthd::mojom::DiagnosticRoutineEnum::kNvmeWearLevel);
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunSmartctlCheckRoutineSuccess) {
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
  EXPECT_EQ(cros_healthd::FakeCrosHealthdClient::Get()->GetLastRunRoutine(),
            cros_healthd::mojom::DiagnosticRoutineEnum::kSmartctlCheck);
}

}  // namespace chromeos
