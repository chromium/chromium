// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/metric_browsertest_utils.h"
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

class MemoryInfoSamplerBrowserTest
    : public policy::DevicePolicyCrosBrowserTest {
 protected:
  MemoryInfoSamplerBrowserTest() = default;

  ~MemoryInfoSamplerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    policy::DevicePolicyCrosBrowserTest::SetUpOnMainThread();
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        kReportDeviceMemoryInfo, true);
  }

  static bool IsRecordMemoryInfo(const Record& record) {
    if (record.destination() != Destination::INFO_METRIC) {
      return false;
    }

    MetricData record_data;
    EXPECT_TRUE(record_data.ParseFromString(record.data()));
    if (!record_data.has_info_data()) {
      return false;
    }

    return record_data.info_data().has_memory_info();
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
    EXPECT_THAT(priority, Eq(Priority::SLOW_BATCH));
    EXPECT_THAT(record.destination(), Eq(Destination::INFO_METRIC));
    MetricData record_data;
    ASSERT_TRUE(record_data.ParseFromString(record.data()));
    EXPECT_TRUE(record_data.has_timestamp_ms());
    EXPECT_FALSE(record_data.has_telemetry_data());
    ASSERT_TRUE(record_data.has_info_data());

    const auto& info_data = record_data.info_data();
    ASSERT_TRUE(info_data.has_memory_info());
    EXPECT_THAT(info_data.memory_info().tme_info().max_keys(), Eq(0));
  }

  cros_healthd::MemoryEncryptionInfoPtr CreateMemoryEncryptionInfo(
      cros_healthd::EncryptionState encryption_state,
      int64_t max_keys,
      int64_t key_length,
      cros_healthd::CryptoAlgorithm encryption_algorithm) {
    return cros_healthd::MemoryEncryptionInfo::New(
        encryption_state, max_keys, key_length, encryption_algorithm);
  }

  cros_healthd::TelemetryInfoPtr CreateMemoryResult(
      cros_healthd::MemoryEncryptionInfoPtr memory_encryption_info) {
    auto telemetry_info = cros_healthd::TelemetryInfo::New();
    telemetry_info->memory_result =
        cros_healthd::MemoryResult::NewMemoryInfo(cros_healthd::MemoryInfo::New(
            /*total_memory=*/0, /*free_memory=*/0, /*available_memory=*/0,
            /*page_faults_since_last_boot=*/0,
            std::move(memory_encryption_info)));
    return telemetry_info;
  }

 private:
  CrosHealthdInfoMetricsHelper cros_healthd_info_metrics_helper_;
  ScopedTestingCrosSettings scoped_testing_cros_settings_;
};

IN_PROC_BROWSER_TEST_F(MemoryInfoSamplerBrowserTest, ReportMemoryInfo) {
  auto memory_result = CreateMemoryResult(
      CreateMemoryEncryptionInfo(cros_healthd::EncryptionState::kUnknown, 0, 0,
                                 cros_healthd::CryptoAlgorithm::kUnknown));

  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(memory_result);
  MissiveClientTestObserver observer(Destination::INFO_METRIC);
  AssertMemoryInfo(&observer);
}

}  // namespace

}  // namespace ash::reporting
