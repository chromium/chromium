// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/strings/string_util.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/api_guard_delegate.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/base_telemetry_extension_browser_test.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/fake_api_guard_delegate.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/fake_hardware_info_delegate.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/test_management_policy.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

std::string GetServiceWorkerForError(const std::string& error) {
  std::string service_worker = R"(
    chrome.test.runTests([
      // Telemetry APIs.
      async function getVpdInfo() {
        await chrome.test.assertPromiseRejects(
            chrome.os.telemetry.getVpdInfo(),
            'Error: Unauthorized access to chrome.os.telemetry.getVpdInfo. ' +
            '%s'
        );
        chrome.test.succeed();
      },
      async function getOemData() {
        await chrome.test.assertPromiseRejects(
            chrome.os.telemetry.getOemData(),
            'Error: Unauthorized access to chrome.os.telemetry.getOemData. ' +
            '%s'
        );
        chrome.test.succeed();
      },

      // Diagnostics APIs.
      async function getAvailableRoutines() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.getAvailableRoutines(),
            'Error: Unauthorized access to ' +
            'chrome.os.diagnostics.getAvailableRoutines. ' +
            '%s'
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
            '%s'
        );
        chrome.test.succeed();
      },
      async function runBatteryCapacityRoutine() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.runBatteryCapacityRoutine(),
            'Error: Unauthorized access to ' +
            'chrome.os.diagnostics.runBatteryCapacityRoutine. ' +
            '%s'
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
            '%s'
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
            '%s'
        );
        chrome.test.succeed();
      },
      async function runBatteryHealthRoutine() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.runBatteryHealthRoutine(),
            'Error: Unauthorized access to ' +
            'chrome.os.diagnostics.runBatteryHealthRoutine. ' +
            '%s'
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
            '%s'
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
            '%s'
        );
        chrome.test.succeed();
      },
      async function runMemoryRoutine() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.runMemoryRoutine(),
            'Error: Unauthorized access to ' +
            'chrome.os.diagnostics.runMemoryRoutine. ' +
            '%s'
        );
        chrome.test.succeed();
      },
    ]);
  )";

  base::ReplaceSubstringsAfterOffset(&service_worker, /*start_offset=*/0, "%s",
                                     error);
  return service_worker;
}

}  // namespace

using TelemetryExtensionApiGuardBrowserTest = BaseTelemetryExtensionBrowserTest;

IN_PROC_BROWSER_TEST_P(TelemetryExtensionApiGuardBrowserTest,
                       ActiveUserNotOwner) {
  // Make sure that current user is not a device owner.
  auto* const user_manager = GetFakeUserManager();
  const AccountId regular_user = AccountId::FromUserEmail("regular@gmail.com");
  user_manager->SetOwnerId(regular_user);

  CreateExtensionAndRunServiceWorker(GetServiceWorkerForError(
      "This extension is not run by the device owner"));
}

IN_PROC_BROWSER_TEST_P(TelemetryExtensionApiGuardBrowserTest,
                       NotAllowedDeviceManufacturer) {
  hardware_info_delegate_factory_ =
      std::make_unique<FakeHardwareInfoDelegate::Factory>("Google\n");
  HardwareInfoDelegate::Factory::SetForTesting(
      hardware_info_delegate_factory_.get());

  CreateExtensionAndRunServiceWorker(GetServiceWorkerForError(
      "This extension is not allowed to access the API on this device"));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    TelemetryExtensionApiGuardBrowserTest,
    testing::Combine(
        testing::Values(false),
        testing::ValuesIn(
            BaseTelemetryExtensionBrowserTest::kAllExtensionInfoTestParams)));

using TelemetryExtensionApiGuardManagedUserNotPolicyInstalledExtensionBrowserTest =
    BaseTelemetryExtensionBrowserTest;

IN_PROC_BROWSER_TEST_P(
    TelemetryExtensionApiGuardManagedUserNotPolicyInstalledExtensionBrowserTest,
    AffiliatedUserNotPolicyInstalledExtension) {
  // Make sure that ApiGuardDelegate::IsExtensionForceInstalled() returns false.
  api_guard_delegate_factory_ = std::make_unique<FakeApiGuardDelegate::Factory>(
      /*is_extension_force_installed=*/false);
  ApiGuardDelegate::Factory::SetForTesting(api_guard_delegate_factory_.get());

  CreateExtensionAndRunServiceWorker(
      GetServiceWorkerForError("This extension is not installed by the admin"));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    TelemetryExtensionApiGuardManagedUserNotPolicyInstalledExtensionBrowserTest,
    testing::Combine(
        testing::Values(true),
        testing::ValuesIn(
            BaseTelemetryExtensionBrowserTest::kAllExtensionInfoTestParams)));

class TelemetryExtensionApiGuardWithoutPwaBrowserTest
    : public BaseTelemetryExtensionBrowserTest {
 public:
  TelemetryExtensionApiGuardWithoutPwaBrowserTest() {
    should_open_pwa_ui_ = false;
  }
  ~TelemetryExtensionApiGuardWithoutPwaBrowserTest() override = default;

  TelemetryExtensionApiGuardWithoutPwaBrowserTest(
      const TelemetryExtensionApiGuardWithoutPwaBrowserTest&) = delete;
  TelemetryExtensionApiGuardWithoutPwaBrowserTest& operator=(
      const TelemetryExtensionApiGuardWithoutPwaBrowserTest&) = delete;
};

IN_PROC_BROWSER_TEST_P(TelemetryExtensionApiGuardWithoutPwaBrowserTest,
                       PwaUiNotOpen) {
  CreateExtensionAndRunServiceWorker(
      GetServiceWorkerForError("Companion PWA UI is not open"));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    TelemetryExtensionApiGuardWithoutPwaBrowserTest,
    testing::Combine(
        testing::Bool(),
        testing::ValuesIn(
            BaseTelemetryExtensionBrowserTest::kAllExtensionInfoTestParams)));

}  // namespace chromeos
