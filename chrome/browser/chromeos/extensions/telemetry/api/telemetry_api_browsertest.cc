// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "chrome/browser/ash/telemetry_extension/fake_probe_service.h"
#include "chrome/browser/ash/telemetry_extension/probe_service.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/base_telemetry_extension_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class TelemetryExtensionTelemetryApiBrowserTest
    : public BaseTelemetryExtensionBrowserTest {
 public:
  TelemetryExtensionTelemetryApiBrowserTest() {
    ash::ProbeService::Factory::SetForTesting(&fake_probe_factory_);
  }
  ~TelemetryExtensionTelemetryApiBrowserTest() override = default;

  TelemetryExtensionTelemetryApiBrowserTest(
      const TelemetryExtensionTelemetryApiBrowserTest&) = delete;
  TelemetryExtensionTelemetryApiBrowserTest& operator=(
      const TelemetryExtensionTelemetryApiBrowserTest&) = delete;

 protected:
  void SetServiceForTesting(
      std::unique_ptr<ash::FakeProbeService> fake_diagnostics_service_impl) {
    fake_probe_factory_.SetCreateInstanceResponse(
        std::move(fake_diagnostics_service_impl));
  }

  ash::FakeProbeService::Factory fake_probe_factory_;
};

IN_PROC_BROWSER_TEST_F(TelemetryExtensionTelemetryApiBrowserTest,
                       GetBatteryInfo_Error) {
  // Configure FakeProbeService.
  {
    auto fake_service_impl = std::make_unique<ash::FakeProbeService>();
    fake_service_impl->SetExpectedLastRequestedCategories(
        {ash::health::mojom::ProbeCategoryEnum::kBattery});

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
  // Configure FakeProbeService.
  {
    auto telemetry_info = ash::health::mojom::TelemetryInfo::New();
    {
      auto battery_info = ash::health::mojom::BatteryInfo::New();
      battery_info->cycle_count =
          ash::health::mojom::Int64Value::New(100000000000000);
      battery_info->voltage_now =
          ash::health::mojom::DoubleValue::New(1234567890.123456);
      battery_info->vendor = "Google";
      battery_info->serial_number = "abcdef";
      battery_info->charge_full_design =
          ash::health::mojom::DoubleValue::New(3000000000000000);
      battery_info->charge_full =
          ash::health::mojom::DoubleValue::New(9000000000000000);
      battery_info->voltage_min_design =
          ash::health::mojom::DoubleValue::New(1000000000.1001);
      battery_info->model_name = "Google Battery";
      battery_info->charge_now =
          ash::health::mojom::DoubleValue::New(7777777777.777);
      battery_info->current_now =
          ash::health::mojom::DoubleValue::New(0.9999999999999);
      battery_info->technology = "Li-ion";
      battery_info->status = "Charging";
      battery_info->manufacture_date = "2020-07-30";
      battery_info->temperature =
          ash::health::mojom::UInt64Value::New(7777777777777777);

      telemetry_info->battery_result =
          ash::health::mojom::BatteryResult::NewBatteryInfo(
              std::move(battery_info));
    }

    auto fake_service_impl = std::make_unique<ash::FakeProbeService>();
    fake_service_impl->SetProbeTelemetryInfoResponse(std::move(telemetry_info));
    fake_service_impl->SetExpectedLastRequestedCategories(
        {ash::health::mojom::ProbeCategoryEnum::kBattery});

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
  // Configure FakeProbeService.
  {
    auto fake_service_impl = std::make_unique<ash::FakeProbeService>();
    fake_service_impl->SetExpectedLastRequestedCategories(
        {ash::health::mojom::ProbeCategoryEnum::kCpu});

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
  // Configure FakeProbeService.
  {
    auto telemetry_info = ash::health::mojom::TelemetryInfo::New();

    {
      auto c_state1 = ash::health::mojom::CpuCStateInfo::New();
      c_state1->name = "C1";
      c_state1->time_in_state_since_last_boot_us =
          ash::health::mojom::UInt64Value::New(1125899906875957);

      auto c_state2 = ash::health::mojom::CpuCStateInfo::New();
      c_state2->name = "C2";
      c_state2->time_in_state_since_last_boot_us =
          ash::health::mojom::UInt64Value::New(1125899906877777);

      auto logical_info1 = ash::health::mojom::LogicalCpuInfo::New();
      logical_info1->max_clock_speed_khz =
          ash::health::mojom::UInt32Value::New(2147473647);
      logical_info1->scaling_max_frequency_khz =
          ash::health::mojom::UInt32Value::New(1073764046);
      logical_info1->scaling_current_frequency_khz =
          ash::health::mojom::UInt32Value::New(536904245);
      // Idle time cannot be tested in browser test, because it requires USER_HZ
      // system constant to convert idle_time_user_hz to milliseconds.
      logical_info1->idle_time_ms = ash::health::mojom::UInt64Value::New(0);
      logical_info1->c_states.push_back(std::move(c_state1));
      logical_info1->c_states.push_back(std::move(c_state2));

      auto logical_info2 = ash::health::mojom::LogicalCpuInfo::New();
      logical_info2->max_clock_speed_khz =
          ash::health::mojom::UInt32Value::New(1147494759);
      logical_info2->scaling_max_frequency_khz =
          ash::health::mojom::UInt32Value::New(1063764046);
      logical_info2->scaling_current_frequency_khz =
          ash::health::mojom::UInt32Value::New(936904246);
      // Idle time cannot be tested in browser test, because it requires USER_HZ
      // system constant to convert idle_time_user_hz to milliseconds.
      logical_info2->idle_time_ms = ash::health::mojom::UInt64Value::New(0);

      auto physical_info1 = ash::health::mojom::PhysicalCpuInfo::New();
      physical_info1->model_name = "i9";
      physical_info1->logical_cpus.push_back(std::move(logical_info1));
      physical_info1->logical_cpus.push_back(std::move(logical_info2));

      auto logical_info3 = ash::health::mojom::LogicalCpuInfo::New();
      logical_info3->max_clock_speed_khz =
          ash::health::mojom::UInt32Value::New(1247494759);
      logical_info3->scaling_max_frequency_khz =
          ash::health::mojom::UInt32Value::New(1263764046);
      logical_info3->scaling_current_frequency_khz =
          ash::health::mojom::UInt32Value::New(946904246);
      // Idle time cannot be tested in browser test, because it requires USER_HZ
      // system constant to convert idle_time_user_hz to milliseconds.
      logical_info3->idle_time_ms = ash::health::mojom::UInt64Value::New(0);

      auto physical_info2 = ash::health::mojom::PhysicalCpuInfo::New();
      physical_info2->model_name = "i9-low-powered";
      physical_info2->logical_cpus.push_back(std::move(logical_info3));

      auto cpu_info = ash::health::mojom::CpuInfo::New();
      cpu_info->num_total_threads =
          ash::health::mojom::UInt32Value::New(2147483647);
      cpu_info->architecture = ash::health::mojom::CpuArchitectureEnum::kArmv7l;
      cpu_info->physical_cpus.push_back(std::move(physical_info1));
      cpu_info->physical_cpus.push_back(std::move(physical_info2));

      telemetry_info->cpu_result =
          ash::health::mojom::CpuResult::NewCpuInfo(std::move(cpu_info));
    }

    auto fake_service_impl = std::make_unique<ash::FakeProbeService>();
    fake_service_impl->SetProbeTelemetryInfoResponse(std::move(telemetry_info));
    fake_service_impl->SetExpectedLastRequestedCategories(
        {ash::health::mojom::ProbeCategoryEnum::kCpu});

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
  // Configure FakeProbeService.
  {
    auto fake_service_impl = std::make_unique<ash::FakeProbeService>();
    fake_service_impl->SetExpectedLastRequestedCategories(
        {ash::health::mojom::ProbeCategoryEnum::kMemory});

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
  // Configure FakeProbeService.
  {
    auto telemetry_info = ash::health::mojom::TelemetryInfo::New();

    {
      auto memory_info = ash::health::mojom::MemoryInfo::New();
      memory_info->total_memory_kib =
          ash::health::mojom::UInt32Value::New(2147483647);
      memory_info->free_memory_kib =
          ash::health::mojom::UInt32Value::New(2147483646);
      memory_info->available_memory_kib =
          ash::health::mojom::UInt32Value::New(2147483645);
      memory_info->page_faults_since_last_boot =
          ash::health::mojom::UInt64Value::New(4611686018427388000);

      telemetry_info->memory_result =
          ash::health::mojom::MemoryResult::NewMemoryInfo(
              std::move(memory_info));
    }

    auto fake_service_impl = std::make_unique<ash::FakeProbeService>();
    fake_service_impl->SetProbeTelemetryInfoResponse(std::move(telemetry_info));
    fake_service_impl->SetExpectedLastRequestedCategories(
        {ash::health::mojom::ProbeCategoryEnum::kMemory});

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
  // Configure FakeProbeService.
  {
    auto fake_service_impl = std::make_unique<ash::FakeProbeService>();

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
  // Configure FakeProbeService.
  {
    auto fake_service_impl = std::make_unique<ash::FakeProbeService>();

    auto oem_data = ash::health::mojom::OemData::New();
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
  // Configure FakeProbeService.
  {
    auto fake_service_impl = std::make_unique<ash::FakeProbeService>();
    fake_service_impl->SetExpectedLastRequestedCategories(
        {ash::health::mojom::ProbeCategoryEnum::kSystem});

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
  // Configure FakeProbeService.
  {
    auto telemetry_info = ash::health::mojom::TelemetryInfo::New();
    {
      auto os_version_info = ash::health::mojom::OsVersion::New();
      os_version_info->release_milestone = "87";
      os_version_info->build_number = "13544";
      os_version_info->patch_number = "59.0";
      os_version_info->release_channel = "stable-channel";

      auto os_info = ash::health::mojom::OsInfo::New();
      os_info->os_version = std::move(os_version_info);

      auto system_info = ash::health::mojom::SystemInfo::New();
      system_info->os_info = std::move(os_info);

      telemetry_info->system_result =
          ash::health::mojom::SystemResult::NewSystemInfo(
              std::move(system_info));
    }

    auto fake_service_impl = std::make_unique<ash::FakeProbeService>();
    fake_service_impl->SetProbeTelemetryInfoResponse(std::move(telemetry_info));
    fake_service_impl->SetExpectedLastRequestedCategories(
        {ash::health::mojom::ProbeCategoryEnum::kSystem});

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
  // Configure FakeProbeService.
  {
    auto fake_service_impl = std::make_unique<ash::FakeProbeService>();
    fake_service_impl->SetExpectedLastRequestedCategories(
        {ash::health::mojom::ProbeCategoryEnum::kCachedVpdData});

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
  // Configure FakeProbeService.
  {
    auto telemetry_info = ash::health::mojom::TelemetryInfo::New();

    {
      auto vpd_info = ash::health::mojom::CachedVpdInfo::New();
      vpd_info->first_power_date = "2021-50";
      vpd_info->model_name = "COOL-LAPTOP-CHROME";
      vpd_info->serial_number = "5CD9132880";
      vpd_info->sku_number = "sku15";

      telemetry_info->vpd_result =
          ash::health::mojom::CachedVpdResult::NewVpdInfo(std::move(vpd_info));
    }

    auto fake_service_impl = std::make_unique<ash::FakeProbeService>();
    fake_service_impl->SetProbeTelemetryInfoResponse(std::move(telemetry_info));
    fake_service_impl->SetExpectedLastRequestedCategories(
        {ash::health::mojom::ProbeCategoryEnum::kCachedVpdData});

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
  // Configure FakeProbeService.
  {
    auto fake_service_impl = std::make_unique<ash::FakeProbeService>();
    fake_service_impl->SetExpectedLastRequestedCategories(
        {ash::health::mojom::ProbeCategoryEnum::kStatefulPartition});

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
  // Configure FakeProbeService.
  {
    auto telemetry_info = ash::health::mojom::TelemetryInfo::New();
    {
      auto stateful_part_info =
          ash::health::mojom::StatefulPartitionInfo::New();
      stateful_part_info->available_space =
          ash::health::mojom::UInt64Value::New(3000000000000000);
      stateful_part_info->total_space =
          ash::health::mojom::UInt64Value::New(9000000000000000);

      telemetry_info->stateful_partition_result =
          ash::health::mojom::StatefulPartitionResult::NewPartitionInfo(
              std::move(stateful_part_info));
    }

    auto fake_service_impl = std::make_unique<ash::FakeProbeService>();
    fake_service_impl->SetProbeTelemetryInfoResponse(std::move(telemetry_info));
    fake_service_impl->SetExpectedLastRequestedCategories(
        {ash::health::mojom::ProbeCategoryEnum::kStatefulPartition});

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
  // Configure FakeProbeService.
  {
    auto telemetry_info = ash::health::mojom::TelemetryInfo::New();
    {
      auto battery_info = ash::health::mojom::BatteryInfo::New();
      battery_info->cycle_count =
          ash::health::mojom::Int64Value::New(100000000000000);
      battery_info->voltage_now =
          ash::health::mojom::DoubleValue::New(1234567890.123456);
      battery_info->vendor = "Google";
      battery_info->serial_number = "abcdef";
      battery_info->charge_full_design =
          ash::health::mojom::DoubleValue::New(3000000000000000);
      battery_info->charge_full =
          ash::health::mojom::DoubleValue::New(9000000000000000);
      battery_info->voltage_min_design =
          ash::health::mojom::DoubleValue::New(1000000000.1001);
      battery_info->model_name = "Google Battery";
      battery_info->charge_now =
          ash::health::mojom::DoubleValue::New(7777777777.777);
      battery_info->current_now =
          ash::health::mojom::DoubleValue::New(0.9999999999999);
      battery_info->technology = "Li-ion";
      battery_info->status = "Charging";
      battery_info->manufacture_date = "2020-07-30";
      battery_info->temperature =
          ash::health::mojom::UInt64Value::New(7777777777777777);

      telemetry_info->battery_result =
          ash::health::mojom::BatteryResult::NewBatteryInfo(
              std::move(battery_info));
    }

    auto fake_service_impl = std::make_unique<ash::FakeProbeService>();
    fake_service_impl->SetProbeTelemetryInfoResponse(std::move(telemetry_info));
    fake_service_impl->SetExpectedLastRequestedCategories(
        {ash::health::mojom::ProbeCategoryEnum::kBattery});

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
  // Configure FakeDiagnosticsService.
  {
    auto fake_service_impl = std::make_unique<ash::FakeProbeService>();
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
  // Configure FakeProbeService.
  {
    auto telemetry_info = ash::health::mojom::TelemetryInfo::New();

    {
      auto vpd_info = ash::health::mojom::CachedVpdInfo::New();
      vpd_info->first_power_date = "2021-50";
      vpd_info->model_name = "COOL-LAPTOP-CHROME";
      vpd_info->serial_number = "5CD9132880";
      vpd_info->sku_number = "sku15";

      telemetry_info->vpd_result =
          ash::health::mojom::CachedVpdResult::NewVpdInfo(std::move(vpd_info));
    }

    auto fake_service_impl = std::make_unique<ash::FakeProbeService>();
    fake_service_impl->SetProbeTelemetryInfoResponse(std::move(telemetry_info));
    fake_service_impl->SetExpectedLastRequestedCategories(
        {ash::health::mojom::ProbeCategoryEnum::kCachedVpdData});

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
