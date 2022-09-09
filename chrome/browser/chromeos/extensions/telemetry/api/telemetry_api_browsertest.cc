// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/base_telemetry_extension_browser_test.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/fake_probe_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/telemetry_extension/probe_service_ash.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/fake_probe_service_factory.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/probe_service.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace chromeos {

class TelemetryExtensionTelemetryApiBrowserTest
    : public BaseTelemetryExtensionBrowserTest {
 public:
  TelemetryExtensionTelemetryApiBrowserTest() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    ash::ProbeServiceAsh::Factory::SetForTesting(&fake_probe_factory_);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }
  ~TelemetryExtensionTelemetryApiBrowserTest() override = default;

  TelemetryExtensionTelemetryApiBrowserTest(
      const TelemetryExtensionTelemetryApiBrowserTest&) = delete;
  TelemetryExtensionTelemetryApiBrowserTest& operator=(
      const TelemetryExtensionTelemetryApiBrowserTest&) = delete;

 protected:
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Returns whether the Probe interface is available. It may
  // not be available on earlier versions of ash-chrome.
  bool IsServiceAvailable() const {
    chromeos::LacrosService* lacros_service = chromeos::LacrosService::Get();
    return lacros_service &&
           lacros_service->IsAvailable<crosapi::mojom::TelemetryProbeService>();
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  void SetServiceForTesting(
      std::unique_ptr<FakeProbeService> fake_probe_service_impl) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    fake_probe_factory_.SetCreateInstanceResponse(
        std::move(fake_probe_service_impl));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // Replace the production Probe service with a mock for testing.
    mojo::Remote<crosapi::mojom::TelemetryProbeService>& remote =
        chromeos::LacrosService::Get()
            ->GetRemote<crosapi::mojom::TelemetryProbeService>();
    remote.reset();
    fake_probe_service_impl->BindPendingReceiver(
        remote.BindNewPipeAndPassReceiver());
    fake_probe_service_impl_ = std::move(fake_probe_service_impl);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  FakeProbeServiceFactory fake_probe_factory_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  std::unique_ptr<FakeProbeService> fake_probe_service_impl_;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
};

#if BUILDFLAG(IS_CHROMEOS_LACROS)
IN_PROC_BROWSER_TEST_F(TelemetryExtensionTelemetryApiBrowserTest,
                       LacrosServiceNotAvailableError) {
  // If Probe interface is available on this version of ash-chrome, this test
  // suite will no-op.
  if (IsServiceAvailable()) {
    return;
  }

  std::string service_worker = R"(
    const tests = [
      // Telemetry APIs.
      async function getBatteryInfo() {
        await chrome.test.assertPromiseRejects(
            chrome.os.telemetry.getBatteryInfo(),
            'Error: API chrome.os.telemetry.getBatteryInfo failed. ' +
            'Not supported by ash browser'
        );
        chrome.test.succeed();
      },
      async function getCpuInfo() {
        await chrome.test.assertPromiseRejects(
            chrome.os.telemetry.getCpuInfo(),
            'Error: API chrome.os.telemetry.getCpuInfo failed. ' +
            'Not supported by ash browser'
        );
        chrome.test.succeed();
      },
      async function getMemoryInfo() {
        await chrome.test.assertPromiseRejects(
            chrome.os.telemetry.getMemoryInfo(),
            'Error: API chrome.os.telemetry.getMemoryInfo failed. ' +
            'Not supported by ash browser'
        );
        chrome.test.succeed();
      },
      async function getOemData() {
        await chrome.test.assertPromiseRejects(
            chrome.os.telemetry.getOemData(),
            'Error: API chrome.os.telemetry.getOemData failed. ' +
            'Not supported by ash browser'
        );
        chrome.test.succeed();
      },
      async function getOsVersionInfo() {
        await chrome.test.assertPromiseRejects(
            chrome.os.telemetry.getOsVersionInfo(),
            'Error: API ' +
            'chrome.os.telemetry.getOsVersionInfo failed. ' +
            'Not supported by ash browser'
        );
        chrome.test.succeed();
      },
      async function getStatefulPartitionInfo() {
        await chrome.test.assertPromiseRejects(
            chrome.os.telemetry.getStatefulPartitionInfo(),
            'Error: API ' +
            'chrome.os.telemetry.getStatefulPartitionInfo failed. ' +
            'Not supported by ash browser'
        );
        chrome.test.succeed();
      },
      async function getVpdInfo() {
        await chrome.test.assertPromiseRejects(
            chrome.os.telemetry.getVpdInfo(),
            'Error: API chrome.os.telemetry.getVpdInfo failed. ' +
            'Not supported by ash browser'
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
          ...getMethods(chrome.os.telemetry),
        ];
        chrome.test.assertEq(getTestNames(tests), apiNames);
        chrome.test.succeed();
      },
      ...tests
    ]);
  )";

  CreateExtensionAndRunServiceWorker(service_worker);
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

IN_PROC_BROWSER_TEST_F(TelemetryExtensionTelemetryApiBrowserTest,
                       GetBatteryInfo_ApiInternalError) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If Probe interface is not available on this version of ash-chrome, this
  // test suite will no-op.
  if (!IsServiceAvailable()) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Configure FakeProbeService.
  {
    auto fake_service_impl = std::make_unique<FakeProbeService>();
    fake_service_impl->SetExpectedLastRequestedCategories(
        {crosapi::mojom::ProbeCategoryEnum::kBattery});

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function getBatteryInfo() {
        await chrome.test.assertPromiseRejects(
            chrome.os.telemetry.getBatteryInfo(),
            'Error: API internal error'
        );
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionTelemetryApiBrowserTest,
                       GetBatteryInfo_Success) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If Probe interface is not available on this version of ash-chrome, this
  // test suite will no-op.
  if (!IsServiceAvailable()) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Configure FakeProbeService.
  {
    auto telemetry_info = crosapi::mojom::ProbeTelemetryInfo::New();
    {
      auto battery_info = crosapi::mojom::ProbeBatteryInfo::New();
      battery_info->cycle_count =
          crosapi::mojom::Int64Value::New(100000000000000);
      battery_info->voltage_now =
          crosapi::mojom::DoubleValue::New(1234567890.123456);
      battery_info->vendor = "Google";
      battery_info->serial_number = "abcdef";
      battery_info->charge_full_design =
          crosapi::mojom::DoubleValue::New(3000000000000000);
      battery_info->charge_full =
          crosapi::mojom::DoubleValue::New(9000000000000000);
      battery_info->voltage_min_design =
          crosapi::mojom::DoubleValue::New(1000000000.1001);
      battery_info->model_name = "Google Battery";
      battery_info->charge_now =
          crosapi::mojom::DoubleValue::New(7777777777.777);
      battery_info->current_now =
          crosapi::mojom::DoubleValue::New(0.9999999999999);
      battery_info->technology = "Li-ion";
      battery_info->status = "Charging";
      battery_info->manufacture_date = "2020-07-30";
      battery_info->temperature =
          crosapi::mojom::UInt64Value::New(7777777777777777);

      telemetry_info->battery_result =
          crosapi::mojom::ProbeBatteryResult::NewBatteryInfo(
              std::move(battery_info));
    }

    auto fake_service_impl = std::make_unique<FakeProbeService>();
    fake_service_impl->SetProbeTelemetryInfoResponse(std::move(telemetry_info));
    fake_service_impl->SetExpectedLastRequestedCategories(
        {crosapi::mojom::ProbeCategoryEnum::kBattery});

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function getBatteryInfo() {
        const result = await chrome.os.telemetry.getBatteryInfo();
         chrome.test.assertEq(
          // The dictionary members are ordered lexicographically by the Unicode
          // codepoints that comprise their identifiers.
          {
            chargeFull: 9000000000000000,
            chargeFullDesign: 3000000000000000,
            chargeNow: 7777777777.777,
            currentNow: 0.9999999999999,
            cycleCount: 100000000000000,
            manufactureDate: '2020-07-30',
            modelName: 'Google Battery',
            serialNumber: 'abcdef',
            status: 'Charging',
            technology: 'Li-ion',
            temperature: 7777777777777777,
            vendor: 'Google',
            voltageMinDesign: 1000000000.1001,
            voltageNow: 1234567890.123456,
          }, result);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionTelemetryApiBrowserTest,
                       GetCpuInfo_Error) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If Probe interface is not available on this version of ash-chrome, this
  // test suite will no-op.
  if (!IsServiceAvailable()) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Configure FakeProbeService.
  {
    auto fake_service_impl = std::make_unique<FakeProbeService>();
    fake_service_impl->SetExpectedLastRequestedCategories(
        {crosapi::mojom::ProbeCategoryEnum::kCpu});

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function getCpuInfo() {
        await chrome.test.assertPromiseRejects(
            chrome.os.telemetry.getCpuInfo(),
            'Error: API internal error'
        );
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionTelemetryApiBrowserTest,
                       GetCpuInfo_Success) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If Probe interface is not available on this version of ash-chrome, this
  // test suite will no-op.
  if (!IsServiceAvailable()) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Configure FakeProbeService.
  {
    auto telemetry_info = crosapi::mojom::ProbeTelemetryInfo::New();

    {
      auto c_state1 = crosapi::mojom::ProbeCpuCStateInfo::New();
      c_state1->name = "C1";
      c_state1->time_in_state_since_last_boot_us =
          crosapi::mojom::UInt64Value::New(1125899906875957);

      auto c_state2 = crosapi::mojom::ProbeCpuCStateInfo::New();
      c_state2->name = "C2";
      c_state2->time_in_state_since_last_boot_us =
          crosapi::mojom::UInt64Value::New(1125899906877777);

      auto logical_info1 = crosapi::mojom::ProbeLogicalCpuInfo::New();
      logical_info1->max_clock_speed_khz =
          crosapi::mojom::UInt32Value::New(2147473647);
      logical_info1->scaling_max_frequency_khz =
          crosapi::mojom::UInt32Value::New(1073764046);
      logical_info1->scaling_current_frequency_khz =
          crosapi::mojom::UInt32Value::New(536904245);
      // Idle time cannot be tested in browser test, because it requires USER_HZ
      // system constant to convert idle_time_user_hz to milliseconds.
      logical_info1->idle_time_ms = crosapi::mojom::UInt64Value::New(0);
      logical_info1->c_states.push_back(std::move(c_state1));
      logical_info1->c_states.push_back(std::move(c_state2));

      auto logical_info2 = crosapi::mojom::ProbeLogicalCpuInfo::New();
      logical_info2->max_clock_speed_khz =
          crosapi::mojom::UInt32Value::New(1147494759);
      logical_info2->scaling_max_frequency_khz =
          crosapi::mojom::UInt32Value::New(1063764046);
      logical_info2->scaling_current_frequency_khz =
          crosapi::mojom::UInt32Value::New(936904246);
      // Idle time cannot be tested in browser test, because it requires USER_HZ
      // system constant to convert idle_time_user_hz to milliseconds.
      logical_info2->idle_time_ms = crosapi::mojom::UInt64Value::New(0);

      auto physical_info1 = crosapi::mojom::ProbePhysicalCpuInfo::New();
      physical_info1->model_name = "i9";
      physical_info1->logical_cpus.push_back(std::move(logical_info1));
      physical_info1->logical_cpus.push_back(std::move(logical_info2));

      auto logical_info3 = crosapi::mojom::ProbeLogicalCpuInfo::New();
      logical_info3->max_clock_speed_khz =
          crosapi::mojom::UInt32Value::New(1247494759);
      logical_info3->scaling_max_frequency_khz =
          crosapi::mojom::UInt32Value::New(1263764046);
      logical_info3->scaling_current_frequency_khz =
          crosapi::mojom::UInt32Value::New(946904246);
      // Idle time cannot be tested in browser test, because it requires USER_HZ
      // system constant to convert idle_time_user_hz to milliseconds.
      logical_info3->idle_time_ms = crosapi::mojom::UInt64Value::New(0);

      auto physical_info2 = crosapi::mojom::ProbePhysicalCpuInfo::New();
      physical_info2->model_name = "i9-low-powered";
      physical_info2->logical_cpus.push_back(std::move(logical_info3));

      auto cpu_info = crosapi::mojom::ProbeCpuInfo::New();
      cpu_info->num_total_threads =
          crosapi::mojom::UInt32Value::New(2147483647);
      cpu_info->architecture =
          crosapi::mojom::ProbeCpuArchitectureEnum::kArmv7l;
      cpu_info->physical_cpus.push_back(std::move(physical_info1));
      cpu_info->physical_cpus.push_back(std::move(physical_info2));

      telemetry_info->cpu_result =
          crosapi::mojom::ProbeCpuResult::NewCpuInfo(std::move(cpu_info));
    }

    auto fake_service_impl = std::make_unique<FakeProbeService>();
    fake_service_impl->SetProbeTelemetryInfoResponse(std::move(telemetry_info));
    fake_service_impl->SetExpectedLastRequestedCategories(
        {crosapi::mojom::ProbeCategoryEnum::kCpu});

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function getCpuInfo() {
        const result = await chrome.os.telemetry.getCpuInfo();

        chrome.test.assertEq(
          // The dictionary members are ordered lexicographically by the Unicode
          // codepoints that comprise their identifiers.
          {
            'architecture': 'armv7l',
            'numTotalThreads': 2147483647,
            'physicalCpus': [{
              'logicalCpus': [{
                'cStates': [{
                  'name': 'C1',
                  'timeInStateSinceLastBootUs': 1125899906875957,
                },
                {
                  'name': 'C2',
                  'timeInStateSinceLastBootUs': 1125899906877777,
                }],
                'idleTimeMs': 0,
                'maxClockSpeedKhz': 2147473647,
                'scalingCurrentFrequencyKhz': 536904245,
                'scalingMaxFrequencyKhz': 1073764046,
            }, {
                'cStates': [],
                'idleTimeMs': 0,
                'maxClockSpeedKhz': 1147494759,
                'scalingCurrentFrequencyKhz': 936904246,
                'scalingMaxFrequencyKhz': 1063764046,
            }],
            'modelName': 'i9',
          }, {
            'logicalCpus': [{
              'cStates': [],
              'idleTimeMs': 0,
              'maxClockSpeedKhz': 1247494759,
              'scalingCurrentFrequencyKhz': 946904246,
              'scalingMaxFrequencyKhz': 1263764046,
            }],
            'modelName': 'i9-low-powered',
          }],
        }, result);

        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionTelemetryApiBrowserTest,
                       GetMemoryInfo_Error) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If Probe interface is not available on this version of ash-chrome, this
  // test suite will no-op.
  if (!IsServiceAvailable()) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Configure FakeProbeService.
  {
    auto fake_service_impl = std::make_unique<FakeProbeService>();
    fake_service_impl->SetExpectedLastRequestedCategories(
        {crosapi::mojom::ProbeCategoryEnum::kMemory});

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function getMemoryInfo() {
        await chrome.test.assertPromiseRejects(
            chrome.os.telemetry.getMemoryInfo(),
            'Error: API internal error'
        );
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionTelemetryApiBrowserTest,
                       GetMemoryInfo_Success) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If Probe interface is not available on this version of ash-chrome, this
  // test suite will no-op.
  if (!IsServiceAvailable()) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Configure FakeProbeService.
  {
    auto telemetry_info = crosapi::mojom::ProbeTelemetryInfo::New();

    {
      auto memory_info = crosapi::mojom::ProbeMemoryInfo::New();
      memory_info->total_memory_kib =
          crosapi::mojom::UInt32Value::New(2147483647);
      memory_info->free_memory_kib =
          crosapi::mojom::UInt32Value::New(2147483646);
      memory_info->available_memory_kib =
          crosapi::mojom::UInt32Value::New(2147483645);
      memory_info->page_faults_since_last_boot =
          crosapi::mojom::UInt64Value::New(4611686018427388000);

      telemetry_info->memory_result =
          crosapi::mojom::ProbeMemoryResult::NewMemoryInfo(
              std::move(memory_info));
    }

    auto fake_service_impl = std::make_unique<FakeProbeService>();
    fake_service_impl->SetProbeTelemetryInfoResponse(std::move(telemetry_info));
    fake_service_impl->SetExpectedLastRequestedCategories(
        {crosapi::mojom::ProbeCategoryEnum::kMemory});

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function getMemoryInfo() {
        const result = await chrome.os.telemetry.getMemoryInfo();
        chrome.test.assertEq(2147483647, result.totalMemoryKiB);
        chrome.test.assertEq(2147483646, result.freeMemoryKiB);
        chrome.test.assertEq(2147483645, result.availableMemoryKiB);
        chrome.test.assertEq(4611686018427388000,
          result.pageFaultsSinceLastBoot);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionTelemetryApiBrowserTest,
                       GetOemDataWithSerialNumberPermission_Error) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If Probe interface is not available on this version of ash-chrome, this
  // test suite will no-op.
  if (!IsServiceAvailable()) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Configure FakeProbeService.
  {
    auto fake_service_impl = std::make_unique<FakeProbeService>();

    SetServiceForTesting(std::move(fake_service_impl));
  }
  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function getOemData() {
        await chrome.test.assertPromiseRejects(
            chrome.os.telemetry.getOemData(),
            'Error: API internal error'
        );
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionTelemetryApiBrowserTest,
                       GetOemDataWithSerialNumberPermission_Success) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If Probe interface is not available on this version of ash-chrome, this
  // test suite will no-op.
  if (!IsServiceAvailable()) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Configure FakeProbeService.
  {
    auto fake_service_impl = std::make_unique<FakeProbeService>();

    auto oem_data = crosapi::mojom::ProbeOemData::New();
    oem_data->oem_data = "123456789";
    fake_service_impl->SetOemDataResponse(std::move(oem_data));

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function getOemData() {
        const result = await chrome.os.telemetry.getOemData();
        chrome.test.assertEq(
          "123456789", result.oemData);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionTelemetryApiBrowserTest,
                       GetOsVersionInfo_Error) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If Probe interface is not available on this version of ash-chrome, this
  // test suite will no-op.
  if (!IsServiceAvailable()) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Configure FakeProbeService.
  {
    auto fake_service_impl = std::make_unique<FakeProbeService>();
    fake_service_impl->SetExpectedLastRequestedCategories(
        {crosapi::mojom::ProbeCategoryEnum::kSystem});

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function getOsVersionInfo() {
        await chrome.test.assertPromiseRejects(
            chrome.os.telemetry.getOsVersionInfo(),
            'Error: API internal error'
        );
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionTelemetryApiBrowserTest,
                       GetOsVersionInfo_Success) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If Probe interface is not available on this version of ash-chrome, this
  // test suite will no-op.
  if (!IsServiceAvailable()) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Configure FakeProbeService.
  {
    auto telemetry_info = crosapi::mojom::ProbeTelemetryInfo::New();
    {
      auto os_version_info = crosapi::mojom::ProbeOsVersion::New();
      os_version_info->release_milestone = "87";
      os_version_info->build_number = "13544";
      os_version_info->patch_number = "59.0";
      os_version_info->release_channel = "stable-channel";

      auto os_info = crosapi::mojom::ProbeOsInfo::New();
      os_info->os_version = std::move(os_version_info);

      auto system_info = crosapi::mojom::ProbeSystemInfo::New();
      system_info->os_info = std::move(os_info);

      telemetry_info->system_result =
          crosapi::mojom::ProbeSystemResult::NewSystemInfo(
              std::move(system_info));
    }

    auto fake_service_impl = std::make_unique<FakeProbeService>();
    fake_service_impl->SetProbeTelemetryInfoResponse(std::move(telemetry_info));
    fake_service_impl->SetExpectedLastRequestedCategories(
        {crosapi::mojom::ProbeCategoryEnum::kSystem});

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function getOsVersionInfo() {
        const result = await chrome.os.telemetry.getOsVersionInfo();
        chrome.test.assertEq(
          {
            releaseMilestone: "87",
            buildNumber: "13544",
            patchNumber: "59.0",
            releaseChannel: "stable-channel"
          }, result);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionTelemetryApiBrowserTest,
                       GetVpdInfoError) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If Probe interface is not available on this version of ash-chrome, this
  // test suite will no-op.
  if (!IsServiceAvailable()) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Configure FakeProbeService.
  {
    auto fake_service_impl = std::make_unique<FakeProbeService>();
    fake_service_impl->SetExpectedLastRequestedCategories(
        {crosapi::mojom::ProbeCategoryEnum::kCachedVpdData});

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function getVpdInfo() {
        await chrome.test.assertPromiseRejects(
            chrome.os.telemetry.getVpdInfo(),
            'Error: API internal error'
        );
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionTelemetryApiBrowserTest,
                       GetVpdInfoWithSerialNumberPermission) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If Probe interface is not available on this version of ash-chrome, this
  // test suite will no-op.
  if (!IsServiceAvailable()) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Configure FakeProbeService.
  {
    auto telemetry_info = crosapi::mojom::ProbeTelemetryInfo::New();

    {
      auto vpd_info = crosapi::mojom::ProbeCachedVpdInfo::New();
      vpd_info->first_power_date = "2021-50";
      vpd_info->model_name = "COOL-LAPTOP-CHROME";
      vpd_info->serial_number = "5CD9132880";
      vpd_info->sku_number = "sku15";

      telemetry_info->vpd_result =
          crosapi::mojom::ProbeCachedVpdResult::NewVpdInfo(std::move(vpd_info));
    }

    auto fake_service_impl = std::make_unique<FakeProbeService>();
    fake_service_impl->SetProbeTelemetryInfoResponse(std::move(telemetry_info));
    fake_service_impl->SetExpectedLastRequestedCategories(
        {crosapi::mojom::ProbeCategoryEnum::kCachedVpdData});

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function getVpdInfo() {
        const result = await chrome.os.telemetry.getVpdInfo();
        chrome.test.assertEq("2021-50", result.activateDate);
        chrome.test.assertEq("COOL-LAPTOP-CHROME", result.modelName);
        chrome.test.assertEq("5CD9132880", result.serialNumber);
        chrome.test.assertEq("sku15", result.skuNumber);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionTelemetryApiBrowserTest,
                       GetStatefulPartitionInfo_Error) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If Probe interface is not available on this version of ash-chrome, this
  // test suite will no-op.
  if (!IsServiceAvailable()) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Configure FakeProbeService.
  {
    auto fake_service_impl = std::make_unique<FakeProbeService>();
    fake_service_impl->SetExpectedLastRequestedCategories(
        {crosapi::mojom::ProbeCategoryEnum::kStatefulPartition});

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function getStatefulPartitionInfo() {
        await chrome.test.assertPromiseRejects(
            chrome.os.telemetry.getStatefulPartitionInfo(),
            'Error: API internal error'
        );
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionTelemetryApiBrowserTest,
                       GetStatefulPartitionInfo_Success) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If Probe interface is not available on this version of ash-chrome, this
  // test suite will no-op.
  if (!IsServiceAvailable()) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Configure FakeProbeService.
  {
    auto telemetry_info = crosapi::mojom::ProbeTelemetryInfo::New();
    {
      auto stateful_part_info =
          crosapi::mojom::ProbeStatefulPartitionInfo::New();
      stateful_part_info->available_space =
          crosapi::mojom::UInt64Value::New(3000000000000000);
      stateful_part_info->total_space =
          crosapi::mojom::UInt64Value::New(9000000000000000);

      telemetry_info->stateful_partition_result =
          crosapi::mojom::ProbeStatefulPartitionResult::NewPartitionInfo(
              std::move(stateful_part_info));
    }

    auto fake_service_impl = std::make_unique<FakeProbeService>();
    fake_service_impl->SetProbeTelemetryInfoResponse(std::move(telemetry_info));
    fake_service_impl->SetExpectedLastRequestedCategories(
        {crosapi::mojom::ProbeCategoryEnum::kStatefulPartition});

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function getStatefulPartitionInfo() {
        const result = await chrome.os.telemetry.getStatefulPartitionInfo();
        chrome.test.assertEq(
          // The dictionary members are ordered lexicographically by the Unicode
          // codepoints that comprise their identifiers.
          {
            availableSpace: 3000000000000000,
            totalSpace: 9000000000000000,
          }, result);
        chrome.test.succeed();
      }
    ]);
  )");
}

class TelemetryExtensionTelemetryApiWithoutSerialNumberBrowserTest
    : public TelemetryExtensionTelemetryApiBrowserTest {
 public:
  TelemetryExtensionTelemetryApiWithoutSerialNumberBrowserTest() = default;
  ~TelemetryExtensionTelemetryApiWithoutSerialNumberBrowserTest() override =
      default;

  TelemetryExtensionTelemetryApiWithoutSerialNumberBrowserTest(
      const BaseTelemetryExtensionBrowserTest&) = delete;
  TelemetryExtensionTelemetryApiWithoutSerialNumberBrowserTest& operator=(
      const TelemetryExtensionTelemetryApiWithoutSerialNumberBrowserTest&) =
      delete;

 protected:
  std::string GetManifestFile(const std::string& matches_origin) override {
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
            "permissions": [ "os.diagnostics", "os.telemetry" ],
            "externally_connectable": {
              "matches": [
                "%s"
              ]
            },
            "options_page": "options.html"
          }
        )",
                              public_key().c_str(), matches_origin.c_str());
  }
};

IN_PROC_BROWSER_TEST_F(
    TelemetryExtensionTelemetryApiWithoutSerialNumberBrowserTest,
    GetBatteryInfoWithoutSerialNumberPermission) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If Probe interface is not available on this version of ash-chrome, this
  // test suite will no-op.
  if (!IsServiceAvailable()) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Configure FakeProbeService.
  {
    auto telemetry_info = crosapi::mojom::ProbeTelemetryInfo::New();
    {
      auto battery_info = crosapi::mojom::ProbeBatteryInfo::New();
      battery_info->cycle_count =
          crosapi::mojom::Int64Value::New(100000000000000);
      battery_info->voltage_now =
          crosapi::mojom::DoubleValue::New(1234567890.123456);
      battery_info->vendor = "Google";
      battery_info->serial_number = "abcdef";
      battery_info->charge_full_design =
          crosapi::mojom::DoubleValue::New(3000000000000000);
      battery_info->charge_full =
          crosapi::mojom::DoubleValue::New(9000000000000000);
      battery_info->voltage_min_design =
          crosapi::mojom::DoubleValue::New(1000000000.1001);
      battery_info->model_name = "Google Battery";
      battery_info->charge_now =
          crosapi::mojom::DoubleValue::New(7777777777.777);
      battery_info->current_now =
          crosapi::mojom::DoubleValue::New(0.9999999999999);
      battery_info->technology = "Li-ion";
      battery_info->status = "Charging";
      battery_info->manufacture_date = "2020-07-30";
      battery_info->temperature =
          crosapi::mojom::UInt64Value::New(7777777777777777);

      telemetry_info->battery_result =
          crosapi::mojom::ProbeBatteryResult::NewBatteryInfo(
              std::move(battery_info));
    }

    auto fake_service_impl = std::make_unique<FakeProbeService>();
    fake_service_impl->SetProbeTelemetryInfoResponse(std::move(telemetry_info));
    fake_service_impl->SetExpectedLastRequestedCategories(
        {crosapi::mojom::ProbeCategoryEnum::kBattery});

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function getBatteryInfo() {
        const result = await chrome.os.telemetry.getBatteryInfo();
         chrome.test.assertEq(
          // The dictionary members are ordered lexicographically by the Unicode
          // codepoints that comprise their identifiers.
          {
            chargeFull: 9000000000000000,
            chargeFullDesign: 3000000000000000,
            chargeNow: 7777777777.777,
            currentNow: 0.9999999999999,
            cycleCount: 100000000000000,
            manufactureDate: '2020-07-30',
            modelName: 'Google Battery',
            // serialNumber: null,
            status: 'Charging',
            technology: 'Li-ion',
            temperature: 7777777777777777,
            vendor: 'Google',
            voltageMinDesign: 1000000000.1001,
            voltageNow: 1234567890.123456,
          }, result);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(
    TelemetryExtensionTelemetryApiWithoutSerialNumberBrowserTest,
    GetOemDataWithoutSerialNumberPermission) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If Probe interface is not available on this version of ash-chrome, this
  // test suite will no-op.
  if (!IsServiceAvailable()) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Configure FakeProbeService.
  {
    auto fake_service_impl = std::make_unique<FakeProbeService>();
    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function getOemData() {
        await chrome.test.assertPromiseRejects(
            chrome.os.telemetry.getOemData(),
            'Error: Unauthorized access to chrome.os.telemetry.getOemData. ' +
            'Extension doesn\'t have the permission.'
        );
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(
    TelemetryExtensionTelemetryApiWithoutSerialNumberBrowserTest,
    GetVpdInfoWithoutSerialNumberPermission) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If Probe interface is not available on this version of ash-chrome, this
  // test suite will no-op.
  if (!IsServiceAvailable()) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Configure FakeProbeService.
  {
    auto telemetry_info = crosapi::mojom::ProbeTelemetryInfo::New();

    {
      auto vpd_info = crosapi::mojom::ProbeCachedVpdInfo::New();
      vpd_info->first_power_date = "2021-50";
      vpd_info->model_name = "COOL-LAPTOP-CHROME";
      vpd_info->serial_number = "5CD9132880";
      vpd_info->sku_number = "sku15";

      telemetry_info->vpd_result =
          crosapi::mojom::ProbeCachedVpdResult::NewVpdInfo(std::move(vpd_info));
    }

    auto fake_service_impl = std::make_unique<FakeProbeService>();
    fake_service_impl->SetProbeTelemetryInfoResponse(std::move(telemetry_info));
    fake_service_impl->SetExpectedLastRequestedCategories(
        {crosapi::mojom::ProbeCategoryEnum::kCachedVpdData});

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function getVpdInfo() {
        const result = await chrome.os.telemetry.getVpdInfo();
        chrome.test.assertEq("2021-50", result.activateDate);
        chrome.test.assertEq("COOL-LAPTOP-CHROME", result.modelName);
        chrome.test.assertEq(null, result.serialNumber);
        chrome.test.assertEq("sku15", result.skuNumber);
        chrome.test.succeed();
      }
    ]);
  )");
}

}  // namespace chromeos
