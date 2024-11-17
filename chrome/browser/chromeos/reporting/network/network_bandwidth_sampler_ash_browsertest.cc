// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/user_auth_config.h"
#include "chrome/browser/ash/policy/affiliation/affiliation_mixin.h"
#include "chrome/browser/ash/policy/affiliation/affiliation_test_helper.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/reporting/metric_default_utils.h"
#include "chrome/browser/chromeos/reporting/network/network_bandwidth_sampler.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/dbus/missive/missive_client_test_observer.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/mock_clock.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Eq;
using ::testing::StrEq;

namespace reporting {
namespace {

constexpr char kDMToken[] = "token";
constexpr int64_t kDownloadSpeedKbps = 100000;

// Assert network bandwidth telemetry data in a record with relevant DM
// token and download speed.
void AssertNetworkBandwidthTelemetryData(Priority priority,
                                         const Record& record,
                                         int64_t download_speed_kbps) {
  EXPECT_THAT(priority, Eq(Priority::MANUAL_BATCH));
  ASSERT_TRUE(record.has_destination());
  EXPECT_THAT(record.destination(), Eq(Destination::TELEMETRY_METRIC));
  ASSERT_TRUE(record.has_dm_token());
  EXPECT_THAT(record.dm_token(), StrEq(kDMToken));
  ASSERT_TRUE(record.has_source_info());
  EXPECT_THAT(record.source_info().source(), Eq(SourceInfo::ASH));

  MetricData record_data;
  ASSERT_TRUE(record_data.ParseFromString(record.data()));
  EXPECT_TRUE(record_data.has_timestamp_ms());
  EXPECT_THAT(record_data.telemetry_data()
                  .networks_telemetry()
                  .bandwidth_data()
                  .download_speed_kbps(),
              Eq(download_speed_kbps));
}

// Returns true if the record includes network bandwidth telemetry. False
// otherwise.
bool IsNetworkBandwidthTelemetry(const Record& record) {
  MetricData record_data;
  return record_data.ParseFromString(record.data()) &&
         record_data.has_telemetry_data() &&
         record_data.telemetry_data().has_networks_telemetry() &&
         record_data.telemetry_data().networks_telemetry().has_bandwidth_data();
}

// Browser test that validates network bandwidth telemetry reported by the
// `NetworkBandwidthSampler`. Inheriting from `DevicePolicyCrosBrowserTest`
// enables use of `AffiliationMixin` for setting up profile/device affiliation.
// Only available in Ash.
class NetworkBandwidthSamplerBrowserTest
    : public ::policy::DevicePolicyCrosBrowserTest {
 protected:
  NetworkBandwidthSamplerBrowserTest() {
    // Initialize the MockClock.
    test::MockClock::Get();
    scoped_feature_list_.InitAndEnableFeature(kEnableNetworkBandwidthReporting);
    crypto_home_mixin_.MarkUserAsExisting(affiliation_mixin_.account_id());
    crypto_home_mixin_.ApplyAuthConfig(
        affiliation_mixin_.account_id(),
        ash::test::UserAuthConfig::Create(ash::test::kDefaultAuthSetup));
    ::policy::SetDMTokenForTesting(
        ::policy::DMToken::CreateValidToken(kDMToken));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ::policy::AffiliationTestHelper::AppendCommandLineSwitchesForLoginManager(
        command_line);
    ::policy::DevicePolicyCrosBrowserTest::SetUpCommandLine(command_line);
  }

  void UpdateDownloadSpeedKbps(int64_t download_speed_kbps) {
    g_browser_process->network_quality_tracker()
        ->ReportRTTsAndThroughputForTesting(base::Milliseconds(100),
                                            download_speed_kbps);
  }

  void SetDeviceSettingValue(bool value) {
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        ::ash::kReportDeviceNetworkStatus, value);
  }

  ::policy::DevicePolicyCrosTestHelper test_helper_;
  ::policy::AffiliationMixin affiliation_mixin_{&mixin_host_, &test_helper_};
  ::ash::CryptohomeMixin crypto_home_mixin_{&mixin_host_};
  base::test::ScopedFeatureList scoped_feature_list_;
  ::ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;
};

IN_PROC_BROWSER_TEST_F(NetworkBandwidthSamplerBrowserTest,
                       PRE_ReportNetworkBandwidthWhenSettingEnabled) {
  ::policy::AffiliationTestHelper::PreLoginUser(
      affiliation_mixin_.account_id());
}

IN_PROC_BROWSER_TEST_F(NetworkBandwidthSamplerBrowserTest,
                       ReportNetworkBandwidthWhenSettingEnabled) {
  ::policy::AffiliationTestHelper::LoginUser(affiliation_mixin_.account_id());
  UpdateDownloadSpeedKbps(kDownloadSpeedKbps);
  SetDeviceSettingValue(true);

  // Force telemetry collection by advancing the timer and verify data that is
  // being enqueued via ERP.
  ::chromeos::MissiveClientTestObserver missive_observer(
      base::BindRepeating(&IsNetworkBandwidthTelemetry));
  test::MockClock::Get().Advance(
      metrics::kDefaultNetworkTelemetryCollectionRate);
  const auto [priority, record] = missive_observer.GetNextEnqueuedRecord();
  AssertNetworkBandwidthTelemetryData(priority, record, kDownloadSpeedKbps);
}

IN_PROC_BROWSER_TEST_F(NetworkBandwidthSamplerBrowserTest,
                       PRE_DoesNotReportNetworkBandwidthWhenSettingDisabled) {
  ::policy::AffiliationTestHelper::PreLoginUser(
      affiliation_mixin_.account_id());
}

IN_PROC_BROWSER_TEST_F(NetworkBandwidthSamplerBrowserTest,
                       DoesNotReportNetworkBandwidthWhenSettingDisabled) {
  ::policy::AffiliationTestHelper::LoginUser(affiliation_mixin_.account_id());
  SetDeviceSettingValue(false);

  // Force telemetry collection by advancing the timer and verify no data is
  // being enqueued via ERP.
  ::chromeos::MissiveClientTestObserver missive_observer(
      base::BindRepeating(&IsNetworkBandwidthTelemetry));
  test::MockClock::Get().Advance(
      metrics::kDefaultNetworkTelemetryCollectionRate);
  EXPECT_FALSE(missive_observer.HasNewEnqueuedRecord());
}

}  // namespace
}  // namespace reporting
