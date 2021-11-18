// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/cpu/cpu_info_sampler.h"

#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {
namespace test {

void FetchFakeCpuInfo(
    cros_healthd::TelemetryInfoPtr info,
    base::OnceCallback<void(cros_healthd::TelemetryInfoPtr)> healthd_callback) {
  std::move(healthd_callback).Run(std::move(info));
}

cros_healthd::KeylockerInfoPtr CreateKeylockerInfo(bool configured) {
  return cros_healthd::KeylockerInfo::New(configured);
}

cros_healthd::CpuResultPtr CreateCpuResult(
    cros_healthd::KeylockerInfoPtr keylocker_info) {
  return cros_healthd::CpuResult::NewCpuInfo(cros_healthd::CpuInfo::New(
      /*num_total_threads=*/0,
      /*architecture=*/cros_healthd::CpuArchitectureEnum::kX86_64,
      /*physical_cpus=*/std::vector<cros_healthd::PhysicalCpuInfoPtr>(),
      /*temperature_channels=*/
      std::vector<cros_healthd::CpuTemperatureChannelPtr>(),
      /*keylocker_info=*/std::move(keylocker_info)));
}

MetricData CollectData(cros_healthd::CpuResultPtr cpu_result) {
  MetricData data;
  auto telemetry_info = cros_healthd::TelemetryInfo::New();
  telemetry_info->cpu_result = std::move(cpu_result);
  CpuInfoSampler sampler(base::BindRepeating(
      test::FetchFakeCpuInfo, base::Passed(std::move(telemetry_info))));
  sampler.Collect(base::BindLambdaForTesting(
      [&data](MetricData metric_data) { data = std::move(metric_data); }));
  return data;
}
}  // namespace test

TEST(CpuInfoSamplerTest, TestConfigured) {
  MetricData result = test::CollectData(test::CreateCpuResult(
      test::CreateKeylockerInfo(/*keylocker_configured=*/true)));

  ASSERT_TRUE(result.has_info_data());
  ASSERT_TRUE(result.info_data().has_cpu_info());
  ASSERT_TRUE(result.info_data().cpu_info().has_keylocker_info());
  EXPECT_TRUE(result.info_data().cpu_info().keylocker_info().configured());
  EXPECT_TRUE(result.info_data().cpu_info().keylocker_info().supported());
}

TEST(CpuInfoSamplerTest, TestUnconfigured) {
  MetricData result = test::CollectData(test::CreateCpuResult(
      test::CreateKeylockerInfo(/*keylocker_configured=*/false)));

  ASSERT_TRUE(result.has_info_data());
  ASSERT_TRUE(result.info_data().has_cpu_info());
  ASSERT_TRUE(result.info_data().cpu_info().has_keylocker_info());
  EXPECT_FALSE(result.info_data().cpu_info().keylocker_info().configured());
  EXPECT_TRUE(result.info_data().cpu_info().keylocker_info().supported());
}

TEST(CpuInfoSamplerTest, TestKeylockerUnsupported) {
  MetricData result = test::CollectData(test::CreateCpuResult(nullptr));

  ASSERT_TRUE(result.has_info_data());
  ASSERT_TRUE(result.info_data().has_cpu_info());
  ASSERT_TRUE(result.info_data().cpu_info().has_keylocker_info());
  EXPECT_FALSE(result.info_data().cpu_info().keylocker_info().configured());
  EXPECT_FALSE(result.info_data().cpu_info().keylocker_info().supported());
}

TEST(CpuInfoSamplerTest, TestMojomError) {
  MetricData result = test::CollectData(
      cros_healthd::CpuResult::NewError(cros_healthd::ProbeError::New(
          cros_healthd::ErrorType::kFileReadError, "")));
  ASSERT_FALSE(result.has_info_data());
}

TEST(CpuInfoSamplerTest, TestNullCpuResult) {
  MetricData result = test::CollectData(nullptr);
  ASSERT_FALSE(result.has_info_data());
}
}  // namespace reporting
