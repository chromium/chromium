// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_info_metric_sampler_test_utils.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_metric_sampler.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/browser/chromeos/reporting/metric_default_utils.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "chromeos/dbus/missive/missive_client_test_observer.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "content/public/test/browser_test.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash::reporting {

// Helper for setting up CrosHealthdInfoMetrics tests.
class CrosHealthdInfoMetricsHelper {
 public:
  CrosHealthdInfoMetricsHelper() {
    // Don't allow delay in initialization. We don't use
    // |ScopedMockTimeMessageLoopTaskRunner| here because we are not able to
    // make it work with mojom.
    ::reporting::metrics::InitDelayParam::SetForTesting(base::Seconds(0));
  }
};

namespace {

namespace cros_healthd = ::ash::cros_healthd::mojom;

using ::chromeos::MissiveClientTestObserver;
using ::reporting::Destination;
using ::reporting::MetricData;
using ::reporting::Priority;
using ::reporting::Record;
using ::testing::Eq;

// Is the given record about info metric? If yes, return the underlying
// MetricData object.
absl::optional<MetricData> IsRecordInfo(const Record& record) {
  if (record.destination() != Destination::INFO_METRIC) {
    return absl::nullopt;
  }

  MetricData record_data;
  EXPECT_TRUE(record_data.ParseFromString(record.data()));
  EXPECT_TRUE(record_data.has_info_data());
  return record_data;
}

// Assert info in a record and returns the underlying MetricData object.
MetricData AssertInfo(Priority priority, const Record& record) {
  EXPECT_THAT(priority, Eq(Priority::SLOW_BATCH));
  EXPECT_THAT(record.destination(), Eq(Destination::INFO_METRIC));
  MetricData record_data;
  EXPECT_TRUE(record_data.ParseFromString(record.data()));
  EXPECT_TRUE(record_data.has_timestamp_ms());
  EXPECT_TRUE(record_data.has_info_data());
  return record_data;
}

}  // namespace

// ---- CPU ----

class CpuInfoSamplerBrowserTest : public policy::DevicePolicyCrosBrowserTest {
 public:
  CpuInfoSamplerBrowserTest(const CpuInfoSamplerBrowserTest&) = delete;
  CpuInfoSamplerBrowserTest& operator=(const CpuInfoSamplerBrowserTest&) =
      delete;

 protected:
  CpuInfoSamplerBrowserTest() = default;
  ~CpuInfoSamplerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    policy::DevicePolicyCrosBrowserTest::SetUpOnMainThread();
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        kReportDeviceCpuInfo, true);
  }

  // Is the given record about memory info metric?
  static bool IsRecordCpuInfo(const Record& record) {
    auto record_data = IsRecordInfo(record);
    return record_data.has_value() &&
           record_data.value().info_data().has_cpu_info();
  }

  // Gets next enqueued memory info record. This is useful in excluding
  // other types of records from being examined.
  static std::tuple<Priority, Record> GetNextEnqueuedCpuInfoRecord(
      MissiveClientTestObserver* observer) {
    Priority priority;
    Record record;
    do {
      // If no record is enqueued, this line would time out when the loop
      // is entered for the first time.
      std::tie(priority, record) = observer->GetNextEnqueuedRecord();
    } while (!IsRecordCpuInfo(record));

    return std::make_tuple(priority, record);
  }

 private:
  CrosHealthdInfoMetricsHelper cros_healthd_info_metrics_helper_;
  ScopedTestingCrosSettings scoped_testing_cros_settings_;
};

IN_PROC_BROWSER_TEST_F(CpuInfoSamplerBrowserTest, KeylockerUnsupported) {
  auto cpu_result = ::reporting::test::CreateCpuResult(nullptr);
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(cpu_result);
  MissiveClientTestObserver observer(Destination::INFO_METRIC);
  auto [priority, record] = GetNextEnqueuedCpuInfoRecord(&observer);
  auto info_data = AssertInfo(priority, record).info_data();
  ASSERT_TRUE(info_data.cpu_info().has_keylocker_info());
  EXPECT_FALSE(info_data.cpu_info().keylocker_info().configured());
  EXPECT_FALSE(info_data.cpu_info().keylocker_info().supported());
}

IN_PROC_BROWSER_TEST_F(CpuInfoSamplerBrowserTest, KeylockerConfigured) {
  auto cpu_result = ::reporting::test::CreateCpuResult(
      ::reporting::test::CreateKeylockerInfo(true));
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(cpu_result);
  MissiveClientTestObserver observer(Destination::INFO_METRIC);
  auto [priority, record] = GetNextEnqueuedCpuInfoRecord(&observer);
  auto info_data = AssertInfo(priority, record).info_data();
  ASSERT_TRUE(info_data.cpu_info().has_keylocker_info());
  EXPECT_TRUE(info_data.cpu_info().keylocker_info().configured());
  EXPECT_TRUE(info_data.cpu_info().keylocker_info().supported());
}

// ---- Memory ----

// Memory constants.
static constexpr int64_t kTmeMaxKeys = 2;
static constexpr int64_t kTmeKeysLength = 4;

class MemoryInfoSamplerBrowserTest
    : public policy::DevicePolicyCrosBrowserTest,
      public testing::WithParamInterface<
          ::reporting::test::MemoryInfoTestCase> {
 public:
  MemoryInfoSamplerBrowserTest(const MemoryInfoSamplerBrowserTest&) = delete;
  MemoryInfoSamplerBrowserTest& operator=(const MemoryInfoSamplerBrowserTest&) =
      delete;

 protected:
  MemoryInfoSamplerBrowserTest() = default;
  ~MemoryInfoSamplerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    policy::DevicePolicyCrosBrowserTest::SetUpOnMainThread();
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        kReportDeviceMemoryInfo, true);
  }

  // Is the given record about memory info metric?
  static bool IsRecordMemoryInfo(const Record& record) {
    auto record_data = IsRecordInfo(record);
    return record_data.has_value() &&
           record_data.value().info_data().has_memory_info();
  }

  // Gets next enqueued memory info record. This is useful in excluding
  // other types of records from being examined.
  static std::tuple<Priority, Record> GetNextEnqueuedMemoryInfoRecord(
      MissiveClientTestObserver* observer) {
    Priority priority;
    Record record;
    do {
      // If no record is enqueued, this line would time out when the loop
      // is entered for the first time.
      std::tie(priority, record) = observer->GetNextEnqueuedRecord();
    } while (!IsRecordMemoryInfo(record));

    return std::make_tuple(priority, record);
  }

  static void AssertMemoryInfo(MissiveClientTestObserver* observer) {
    auto [priority, record] = GetNextEnqueuedMemoryInfoRecord(observer);
    MetricData record_data = AssertInfo(priority, record);
    ::reporting::test::AssertMemoryInfo(record_data, GetParam());
  }

 private:
  CrosHealthdInfoMetricsHelper cros_healthd_info_metrics_helper_;
  ScopedTestingCrosSettings scoped_testing_cros_settings_;
};

IN_PROC_BROWSER_TEST_P(MemoryInfoSamplerBrowserTest, ReportMemoryInfo) {
  const auto& test_case = GetParam();
  auto memory_result = ::reporting::test::CreateMemoryResult(
      ::reporting::test::CreateMemoryEncryptionInfo(
          test_case.healthd_encryption_state, test_case.max_keys,
          test_case.key_length, test_case.healthd_encryption_algorithm));

  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(memory_result);
  MissiveClientTestObserver observer(Destination::INFO_METRIC);
  AssertMemoryInfo(&observer);
}

INSTANTIATE_TEST_SUITE_P(
    MemoryInfoSamplerBrowserTests,
    MemoryInfoSamplerBrowserTest,
    testing::ValuesIn<::reporting::test::MemoryInfoTestCase>({
        {"UnknownEncryptionState", cros_healthd::EncryptionState::kUnknown,
         ::reporting::MEMORY_ENCRYPTION_STATE_UNKNOWN,
         cros_healthd::CryptoAlgorithm::kUnknown,
         ::reporting::MEMORY_ENCRYPTION_ALGORITHM_UNKNOWN, 0, 0},
        {"KeyValuesSet", cros_healthd::EncryptionState::kUnknown,
         ::reporting::MEMORY_ENCRYPTION_STATE_UNKNOWN,
         cros_healthd::CryptoAlgorithm::kUnknown,
         ::reporting::MEMORY_ENCRYPTION_ALGORITHM_UNKNOWN, kTmeMaxKeys,
         kTmeKeysLength},
    }),
    [](const testing::TestParamInfo<MemoryInfoSamplerBrowserTest::ParamType>&
           info) { return info.param.test_name; });

}  // namespace ash::reporting
