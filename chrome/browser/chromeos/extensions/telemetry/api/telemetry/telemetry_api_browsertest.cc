// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/common/base_telemetry_extension_browser_test.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/common/remote_probe_service_strategy.h"
#include "chromeos/crosapi/cpp/telemetry/fake_probe_service.h"
#include "chromeos/crosapi/mojom/nullable_primitives.mojom.h"
#include "chromeos/crosapi/mojom/probe_service.mojom.h"
#include "chromeos/services/network_config/public/mojom/network_types.mojom.h"
#include "chromeos/services/network_health/public/mojom/network_health_types.mojom.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/telemetry_extension/telemetry/probe_service_ash.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace chromeos {

namespace {

namespace crosapi = ::crosapi::mojom;

using testing::UnorderedElementsAreArray;

}  // namespace

class TelemetryExtensionTelemetryApiBrowserTest
    : public BaseTelemetryExtensionBrowserTest {
 public:
  TelemetryExtensionTelemetryApiBrowserTest() {}
  ~TelemetryExtensionTelemetryApiBrowserTest() override = default;

  TelemetryExtensionTelemetryApiBrowserTest(
      const TelemetryExtensionTelemetryApiBrowserTest&) = delete;
  TelemetryExtensionTelemetryApiBrowserTest& operator=(
      const TelemetryExtensionTelemetryApiBrowserTest&) = delete;

  // Set up the fake probe service for accessing healthd.
  void SetUpProbeService() {
    probe_service_ = std::make_unique<FakeProbeService>();
    RemoteProbeServiceStrategy::Get()->SetServiceForTesting(
        probe_service_->BindNewPipeAndPassRemote());
  }

 protected:
  std::unique_ptr<FakeProbeService> probe_service_;
};

IN_PROC_BROWSER_TEST_F(TelemetryExtensionTelemetryApiBrowserTest,
                       GetAudioInfo_Error) {
  SetUpProbeService();
  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function getAudioInfo() {
        await chrome.test.assertPromiseRejects(
            chrome.os.telemetry.getAudioInfo(),
            'Error: API internal error'
        );
        chrome.test.succeed();
      }
    ]);
  )");
  EXPECT_THAT(probe_service_->GetLastRequestedCategories(),
              UnorderedElementsAreArray({crosapi::ProbeCategoryEnum::kAudio}));
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionTelemetryApiBrowserTest,
                       GetAudioInfo_Success) {
  SetUpProbeService();
  // Configure FakeProbeService.
  {
    auto telemetry_info = crosapi::ProbeTelemetryInfo::New();
    {
      std::vector<crosapi::ProbeAudioOutputNodeInfoPtr> output_infos;
      auto output_node_info = crosapi::ProbeAudioOutputNodeInfo::New();
      output_node_info->id = crosapi::UInt64Value::New(43);
      output_node_info->name = "Internal Speaker";
      output_node_info->device_name = "HDA Intel PCH: CA0132 Analog:0,0";
      output_node_info->active = crosapi::BoolValue::New(false);
      output_node_info->node_volume = crosapi::UInt8Value::New(212);
      output_infos.push_back(std::move(output_node_info));

      std::vector<crosapi::ProbeAudioInputNodeInfoPtr> input_infos;
      auto input_node_info = crosapi::ProbeAudioInputNodeInfo::New();
      input_node_info->id = crosapi::UInt64Value::New(42);
      input_node_info->name = "External Mic";
      input_node_info->device_name = "HDA Intel PCH: CA0132 Analog:1,0";
      input_node_info->active = crosapi::BoolValue::New(true);
      input_node_info->node_gain = crosapi::UInt8Value::New(1);
      input_infos.push_back(std::move(input_node_info));

      auto audio_info = crosapi::ProbeAudioInfo::New();
      audio_info->output_mute = crosapi::BoolValue::New(true);
      audio_info->input_mute = crosapi::BoolValue::New(false);
      audio_info->underruns = crosapi::UInt32Value::New(56);
      audio_info->severe_underruns = crosapi::UInt32Value::New(3);
      audio_info->output_nodes = std::move(output_infos);
      audio_info->input_nodes = std::move(input_infos);

      telemetry_info->audio_result =
          crosapi::ProbeAudioResult::NewAudioInfo(std::move(audio_info));
    }
    probe_service_->SetProbeTelemetryInfoResponse(std::move(telemetry_info));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function getAudioInfo() {
        const result = await chrome.os.telemetry.getAudioInfo();
        chrome.test.assertEq(
          // The dictionary members are ordered lexicographically by the Unicode
          // codepoints that comprise their identifiers.
          {
            inputMute: false,
            inputNodes: [{
              active: true,
              deviceName: 'HDA Intel PCH: CA0132 Analog:1,0',
              id: 42,
              name: 'External Mic',
              nodeGain: 1,
            }],
            outputMute: true,
            outputNodes: [{
              active: false,
              deviceName: 'HDA Intel PCH: CA0132 Analog:0,0',
              id: 43,
              name: 'Internal Speaker',
              nodeVolume: 212
            }],
            severeUnderruns: 3,
            underruns: 56,
          }, result);

          chrome.test.succeed();
      }
    ]);
  )");
  EXPECT_THAT(probe_service_->GetLastRequestedCategories(),
              UnorderedElementsAreArray({crosapi::ProbeCategoryEnum::kAudio}));
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionTelemetryApiBrowserTest,
                       GetBatteryInfo_ApiInternalError) {
  SetUpProbeService();
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
  EXPECT_THAT(
      probe_service_->GetLastRequestedCategories(),
      UnorderedElementsAreArray({crosapi::ProbeCategoryEnum::kBattery}));
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionTelemetryApiBrowserTest,
                       GetBatteryInfo_Success) {
  SetUpProbeService();
  // Configure FakeProbeService.
  {
    auto telemetry_info = crosapi::ProbeTelemetryInfo::New();
    {
      auto battery_info = crosapi::ProbeBatteryInfo::New();
      battery_info->cycle_count = crosapi::Int64Value::New(100000000000000);
      battery_info->voltage_now = crosapi::DoubleValue::New(1234567890.123456);
      battery_info->vendor = "Google";
      battery_info->serial_number = "abcdef";
      battery_info->charge_full_design =
          crosapi::DoubleValue::New(3000000000000000);
      battery_info->charge_full = crosapi::DoubleValue::New(9000000000000000);
      battery_info->voltage_min_design =
          crosapi::DoubleValue::New(1000000000.1001);
      battery_info->model_name = "Google Battery";
      battery_info->charge_now = crosapi::DoubleValue::New(7777777777.777);
      battery_info->current_now = crosapi::DoubleValue::New(0.9999999999999);
      battery_info->technology = "Li-ion";
      battery_info->status = "Charging";
      battery_info->manufacture_date = "2020-07-30";
      battery_info->temperature = crosapi::UInt64Value::New(7777777777777777);

      telemetry_info->battery_result =
          crosapi::ProbeBatteryResult::NewBatteryInfo(std::move(battery_info));
    }
    probe_service_->SetProbeTelemetryInfoResponse(std::move(telemetry_info));
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
  EXPECT_THAT(
      probe_service_->GetLastRequestedCategories(),
      UnorderedElementsAreArray({crosapi::ProbeCategoryEnum::kBattery}));
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionTelemetryApiBrowserTest,
                       GetNonRemovableBlockDeviceInfo_Error) {
  SetUpProbeService();
  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function getNonRemovableBlockDevicesInfo() {
        await chrome.test.assertPromiseRejects(
            chrome.os.telemetry.getNonRemovableBlockDevicesInfo(),
            'Error: API internal error'
        );
        chrome.test.succeed();
      }
    ]);
  )");
  EXPECT_THAT(probe_service_->GetLastRequestedCategories(),
              UnorderedElementsAreArray(
                  {crosapi::ProbeCategoryEnum::kNonRemovableBlockDevices}));
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionTelemetryApiBrowserTest,
                       GetNonRemovableBlockDeviceInfo_Success) {
  SetUpProbeService();
  // Configure FakeProbeService.
  {
    auto telemetry_info = crosapi::ProbeTelemetryInfo::New();
    {
      auto first_element = crosapi::ProbeNonRemovableBlockDeviceInfo::New();
      first_element->size = crosapi::UInt64Value::New(100000000);
      first_element->name = "TestName1";
      first_element->type = "TestType1";

      auto second_element = crosapi::ProbeNonRemovableBlockDeviceInfo::New();
      second_element->size = crosapi::UInt64Value::New(200000000);
      second_element->name = "TestName2";
      second_element->type = "TestType2";

      std::vector<crosapi::ProbeNonRemovableBlockDeviceInfoPtr>
          block_devices_info;
      block_devices_info.push_back(std::move(first_element));
      block_devices_info.push_back(std::move(second_element));

      telemetry_info->block_device_result =
          crosapi::ProbeNonRemovableBlockDeviceResult::NewBlockDeviceInfo(
              std::move(block_devices_info));
    }
    probe_service_->SetProbeTelemetryInfoResponse(std::move(telemetry_info));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function getNonRemovableBlockDevicesInfo() {
        const result = await chrome.os.telemetry
                          .getNonRemovableBlockDevicesInfo();
        chrome.test.assertEq(2, result.deviceInfos.length);

        const deviceResult = result.deviceInfos;
        chrome.test.assertEq(100000000, deviceResult[0].size);
        chrome.test.assertEq("TestName1", deviceResult[0].name);
        chrome.test.assertEq("TestType1", deviceResult[0].type);

        chrome.test.assertEq(200000000, deviceResult[1].size);
        chrome.test.assertEq("TestName2", deviceResult[1].name);
        chrome.test.assertEq("TestType2", deviceResult[1].type);

        chrome.test.succeed();
      }
    ]);
  )");
  EXPECT_THAT(probe_service_->GetLastRequestedCategories(),
              UnorderedElementsAreArray(
                  {crosapi::ProbeCategoryEnum::kNonRemovableBlockDevices}));
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionTelemetryApiBrowserTest,
                       GetCpuInfo_Error) {
  SetUpProbeService();
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
  EXPECT_THAT(probe_service_->GetLastRequestedCategories(),
              UnorderedElementsAreArray({crosapi::ProbeCategoryEnum::kCpu}));
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionTelemetryApiBrowserTest,
                       GetCpuInfo_Success) {
  SetUpProbeService();
  // Configure FakeProbeService.
  {
    auto telemetry_info = crosapi::ProbeTelemetryInfo::New();
    {
      auto c_state1 = crosapi::ProbeCpuCStateInfo::New();
      c_state1->name = "C1";
      c_state1->time_in_state_since_last_boot_us =
          crosapi::UInt64Value::New(1125899906875957);

      auto c_state2 = crosapi::ProbeCpuCStateInfo::New();
      c_state2->name = "C2";
      c_state2->time_in_state_since_last_boot_us =
          crosapi::UInt64Value::New(1125899906877777);

      auto logical_info1 = crosapi::ProbeLogicalCpuInfo::New();
      logical_info1->max_clock_speed_khz =
          crosapi::UInt32Value::New(2147473647);
      logical_info1->scaling_max_frequency_khz =
          crosapi::UInt32Value::New(1073764046);
      logical_info1->scaling_current_frequency_khz =
          crosapi::UInt32Value::New(536904245);
      // Idle time cannot be tested in browser test, because it requires
      // USER_HZ system constant to convert idle_time_user_hz to milliseconds.
      logical_info1->idle_time_ms = crosapi::UInt64Value::New(0);
      logical_info1->c_states.push_back(std::move(c_state1));
      logical_info1->c_states.push_back(std::move(c_state2));
      logical_info1->core_id = crosapi::UInt32Value::New(42);

      auto logical_info2 = crosapi::ProbeLogicalCpuInfo::New();
      logical_info2->max_clock_speed_khz =
          crosapi::UInt32Value::New(1147494759);
      logical_info2->scaling_max_frequency_khz =
          crosapi::UInt32Value::New(1063764046);
      logical_info2->scaling_current_frequency_khz =
          crosapi::UInt32Value::New(936904246);
      // Idle time cannot be tested in browser test, because it requires
      // USER_HZ system constant to convert idle_time_user_hz to milliseconds.
      logical_info2->idle_time_ms = crosapi::UInt64Value::New(0);
      logical_info2->core_id = crosapi::UInt32Value::New(43);

      auto physical_info1 = crosapi::ProbePhysicalCpuInfo::New();
      physical_info1->model_name = "i9";
      physical_info1->logical_cpus.push_back(std::move(logical_info1));
      physical_info1->logical_cpus.push_back(std::move(logical_info2));

      auto logical_info3 = crosapi::ProbeLogicalCpuInfo::New();
      logical_info3->max_clock_speed_khz =
          crosapi::UInt32Value::New(1247494759);
      logical_info3->scaling_max_frequency_khz =
          crosapi::UInt32Value::New(1263764046);
      logical_info3->scaling_current_frequency_khz =
          crosapi::UInt32Value::New(946904246);
      // Idle time cannot be tested in browser test, because it requires
      // USER_HZ system constant to convert idle_time_user_hz to milliseconds.
      logical_info3->idle_time_ms = crosapi::UInt64Value::New(0);
      logical_info3->core_id = crosapi::UInt32Value::New(44);

      auto physical_info2 = crosapi::ProbePhysicalCpuInfo::New();
      physical_info2->model_name = "i9-low-powered";
      physical_info2->logical_cpus.push_back(std::move(logical_info3));

      auto cpu_info = crosapi::ProbeCpuInfo::New();
      cpu_info->num_total_threads = crosapi::UInt32Value::New(2147483647);
      cpu_info->architecture = crosapi::ProbeCpuArchitectureEnum::kArmv7l;
      cpu_info->physical_cpus.push_back(std::move(physical_info1));
      cpu_info->physical_cpus.push_back(std::move(physical_info2));

      telemetry_info->cpu_result =
          crosapi::ProbeCpuResult::NewCpuInfo(std::move(cpu_info));
    }
    probe_service_->SetProbeTelemetryInfoResponse(std::move(telemetry_info));
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
                'coreId': 42,
                'idleTimeMs': 0,
                'maxClockSpeedKhz': 2147473647,
                'scalingCurrentFrequencyKhz': 536904245,
                'scalingMaxFrequencyKhz': 1073764046,
            }, {
                'cStates': [],
                'coreId': 43,
                'idleTimeMs': 0,
                'maxClockSpeedKhz': 1147494759,
                'scalingCurrentFrequencyKhz': 936904246,
                'scalingMaxFrequencyKhz': 1063764046,
            }],
            'modelName': 'i9',
          }, {
            'logicalCpus': [{
              'cStates': [],
              'coreId': 44,
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
  EXPECT_THAT(probe_service_->GetLastRequestedCategories(),
              UnorderedElementsAreArray({crosapi::ProbeCategoryEnum::kCpu}));
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionTelemetryApiBrowserTest,
                       GetMarketingInfo_Error) {
  SetUpProbeService();
  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function getMarketingInfo() {
        await chrome.test.assertPromiseRejects(
            chrome.os.telemetry.getMarketingInfo(),
            'Error: API internal error'
        );
        chrome.test.succeed();
      }
    ]);
  )");
  EXPECT_THAT(probe_service_->GetLastRequestedCategories(),
              UnorderedElementsAreArray({crosapi::ProbeCategoryEnum::kSystem}));
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionTelemetryApiBrowserTest,
                       GetMarketingInfo_Success) {
  SetUpProbeService();
  // Configure FakeProbeService.
  {
    auto telemetry_info = crosapi::ProbeTelemetryInfo::New();
    {
      auto os_info = crosapi::ProbeOsInfo::New();
      os_info->marketing_name = "Test Marketing Name";

      auto system_info = crosapi::ProbeSystemInfo::New();
      system_info->os_info = std::move(os_info);

      telemetry_info->system_result =
          crosapi::ProbeSystemResult::NewSystemInfo(std::move(system_info));
    }
    probe_service_->SetProbeTelemetryInfoResponse(std::move(telemetry_info));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function getMarketingInfo() {
        const result = await chrome.os.telemetry.getMarketingInfo();
        chrome.test.assertEq(
          {
            marketingName: "Test Marketing Name",
          }, result);
        chrome.test.succeed();
      }
    ]);
  )");
  EXPECT_THAT(probe_service_->GetLastRequestedCategories(),
              UnorderedElementsAreArray({crosapi::ProbeCategoryEnum::kSystem}));
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionTelemetryApiBrowserTest,
                       GetMemoryInfo_Error) {
  SetUpProbeService();
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
  EXPECT_THAT(probe_service_->GetLastRequestedCategories(),
              UnorderedElementsAreArray({crosapi::ProbeCategoryEnum::kMemory}));
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionTelemetryApiBrowserTest,
                       GetMemoryInfo_Success) {
  SetUpProbeService();
  // Configure FakeProbeService.
  {
    auto telemetry_info = crosapi::ProbeTelemetryInfo::New();
    {
      auto memory_info = crosapi::ProbeMemoryInfo::New();
      memory_info->total_memory_kib = crosapi::UInt32Value::New(2147483647);
      memory_info->free_memory_kib = crosapi::UInt32Value::New(2147483646);
      memory_info->available_memory_kib = crosapi::UInt32Value::New(2147483645);
      memory_info->page_faults_since_last_boot =
          crosapi::UInt64Value::New(4611686018427388000);

      telemetry_info->memory_result =
          crosapi::ProbeMemoryResult::NewMemoryInfo(std::move(memory_info));
    }
    probe_service_->SetProbeTelemetryInfoResponse(std::move(telemetry_info));
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
  EXPECT_THAT(probe_service_->GetLastRequestedCategories(),
              UnorderedElementsAreArray({crosapi::ProbeCategoryEnum::kMemory}));
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionTelemetryApiBrowserTest,
                       GetInternetConnectivityInfo_Error) {
  SetUpProbeService();
  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function getInternetConnectivityInfo() {
        await chrome.test.assertPromiseRejects(
            chrome.os.telemetry.getInternetConnectivityInfo(),
            'Error: API internal error'
        );
        chrome.test.succeed();
      }
    ]);
  )");
  EXPECT_THAT(
      probe_service_->GetLastRequestedCategories(),
      UnorderedElementsAreArray({crosapi::ProbeCategoryEnum::kNetwork}));
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionTelemetryApiBrowserTest,
                       GetInternetConnectivityInfo_Success) {
  SetUpProbeService();
  // Configure FakeProbeService.
  {
    auto telemetry_info = crosapi::ProbeTelemetryInfo::New();
    {
      auto network = chromeos::network_health::mojom::Network::New();
      network->type = chromeos::network_config::mojom::NetworkType::kWiFi;
      network->state = chromeos::network_health::mojom::NetworkState::kOnline;
      network->mac_address = "00:00:5e:00:53:af";
      network->ipv4_address = "1.1.1.1";
      network->ipv6_addresses = {"FE80:CD00:0000:0CDE:1257:0000:211E:729C"};
      network->signal_strength =
          chromeos::network_health::mojom::UInt32Value::New(100);

      auto network_info =
          chromeos::network_health::mojom::NetworkHealthState::New();
      network_info->networks.push_back(std::move(network));

      telemetry_info->network_result =
          crosapi::ProbeNetworkResult::NewNetworkHealth(
              std::move(network_info));
    }
    probe_service_->SetProbeTelemetryInfoResponse(std::move(telemetry_info));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function getInternetConnectivityInfo() {
        const result = await chrome.os.telemetry.getInternetConnectivityInfo();
        chrome.test.assertEq(1, result.networks.length);

        const network_result = result.networks[0];
        chrome.test.assertEq('wifi', network_result.type);
        chrome.test.assertEq('online', network_result.state);
        chrome.test.assertEq('00:00:5e:00:53:af', network_result.macAddress);
        chrome.test.assertEq('1.1.1.1', network_result.ipv4Address);
        chrome.test.assertEq(['FE80:CD00:0000:0CDE:1257:0000:211E:729C'],
          network_result.ipv6Addresses);
        chrome.test.assertEq(100, network_result.signalStrength);
        chrome.test.succeed();
      }
    ]);
  )");
  EXPECT_THAT(
      probe_service_->GetLastRequestedCategories(),
      UnorderedElementsAreArray({crosapi::ProbeCategoryEnum::kNetwork}));
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionTelemetryApiBrowserTest,
                       GetOemDataWithSerialNumberPermission_Error) {
  SetUpProbeService();
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
  SetUpProbeService();
  // Configure FakeProbeService.
  {
    auto oem_data = crosapi::ProbeOemData::New();
    oem_data->oem_data = "123456789";
    probe_service_->SetOemDataResponse(std::move(oem_data));
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
  SetUpProbeService();
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
  EXPECT_THAT(probe_service_->GetLastRequestedCategories(),
              UnorderedElementsAreArray({crosapi::ProbeCategoryEnum::kSystem}));
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionTelemetryApiBrowserTest,
                       GetOsVersionInfo_Success) {
  SetUpProbeService();
  // Configure FakeProbeService.
  {
    auto telemetry_info = crosapi::ProbeTelemetryInfo::New();
    {
      auto os_version_info = crosapi::ProbeOsVersion::New();
      os_version_info->release_milestone = "87";
      os_version_info->build_number = "13544";
      os_version_info->patch_number = "59.0";
      os_version_info->release_channel = "stable-channel";

      auto os_info = crosapi::ProbeOsInfo::New();
      os_info->os_version = std::move(os_version_info);

      auto system_info = crosapi::ProbeSystemInfo::New();
      system_info->os_info = std::move(os_info);

      telemetry_info->system_result =
          crosapi::ProbeSystemResult::NewSystemInfo(std::move(system_info));
    }
    probe_service_->SetProbeTelemetryInfoResponse(std::move(telemetry_info));
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
  EXPECT_THAT(probe_service_->GetLastRequestedCategories(),
              UnorderedElementsAreArray({crosapi::ProbeCategoryEnum::kSystem}));
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionTelemetryApiBrowserTest,
                       GetVpdInfoError) {
  SetUpProbeService();
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
  EXPECT_THAT(
      probe_service_->GetLastRequestedCategories(),
      UnorderedElementsAreArray({crosapi::ProbeCategoryEnum::kCachedVpdData}));
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionTelemetryApiBrowserTest,
                       GetVpdInfoWithSerialNumberPermission) {
  SetUpProbeService();
  // Configure FakeProbeService.
  {
    auto telemetry_info = crosapi::ProbeTelemetryInfo::New();
    {
      auto vpd_info = crosapi::ProbeCachedVpdInfo::New();
      vpd_info->first_power_date = "2021-50";
      vpd_info->model_name = "COOL-LAPTOP-CHROME";
      vpd_info->serial_number = "5CD9132880";
      vpd_info->sku_number = "sku15";

      telemetry_info->vpd_result =
          crosapi::ProbeCachedVpdResult::NewVpdInfo(std::move(vpd_info));
    }
    probe_service_->SetProbeTelemetryInfoResponse(std::move(telemetry_info));
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
  EXPECT_THAT(
      probe_service_->GetLastRequestedCategories(),
      UnorderedElementsAreArray({crosapi::ProbeCategoryEnum::kCachedVpdData}));
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionTelemetryApiBrowserTest,
                       GetStatefulPartitionInfo_Error) {
  SetUpProbeService();
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
  EXPECT_THAT(probe_service_->GetLastRequestedCategories(),
              UnorderedElementsAreArray(
                  {crosapi::ProbeCategoryEnum::kStatefulPartition}));
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionTelemetryApiBrowserTest,
                       GetStatefulPartitionInfo_Success) {
  SetUpProbeService();
  // Configure FakeProbeService.
  {
    auto telemetry_info = crosapi::ProbeTelemetryInfo::New();
    {
      auto stateful_part_info = crosapi::ProbeStatefulPartitionInfo::New();
      stateful_part_info->available_space =
          crosapi::UInt64Value::New(3000000000000000);
      stateful_part_info->total_space =
          crosapi::UInt64Value::New(9000000000000000);

      telemetry_info->stateful_partition_result =
          crosapi::ProbeStatefulPartitionResult::NewPartitionInfo(
              std::move(stateful_part_info));
    }
    probe_service_->SetProbeTelemetryInfoResponse(std::move(telemetry_info));
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
  EXPECT_THAT(probe_service_->GetLastRequestedCategories(),
              UnorderedElementsAreArray(
                  {crosapi::ProbeCategoryEnum::kStatefulPartition}));
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionTelemetryApiBrowserTest,
                       GetTpmInfo_Error) {
  SetUpProbeService();
  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function getTpmInfo() {
        await chrome.test.assertPromiseRejects(
            chrome.os.telemetry.getTpmInfo(),
            'Error: API internal error'
        );
        chrome.test.succeed();
      }
    ]);
  )");
  EXPECT_THAT(probe_service_->GetLastRequestedCategories(),
              UnorderedElementsAreArray({crosapi::ProbeCategoryEnum::kTpm}));
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionTelemetryApiBrowserTest,
                       GetTpmInfo_Success) {
  SetUpProbeService();
  // Configure FakeProbeService.
  {
    auto telemetry_info = crosapi::ProbeTelemetryInfo::New();
    {
      auto tpm_version = crosapi::ProbeTpmVersion::New();
      tpm_version->gsc_version = crosapi::ProbeTpmGSCVersion::kCr50;
      tpm_version->family = crosapi::UInt32Value::New(120);
      tpm_version->spec_level = crosapi::UInt64Value::New(1000);
      tpm_version->manufacturer = crosapi::UInt32Value::New(42);
      tpm_version->tpm_model = crosapi::UInt32Value::New(333);
      tpm_version->firmware_version = crosapi::UInt64Value::New(10000);
      tpm_version->vendor_specific = "VendorSpecific";

      auto tpm_status = crosapi::ProbeTpmStatus::New();
      tpm_status->enabled = crosapi::BoolValue::New(true);
      tpm_status->owned = crosapi::BoolValue::New(false);
      tpm_status->owner_password_is_present = crosapi::BoolValue::New(false);

      auto dictonary_attack = crosapi::ProbeTpmDictionaryAttack::New();
      dictonary_attack->counter = crosapi::UInt32Value::New(5);
      dictonary_attack->threshold = crosapi::UInt32Value::New(1000);
      dictonary_attack->lockout_in_effect = crosapi::BoolValue::New(false);
      dictonary_attack->lockout_seconds_remaining =
          crosapi::UInt32Value::New(0);

      auto tpm_info = crosapi::ProbeTpmInfo::New();
      tpm_info->version = std::move(tpm_version);
      tpm_info->status = std::move(tpm_status);
      tpm_info->dictionary_attack = std::move(dictonary_attack);

      telemetry_info->tpm_result =
          crosapi::ProbeTpmResult::NewTpmInfo(std::move(tpm_info));
    }
    probe_service_->SetProbeTelemetryInfoResponse(std::move(telemetry_info));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function getTpmInfo() {
        const result = await chrome.os.telemetry.getTpmInfo();
        chrome.test.assertEq(
          // The dictionary members are ordered lexicographically by the Unicode
          // codepoints that comprise their identifiers.
          {
            version: {
              gscVersion: "cr50",
              family: 120,
              specLevel: 1000,
              manufacturer: 42,
              tpmModel: 333,
              firmwareVersion: 10000,
              vendorSpecific: "VendorSpecific",
            },
            status: {
              enabled: true,
              owned: false,
              ownerPasswordIsPresent: false,
            },
            dictionaryAttack: {
              counter: 5,
              threshold: 1000,
              lockoutInEffect: false,
              lockoutSecondsRemaining: 0,
            },
          }, result);
        chrome.test.succeed();
      }
    ]);
  )");
  EXPECT_THAT(probe_service_->GetLastRequestedCategories(),
              UnorderedElementsAreArray({crosapi::ProbeCategoryEnum::kTpm}));
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionTelemetryApiBrowserTest,
                       GetUsbBusInfo_Error) {
  SetUpProbeService();
  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function getUsbBusInfo() {
        await chrome.test.assertPromiseRejects(
            chrome.os.telemetry.getUsbBusInfo(),
            'Error: API internal error'
        );
        chrome.test.succeed();
      }
    ]);
  )");
  EXPECT_THAT(probe_service_->GetLastRequestedCategories(),
              UnorderedElementsAreArray({crosapi::ProbeCategoryEnum::kBus}));
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionTelemetryApiBrowserTest,
                       GetUsbBusInfo_Success) {
  SetUpProbeService();
  // Configure FakeProbeService.
  {
    auto telemetry_info = crosapi::ProbeTelemetryInfo::New();
    {
      std::vector<crosapi::ProbeUsbBusInterfaceInfoPtr> interfaces;
      interfaces.push_back(crosapi::ProbeUsbBusInterfaceInfo::New(
          crosapi::UInt8Value::New(42), crosapi::UInt8Value::New(41),
          crosapi::UInt8Value::New(43), crosapi::UInt8Value::New(44),
          "MyDriver"));

      auto fwupd_version = crosapi::ProbeFwupdFirmwareVersionInfo::New(
          "MyVersion", crosapi::ProbeFwupdVersionFormat::kPair);

      auto usb_bus_info = crosapi::ProbeUsbBusInfo::New();
      usb_bus_info->class_id = crosapi::UInt8Value::New(45);
      usb_bus_info->subclass_id = crosapi::UInt8Value::New(46);
      usb_bus_info->protocol_id = crosapi::UInt8Value::New(47);
      usb_bus_info->vendor_id = crosapi::UInt16Value::New(48);
      usb_bus_info->product_id = crosapi::UInt16Value::New(49);
      usb_bus_info->interfaces = std::move(interfaces);
      usb_bus_info->fwupd_firmware_version_info = std::move(fwupd_version);
      usb_bus_info->version = crosapi::ProbeUsbVersion::kUsb3;
      usb_bus_info->spec_speed = crosapi::ProbeUsbSpecSpeed::k20Gbps;

      auto bus_info =
          crosapi::ProbeBusInfo::NewUsbBusInfo(std::move(usb_bus_info));

      std::vector<crosapi::ProbeBusInfoPtr> input;
      input.push_back(std::move(bus_info));

      telemetry_info->bus_result =
          crosapi::ProbeBusResult::NewBusDevicesInfo(std::move(input));
    }
    probe_service_->SetProbeTelemetryInfoResponse(std::move(telemetry_info));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function getUsbBusInfo() {
        const result = await chrome.os.telemetry.getUsbBusInfo();
        chrome.test.assertEq(
          // The dictionary members are ordered lexicographically by the Unicode
          // codepoints that comprise their identifiers.
          {
            "devices": [
              {
                "classId": 45,
                "fwupdFirmwareVersionInfo": {
                  "version": "MyVersion",
                  "version_format":"pair"
                },
                "interfaces": [
                  {
                    "classId": 41,
                    "driver": "MyDriver",
                    "interfaceNumber": 42,
                    "protocolId": 44,
                    "subclassId": 43
                  }
                ],
                "productId": 49,
                "protocolId": 47,
                "spec_speed": "n20Gbps",
                "subclassId": 46,
                "vendorId": 48,
                "version": "usb3"
              }
            ]
          }, result);
        chrome.test.succeed();
      }
    ]);
  )");
  EXPECT_THAT(probe_service_->GetLastRequestedCategories(),
              UnorderedElementsAreArray({crosapi::ProbeCategoryEnum::kBus}));
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionTelemetryApiBrowserTest,
                       GetDisplayInfo_Error) {
  SetUpProbeService();
  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function getDisplayInfo() {
        await chrome.test.assertPromiseRejects(
            chrome.os.telemetry.getDisplayInfo(),
            'Error: API internal error'
        );
        chrome.test.succeed();
      }
    ]);
  )");
  EXPECT_THAT(
      probe_service_->GetLastRequestedCategories(),
      UnorderedElementsAreArray({crosapi::ProbeCategoryEnum::kDisplay}));
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionTelemetryApiBrowserTest,
                       GetDisplayInfo_Success) {
  SetUpProbeService();
  // Configure FakeProbeService.
  {
    auto telemetry_info = crosapi::ProbeTelemetryInfo::New();
    {
      auto embedded_display = crosapi::ProbeEmbeddedDisplayInfo::New();
      embedded_display->privacy_screen_supported = true;
      embedded_display->privacy_screen_enabled = false;
      embedded_display->display_width = 1;
      embedded_display->display_height = 2;
      embedded_display->resolution_horizontal = 3;
      embedded_display->resolution_vertical = 4;
      embedded_display->refresh_rate = 5;
      embedded_display->manufacturer = "manufacturer1";
      embedded_display->model_id = 6;
      embedded_display->serial_number = 7;
      embedded_display->manufacture_week = 8;
      embedded_display->manufacture_year = 9;
      embedded_display->edid_version = "1.4";
      embedded_display->input_type = crosapi::ProbeDisplayInputType::kDigital;
      embedded_display->display_name = "display1";

      auto dp_info_1 = crosapi::ProbeExternalDisplayInfo::New();
      dp_info_1->display_width = 11;
      dp_info_1->display_height = 12;
      dp_info_1->resolution_horizontal = 13;
      dp_info_1->resolution_vertical = 14;
      dp_info_1->refresh_rate = 15;
      dp_info_1->manufacturer = "manufacturer2";
      dp_info_1->model_id = 16;
      dp_info_1->serial_number = 17;
      dp_info_1->manufacture_week = 18;
      dp_info_1->manufacture_year = 19;
      dp_info_1->edid_version = "1.4";
      dp_info_1->input_type = crosapi::ProbeDisplayInputType::kAnalog;
      dp_info_1->display_name = "display2";

      auto dp_info_2 = crosapi::ProbeExternalDisplayInfo::New();

      std::vector<crosapi::ProbeExternalDisplayInfoPtr> external_displays;
      external_displays.push_back(std::move(dp_info_1));
      external_displays.push_back(std::move(dp_info_2));

      auto display_info = crosapi::ProbeDisplayInfo::New(
          std::move(embedded_display), std::move(external_displays));

      telemetry_info->display_result =
          crosapi::ProbeDisplayResult::NewDisplayInfo(std::move(display_info));
    }
    probe_service_->SetProbeTelemetryInfoResponse(std::move(telemetry_info));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function getDisplayInfo() {
        const result = await chrome.os.telemetry.getDisplayInfo();
        chrome.test.assertEq(
          // The dictionary members are ordered lexicographically by the Unicode
          // codepoints that comprise their identifiers.
          {
            "externalDisplays": [
              {
                "displayHeight": 12,
                "displayName": "display2",
                "displayWidth": 11,
                "edidVersion": "1.4",
                "inputType": "analog",
                "manufactureWeek": 18,
                "manufactureYear": 19,
                "manufacturer": "manufacturer2",
                "modelId": 16,
                "refreshRate": 15,
                "resolutionHorizontal": 13,
                "resolutionVertical": 14
              },
              {
                "inputType": "unknown"
              }
            ],
            "embeddedDisplay": {
              "displayHeight": 2,
              "displayName": "display1",
              "displayWidth": 1,
              "edidVersion": "1.4",
              "inputType": "digital",
              "manufactureWeek": 8,
              "manufactureYear": 9,
              "manufacturer": "manufacturer1",
              "modelId": 6,
              "privacyScreenEnabled": false,
              "privacyScreenSupported": true,
              "refreshRate": 5,
              "resolutionHorizontal": 3,
              "resolutionVertical": 4
            }
          }, result);
        chrome.test.succeed();
      }
    ]);
  )");
  EXPECT_THAT(
      probe_service_->GetLastRequestedCategories(),
      UnorderedElementsAreArray({crosapi::ProbeCategoryEnum::kDisplay}));
}
class TelemetryExtensionTelemetryApiWithoutAdditionalPermissionsBrowserTest
    : public TelemetryExtensionTelemetryApiBrowserTest {
 public:
  TelemetryExtensionTelemetryApiWithoutAdditionalPermissionsBrowserTest() =
      default;
  ~TelemetryExtensionTelemetryApiWithoutAdditionalPermissionsBrowserTest()
      override = default;

  TelemetryExtensionTelemetryApiWithoutAdditionalPermissionsBrowserTest(
      const BaseTelemetryExtensionBrowserTest&) = delete;
  TelemetryExtensionTelemetryApiWithoutAdditionalPermissionsBrowserTest&
  operator=(
      const TelemetryExtensionTelemetryApiWithoutAdditionalPermissionsBrowserTest&) =
      delete;

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
            "permissions": [ "os.diagnostics", "os.telemetry" ],
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
    TelemetryExtensionTelemetryApiWithoutAdditionalPermissionsBrowserTest,
    GetBatteryInfoWithoutSerialNumberPermission) {
  SetUpProbeService();
  // Configure FakeProbeService.
  {
    auto telemetry_info = crosapi::ProbeTelemetryInfo::New();
    {
      auto battery_info = crosapi::ProbeBatteryInfo::New();
      battery_info->cycle_count = crosapi::Int64Value::New(100000000000000);
      battery_info->voltage_now = crosapi::DoubleValue::New(1234567890.123456);
      battery_info->vendor = "Google";
      battery_info->serial_number = "abcdef";
      battery_info->charge_full_design =
          crosapi::DoubleValue::New(3000000000000000);
      battery_info->charge_full = crosapi::DoubleValue::New(9000000000000000);
      battery_info->voltage_min_design =
          crosapi::DoubleValue::New(1000000000.1001);
      battery_info->model_name = "Google Battery";
      battery_info->charge_now = crosapi::DoubleValue::New(7777777777.777);
      battery_info->current_now = crosapi::DoubleValue::New(0.9999999999999);
      battery_info->technology = "Li-ion";
      battery_info->status = "Charging";
      battery_info->manufacture_date = "2020-07-30";
      battery_info->temperature = crosapi::UInt64Value::New(7777777777777777);

      telemetry_info->battery_result =
          crosapi::ProbeBatteryResult::NewBatteryInfo(std::move(battery_info));
    }
    probe_service_->SetProbeTelemetryInfoResponse(std::move(telemetry_info));
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
  EXPECT_THAT(
      probe_service_->GetLastRequestedCategories(),
      UnorderedElementsAreArray({crosapi::ProbeCategoryEnum::kBattery}));
}

IN_PROC_BROWSER_TEST_F(
    TelemetryExtensionTelemetryApiWithoutAdditionalPermissionsBrowserTest,
    GetOemInternetConnectivityWithoutPermission) {
  SetUpProbeService();
  // Configure FakeProbeService.
  {
    auto telemetry_info = crosapi::ProbeTelemetryInfo::New();
    {
      auto network = chromeos::network_health::mojom::Network::New();
      network->type = chromeos::network_config::mojom::NetworkType::kWiFi;
      network->state = chromeos::network_health::mojom::NetworkState::kOnline;
      network->mac_address = "00:00:5e:00:53:af";
      network->ipv4_address = "1.1.1.1";
      network->ipv6_addresses = {"FE80:CD00:0000:0CDE:1257:0000:211E:729C"};
      network->signal_strength =
          chromeos::network_health::mojom::UInt32Value::New(100);

      auto network_info =
          chromeos::network_health::mojom::NetworkHealthState::New();
      network_info->networks.push_back(std::move(network));

      telemetry_info->network_result =
          crosapi::ProbeNetworkResult::NewNetworkHealth(
              std::move(network_info));
    }
    probe_service_->SetProbeTelemetryInfoResponse(std::move(telemetry_info));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function getInternetConnectivityInfo() {
        const result = await chrome.os.telemetry.getInternetConnectivityInfo();
        chrome.test.assertEq(1, result.networks.length);

        const network_result = result.networks[0];
        chrome.test.assertEq('wifi', network_result.type);
        chrome.test.assertEq('online', network_result.state);
        chrome.test.assertEq('1.1.1.1', network_result.ipv4Address);
        chrome.test.assertEq(null, network_result.macAddress);
        chrome.test.assertEq(['FE80:CD00:0000:0CDE:1257:0000:211E:729C'],
          network_result.ipv6Addresses);
        chrome.test.assertEq(100, network_result.signalStrength);
        chrome.test.succeed();
      }
    ]);
  )");
  EXPECT_THAT(
      probe_service_->GetLastRequestedCategories(),
      UnorderedElementsAreArray({crosapi::ProbeCategoryEnum::kNetwork}));
}

IN_PROC_BROWSER_TEST_F(
    TelemetryExtensionTelemetryApiWithoutAdditionalPermissionsBrowserTest,
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
    TelemetryExtensionTelemetryApiWithoutAdditionalPermissionsBrowserTest,
    GetUsbBusInfoWithoutPermission) {
  SetUpProbeService();
  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function getUsbBusInfo() {
        await chrome.test.assertPromiseRejects(
            chrome.os.telemetry.getUsbBusInfo(),
            'Error: Unauthorized access to chrome.os.telemetry.' +
            'getUsbBusInfo. Extension doesn\'t have the permission.'
        );
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(
    TelemetryExtensionTelemetryApiWithoutAdditionalPermissionsBrowserTest,
    GetVpdInfoWithoutSerialNumberPermission) {
  SetUpProbeService();
  // Configure FakeProbeService.
  {
    auto telemetry_info = crosapi::ProbeTelemetryInfo::New();
    {
      auto vpd_info = crosapi::ProbeCachedVpdInfo::New();
      vpd_info->first_power_date = "2021-50";
      vpd_info->model_name = "COOL-LAPTOP-CHROME";
      vpd_info->serial_number = "5CD9132880";
      vpd_info->sku_number = "sku15";

      telemetry_info->vpd_result =
          crosapi::ProbeCachedVpdResult::NewVpdInfo(std::move(vpd_info));
    }
    probe_service_->SetProbeTelemetryInfoResponse(std::move(telemetry_info));
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
  EXPECT_THAT(
      probe_service_->GetLastRequestedCategories(),
      UnorderedElementsAreArray({crosapi::ProbeCategoryEnum::kCachedVpdData}));
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionTelemetryApiBrowserTest,
                       GetThermalInfo_Success) {
  SetUpProbeService();
  // Configure FakeProbeService.
  {
    auto telemetry_info = crosapi::ProbeTelemetryInfo::New();
    {
      auto thermal_sensor_1 = crosapi::ProbeThermalSensorInfo::New();
      thermal_sensor_1->name = "thermal_sensor_1";
      thermal_sensor_1->temperature_celsius = 100;
      thermal_sensor_1->source = crosapi::ProbeThermalSensorSource::kEc;

      auto thermal_sensor_2 = crosapi::ProbeThermalSensorInfo::New();
      thermal_sensor_2->name = "thermal_sensor_2";
      thermal_sensor_2->temperature_celsius = 50;
      thermal_sensor_2->source = crosapi::ProbeThermalSensorSource::kSysFs;

      std::vector<crosapi::ProbeThermalSensorInfoPtr> thermal_sensors;
      thermal_sensors.push_back(std::move(thermal_sensor_1));
      thermal_sensors.push_back(std::move(thermal_sensor_2));

      auto Thermal_info =
          crosapi::ProbeThermalInfo::New(std::move(thermal_sensors));

      telemetry_info->thermal_result =
          crosapi::ProbeThermalResult::NewThermalInfo(std::move(Thermal_info));
    }
    probe_service_->SetProbeTelemetryInfoResponse(std::move(telemetry_info));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function getThermalInfo() {
        const result = await chrome.os.telemetry.getThermalInfo();
        chrome.test.assertEq(
          // The dictionary members are ordered lexicographically by the Unicode
          // codepoints that comprise their identifiers.
          {
            "thermalSensors": [
              {
                "name": "thermal_sensor_1",
                "temperatureCelsius": 100,
                "source": "ec",
              },
              {
                "name": "thermal_sensor_2",
                "temperatureCelsius": 50,
                "source": "sysFs",
              }
            ]
          }, result);
        chrome.test.succeed();
      }
    ]);
  )");
  EXPECT_THAT(
      probe_service_->GetLastRequestedCategories(),
      UnorderedElementsAreArray({crosapi::ProbeCategoryEnum::kThermal}));
}

}  // namespace chromeos
