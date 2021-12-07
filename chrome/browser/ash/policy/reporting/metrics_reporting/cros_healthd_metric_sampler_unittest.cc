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
#include "testing/gtest/include/gtest/gtest.h"

namespace cros_healthd = chromeos::cros_healthd::mojom;

namespace reporting {
namespace {

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
}  // namespace

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

TEST_F(CrosHealthdMetricSamplerTest, TestUnconfigured) {
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
  MetricData data;
  auto telemetry_info = cros_healthd::TelemetryInfo::New();
  telemetry_info->cpu_result =
      cros_healthd::CpuResult::NewError(cros_healthd::ProbeError::New(
          cros_healthd::ErrorType::kFileReadError, ""));
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetProbeTelemetryInfoResponseForTesting(telemetry_info);

  // The metric callback should not be ran in this case so TestEvent cannot
  // be used.
  CrosHealthdMetricSampler sampler(cros_healthd::ProbeCategoryEnum::kCpu,
                                   CrosHealthdMetricSampler::MetricType::kInfo);
  sampler.Collect(base::BindLambdaForTesting(
      [&data](MetricData metric_data) { data = std::move(metric_data); }));
  ASSERT_FALSE(data.has_info_data());
}
}  // namespace reporting
