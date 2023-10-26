// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/metrics/website_metrics_browser_test_mixin.h"
#include "chrome/browser/chromeos/reporting/metric_default_utils.h"
#include "chrome/browser/chromeos/reporting/metric_reporting_prefs.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/crosapi/mojom/crosapi.mojom-forward.h"
#include "chromeos/dbus/missive/missive_client_test_observer.h"
#include "chromeos/startup/browser_init_params.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/core/common/policy_loader_lacros.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/mock_clock.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Ge;
using ::testing::Le;
using ::testing::SizeIs;
using ::testing::StrEq;

namespace reporting {
namespace {

using std::literals::string_view_literals::operator""sv;

constexpr auto kFakeProfileClientId = "fake-profile-client-id"sv;
constexpr auto kAffiliationId = "affiliation-id"sv;
constexpr auto kDomain = "domain.com"sv;
constexpr auto kTestDMToken = "token"sv;
constexpr auto kTestUrl = "https://a.example.org/"sv;
constexpr base::TimeDelta kWebsiteUsageCollectionInterval = base::Minutes(5);
constexpr base::TimeDelta kWebsiteUsageDuration = base::Minutes(2);

// Additional website usage buffer period before the browser is actually closed.
// Used when validating reported website usage data.
constexpr base::TimeDelta kWebsiteUsageBufferPeriod = base::Seconds(5);

void SetupUserDeviceAffiliation() {
  ::enterprise_management::PolicyData profile_policy_data;
  profile_policy_data.add_user_affiliation_ids(std::string{kAffiliationId});
  profile_policy_data.set_managed_by(std::string{kDomain});
  profile_policy_data.set_device_id(std::string{kFakeProfileClientId});
  profile_policy_data.set_request_token(std::string{kTestDMToken});
  ::policy::PolicyLoaderLacros::set_main_user_policy_data_for_testing(
      std::move(profile_policy_data));

  ::crosapi::mojom::BrowserInitParamsPtr init_params =
      ::crosapi::mojom::BrowserInitParams::New();
  init_params->device_properties = crosapi::mojom::DeviceProperties::New();
  init_params->device_properties->device_dm_token = kTestDMToken;
  init_params->device_properties->device_affiliation_ids = {
      std::string{kAffiliationId}};
  ::chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
}

void AssertRecordData(Priority priority, const Record& record) {
  EXPECT_THAT(priority, Eq(Priority::MANUAL_BATCH_LACROS));
  ASSERT_TRUE(record.has_destination());
  EXPECT_THAT(record.destination(), Eq(Destination::TELEMETRY_METRIC));
  ASSERT_TRUE(record.has_dm_token());
  EXPECT_THAT(record.dm_token(), StrEq(kTestDMToken));
  ASSERT_TRUE(record.has_source_info());
  EXPECT_THAT(record.source_info().source(), Eq(SourceInfo::LACROS));
}

// Returns true if the record includes website usage telemetry. False otherwise.
bool IsWebsiteUsageTelemetry(const Record& record) {
  MetricData record_data;
  return record_data.ParseFromString(record.data()) &&
         record_data.has_telemetry_data() &&
         record_data.telemetry_data().has_website_telemetry() &&
         record_data.telemetry_data()
             .website_telemetry()
             .has_website_usage_data();
}

// Browser test that validates website usage telemetry reported by the
// `WebsiteUsageTelemetrySampler` in Lacros.
class WebsiteUsageTelemetrySamplerBrowserTest
    : public MixinBasedInProcessBrowserTest {
 protected:
  WebsiteUsageTelemetrySamplerBrowserTest() {
    // Initialize the MockClock.
    test::MockClock::Get();
    ::policy::SetDMTokenForTesting(
        ::policy::DMToken::CreateValidToken(std::string{kTestDMToken}));
  }

  void CreatedBrowserMainParts(
      ::content::BrowserMainParts* browser_parts) override {
    SetupUserDeviceAffiliation();
    MixinBasedInProcessBrowserTest::CreatedBrowserMainParts(browser_parts);
  }

  void TearDownInProcessBrowserTestFixture() override {
    ::chromeos::BrowserInitParams::SetInitParamsForTests(nullptr);
    MixinBasedInProcessBrowserTest::TearDownInProcessBrowserTestFixture();
  }

  // Simulates website usage with the test URL for the specified duration.
  void SimulateWebsiteUsage(base::TimeDelta running_time) {
    // Create a new browser instance and open URL in a new tab.
    auto* const browser = website_metrics_browser_test_mixin_.CreateBrowser();
    website_metrics_browser_test_mixin_.InsertForegroundTab(
        browser, std::string{kTestUrl});

    // Advance timer to simulate usage and close tab to prevent further usage
    // tracking.
    test::MockClock::Get().Advance(running_time);
    browser->tab_strip_model()->CloseAllTabs();

    // Advance timer to ensure app service reports tracked usage to the
    // website usage observer.
    test::MockClock::Get().Advance(kWebsiteUsageCollectionInterval);
    ::content::RunAllTasksUntilIdle();
  }

  void VerifyReportedMetricData(const MetricData& metric_data,
                                base::TimeDelta running_time) {
    EXPECT_TRUE(metric_data.has_timestamp_ms());
    const auto& website_usage_data =
        metric_data.telemetry_data().website_telemetry().website_usage_data();
    ASSERT_THAT(website_usage_data.website_usage(), SizeIs(1));
    const auto& website_usage = website_usage_data.website_usage(0);
    EXPECT_THAT(website_usage.url(), StrEq(kTestUrl));

    // There is some minor usage (usually in milliseconds) as we attempt to
    // close the tab/browser and before it is actually closed, so we account for
    // that below as we validate reported website usage.
    const auto& max_expected_usage = running_time + kWebsiteUsageBufferPeriod;
    EXPECT_THAT(website_usage.running_time_ms(),
                AllOf(Ge(running_time.InMilliseconds()),
                      Le(max_expected_usage.InMilliseconds())));
  }

  void SetAllowlistedUrls(const std::vector<std::string>& allowlisted_urls) {
    base::Value::List allowed_urls;
    for (const auto& url : allowlisted_urls) {
      allowed_urls.Append(url);
    }
    profile()->GetPrefs()->SetList(kReportWebsiteTelemetryAllowlist,
                                   std::move(allowed_urls));
  }

  void SetAllowlistedTelemetryTypes(
      const std::vector<std::string>& allowlisted_telemetry_types) {
    base::Value::List allowed_telemetry_types;
    for (const auto& url : allowlisted_telemetry_types) {
      allowed_telemetry_types.Append(url);
    }
    profile()->GetPrefs()->SetList(kReportWebsiteTelemetry,
                                   std::move(allowed_telemetry_types));
  }

  Profile* profile() const { return ProfileManager::GetPrimaryUserProfile(); }

  ::apps::WebsiteMetricsBrowserTestMixin website_metrics_browser_test_mixin_{
      &mixin_host_};
};

IN_PROC_BROWSER_TEST_F(WebsiteUsageTelemetrySamplerBrowserTest,
                       ReportAllUrlUsage) {
  SetAllowlistedUrls({ContentSettingsPattern::Wildcard().ToString()});
  SetAllowlistedTelemetryTypes({kWebsiteTelemetryUsageType});

  // Simulate website usage and advance timer by the collection interval to
  // ensure telemetry data is enqueued.
  SimulateWebsiteUsage(kWebsiteUsageDuration);
  ::chromeos::MissiveClientTestObserver missive_observer(
      base::BindRepeating(&IsWebsiteUsageTelemetry));
  test::MockClock::Get().Advance(
      metrics::kDefaultWebsiteTelemetryCollectionRate);

  // Verify data being collected.
  const auto [priority, record] = missive_observer.GetNextEnqueuedRecord();
  AssertRecordData(priority, record);
  MetricData metric_data;
  ASSERT_TRUE(metric_data.ParseFromString(record.data()));
  VerifyReportedMetricData(metric_data, kWebsiteUsageDuration);

  // Ensure there is no additional usage being reported with the next periodic
  // collection.
  test::MockClock::Get().Advance(
      metrics::kDefaultWebsiteTelemetryCollectionRate);
  EXPECT_FALSE(missive_observer.HasNewEnqueuedRecords());
}

IN_PROC_BROWSER_TEST_F(WebsiteUsageTelemetrySamplerBrowserTest,
                       DisallowedUrlUsage) {
  SetAllowlistedUrls({});
  SetAllowlistedTelemetryTypes({kWebsiteTelemetryUsageType});

  // Simulate website usage and advance timer by the collection interval.
  SimulateWebsiteUsage(kWebsiteUsageDuration);
  ::chromeos::MissiveClientTestObserver missive_observer(
      base::BindRepeating(&IsWebsiteUsageTelemetry));
  test::MockClock::Get().Advance(
      metrics::kDefaultWebsiteTelemetryCollectionRate);

  // Verify no telemetry data is enqueued.
  EXPECT_FALSE(missive_observer.HasNewEnqueuedRecords());
}

IN_PROC_BROWSER_TEST_F(WebsiteUsageTelemetrySamplerBrowserTest,
                       DisallowedUsageTelemetryType) {
  SetAllowlistedUrls({ContentSettingsPattern::Wildcard().ToString()});
  SetAllowlistedTelemetryTypes({});

  // Simulate website usage and advance timer by the collection interval.
  SimulateWebsiteUsage(kWebsiteUsageDuration);
  ::chromeos::MissiveClientTestObserver missive_observer(
      base::BindRepeating(&IsWebsiteUsageTelemetry));
  test::MockClock::Get().Advance(
      metrics::kDefaultWebsiteTelemetryCollectionRate);

  // Verify no telemetry data is enqueued.
  EXPECT_FALSE(missive_observer.HasNewEnqueuedRecords());
}

IN_PROC_BROWSER_TEST_F(WebsiteUsageTelemetrySamplerBrowserTest,
                       ReportSubsequentUsage) {
  SetAllowlistedUrls({ContentSettingsPattern::Wildcard().ToString()});
  SetAllowlistedTelemetryTypes({kWebsiteTelemetryUsageType});

  // Simulate website usage.
  SimulateWebsiteUsage(kWebsiteUsageDuration);
  SimulateWebsiteUsage(kWebsiteUsageDuration);

  // Advance timer by the collection interval to ensure telemetry data is
  // enqueued.
  ::chromeos::MissiveClientTestObserver missive_observer(
      base::BindRepeating(&IsWebsiteUsageTelemetry));
  test::MockClock::Get().Advance(
      metrics::kDefaultWebsiteTelemetryCollectionRate);

  // Verify data being collected.
  const auto [priority, record] = missive_observer.GetNextEnqueuedRecord();
  AssertRecordData(priority, record);
  MetricData metric_data;
  ASSERT_TRUE(metric_data.ParseFromString(record.data()));
  VerifyReportedMetricData(metric_data,
                           kWebsiteUsageDuration + kWebsiteUsageDuration);

  // Ensure there is no additional usage being reported with the next periodic
  // collection.
  test::MockClock::Get().Advance(
      metrics::kDefaultWebsiteTelemetryCollectionRate);
  EXPECT_FALSE(missive_observer.HasNewEnqueuedRecords());
}

}  // namespace
}  // namespace reporting
