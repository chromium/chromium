// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_metric_sampler.h"

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/cros_healthd/cros_healthd_client.h"
#include "chromeos/dbus/cros_healthd/fake_cros_healthd_client.h"
#include "chromeos/services/cros_healthd/public/cpp/service_connection.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cros_healthd = chromeos::cros_healthd::mojom;
using ::testing::Eq;

struct TbtTestCase {
  std::string test_name;
  cros_healthd::ThunderboltSecurityLevel healthd_security_level;
  reporting::ThunderboltSecurityLevel reporting_security_level;
};

namespace reporting {
namespace test {

cros_healthd::KeylockerInfoPtr CreateKeylockerInfo(bool configured) {
  return cros_healthd::KeylockerInfo::New(configured);
}

cros_healthd::TelemetryInfoPtr CreateCpuResult(
    cros_healthd::KeylockerInfoPtr keylocker_info) {
  auto telemetry_info = cros_healthd::TelemetryInfo::New();
  telemetry_info->cpu_result =
      cros_healthd::CpuResult::NewCpuInfo(cros_healthd::CpuInfo::New(
          /*num_total_threads=*/0,
          /*architecture=*/cros_healthd::CpuArchitectureEnum::kX86_64,
          /*physical_cpus=*/std::vector<cros_healthd::PhysicalCpuInfoPtr>(),
          /*temperature_channels=*/
          std::vector<cros_healthd::CpuTemperatureChannelPtr>(),
          /*keylocker_info=*/std::move(keylocker_info)));

  return telemetry_info;
}

cros_healthd::TelemetryInfoPtr CreateBusResult(
    cros_healthd::ThunderboltSecurityLevel security_level) {
  auto telemetry_info = cros_healthd::TelemetryInfo::New();
  std::vector<cros_healthd::BusDevicePtr> bus_devices;

  auto tbt_device = cros_healthd::BusDevice::New();
  // Subtract one from security level to offset the unspecified index.
  tbt_device->bus_info = cros_healthd::BusInfo::NewThunderboltBusInfo(
      cros_healthd::ThunderboltBusInfo::New(
          security_level,
          std::vector<cros_healthd::ThunderboltBusInterfaceInfoPtr>()));
  bus_devices.push_back(std::move(tbt_device));

  telemetry_info->bus_result =
      cros_healthd::BusResult::NewBusDevices(std::move(bus_devices));
  return telemetry_info;
}

cros_healthd::AudioInfoPtr CreateAudioInfo(
    bool output_mute,
    bool input_mute,
    uint64_t output_volume,
    const std::string& output_device_name,
    uint32_t input_gain,
    const std::string& input_device_name,
    uint32_t underruns,
    uint32_t severe_underruns) {
  return cros_healthd::AudioInfo::New(
      output_mute, input_mute, output_volume, output_device_name, input_gain,
      input_device_name, underruns, severe_underruns);
}

cros_healthd::TelemetryInfoPtr CreateAudioResult(
    cros_healthd::AudioInfoPtr audio_info) {
  auto telemetry_info = cros_healthd::TelemetryInfo::New();
  telemetry_info->audio_result =
      cros_healthd::AudioResult::NewAudioInfo(std::move(audio_info));
  return telemetry_info;
}

MetricData CollectData(cros_healthd::TelemetryInfoPtr telemetry_info,
                       cros_healthd::ProbeCategoryEnum probe_category,
                       CrosHealthdMetricSampler::MetricType metric_type) {
  MetricData data;
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetProbeTelemetryInfoResponseForTesting(telemetry_info);
  CrosHealthdMetricSampler sampler(probe_category, metric_type);
  test::TestEvent<MetricData> metric_collect_event;

  sampler.Collect(metric_collect_event.cb());
  return metric_collect_event.result();
}

MetricData CollectError(cros_healthd::TelemetryInfoPtr telemetry_info,
                        cros_healthd::ProbeCategoryEnum probe_category,
                        CrosHealthdMetricSampler::MetricType metric_type) {
  MetricData data;
  base::RunLoop run_loop;
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetProbeTelemetryInfoResponseForTesting(telemetry_info);
  CrosHealthdMetricSampler sampler(probe_category, metric_type);

  sampler.Collect(base::BindLambdaForTesting(
      [&data](MetricData metric_data) { data = std::move(metric_data); }));
  run_loop.RunUntilIdle();

  return data;
}

class CrosHealthdMetricSamplerTest : public testing::Test {
 public:
  CrosHealthdMetricSamplerTest() {
    chromeos::CrosHealthdClient::InitializeFake();
  }

  ~CrosHealthdMetricSamplerTest() override {
    chromeos::CrosHealthdClient::Shutdown();
    chromeos::cros_healthd::ServiceConnection::GetInstance()->FlushForTesting();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

class CrosHealthdMetricSamplerTbtTest
    : public CrosHealthdMetricSamplerTest,
      public testing::WithParamInterface<TbtTestCase> {};

TEST_P(CrosHealthdMetricSamplerTbtTest, TestTbtSecurityLevels) {
  const TbtTestCase& test_case = GetParam();
  MetricData result =
      CollectData(CreateBusResult(test_case.healthd_security_level),
                  cros_healthd::ProbeCategoryEnum::kBus,
                  CrosHealthdMetricSampler::MetricType::kInfo);
  ASSERT_TRUE(result.has_info_data());
  ASSERT_TRUE(result.info_data().has_bus_device_info());
  ASSERT_TRUE(result.info_data().bus_device_info().has_thunderbolt_info());
  EXPECT_EQ(
      result.info_data().bus_device_info().thunderbolt_info().security_level(),
      test_case.reporting_security_level);
}

TEST_F(CrosHealthdMetricSamplerTest, TestKeylockerConfigured) {
  MetricData result = CollectData(CreateCpuResult(CreateKeylockerInfo(true)),
                                  cros_healthd::ProbeCategoryEnum::kCpu,
                                  CrosHealthdMetricSampler::MetricType::kInfo);

  ASSERT_TRUE(result.has_info_data());
  ASSERT_TRUE(result.info_data().has_cpu_info());
  ASSERT_TRUE(result.info_data().cpu_info().has_keylocker_info());
  EXPECT_TRUE(result.info_data().cpu_info().keylocker_info().configured());
  EXPECT_TRUE(result.info_data().cpu_info().keylocker_info().supported());
}

TEST_F(CrosHealthdMetricSamplerTest, TestKeylockerUnconfigured) {
  MetricData result = CollectData(CreateCpuResult(CreateKeylockerInfo(false)),
                                  cros_healthd::ProbeCategoryEnum::kCpu,
                                  CrosHealthdMetricSampler::MetricType::kInfo);

  ASSERT_TRUE(result.has_info_data());
  ASSERT_TRUE(result.info_data().has_cpu_info());
  ASSERT_TRUE(result.info_data().cpu_info().has_keylocker_info());
  EXPECT_FALSE(result.info_data().cpu_info().keylocker_info().configured());
  EXPECT_TRUE(result.info_data().cpu_info().keylocker_info().supported());
}

TEST_F(CrosHealthdMetricSamplerTest, TestKeylockerUnsupported) {
  MetricData result = CollectData(CreateCpuResult(nullptr),
                                  cros_healthd::ProbeCategoryEnum::kCpu,
                                  CrosHealthdMetricSampler::MetricType::kInfo);

  ASSERT_TRUE(result.has_info_data());
  ASSERT_TRUE(result.info_data().has_cpu_info());
  ASSERT_TRUE(result.info_data().cpu_info().has_keylocker_info());
  EXPECT_FALSE(result.info_data().cpu_info().keylocker_info().configured());
  EXPECT_FALSE(result.info_data().cpu_info().keylocker_info().supported());
}

TEST_F(CrosHealthdMetricSamplerTest, TestMojomError) {
  auto telemetry_info = cros_healthd::TelemetryInfo::New();
  telemetry_info->cpu_result =
      cros_healthd::CpuResult::NewError(cros_healthd::ProbeError::New(
          cros_healthd::ErrorType::kFileReadError, ""));
  const auto& cpu_data = CollectError(
      std::move(telemetry_info), cros_healthd::ProbeCategoryEnum::kCpu,
      CrosHealthdMetricSampler::MetricType::kInfo);
  ASSERT_FALSE(cpu_data.has_info_data());

  telemetry_info = cros_healthd::TelemetryInfo::New();
  telemetry_info->bus_result =
      cros_healthd::BusResult::NewError(cros_healthd::ProbeError::New(
          cros_healthd::ErrorType::kFileReadError, ""));
  const auto& bus_data = CollectError(
      std::move(telemetry_info), cros_healthd::ProbeCategoryEnum::kCpu,
      CrosHealthdMetricSampler::MetricType::kInfo);

  ASSERT_FALSE(bus_data.has_info_data());

  telemetry_info = cros_healthd::TelemetryInfo::New();
  telemetry_info->audio_result =
      cros_healthd::AudioResult::NewError(cros_healthd::ProbeError::New(
          cros_healthd::ErrorType::kFileReadError, ""));
  const auto& audio_data = CollectError(
      std::move(telemetry_info), cros_healthd::ProbeCategoryEnum::kAudio,
      CrosHealthdMetricSampler::MetricType::kTelemetry);
  ASSERT_FALSE(audio_data.has_telemetry_data());
}

TEST_F(CrosHealthdMetricSamplerTest, TestAudioNormalTest) {
  MetricData result = CollectData(
      CreateAudioResult(CreateAudioInfo(
          /*output_mute=*/true,
          /*input_mute=*/true, /*output_volume=*/25,
          /*output_device_name=*/"airpods",
          /*input_gain=*/50, /*input_device_name=*/"airpods", /*underruns=*/2,
          /*severe_underruns=*/2)),
      cros_healthd::ProbeCategoryEnum::kAudio,
      CrosHealthdMetricSampler::MetricType::kTelemetry);

  ASSERT_TRUE(result.has_telemetry_data());
  ASSERT_TRUE(result.telemetry_data().has_audio_telemetry());
  ASSERT_TRUE(result.telemetry_data().audio_telemetry().output_mute());
  ASSERT_THAT(result.telemetry_data().audio_telemetry().output_volume(),
              Eq(25));
}

TEST_F(CrosHealthdMetricSamplerTest, TestAudioEmptyTest) {
  MetricData result = CollectData(
      CreateAudioResult(CreateAudioInfo(
          /*output_mute=*/false,
          /*input_mute=*/false, /*output_volume=*/0,
          /*output_device_name=*/"",
          /*input_gain=*/0, /*input_device_name=*/"", /*underruns=*/0,
          /*severe_underruns=*/0)),
      cros_healthd::ProbeCategoryEnum::kAudio,
      CrosHealthdMetricSampler::MetricType::kTelemetry);

  ASSERT_TRUE(result.has_telemetry_data());
  ASSERT_TRUE(result.telemetry_data().has_audio_telemetry());
  ASSERT_FALSE(result.telemetry_data().audio_telemetry().output_mute());
  ASSERT_FALSE(result.telemetry_data().audio_telemetry().input_mute());
  ASSERT_THAT(result.telemetry_data().audio_telemetry().output_volume(), Eq(0));
}

INSTANTIATE_TEST_SUITE_P(
    CrosHealthdMetricSamplerTbtTests,
    CrosHealthdMetricSamplerTbtTest,
    testing::ValuesIn<TbtTestCase>({
        {"TbtSecurityNoneLevel", cros_healthd::ThunderboltSecurityLevel::kNone,
         THUNDERBOLT_SECURITY_NONE_LEVEL},
        {"TbtSecurityUserLevel",
         cros_healthd::ThunderboltSecurityLevel::kUserLevel,
         THUNDERBOLT_SECURITY_USER_LEVEL},
        {"TbtSecuritySecureLevel",
         cros_healthd::ThunderboltSecurityLevel::kSecureLevel,
         THUNDERBOLT_SECURITY_SECURE_LEVEL},
        {"TbtSecurityDpOnlyLevel",
         cros_healthd::ThunderboltSecurityLevel::kDpOnlyLevel,
         THUNDERBOLT_SECURITY_DP_ONLY_LEVEL},
        {"TbtSecurityUsbOnlyLevel",
         cros_healthd::ThunderboltSecurityLevel::kUsbOnlyLevel,
         THUNDERBOLT_SECURITY_USB_ONLY_LEVEL},
        {"TbtSecurityNoPcieLevel",
         cros_healthd::ThunderboltSecurityLevel::kNoPcieLevel,
         THUNDERBOLT_SECURITY_NO_PCIE_LEVEL},
    }),
    [](const testing::TestParamInfo<CrosHealthdMetricSamplerTbtTest::ParamType>&
           info) { return info.param.test_name; });
}  // namespace test
}  // namespace reporting
