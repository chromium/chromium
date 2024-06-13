// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/common/api_guard_delegate.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/common/base_telemetry_extension_browser_test.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/common/fake_api_guard_delegate.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/common/remote_probe_service_strategy.h"
#include "chrome/common/chromeos/extensions/chromeos_system_extension_info.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/crosapi/cpp/telemetry/fake_probe_service.h"
#include "chromeos/crosapi/mojom/probe_service.mojom.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension_features.h"
#include "net/base/net_errors.h"
#include "net/cert/x509_certificate.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/crosapi/mojom/diagnostics_service.mojom.h"
#include "chromeos/startup/browser_init_params.h"
#include "components/policy/core/common/policy_loader_lacros.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace chromeos {

namespace {

namespace crosapi = ::crosapi::mojom;

// The tests cases must be kept sorted for the test to pass. Tests should be
// grouped by the API type, then sorted alphabetically within the same type.
std::string GetServiceWorkerForError(const std::string& error) {
  std::string service_worker = R"(
    const tests = [
      // Telemetry APIs.
      async function getAudioInfo() {
        await chrome.test.assertPromiseRejects(
            chrome.os.telemetry.getAudioInfo(),
            'Error: Unauthorized access to ' +
            'chrome.os.telemetry.getAudioInfo.' + ' %s'
        );
        chrome.test.succeed();
      },
      async function getBatteryInfo() {
        await chrome.test.assertPromiseRejects(
            chrome.os.telemetry.getBatteryInfo(),
            'Error: Unauthorized access to ' +
            'chrome.os.telemetry.getBatteryInfo.' + ' %s'
        );
        chrome.test.succeed();
      },
      async function getCpuInfo() {
        await chrome.test.assertPromiseRejects(
            chrome.os.telemetry.getCpuInfo(),
            'Error: Unauthorized access to chrome.os.telemetry.getCpuInfo.' +
            ' %s'
        );
        chrome.test.succeed();
      },
      async function getDisplayInfo() {
        await chrome.test.assertPromiseRejects(
            chrome.os.telemetry.getDisplayInfo(),
            'Error: Unauthorized access to chrome.os.telemetry.' +
            'getDisplayInfo. %s'
        );
        chrome.test.succeed();
      },
      async function getInternetConnectivityInfo() {
        await chrome.test.assertPromiseRejects(
            chrome.os.telemetry.getInternetConnectivityInfo(),
            'Error: Unauthorized access to chrome.os.telemetry.' +
            'getInternetConnectivityInfo. %s'
        );
        chrome.test.succeed();
      },
      async function getMarketingInfo() {
        await chrome.test.assertPromiseRejects(
            chrome.os.telemetry.getMarketingInfo(),
            'Error: Unauthorized access to chrome.os.telemetry.' +
            'getMarketingInfo. %s'
        );
        chrome.test.succeed();
      },
      async function getMemoryInfo() {
        await chrome.test.assertPromiseRejects(
            chrome.os.telemetry.getMemoryInfo(),
            'Error: Unauthorized access to chrome.os.telemetry.getMemoryInfo.' +
            ' %s'
        );
        chrome.test.succeed();
      },
      async function getNonRemovableBlockDevicesInfo() {
        await chrome.test.assertPromiseRejects(
            chrome.os.telemetry.getNonRemovableBlockDevicesInfo(),
            'Error: Unauthorized access to chrome.os.telemetry.' +
            'getNonRemovableBlockDevicesInfo. %s'
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
      async function getOsVersionInfo() {
        await chrome.test.assertPromiseRejects(
            chrome.os.telemetry.getOsVersionInfo(),
            'Error: Unauthorized access to ' +
            'chrome.os.telemetry.getOsVersionInfo. ' +
            '%s'
        );
        chrome.test.succeed();
      },
      async function getStatefulPartitionInfo() {
        await chrome.test.assertPromiseRejects(
            chrome.os.telemetry.getStatefulPartitionInfo(),
            'Error: Unauthorized access to ' +
            'chrome.os.telemetry.getStatefulPartitionInfo.' +
            ' %s'
        );
        chrome.test.succeed();
      },
      async function getThermalInfo() {
        await chrome.test.assertPromiseRejects(
            chrome.os.telemetry.getThermalInfo(),
            'Error: Unauthorized access to chrome.os.telemetry.' +
            'getThermalInfo. %s'
        );
        chrome.test.succeed();
      },
      async function getTpmInfo() {
        await chrome.test.assertPromiseRejects(
            chrome.os.telemetry.getTpmInfo(),
            'Error: Unauthorized access to chrome.os.telemetry.getTpmInfo. ' +
            '%s'
        );
        chrome.test.succeed();
      },
      async function getUsbBusInfo() {
        await chrome.test.assertPromiseRejects(
            chrome.os.telemetry.getUsbBusInfo(),
            'Error: Unauthorized access to ' +
            'chrome.os.telemetry.getUsbBusInfo. ' +
            '%s'
        );
        chrome.test.succeed();
      },
      async function getVpdInfo() {
        await chrome.test.assertPromiseRejects(
            chrome.os.telemetry.getVpdInfo(),
            'Error: Unauthorized access to chrome.os.telemetry.getVpdInfo. ' +
            '%s'
        );
        chrome.test.succeed();
      },

      // Diagnostics APIs.
      async function cancelRoutine() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.cancelRoutine({
              uuid: '123',
            }),
            'Error: Unauthorized access to ' +
            'chrome.os.diagnostics.cancelRoutine. ' +
            '%s'
        );
        chrome.test.succeed();
      },
      async function createFanRoutine() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.createFanRoutine({
            }),
            'Error: Unauthorized access to ' +
            'chrome.os.diagnostics.createFanRoutine. ' +
            '%s'
        );
        chrome.test.succeed();
      },
      async function createMemoryRoutine() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.createMemoryRoutine({
              maxTestingMemKib: 42,
            }),
            'Error: Unauthorized access to ' +
            'chrome.os.diagnostics.createMemoryRoutine. ' +
            '%s'
        );
        chrome.test.succeed();
      },
      async function createRoutine() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.createRoutine({
              memory: {}
            }),
            'Error: Unauthorized access to ' +
            'chrome.os.diagnostics.createRoutine. ' +
            '%s'
        );
        chrome.test.succeed();
      },
      async function createVolumeButtonRoutine() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.createVolumeButtonRoutine({
              button_type: "volume_up",
              timeout_seconds: 10,
            }),
            'Error: Unauthorized access to ' +
            'chrome.os.diagnostics.createVolumeButtonRoutine. ' +
            '%s'
        );
        chrome.test.succeed();
      },
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
      async function isFanRoutineArgumentSupported() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.isFanRoutineArgumentSupported({
            }),
            'Error: Unauthorized access to ' +
            'chrome.os.diagnostics.isFanRoutineArgumentSupported. ' +
            '%s'
        );
        chrome.test.succeed();
      },
      async function isMemoryRoutineArgumentSupported() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.isMemoryRoutineArgumentSupported({
              maxTestingMemKib: 42,
            }),
            'Error: Unauthorized access to ' +
            'chrome.os.diagnostics.isMemoryRoutineArgumentSupported. ' +
            '%s'
        );
        chrome.test.succeed();
      },
      async function isRoutineArgumentSupported() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.isRoutineArgumentSupported({
              memory: {},
            }),
            'Error: Unauthorized access to ' +
            'chrome.os.diagnostics.isRoutineArgumentSupported. ' +
            '%s'
        );
        chrome.test.succeed();
      },
      async function isVolumeButtonRoutineArgumentSupported() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.isVolumeButtonRoutineArgumentSupported({
              button_type: "volume_up",
              timeout_seconds: 10,
            }),
            'Error: Unauthorized access to ' +
            'chrome.os.diagnostics.isVolumeButtonRoutineArgumentSupported. ' +
            '%s'
        );
        chrome.test.succeed();
      },
      async function replyToRoutineInquiry() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.replyToRoutineInquiry({
              uuid: '123',
              reply: {},
            }),
            'Error: Unauthorized access to ' +
            'chrome.os.diagnostics.replyToRoutineInquiry. ' +
            '%s'
        );
        chrome.test.succeed();
      },
      async function runAcPowerRoutine() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.runAcPowerRoutine(
              {
                expected_status: "connected",
                expected_power_type: "ac_power"
              }
            ),
            'Error: Unauthorized access to ' +
            'chrome.os.diagnostics.runAcPowerRoutine. ' +
            '%s'
        );
        chrome.test.succeed();
      },
      async function runAudioDriverRoutine() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.runAudioDriverRoutine(),
            'Error: Unauthorized access to ' +
            'chrome.os.diagnostics.runAudioDriverRoutine. ' +
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
      async function runBluetoothDiscoveryRoutine() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.runBluetoothDiscoveryRoutine(),
            'Error: Unauthorized access to ' +
            'chrome.os.diagnostics.runBluetoothDiscoveryRoutine. ' +
            '%s'
        );
        chrome.test.succeed();
      },
      async function runBluetoothPairingRoutine() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.runBluetoothPairingRoutine(
              {
                peripheral_id: "HEALTHD_TEST_ID"
              }
            ),
            'Error: Unauthorized access to ' +
            'chrome.os.diagnostics.runBluetoothPairingRoutine. ' +
            '%s'
        );
        chrome.test.succeed();
      },
      async function runBluetoothPowerRoutine() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.runBluetoothPowerRoutine(),
            'Error: Unauthorized access to ' +
            'chrome.os.diagnostics.runBluetoothPowerRoutine. ' +
            '%s'
        );
        chrome.test.succeed();
      },
      async function runBluetoothScanningRoutine() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.runBluetoothScanningRoutine(
              {
                length_seconds: 10
              }
            ),
            'Error: Unauthorized access to ' +
            'chrome.os.diagnostics.runBluetoothScanningRoutine. ' +
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
      async function runCpuFloatingPointAccuracyRoutine() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.runCpuFloatingPointAccuracyRoutine(
              {
                length_seconds: 120
              }
            ),
            'Error: Unauthorized access to ' +
            'chrome.os.diagnostics.runCpuFloatingPointAccuracyRoutine. ' +
            '%s'
        );
        chrome.test.succeed();
      },
      async function runCpuPrimeSearchRoutine() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.runCpuPrimeSearchRoutine(
              {
                length_seconds: 120
              }
            ),
            'Error: Unauthorized access to ' +
            'chrome.os.diagnostics.runCpuPrimeSearchRoutine. ' +
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
      async function runDiskReadRoutine() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.runDiskReadRoutine(
              {
                type: "random",
                length_seconds: 60,
                file_size_mb: 200
              }
            ),
            'Error: Unauthorized access to ' +
            'chrome.os.diagnostics.runDiskReadRoutine. ' +
            '%s'
        );
        chrome.test.succeed();
      },
      async function runDnsResolutionRoutine() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.runDnsResolutionRoutine(),
            'Error: Unauthorized access to ' +
            'chrome.os.diagnostics.runDnsResolutionRoutine. ' +
            '%s'
        );
        chrome.test.succeed();
      },
      async function runDnsResolverPresentRoutine() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.runDnsResolverPresentRoutine(),
            'Error: Unauthorized access to ' +
            'chrome.os.diagnostics.runDnsResolverPresentRoutine. ' +
            '%s'
        );
        chrome.test.succeed();
      },
      async function runEmmcLifetimeRoutine() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.runEmmcLifetimeRoutine(),
            'Error: Unauthorized access to ' +
            'chrome.os.diagnostics.runEmmcLifetimeRoutine. ' +
            '%s'
        );
        chrome.test.succeed();
      },
      async function runFanRoutine() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.runFanRoutine(),
            'Error: Unauthorized access to ' +
            'chrome.os.diagnostics.runFanRoutine. ' +
            '%s'
        );
        chrome.test.succeed();
      },
      async function runFingerprintAliveRoutine() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.runFingerprintAliveRoutine(),
            'Error: Unauthorized access to ' +
            'chrome.os.diagnostics.runFingerprintAliveRoutine. ' +
                        '%s'
        );
        chrome.test.succeed();
      },
      async function runGatewayCanBePingedRoutine() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.runGatewayCanBePingedRoutine(),
            'Error: Unauthorized access to ' +
            'chrome.os.diagnostics.runGatewayCanBePingedRoutine. ' +
            '%s'
        );
        chrome.test.succeed();
      },
      async function runLanConnectivityRoutine() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.runLanConnectivityRoutine(),
            'Error: Unauthorized access to ' +
            'chrome.os.diagnostics.runLanConnectivityRoutine. ' +
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
      async function runNvmeSelfTestRoutine() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.runNvmeSelfTestRoutine(
              {
                test_type: 'short_test'
              }
            ),
            'Error: Unauthorized access to ' +
            'chrome.os.diagnostics.runNvmeSelfTestRoutine. ' +
            '%s'
        );
        chrome.test.succeed();
      },
      async function runPowerButtonRoutine() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.runPowerButtonRoutine(
              {
                timeout_seconds: 10
              }
            ),
            'Error: Unauthorized access to ' +
            'chrome.os.diagnostics.runPowerButtonRoutine. ' +
            '%s'
        );
        chrome.test.succeed();
      },
      async function runSensitiveSensorRoutine() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.runSensitiveSensorRoutine(),
            'Error: Unauthorized access to ' +
            'chrome.os.diagnostics.runSensitiveSensorRoutine. ' +
            '%s'
        );
        chrome.test.succeed();
      },
      async function runSignalStrengthRoutine() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.runSignalStrengthRoutine(),
            'Error: Unauthorized access to ' +
            'chrome.os.diagnostics.runSignalStrengthRoutine. ' +
            '%s'
        );
        chrome.test.succeed();
      },
      async function runSmartctlCheckRoutine() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.runSmartctlCheckRoutine(),
            'Error: Unauthorized access to ' +
            'chrome.os.diagnostics.runSmartctlCheckRoutine. ' +
            '%s'
        );
        chrome.test.succeed();
      },
      async function runUfsLifetimeRoutine() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.runUfsLifetimeRoutine(),
            'Error: Unauthorized access to ' +
            'chrome.os.diagnostics.runUfsLifetimeRoutine. ' +
            '%s'
        );
        chrome.test.succeed();
      },
      async function startRoutine() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.startRoutine({
              uuid: '123',
            }),
            'Error: Unauthorized access to ' +
            'chrome.os.diagnostics.startRoutine. ' +
            '%s'
        );
        chrome.test.succeed();
      },
      // Event APIs.
      async function isEventSupported() {
        await chrome.test.assertPromiseRejects(
            chrome.os.events.isEventSupported("audio_jack"),
            'Error: Unauthorized access to ' +
            'chrome.os.events.isEventSupported. ' +
            '%s'
        );
        chrome.test.succeed();
      },
      async function startCapturingEvents() {
        await chrome.test.assertPromiseRejects(
            chrome.os.events.startCapturingEvents("audio_jack"),
            'Error: Unauthorized access to ' +
            'chrome.os.events.startCapturingEvents. ' +
            '%s'
        );
        chrome.test.succeed();
      },
      async function stopCapturingEvents() {
        await chrome.test.assertPromiseRejects(
            chrome.os.events.stopCapturingEvents("audio_jack"),
            'Error: Unauthorized access to ' +
            'chrome.os.events.stopCapturingEvents. ' +
            '%s'
        );
        chrome.test.succeed();
      },
      // Management APIs.
      async function setAudioGain() {
        await chrome.test.assertPromiseRejects(
            chrome.os.management.setAudioGain(
              {
                nodeId: 1,
                gain: 100,
              }
            ),
            'Error: Unauthorized access to ' +
            'chrome.os.management.setAudioGain. ' +
            '%s'
        );
        chrome.test.succeed();
      },
      async function setAudioVolume() {
        await chrome.test.assertPromiseRejects(
            chrome.os.management.setAudioVolume(
              {
                nodeId: 1,
                volume: 100,
                isMuted: false,
              }
            ),
            'Error: Unauthorized access to ' +
            'chrome.os.management.setAudioVolume. ' +
            '%s'
        );
        chrome.test.succeed();
      },
    ];

    chrome.test.runTests([
      async function allAPIsTested() {
        getTestNames = function(arr) {
          return arr.map(item => item.name);
        }
        getMethods = function(obj) {
          return Object.getOwnPropertyNames(obj).filter(
            item => typeof obj[item] === 'function');
        }
        apiNames = [
          ...getMethods(chrome.os.telemetry).sort(),
          ...getMethods(chrome.os.diagnostics).sort(),
          ...getMethods(chrome.os.events).sort(),
          ...getMethods(chrome.os.management).sort()
        ];
        chrome.test.assertEq(getTestNames(tests), apiNames);
        chrome.test.succeed();
      },
      ...tests
    ]);
  )";

  base::ReplaceSubstringsAfterOffset(&service_worker, /*start_offset=*/0, "%s",
                                     error);
  return service_worker;
}

}  // namespace

class TelemetryExtensionApiGuardBrowserTest
    : public BaseTelemetryExtensionBrowserTest {
 public:
  TelemetryExtensionApiGuardBrowserTest() {
    // Include unreleased APIs.
    feature_list_.InitAndEnableFeature(
        extensions_features::kTelemetryExtensionPendingApprovalApi);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(TelemetryExtensionApiGuardBrowserTest,
                       CanAccessApiReturnsError) {
  constexpr char kErrorMessage[] = "Test error message";
  // Make sure ApiGuardDelegate::CanAccessApi() returns specified error message.
  api_guard_delegate_factory_ =
      std::make_unique<FakeApiGuardDelegate::Factory>(kErrorMessage);
  ApiGuardDelegate::Factory::SetForTesting(api_guard_delegate_factory_.get());

  CreateExtensionAndRunServiceWorker(GetServiceWorkerForError(kErrorMessage));
}

// Class that use real ApiGuardDelegate instance to verify its behavior.
class TelemetryExtensionApiGuardRealDelegateBrowserTest
    : public BaseTelemetryExtensionBrowserTest {
 public:
  TelemetryExtensionApiGuardRealDelegateBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    https_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  }
  ~TelemetryExtensionApiGuardRealDelegateBrowserTest() override = default;

  TelemetryExtensionApiGuardRealDelegateBrowserTest(
      const TelemetryExtensionApiGuardRealDelegateBrowserTest&) = delete;
  TelemetryExtensionApiGuardRealDelegateBrowserTest& operator=(
      const TelemetryExtensionApiGuardRealDelegateBrowserTest&) = delete;

  // BaseTelemetryExtensionBrowserTest:
  void SetUp() override {
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
    ASSERT_TRUE(https_server_.InitializeAndListen());

    BaseTelemetryExtensionBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    BaseTelemetryExtensionBrowserTest::SetUpCommandLine(command_line);

    command_line->AppendSwitchASCII(
        chromeos::switches::kTelemetryExtensionPwaOriginOverrideForTesting,
        pwa_page_url());
  }

  void SetUpOnMainThread() override {
    // Skip BaseTelemetryExtensionBrowserTest::SetUpOnMainThread() as it sets up
    // a FakeApiGuardDelegate instance.
    extensions::ExtensionBrowserTest::SetUpOnMainThread();

#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Must be initialized before dealing with UserManager.
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        std::make_unique<ash::FakeChromeUserManager>());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    https_server_.StartAcceptingConnections();

    // This is needed when navigating to a network URL (e.g.
    // ui_test_utils::NavigateToURL). Rules can only be added before
    // BrowserTestBase::InitializeNetworkProcess() is called because host
    // changes will be disabled afterwards.
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void SetUpProbeService() {
    fake_probe_service_ = std::make_unique<FakeProbeService>();
    auto telemetry_info = crosapi::ProbeTelemetryInfo::New();
    telemetry_info->system_result = crosapi::ProbeSystemResult::NewSystemInfo(
        crosapi::ProbeSystemInfo::New(crosapi::ProbeOsInfo::New("HP")));
    fake_probe_service_->SetProbeTelemetryInfoResponse(
        std::move(telemetry_info));
    RemoteProbeServiceStrategy::Get()->SetServiceForTesting(
        fake_probe_service_->BindNewPipeAndPassRemote());
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void TearDownOnMainThread() override {
    // Explicitly removing the user is required; otherwise ProfileHelper keeps
    // a dangling pointer to the User.
    // TODO(b/208629291): Consider removing all users from ProfileHelper in the
    // destructor of ash::FakeChromeUserManager.
    GetFakeUserManager()->RemoveUserFromList(
        GetFakeUserManager()->GetActiveUser()->GetAccountId());
    user_manager_enabler_.reset();
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

 protected:
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::FakeChromeUserManager* GetFakeUserManager() const {
    return static_cast<ash::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Returns whether the Diagnostics interface is available. It may
  // not be available on earlier versions of ash-chrome.
  bool IsServiceAvailable() const {
    chromeos::LacrosService* lacros_service = chromeos::LacrosService::Get();
    return lacros_service &&
           lacros_service->IsAvailable<crosapi::DiagnosticsService>();
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  GURL GetPwaGURL() const { return https_server_.GetURL("/ssl/google.html"); }

  // BaseTelemetryExtensionBrowserTest:
  std::string pwa_page_url() const override { return GetPwaGURL().spec(); }
  std::string matches_origin() const override { return GetPwaGURL().spec(); }

  net::EmbeddedTestServer https_server_;

  std::unique_ptr<FakeProbeService> fake_probe_service_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
};

// Smoke test to verify that real ApiGuardDelegate works in prod.
// TODO(b/338199240): Test is flaky.
IN_PROC_BROWSER_TEST_F(TelemetryExtensionApiGuardRealDelegateBrowserTest,
                       DISABLED_CanAccessRunBatteryCapacityRoutine) {
  SetUpProbeService();

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // We can't run this test if Ash doesn't support the crosapi
  // interface.
  if (!IsServiceAvailable()) {
    return;
  }

  // Check that device ownership is set up.
  ASSERT_TRUE(
      chromeos::BrowserInitParams::GetForTests()->is_current_user_device_owner);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Add a new user and make it owner.
  auto* const user_manager = GetFakeUserManager();
  const AccountId account_id = AccountId::FromUserEmail("user@example.com");
  user_manager->AddUser(account_id);
  user_manager->LoginUser(account_id);
  user_manager->SwitchActiveUser(account_id);
  user_manager->SetOwnerId(account_id);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Make sure PWA UI is open and secure.
  auto* pwa_page_rfh =
      ui_test_utils::NavigateToURL(browser(), GURL(pwa_page_url()));
  ASSERT_TRUE(pwa_page_rfh);

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

// Verify that manufacturer will be cached and only one call to probe service
// will be made.
// TODO(b/346211419): The test shows excessive flakiness.
IN_PROC_BROWSER_TEST_F(TelemetryExtensionApiGuardRealDelegateBrowserTest,
                       DISABLED_UseCacheForMultipleApiAccess) {
  SetUpProbeService();

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // We can't run this test if Ash doesn't support the crosapi
  // interface.
  if (!IsServiceAvailable()) {
    return;
  }

  auto init_params = chromeos::BrowserInitParams::GetForTests()->Clone();
  init_params->is_current_user_device_owner = true;
  chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Add a new user and make it owner.
  auto* const user_manager = GetFakeUserManager();
  const AccountId account_id = AccountId::FromUserEmail("user@example.com");
  user_manager->AddUser(account_id);
  user_manager->LoginUser(account_id);
  user_manager->SwitchActiveUser(account_id);
  user_manager->SetOwnerId(account_id);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Make sure PWA UI is open and secure.
  auto* pwa_page_rfh =
      ui_test_utils::NavigateToURL(browser(), GURL(pwa_page_url()));
  ASSERT_TRUE(pwa_page_rfh);

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function runBatteryCapacityRoutine() {
        let response =
          await chrome.os.diagnostics.runBatteryCapacityRoutine();
        chrome.test.assertEq({id: 0, status: "ready"}, response);
        response =
          await chrome.os.diagnostics.runBatteryCapacityRoutine();
        chrome.test.assertEq({id: 0, status: "ready"}, response);
        chrome.test.succeed();
      }
    ]);
  )");
  // Make sure that the manufacturer info is only gathered once on multiple API
  // access.
  EXPECT_EQ(fake_probe_service_->GetProbeTelemetryInfoCallCount(), 1);
}

}  // namespace chromeos
