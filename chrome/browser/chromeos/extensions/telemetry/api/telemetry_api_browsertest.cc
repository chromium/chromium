// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "chrome/browser/chromeos/extensions/telemetry/api/base_telemetry_extension_browser_test.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon/fake_debug_daemon_client.h"
#include "chromeos/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

using TelemetryExtensionTelemetryApiBrowserTest =
    BaseTelemetryExtensionBrowserTest;

namespace {

class TestDebugDaemonClient : public FakeDebugDaemonClient {
 public:
  TestDebugDaemonClient() = default;
  ~TestDebugDaemonClient() override = default;

  void GetLog(const std::string& log_name,
              DBusMethodCallback<std::string> callback) override {
    EXPECT_EQ(log_name, "oemdata");
    std::move(callback).Run(absl::nullopt);
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(TelemetryExtensionTelemetryApiBrowserTest,
                       GetBatteryInfo_Error) {
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
  // Configure fake cros_healthd response.
  {
    auto telemetry_info = chromeos::cros_healthd::mojom::TelemetryInfo::New();
    {
      auto battery_info = chromeos::cros_healthd::mojom::BatteryInfo::New();
      battery_info->cycle_count = 100000000000000;
      battery_info->voltage_now = 1234567890.123456;
      battery_info->vendor = "Google";
      battery_info->serial_number = "abcdef";
      battery_info->charge_full_design = 3000000000000000;
      battery_info->charge_full = 9000000000000000;
      battery_info->voltage_min_design = 1000000000.1001;
      battery_info->model_name = "Google Battery";
      battery_info->charge_now = 7777777777.777;
      battery_info->current_now = 0.9999999999999;
      battery_info->technology = "Li-ion";
      battery_info->status = "Charging";
      battery_info->manufacture_date = "2020-07-30";
      battery_info->temperature =
          chromeos::cros_healthd::mojom::NullableUint64::New(7777777777777777);

      telemetry_info->battery_result =
          chromeos::cros_healthd::mojom::BatteryResult::NewBatteryInfo(
              std::move(battery_info));
    }

    ASSERT_TRUE(cros_healthd::FakeCrosHealthd::Get());
    cros_healthd::FakeCrosHealthd::Get()
        ->SetProbeTelemetryInfoResponseForTesting(telemetry_info);
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
  // Configure fake cros_healthd response.
  {
    auto telemetry_info = cros_healthd::mojom::TelemetryInfo::New();

    {
      auto c_state1 = chromeos::cros_healthd::mojom::CpuCStateInfo::New();
      c_state1->name = "C1";
      c_state1->time_in_state_since_last_boot_us = 1125899906875957;

      auto c_state2 = chromeos::cros_healthd::mojom::CpuCStateInfo::New();
      c_state2->name = "C2";
      c_state2->time_in_state_since_last_boot_us = 1125899906877777;

      auto logical_info1 = chromeos::cros_healthd::mojom::LogicalCpuInfo::New();
      logical_info1->max_clock_speed_khz = 2147473647;
      logical_info1->scaling_max_frequency_khz = 1073764046;
      logical_info1->scaling_current_frequency_khz = 536904245;
      // Idle time cannot be tested in browser test, because it requires USER_HZ
      // system constant to convert idle_time_user_hz to milliseconds.
      logical_info1->idle_time_user_hz = 0;
      logical_info1->c_states.push_back(std::move(c_state1));
      logical_info1->c_states.push_back(std::move(c_state2));

      auto logical_info2 = chromeos::cros_healthd::mojom::LogicalCpuInfo::New();
      logical_info2->max_clock_speed_khz = 1147494759;
      logical_info2->scaling_max_frequency_khz = 1063764046;
      logical_info2->scaling_current_frequency_khz = 936904246;
      // Idle time cannot be tested in browser test, because it requires USER_HZ
      // system constant to convert idle_time_user_hz to milliseconds.
      logical_info2->idle_time_user_hz = 0;

      auto physical_info1 =
          chromeos::cros_healthd::mojom::PhysicalCpuInfo::New();
      physical_info1->model_name = "i9";
      physical_info1->logical_cpus.push_back(std::move(logical_info1));
      physical_info1->logical_cpus.push_back(std::move(logical_info2));

      auto logical_info3 = chromeos::cros_healthd::mojom::LogicalCpuInfo::New();
      logical_info3->max_clock_speed_khz = 1247494759;
      logical_info3->scaling_max_frequency_khz = 1263764046;
      logical_info3->scaling_current_frequency_khz = 946904246;
      // Idle time cannot be tested in browser test, because it requires USER_HZ
      // system constant to convert idle_time_user_hz to milliseconds.
      logical_info3->idle_time_user_hz = 0;

      auto physical_info2 =
          chromeos::cros_healthd::mojom::PhysicalCpuInfo::New();
      physical_info2->model_name = "i9-low-powered";
      physical_info2->logical_cpus.push_back(std::move(logical_info3));

      auto cpu_info = chromeos::cros_healthd::mojom::CpuInfo::New();
      cpu_info->num_total_threads = 2147483647;
      cpu_info->architecture =
          chromeos::cros_healthd::mojom::CpuArchitectureEnum::kArmv7l;
      cpu_info->physical_cpus.push_back(std::move(physical_info1));
      cpu_info->physical_cpus.push_back(std::move(physical_info2));

      telemetry_info->cpu_result =
          chromeos::cros_healthd::mojom::CpuResult::NewCpuInfo(
              std::move(cpu_info));
    }

    ASSERT_TRUE(cros_healthd::FakeCrosHealthd::Get());

    cros_healthd::FakeCrosHealthd::Get()
        ->SetProbeTelemetryInfoResponseForTesting(telemetry_info);
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
  // Configure fake cros_healthd response.
  {
    auto telemetry_info = cros_healthd::mojom::TelemetryInfo::New();

    {
      auto memory_info = chromeos::cros_healthd::mojom::MemoryInfo::New();
      memory_info->total_memory_kib = 2147483647;
      memory_info->free_memory_kib = 2147483646;
      memory_info->available_memory_kib = 2147483645;
      memory_info->page_faults_since_last_boot = 4611686018427388000;

      telemetry_info->memory_result =
          chromeos::cros_healthd::mojom::MemoryResult::NewMemoryInfo(
              std::move(memory_info));
    }

    ASSERT_TRUE(cros_healthd::FakeCrosHealthd::Get());

    cros_healthd::FakeCrosHealthd::Get()
        ->SetProbeTelemetryInfoResponseForTesting(telemetry_info);
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
  DBusThreadManager::GetSetterForTesting()->SetDebugDaemonClient(
      std::make_unique<TestDebugDaemonClient>());

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
  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function getOemData() {
        const result = await chrome.os.telemetry.getOemData();
        chrome.test.assertEq(
          "oemdata: response from GetLog", result.oemData);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionTelemetryApiBrowserTest,
                       GetVpdInfoError) {
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
  // Configure fake cros_healthd response.
  {
    auto telemetry_info = cros_healthd::mojom::TelemetryInfo::New();

    {
      auto os_version = cros_healthd::mojom::OsVersion::New();

      auto system_info = cros_healthd::mojom::SystemInfo::New();
      system_info->first_power_date = "2021-50";
      system_info->product_model_name = "COOL-LAPTOP-CHROME";
      system_info->product_serial_number = "5CD9132880";
      system_info->product_sku_number = "sku15";
      system_info->os_version = std::move(os_version);

      telemetry_info->system_result =
          cros_healthd::mojom::SystemResult::NewSystemInfo(
              std::move(system_info));
    }

    ASSERT_TRUE(cros_healthd::FakeCrosHealthd::Get());

    cros_healthd::FakeCrosHealthd::Get()
        ->SetProbeTelemetryInfoResponseForTesting(telemetry_info);
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
  // Configure fake cros_healthd response.
  {
    auto telemetry_info = chromeos::cros_healthd::mojom::TelemetryInfo::New();
    {
      auto battery_info = chromeos::cros_healthd::mojom::BatteryInfo::New();
      battery_info->cycle_count = 100000000000000;
      battery_info->voltage_now = 1234567890.123456;
      battery_info->vendor = "Google";
      battery_info->serial_number = "abcdef";
      battery_info->charge_full_design = 3000000000000000;
      battery_info->charge_full = 9000000000000000;
      battery_info->voltage_min_design = 1000000000.1001;
      battery_info->model_name = "Google Battery";
      battery_info->charge_now = 7777777777.777;
      battery_info->current_now = 0.9999999999999;
      battery_info->technology = "Li-ion";
      battery_info->status = "Charging";
      battery_info->manufacture_date = "2020-07-30";
      battery_info->temperature =
          chromeos::cros_healthd::mojom::NullableUint64::New(7777777777777777);

      telemetry_info->battery_result =
          chromeos::cros_healthd::mojom::BatteryResult::NewBatteryInfo(
              std::move(battery_info));
    }

    ASSERT_TRUE(cros_healthd::FakeCrosHealthd::Get());
    cros_healthd::FakeCrosHealthd::Get()
        ->SetProbeTelemetryInfoResponseForTesting(telemetry_info);
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
  // Configure fake cros_healthd response.
  {
    auto telemetry_info = cros_healthd::mojom::TelemetryInfo::New();

    {
      auto os_version = cros_healthd::mojom::OsVersion::New();

      auto system_info = cros_healthd::mojom::SystemInfo::New();
      system_info->first_power_date = "2021-50";
      system_info->product_model_name = "COOL-LAPTOP-CHROME";
      system_info->product_serial_number = "5CD9132880";
      system_info->product_sku_number = "sku15";
      system_info->os_version = std::move(os_version);

      telemetry_info->system_result =
          cros_healthd::mojom::SystemResult::NewSystemInfo(
              std::move(system_info));
    }

    ASSERT_TRUE(cros_healthd::FakeCrosHealthd::Get());

    cros_healthd::FakeCrosHealthd::Get()
        ->SetProbeTelemetryInfoResponseForTesting(telemetry_info);
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
