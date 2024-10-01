// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/metrics/website_metrics_browser_test_mixin.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/user_auth_config.h"
#include "chrome/browser/ash/policy/affiliation/affiliation_mixin.h"
#include "chrome/browser/ash/policy/affiliation/affiliation_test_helper.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_test_helper.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/reporting/metric_default_utils.h"
#include "chrome/browser/chromeos/reporting/metric_reporting_prefs.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/login/session/session_termination_manager.h"
#include "chromeos/dbus/missive/missive_client_test_observer.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/prefs/pref_service.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/mock_clock.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/login_manager/dbus-constants.h"

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Ge;
using ::testing::Le;
using ::testing::SizeIs;
using ::testing::StrEq;

namespace reporting {
namespace {

constexpr std::string_view kTestDMToken = "token";
constexpr std::string_view kTestUrl = "https://a.example.org/";
constexpr base::TimeDelta kWebsiteUsageCollectionInterval = base::Minutes(5);
constexpr base::TimeDelta kWebsiteUsageDuration = base::Minutes(2);

// Additional website usage buffer period before the browser is actually closed.
// Used when validating reported website usage data.
constexpr base::TimeDelta kWebsiteUsageBufferPeriod = base::Seconds(10);

void AssertRecordData(Priority priority, const Record& record) {
  EXPECT_THAT(priority, Eq(Priority::MANUAL_BATCH));
  ASSERT_TRUE(record.has_destination());
  EXPECT_THAT(record.destination(), Eq(Destination::TELEMETRY_METRIC));
  ASSERT_TRUE(record.has_dm_token());
  EXPECT_THAT(record.dm_token(), StrEq(kTestDMToken));
  ASSERT_TRUE(record.has_source_info());
  EXPECT_THAT(record.source_info().source(), Eq(SourceInfo::ASH));
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
// `WebsiteUsageTelemetrySampler` in Ash. Inheriting from
// `DevicePolicyCrosBrowserTest` enables use of `AffiliationMixin` for setting
// up profile/device affiliation.
class WebsiteUsageTelemetrySamplerBrowserTest
    : public ::policy::DevicePolicyCrosBrowserTest {
 protected:
  WebsiteUsageTelemetrySamplerBrowserTest() {
    // Initialize the mock clock.
    test::MockClock::Get();
    crypto_home_mixin_.MarkUserAsExisting(affiliation_mixin_.account_id());
    crypto_home_mixin_.ApplyAuthConfig(
        affiliation_mixin_.account_id(),
        ash::test::UserAuthConfig::Create(ash::test::kDefaultAuthSetup));
    ::policy::SetDMTokenForTesting(
        ::policy::DMToken::CreateValidToken(std::string{kTestDMToken}));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ::policy::AffiliationTestHelper::AppendCommandLineSwitchesForLoginManager(
        command_line);
    ::policy::DevicePolicyCrosBrowserTest::SetUpCommandLine(command_line);
  }

  // Simulates website usage with the test URL for the specified duration.
  void SimulateWebsiteUsage(base::TimeDelta running_time) {
    // Create a new browser instance and open test URL in a new tab.
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
    EXPECT_THAT(website_usage.url(), StrEq(std::string{kTestUrl}));

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

  Profile* profile() const {
    return ::ash::ProfileHelper::Get()->GetProfileByAccountId(
        affiliation_mixin_.account_id());
  }

  ::policy::DevicePolicyCrosTestHelper test_helper_;
  ::policy::AffiliationMixin affiliation_mixin_{&mixin_host_, &test_helper_};
  ::ash::CryptohomeMixin crypto_home_mixin_{&mixin_host_};
  ::apps::WebsiteMetricsBrowserTestMixin website_metrics_browser_test_mixin_{
      &mixin_host_};
};

IN_PROC_BROWSER_TEST_F(WebsiteUsageTelemetrySamplerBrowserTest,
                       PRE_ReportUrlUsage) {
  // Set up affiliated user.
  ::policy::AffiliationTestHelper::PreLoginUser(
      affiliation_mixin_.account_id());
}

IN_PROC_BROWSER_TEST_F(WebsiteUsageTelemetrySamplerBrowserTest,
                       ReportUrlUsage) {
  // Login as affiliated user and set policy.
  ::policy::AffiliationTestHelper::LoginUser(affiliation_mixin_.account_id());
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

  // Ensure there is no additional usage data being reported with the next
  // periodic collection.
  test::MockClock::Get().Advance(
      metrics::kDefaultWebsiteTelemetryCollectionRate);
  EXPECT_FALSE(missive_observer.HasNewEnqueuedRecord());
}

IN_PROC_BROWSER_TEST_F(WebsiteUsageTelemetrySamplerBrowserTest,
                       PRE_DisallowedUrlUsage) {
  // Set up affiliated user.
  ::policy::AffiliationTestHelper::PreLoginUser(
      affiliation_mixin_.account_id());
}

IN_PROC_BROWSER_TEST_F(WebsiteUsageTelemetrySamplerBrowserTest,
                       DisallowedUrlUsage) {
  // Login as affiliated user and set policy.
  ::policy::AffiliationTestHelper::LoginUser(affiliation_mixin_.account_id());
  SetAllowlistedUrls({});
  SetAllowlistedTelemetryTypes({kWebsiteTelemetryUsageType});

  // Simulate website usage and advance timer by the collection interval.
  SimulateWebsiteUsage(kWebsiteUsageDuration);
  ::chromeos::MissiveClientTestObserver missive_observer(
      base::BindRepeating(&IsWebsiteUsageTelemetry));
  test::MockClock::Get().Advance(
      metrics::kDefaultWebsiteTelemetryCollectionRate);

  // Verify no telemetry data is enqueued.
  EXPECT_FALSE(missive_observer.HasNewEnqueuedRecord());
}

IN_PROC_BROWSER_TEST_F(WebsiteUsageTelemetrySamplerBrowserTest,
                       PRE_DisallowedUsageTelemetryType) {
  // Set up affiliated user.
  ::policy::AffiliationTestHelper::PreLoginUser(
      affiliation_mixin_.account_id());
}

IN_PROC_BROWSER_TEST_F(WebsiteUsageTelemetrySamplerBrowserTest,
                       DisallowedUsageTelemetryType) {
  // Login as affiliated user and set policy.
  ::policy::AffiliationTestHelper::LoginUser(affiliation_mixin_.account_id());
  SetAllowlistedUrls({ContentSettingsPattern::Wildcard().ToString()});
  SetAllowlistedTelemetryTypes({});

  // Simulate website usage and advance timer by the collection interval.
  SimulateWebsiteUsage(kWebsiteUsageDuration);
  ::chromeos::MissiveClientTestObserver missive_observer(
      base::BindRepeating(&IsWebsiteUsageTelemetry));
  test::MockClock::Get().Advance(
      metrics::kDefaultWebsiteTelemetryCollectionRate);

  // Verify no telemetry data is enqueued.
  EXPECT_FALSE(missive_observer.HasNewEnqueuedRecord());
}

IN_PROC_BROWSER_TEST_F(WebsiteUsageTelemetrySamplerBrowserTest,
                       PRE_ReportSubsequentUsage) {
  // Set up affiliated user.
  ::policy::AffiliationTestHelper::PreLoginUser(
      affiliation_mixin_.account_id());
}

IN_PROC_BROWSER_TEST_F(WebsiteUsageTelemetrySamplerBrowserTest,
                       ReportSubsequentUsage) {
  // Login as affiliated user and set policy.
  ::policy::AffiliationTestHelper::LoginUser(affiliation_mixin_.account_id());
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
  EXPECT_FALSE(missive_observer.HasNewEnqueuedRecord());
}

IN_PROC_BROWSER_TEST_F(WebsiteUsageTelemetrySamplerBrowserTest,
                       PRE_ReportUsageDataOnSessionTermination) {
  // Set up affiliated user.
  ::policy::AffiliationTestHelper::PreLoginUser(
      affiliation_mixin_.account_id());
}

IN_PROC_BROWSER_TEST_F(WebsiteUsageTelemetrySamplerBrowserTest,
                       ReportUsageDataOnSessionTermination) {
  // Login as affiliated user and set policy.
  ::policy::AffiliationTestHelper::LoginUser(affiliation_mixin_.account_id());
  SetAllowlistedUrls({ContentSettingsPattern::Wildcard().ToString()});
  SetAllowlistedTelemetryTypes({kWebsiteTelemetryUsageType});

  // Simulate website usage and terminate session.
  SimulateWebsiteUsage(kWebsiteUsageDuration);
  ::chromeos::MissiveClientTestObserver missive_observer(
      base::BindRepeating(&IsWebsiteUsageTelemetry));
  ::ash::SessionTerminationManager::Get()->StopSession(
      ::login_manager::SessionStopReason::USER_REQUESTS_SIGNOUT);

  // Verify data being collected.
  const auto [priority, record] = missive_observer.GetNextEnqueuedRecord();
  AssertRecordData(priority, record);
  MetricData metric_data;
  ASSERT_TRUE(metric_data.ParseFromString(record.data()));
  VerifyReportedMetricData(metric_data, kWebsiteUsageDuration);
}

}  // namespace
}  // namespace reporting
