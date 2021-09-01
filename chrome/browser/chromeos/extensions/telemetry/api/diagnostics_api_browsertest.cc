// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/base_telemetry_extension_browser_test.h"
#include "chromeos/dbus/cros_healthd/fake_cros_healthd_client.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_diagnostics.mojom.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

using TelemetryExtensionDiagnosticsApiBrowserTest =
    BaseTelemetryExtensionBrowserTest;

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       GetAvailableRoutinesSuccess) {
  cros_healthd::FakeCrosHealthdClient::Get()->SetAvailableRoutinesForTesting(
      {cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryCapacity,
       cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryCharge,
       cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryDischarge,
       cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryHealth});

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
              "battery_health"
            ]
          }, response);
        chrome.test.succeed();
      }
    ]);
  )");
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

}  // namespace chromeos
