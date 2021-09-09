// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/base_telemetry_extension_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

using TelemetryExtensionBaseApiGuardBrowserTest =
    BaseTelemetryExtensionBrowserTest;

IN_PROC_BROWSER_TEST_F(TelemetryExtensionBaseApiGuardBrowserTest,
                       OnlyDeviceOwnerCanCallThisApiError) {
  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      // Telemetry APIs.
      async function getVpdInfo() {
        await chrome.test.assertPromiseRejects(
            chrome.os.telemetry.getVpdInfo(),
            'Error: Unauthorized access to chrome.os.telemetry.getVpdInfo. ' +
            'This extension is not run by the device owner'
        );
        chrome.test.succeed();
      },
      async function getOemData() {
        await chrome.test.assertPromiseRejects(
            chrome.os.telemetry.getOemData(),
            'Error: Unauthorized access to chrome.os.telemetry.getOemData. ' +
            'This extension is not run by the device owner'
        );
        chrome.test.succeed();
      },

      // Diagnostics APIs.
      async function getAvailableRoutines() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.getAvailableRoutines(),
            'Error: Unauthorized access to ' +
            'chrome.os.diagnostics.getAvailableRoutines. ' +
            'This extension is not run by the device owner'
        );
        chrome.test.succeed();
      },
      async function getRoutineUpdate() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.getRoutineUpdate(
              {
                id: 12345,
                command: "status"
              }
            ),
            'Error: Unauthorized access to ' +
            'chrome.os.diagnostics.getRoutineUpdate. ' +
            'This extension is not run by the device owner'
        );
        chrome.test.succeed();
      },
      async function runBatteryCapacityRoutine() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.runBatteryCapacityRoutine(),
            'Error: Unauthorized access to ' +
            'chrome.os.diagnostics.runBatteryCapacityRoutine. ' +
            'This extension is not run by the device owner'
        );
        chrome.test.succeed();
      },
      async function runBatteryChargeRoutine() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.runBatteryChargeRoutine(
              {
                length_seconds: 1000,
                minimum_charge_percent_required: 1
              }
            ),
            'Error: Unauthorized access to ' +
            'chrome.os.diagnostics.runBatteryChargeRoutine. ' +
            'This extension is not run by the device owner'
        );
        chrome.test.succeed();
      },
      async function runBatteryDischargeRoutine() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.runBatteryDischargeRoutine(
              {
                length_seconds: 10,
                maximum_discharge_percent_allowed: 15
              }
            ),
            'Error: Unauthorized access to ' +
            'chrome.os.diagnostics.runBatteryDischargeRoutine. ' +
            'This extension is not run by the device owner'
        );
        chrome.test.succeed();
      },
      async function runBatteryHealthRoutine() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.runBatteryHealthRoutine(),
            'Error: Unauthorized access to ' +
            'chrome.os.diagnostics.runBatteryHealthRoutine. ' +
            'This extension is not run by the device owner'
        );
        chrome.test.succeed();
      },
      async function runCpuCacheRoutine() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.runCpuCacheRoutine(
              {
                length_seconds: 120
              }
            ),
            'Error: Unauthorized access to ' +
            'chrome.os.diagnostics.runCpuCacheRoutine. ' +
            'This extension is not run by the device owner'
        );
        chrome.test.succeed();
      },
      async function runCpuStressRoutine() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.runCpuStressRoutine(
              {
                length_seconds: 120
              }
            ),
            'Error: Unauthorized access to ' +
            'chrome.os.diagnostics.runCpuStressRoutine. ' +
            'This extension is not run by the device owner'
        );
        chrome.test.succeed();
      },
      async function runMemoryRoutine() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.runMemoryRoutine(),
            'Error: Unauthorized access to ' +
            'chrome.os.diagnostics.runMemoryRoutine. ' +
            'This extension is not run by the device owner'
        );
        chrome.test.succeed();
      },
    ]);
  )");
}

}  // namespace chromeos
