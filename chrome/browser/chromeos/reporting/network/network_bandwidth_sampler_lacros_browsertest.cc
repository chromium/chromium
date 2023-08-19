// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/reporting/network/network_bandwidth_sampler.h"

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/reporting/metric_default_utils.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/policy/messaging_layer/util/reporting_server_connector_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/crosapi.mojom-forward.h"
#include "chromeos/dbus/missive/missive_client_test_observer.h"
#include "chromeos/startup/browser_init_params.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/core/common/policy_loader_lacros.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/mock_clock.h"
#include "content/public/test/browser_test.h"
#include "services/network/public/cpp/network_quality_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Property;
using ::testing::StrEq;

namespace reporting {
namespace {

constexpr char kFakeDMToken[] = "fake-dm-token";
constexpr char kFakeProfileClientId[] = "fake-profile-client-id";
constexpr char kAffiliationId[] = "affiliation-id";
constexpr char kDomain[] = "domain.com";
constexpr int64_t kDownloadSpeedKbps = 100000;

void SetupUserDeviceAffiliation() {
  ::enterprise_management::PolicyData profile_policy_data;
  profile_policy_data.add_user_affiliation_ids(kAffiliationId);
  profile_policy_data.set_managed_by(kDomain);
  profile_policy_data.set_device_id(kFakeProfileClientId);
  profile_policy_data.set_request_token(kFakeDMToken);
  ::policy::PolicyLoaderLacros::set_main_user_policy_data_for_testing(
      std::move(profile_policy_data));

  ::crosapi::mojom::BrowserInitParamsPtr init_params =
      ::crosapi::mojom::BrowserInitParams::New();
  init_params->device_properties = crosapi::mojom::DeviceProperties::New();
  init_params->device_properties->device_dm_token = kFakeDMToken;
  init_params->device_properties->device_affiliation_ids = {kAffiliationId};
  ::chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
}

// Assert network bandwidth telemetry data in a record with relevant DM
// token and download speed.
void AssertNetworkBandwidthTelemetryData(Priority priority,
                                         const Record& record,
                                         int64_t download_speed_kbps) {
  EXPECT_THAT(priority, Eq(Priority::MANUAL_BATCH_LACROS));
  ASSERT_TRUE(record.has_destination());
  EXPECT_THAT(record.destination(), Eq(Destination::TELEMETRY_METRIC));
  ASSERT_TRUE(record.has_dm_token());
  EXPECT_THAT(record.dm_token(), StrEq(kFakeDMToken));

  MetricData record_data;
  ASSERT_TRUE(record_data.ParseFromString(record.data()));
  EXPECT_TRUE(record_data.has_timestamp_ms());
  ASSERT_TRUE(record.has_source_info());
  EXPECT_THAT(record.source_info(),
              AllOf(Property(&SourceInfo::source, Eq(SourceInfo::LACROS)),
                    Property(&SourceInfo::source_version, Not(IsEmpty()))));
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
// `NetworkBandwidthSampler` in Lacros.
class NetworkBandwidthSamplerBrowserTest : public InProcessBrowserTest {
 protected:
  NetworkBandwidthSamplerBrowserTest() {
    // Initialize the MockClock.
    test::MockClock::Get();
    scoped_feature_list_.InitAndEnableFeature(kEnableNetworkBandwidthReporting);
    ::policy::SetDMTokenForTesting(
        ::policy::DMToken::CreateValidToken(kFakeDMToken));
  }

  void CreatedBrowserMainParts(
      ::content::BrowserMainParts* browser_parts) override {
    SetupUserDeviceAffiliation();
    InProcessBrowserTest::CreatedBrowserMainParts(browser_parts);
  }

  void TearDownInProcessBrowserTestFixture() override {
    ::chromeos::BrowserInitParams::SetInitParamsForTests(nullptr);
    InProcessBrowserTest::TearDownInProcessBrowserTestFixture();
  }

  void UpdateDownloadSpeedKbps(int64_t download_speed_kbps) {
    g_browser_process->network_quality_tracker()
        ->ReportRTTsAndThroughputForTesting(base::Milliseconds(100),
                                            download_speed_kbps);
  }

  void SetDeviceSettingValue(
      crosapi::mojom::DeviceSettings::OptionalBool value) {
    auto* const device_settings_lacros =
        g_browser_process->browser_policy_connector()->device_settings_lacros();
    crosapi::mojom::DeviceSettingsPtr device_settings =
        crosapi::mojom::DeviceSettings::New();
    device_settings->report_device_network_status = value;
    device_settings_lacros->UpdateDeviceSettings(std::move(device_settings));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(NetworkBandwidthSamplerBrowserTest,
                       ReportNetworkBandwidthWhenSettingEnabled) {
  UpdateDownloadSpeedKbps(kDownloadSpeedKbps);
  SetDeviceSettingValue(crosapi::mojom::DeviceSettings::OptionalBool::kTrue);

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
                       DoesNotReportNetworkBandwidthWhenSettingDisabled) {
  SetDeviceSettingValue(crosapi::mojom::DeviceSettings::OptionalBool::kFalse);

  // Force telemetry collection by advancing the timer and verify no data is
  // being enqueued via ERP.
  ::chromeos::MissiveClientTestObserver missive_observer(
      base::BindRepeating(&IsNetworkBandwidthTelemetry));
  test::MockClock::Get().Advance(
      metrics::kDefaultNetworkTelemetryCollectionRate);
  EXPECT_FALSE(missive_observer.HasNewEnqueuedRecords());
}

}  // namespace
}  // namespace reporting
