// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_logs/reven_log_source.h"

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/dbus/cros_healthd/cros_healthd_client.h"
#include "chromeos/dbus/cros_healthd/fake_cros_healthd_client.h"
#include "chromeos/services/cros_healthd/public/cpp/service_connection.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using ::testing::HasSubstr;

namespace system_logs {

namespace {

namespace healthd = ::chromeos::cros_healthd::mojom;
constexpr char kRevenLogKey[] = "CLOUDREADY_HARDWARE_INFO";

constexpr char kBluetoothAdapterNameKey[] = "bluetooth_adapter_name";
constexpr char kBluetoothAdapterNameVal[] = "BlueZ 5.54";
constexpr char kPoweredKey[] = "powered";

constexpr char kCpuNameKey[] = "cpu_name";
constexpr char kCpuNameVal[] = "Intel(R) Core(TM) i5-10210U CPU @ 1.60GHz";

constexpr char kTotalMemoryKey[] = "total_memory_kib";
constexpr char kFreeMemoryKey[] = "free_memory_kib";
constexpr char kAvailableMemoryKey[] = "available_memory_kib";
constexpr int kTotalMemory = 2048;
constexpr int kFreeMemory = 1024;
constexpr int kAvailableMemory = 512;

void SetBluetoothAdapterInfo(healthd::TelemetryInfoPtr& telemetry_info,
                             bool powered) {
  auto adapter_info = healthd::BluetoothAdapterInfo::New();
  adapter_info->name = kBluetoothAdapterNameVal;
  adapter_info->powered = powered;

  std::vector<healthd::BluetoothAdapterInfoPtr> adapters;
  adapters.push_back(std::move(adapter_info));

  telemetry_info->bluetooth_result =
      healthd::BluetoothResult::NewBluetoothAdapterInfo(std::move(adapters));
}

void SetBluetoothAdapterInfoWithProbeError(
    healthd::TelemetryInfoPtr& telemetry_info) {
  auto probe_error =
      healthd::ProbeError::New(healthd::ErrorType::kFileReadError, "");
  telemetry_info->bluetooth_result =
      healthd::BluetoothResult::NewError(std::move(probe_error));
}

void SetCpuInfo(healthd::TelemetryInfoPtr& telemetry_info) {
  auto cpu_info = healthd::CpuInfo::New();
  auto physical_cpu_info = healthd::PhysicalCpuInfo::New();
  auto logical_cpu_info = healthd::LogicalCpuInfo::New();
  physical_cpu_info->logical_cpus.push_back(std::move(logical_cpu_info));
  physical_cpu_info->model_name = kCpuNameVal;
  cpu_info->num_total_threads = 2;
  cpu_info->physical_cpus.emplace_back(std::move(physical_cpu_info));

  telemetry_info->cpu_result =
      healthd::CpuResult::NewCpuInfo(std::move(cpu_info));
}

void SetCpuInfoWithProbeError(healthd::TelemetryInfoPtr& telemetry_info) {
  auto probe_error =
      healthd::ProbeError::New(healthd::ErrorType::kFileReadError, "");
  telemetry_info->cpu_result =
      healthd::CpuResult::NewError(std::move(probe_error));
}

void SetMemoryInfo(healthd::TelemetryInfoPtr& telemetry_info) {
  auto memory_info =
      healthd::MemoryInfo::New(kTotalMemory, kFreeMemory, kAvailableMemory, 0);
  telemetry_info->memory_result =
      healthd::MemoryResult::NewMemoryInfo(std::move(memory_info));
}

void SetMemoryInfoWithProbeError(healthd::TelemetryInfoPtr& telemetry_info) {
  auto probe_error =
      healthd::ProbeError::New(healthd::ErrorType::kFileReadError, "");
  telemetry_info->memory_result =
      healthd::MemoryResult::NewError(std::move(probe_error));
}

void SetSystemInfoV2(healthd::TelemetryInfoPtr& telemetry_info,
                     healthd::DmiInfoPtr dmi_info) {
  auto system_info2 = healthd::SystemInfoV2::New();
  system_info2->os_info = healthd::OsInfo::New();
  system_info2->os_info->os_version = healthd::OsVersion::New();

  if (dmi_info) {
    system_info2->dmi_info = std::move(dmi_info);
  }
  telemetry_info->system_result_v2 =
      healthd::SystemResultV2::NewSystemInfoV2(std::move(system_info2));
}

healthd::DmiInfoPtr CreateDmiInfo() {
  healthd::DmiInfoPtr dmi_info = healthd::DmiInfo::New();
  dmi_info->sys_vendor = absl::optional<std::string>("LENOVO");
  dmi_info->product_name = absl::optional<std::string>("20U9001PUS");
  dmi_info->product_version =
      absl::optional<std::string>("ThinkPad X1 Carbon Gen 8");
  return dmi_info;
}

}  // namespace

class RevenLogSourceTest : public ::testing::Test {
 public:
  RevenLogSourceTest() {
    chromeos::CrosHealthdClient::InitializeFake();
    source_ = std::make_unique<RevenLogSource>();
  }

  ~RevenLogSourceTest() override {
    source_.reset();
    chromeos::CrosHealthdClient::Shutdown();
    base::RunLoop().RunUntilIdle();
  }

  std::unique_ptr<SystemLogsResponse> Fetch() {
    std::unique_ptr<SystemLogsResponse> result;
    base::RunLoop run_loop;
    source_->Fetch(base::BindOnce(
        [](std::unique_ptr<SystemLogsResponse>* result,
           base::OnceClosure quit_closure,
           std::unique_ptr<SystemLogsResponse> response) {
          *result = std::move(response);
          std::move(quit_closure).Run();
        },
        &result, run_loop.QuitClosure()));
    run_loop.Run();

    return result;
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<RevenLogSource> source_;
};

TEST_F(RevenLogSourceTest, FetchBluetoothAdapterInfoSuccessPowerOn) {
  auto info = healthd::TelemetryInfo::New();
  SetBluetoothAdapterInfo(info, /* powered = */ true);
  ash::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);

  std::unique_ptr<SystemLogsResponse> response = Fetch();
  ASSERT_NE(response, nullptr);
  const auto revenlog_iter = response->find(kRevenLogKey);
  ASSERT_NE(revenlog_iter, response->end());

  EXPECT_THAT(revenlog_iter->second,
              HasSubstr("bluetooth_adapter_name: BlueZ 5.54"));
  EXPECT_THAT(revenlog_iter->second, HasSubstr("powered: on"));
}

TEST_F(RevenLogSourceTest, FetchBluetoothAdapterInfoSuccessPowerOff) {
  auto info = healthd::TelemetryInfo::New();
  SetBluetoothAdapterInfo(info, /* powered = */ false);
  ash::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);

  std::unique_ptr<SystemLogsResponse> response = Fetch();
  ASSERT_NE(response, nullptr);
  const auto revenlog_iter = response->find(kRevenLogKey);
  ASSERT_NE(revenlog_iter, response->end());

  EXPECT_THAT(revenlog_iter->second,
              HasSubstr("bluetooth_adapter_name: BlueZ 5.54"));
  EXPECT_THAT(revenlog_iter->second, HasSubstr("powered: off"));
}

TEST_F(RevenLogSourceTest, FetchBluetoothAdapterInfoFailure) {
  auto info = healthd::TelemetryInfo::New();
  SetBluetoothAdapterInfoWithProbeError(info);
  ash::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);

  std::unique_ptr<SystemLogsResponse> response = Fetch();
  ASSERT_NE(response, nullptr);
  const auto revenlog_iter = response->find(kRevenLogKey);
  ASSERT_NE(revenlog_iter, response->end());

  EXPECT_EQ(revenlog_iter->second.find(kBluetoothAdapterNameKey),
            std::string::npos);
  EXPECT_EQ(revenlog_iter->second.find(kPoweredKey), std::string::npos);
}

TEST_F(RevenLogSourceTest, FetchCpuInfoSuccess) {
  auto info = healthd::TelemetryInfo::New();
  SetCpuInfo(info);
  ash::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);

  std::unique_ptr<SystemLogsResponse> response = Fetch();
  ASSERT_NE(response, nullptr);
  const auto revenlog_iter = response->find(kRevenLogKey);
  ASSERT_NE(revenlog_iter, response->end());

  EXPECT_THAT(revenlog_iter->second,
              HasSubstr("cpu_name: Intel(R) Core(TM) i5-10210U CPU @ 1.60GHz"));
}

TEST_F(RevenLogSourceTest, FetchCpuInfoFailure) {
  auto info = healthd::TelemetryInfo::New();
  SetCpuInfoWithProbeError(info);
  ash::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);

  std::unique_ptr<SystemLogsResponse> response = Fetch();
  ASSERT_NE(response, nullptr);
  const auto revenlog_iter = response->find(kRevenLogKey);
  ASSERT_NE(revenlog_iter, response->end());

  EXPECT_EQ(revenlog_iter->second.find(kCpuNameKey), std::string::npos);
}

TEST_F(RevenLogSourceTest, FetchMemoryInfoSuccess) {
  auto info = healthd::TelemetryInfo::New();
  SetMemoryInfo(info);
  ash::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);

  std::unique_ptr<SystemLogsResponse> response = Fetch();
  ASSERT_NE(response, nullptr);
  const auto revenlog_iter = response->find(kRevenLogKey);
  ASSERT_NE(revenlog_iter, response->end());

  EXPECT_THAT(revenlog_iter->second, HasSubstr("total_memory_kib: 2048"));
  EXPECT_THAT(revenlog_iter->second, HasSubstr("free_memory_kib: 1024"));
  EXPECT_THAT(revenlog_iter->second, HasSubstr("available_memory_kib: 512"));
}

TEST_F(RevenLogSourceTest, FetchMemoryInfoFailure) {
  auto info = healthd::TelemetryInfo::New();
  SetMemoryInfoWithProbeError(info);
  ash::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);

  std::unique_ptr<SystemLogsResponse> response = Fetch();
  ASSERT_NE(response, nullptr);
  const auto revenlog_iter = response->find(kRevenLogKey);
  ASSERT_NE(revenlog_iter, response->end());

  EXPECT_EQ(revenlog_iter->second.find(kTotalMemoryKey), std::string::npos);
  EXPECT_EQ(revenlog_iter->second.find(kFreeMemoryKey), std::string::npos);
  EXPECT_EQ(revenlog_iter->second.find(kAvailableMemoryKey), std::string::npos);
}

TEST_F(RevenLogSourceTest, FetchDmiInfoWithValues) {
  auto info = healthd::TelemetryInfo::New();
  auto dmi_info = CreateDmiInfo();
  SetSystemInfoV2(info, std::move(dmi_info));
  ash::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);

  std::unique_ptr<SystemLogsResponse> response = Fetch();
  ASSERT_NE(response, nullptr);
  const auto revenlog_iter = response->find(kRevenLogKey);
  ASSERT_NE(revenlog_iter, response->end());

  EXPECT_THAT(revenlog_iter->second, HasSubstr("product_vendor: LENOVO"));
  EXPECT_THAT(revenlog_iter->second, HasSubstr("product_name: 20U9001PUS"));
  EXPECT_THAT(revenlog_iter->second,
              HasSubstr("product_version: ThinkPad X1 Carbon Gen 8"));
}

TEST_F(RevenLogSourceTest, FetchDmiInfoWithoutValues) {
  auto info = healthd::TelemetryInfo::New();
  auto dmi_info = healthd::DmiInfo::New();
  SetSystemInfoV2(info, std::move(dmi_info));
  ash::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);

  std::unique_ptr<SystemLogsResponse> response = Fetch();
  ASSERT_NE(response, nullptr);
  const auto revenlog_iter = response->find(kRevenLogKey);
  ASSERT_NE(revenlog_iter, response->end());

  EXPECT_THAT(revenlog_iter->second, HasSubstr("product_vendor: \n"));
  EXPECT_THAT(revenlog_iter->second, HasSubstr("product_name: \n"));
  EXPECT_THAT(revenlog_iter->second, HasSubstr("product_version: \n"));
}

}  // namespace system_logs
