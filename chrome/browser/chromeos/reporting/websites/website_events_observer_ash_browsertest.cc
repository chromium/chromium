// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "chrome/browser/apps/app_service/metrics/website_metrics_browser_test_mixin.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/user_auth_config.h"
#include "chrome/browser/ash/policy/affiliation/affiliation_mixin.h"
#include "chrome/browser/ash/policy/affiliation/affiliation_test_helper.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_test_helper.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/reporting/metric_reporting_prefs.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/dbus/missive/missive_client_test_observer.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/prefs/pref_service.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Eq;
using ::testing::StrEq;

namespace reporting {
namespace {

constexpr char kTestDMToken[] = "token";
constexpr char kTestUrl[] = "https://a.example.org/";

void AssertRecordData(Priority priority, const Record& record) {
  EXPECT_THAT(priority, Eq(Priority::SLOW_BATCH));
  ASSERT_TRUE(record.has_destination());
  EXPECT_THAT(record.destination(), Eq(Destination::EVENT_METRIC));
  ASSERT_TRUE(record.has_dm_token());
  EXPECT_THAT(record.dm_token(), StrEq(kTestDMToken));
  ASSERT_TRUE(record.has_source_info());
  EXPECT_THAT(record.source_info().source(), Eq(SourceInfo::ASH));
}

void AssertMetricData(const MetricData& metric_data) {
  EXPECT_TRUE(metric_data.has_timestamp_ms());
  EXPECT_TRUE(metric_data.has_event_data());
  ASSERT_TRUE(metric_data.has_telemetry_data());
  EXPECT_TRUE(metric_data.telemetry_data().has_website_telemetry());
}

// Returns true if the record includes the specified metric event type. False
// otherwise.
bool IsMetricEventOfType(MetricEventType metric_event_type,
                         const Record& record) {
  MetricData record_data;
  return record_data.ParseFromString(record.data()) &&
         record_data.has_event_data() &&
         (record_data.event_data().type() == metric_event_type);
}

class WebsiteEventsObserverBrowserTest
    : public ::policy::DevicePolicyCrosBrowserTest {
 protected:
  WebsiteEventsObserverBrowserTest() {
    crypto_home_mixin_.MarkUserAsExisting(affiliation_mixin_.account_id());
    crypto_home_mixin_.ApplyAuthConfig(
        affiliation_mixin_.account_id(),
        ash::test::UserAuthConfig::Create(ash::test::kDefaultAuthSetup));
    ::policy::SetDMTokenForTesting(
        ::policy::DMToken::CreateValidToken(kTestDMToken));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ::policy::AffiliationTestHelper::AppendCommandLineSwitchesForLoginManager(
        command_line);
    ::policy::DevicePolicyCrosBrowserTest::SetUpCommandLine(command_line);
  }

  void SetAllowlistedUrls(const std::vector<std::string>& allowlisted_urls) {
    base::Value::List allowed_urls;
    for (const auto& url : allowlisted_urls) {
      allowed_urls.Append(url);
    }
    profile()->GetPrefs()->SetList(kReportWebsiteActivityAllowlist,
                                   std::move(allowed_urls));
  }

  Profile* profile() {
    return ::ash::ProfileHelper::Get()->GetProfileByAccountId(
        affiliation_mixin_.account_id());
  }

  ::policy::DevicePolicyCrosTestHelper test_helper_;
  ::policy::AffiliationMixin affiliation_mixin_{&mixin_host_, &test_helper_};
  ::ash::CryptohomeMixin crypto_home_mixin_{&mixin_host_};
  ::apps::WebsiteMetricsBrowserTestMixin website_metrics_browser_test_mixin_{
      &mixin_host_};
};

IN_PROC_BROWSER_TEST_F(WebsiteEventsObserverBrowserTest,
                       PRE_ReportAllUrlOpened) {
  // Set up affiliated user.
  ::policy::AffiliationTestHelper::PreLoginUser(
      affiliation_mixin_.account_id());
}

IN_PROC_BROWSER_TEST_F(WebsiteEventsObserverBrowserTest, ReportAllUrlOpened) {
  // Login as affiliated user and set policy.
  ::policy::AffiliationTestHelper::LoginUser(affiliation_mixin_.account_id());
  SetAllowlistedUrls({ContentSettingsPattern::Wildcard().ToString()});

  // Simulate URL open.
  auto* const browser = website_metrics_browser_test_mixin_.CreateBrowser();
  ::chromeos::MissiveClientTestObserver missive_observer(
      base::BindRepeating(&IsMetricEventOfType, MetricEventType::URL_OPENED));
  website_metrics_browser_test_mixin_.InsertForegroundTab(browser, kTestUrl);

  // Verify data being enqueued.
  const auto [priority, record] = missive_observer.GetNextEnqueuedRecord();
  AssertRecordData(priority, record);
  MetricData metric_data;
  ASSERT_TRUE(metric_data.ParseFromString(record.data()));
  AssertMetricData(metric_data);
  ASSERT_TRUE(metric_data.telemetry_data()
                  .website_telemetry()
                  .has_website_opened_data());
  const auto& website_opened_data =
      metric_data.telemetry_data().website_telemetry().website_opened_data();
  EXPECT_THAT(website_opened_data.url(), StrEq(kTestUrl));
  EXPECT_TRUE(website_opened_data.has_render_process_host_id());
  EXPECT_TRUE(website_opened_data.has_render_frame_routing_id());
}

IN_PROC_BROWSER_TEST_F(WebsiteEventsObserverBrowserTest,
                       PRE_ReportFilteredUrlOpened) {
  // Set up affiliated user.
  ::policy::AffiliationTestHelper::PreLoginUser(
      affiliation_mixin_.account_id());
}

IN_PROC_BROWSER_TEST_F(WebsiteEventsObserverBrowserTest,
                       ReportFilteredUrlOpened) {
  // Login as affiliated user and set policy.
  ::policy::AffiliationTestHelper::LoginUser(affiliation_mixin_.account_id());
  SetAllowlistedUrls({kTestUrl});

  // Simulate URL open.
  static constexpr char kDisallowedUrl[] = "https://b.example.org/";
  auto* const browser = website_metrics_browser_test_mixin_.CreateBrowser();
  ::chromeos::MissiveClientTestObserver missive_observer(
      base::BindRepeating(&IsMetricEventOfType, MetricEventType::URL_OPENED));
  website_metrics_browser_test_mixin_.NavigateActiveTab(browser,
                                                        kDisallowedUrl);
  website_metrics_browser_test_mixin_.InsertForegroundTab(browser, kTestUrl);

  // Verify only event with allowlisted URL is being enqueued.
  const auto [priority, record] = missive_observer.GetNextEnqueuedRecord();
  AssertRecordData(priority, record);
  MetricData metric_data;
  ASSERT_TRUE(metric_data.ParseFromString(record.data()));
  AssertMetricData(metric_data);
  ASSERT_TRUE(metric_data.telemetry_data()
                  .website_telemetry()
                  .has_website_opened_data());
  const auto& website_opened_data =
      metric_data.telemetry_data().website_telemetry().website_opened_data();
  EXPECT_THAT(website_opened_data.url(), StrEq(kTestUrl));
  EXPECT_TRUE(website_opened_data.has_render_process_host_id());
  EXPECT_TRUE(website_opened_data.has_render_frame_routing_id());
  EXPECT_FALSE(missive_observer.HasNewEnqueuedRecord());
}

IN_PROC_BROWSER_TEST_F(WebsiteEventsObserverBrowserTest,
                       PRE_DisallowAllUrlOpened) {
  // Set up affiliated user.
  ::policy::AffiliationTestHelper::PreLoginUser(
      affiliation_mixin_.account_id());
}

IN_PROC_BROWSER_TEST_F(WebsiteEventsObserverBrowserTest, DisallowAllUrlOpened) {
  // Login as affiliated user and set policy to disallow all URLs.
  ::policy::AffiliationTestHelper::LoginUser(affiliation_mixin_.account_id());
  SetAllowlistedUrls({});

  // Simulate URL open.
  auto* const browser = website_metrics_browser_test_mixin_.CreateBrowser();
  ::chromeos::MissiveClientTestObserver missive_observer(
      base::BindRepeating(&IsMetricEventOfType, MetricEventType::URL_OPENED));
  website_metrics_browser_test_mixin_.InsertForegroundTab(browser, kTestUrl);

  // Verify data is not enqueued.
  EXPECT_FALSE(missive_observer.HasNewEnqueuedRecord());
}

IN_PROC_BROWSER_TEST_F(WebsiteEventsObserverBrowserTest,
                       PRE_ReportAllUrlClosed) {
  // Set up affiliated user.
  ::policy::AffiliationTestHelper::PreLoginUser(
      affiliation_mixin_.account_id());
}

IN_PROC_BROWSER_TEST_F(WebsiteEventsObserverBrowserTest, ReportAllUrlClosed) {
  // Login as affiliated user and set policy.
  ::policy::AffiliationTestHelper::LoginUser(affiliation_mixin_.account_id());
  SetAllowlistedUrls({ContentSettingsPattern::Wildcard().ToString()});

  // Simulate URL close.
  auto* const browser = website_metrics_browser_test_mixin_.CreateBrowser();
  website_metrics_browser_test_mixin_.NavigateActiveTab(browser, kTestUrl);
  ::chromeos::MissiveClientTestObserver missive_observer(
      base::BindRepeating(&IsMetricEventOfType, MetricEventType::URL_CLOSED));
  browser->tab_strip_model()->CloseAllTabs();

  // Verify data being enqueued.
  const auto [priority, record] = missive_observer.GetNextEnqueuedRecord();
  AssertRecordData(priority, record);
  MetricData metric_data;
  ASSERT_TRUE(metric_data.ParseFromString(record.data()));
  AssertMetricData(metric_data);
  ASSERT_TRUE(metric_data.telemetry_data()
                  .website_telemetry()
                  .has_website_closed_data());
  const auto& website_closed_data =
      metric_data.telemetry_data().website_telemetry().website_closed_data();
  EXPECT_THAT(website_closed_data.url(), StrEq(kTestUrl));
  EXPECT_TRUE(website_closed_data.has_render_process_host_id());
  EXPECT_TRUE(website_closed_data.has_render_frame_routing_id());
}

IN_PROC_BROWSER_TEST_F(WebsiteEventsObserverBrowserTest,
                       PRE_ReportFilteredUrlClosed) {
  // Set up affiliated user.
  ::policy::AffiliationTestHelper::PreLoginUser(
      affiliation_mixin_.account_id());
}

IN_PROC_BROWSER_TEST_F(WebsiteEventsObserverBrowserTest,
                       ReportFilteredUrlClosed) {
  // Login as affiliated user and set policy.
  ::policy::AffiliationTestHelper::LoginUser(affiliation_mixin_.account_id());
  SetAllowlistedUrls({kTestUrl});

  // Simulate URL close.
  static constexpr char kDisallowedUrl[] = "https://b.example.org/";
  auto* const browser = website_metrics_browser_test_mixin_.CreateBrowser();
  website_metrics_browser_test_mixin_.NavigateActiveTab(browser,
                                                        kDisallowedUrl);
  website_metrics_browser_test_mixin_.InsertForegroundTab(browser, kTestUrl);
  ::chromeos::MissiveClientTestObserver missive_observer(
      base::BindRepeating(&IsMetricEventOfType, MetricEventType::URL_CLOSED));
  browser->tab_strip_model()->CloseAllTabs();

  // Verify only event with allowlisted URL is being enqueued.
  const auto [priority, record] = missive_observer.GetNextEnqueuedRecord();
  AssertRecordData(priority, record);
  MetricData metric_data;
  ASSERT_TRUE(metric_data.ParseFromString(record.data()));
  AssertMetricData(metric_data);
  ASSERT_TRUE(metric_data.telemetry_data()
                  .website_telemetry()
                  .has_website_closed_data());
  const auto& website_closed_data =
      metric_data.telemetry_data().website_telemetry().website_closed_data();
  EXPECT_THAT(website_closed_data.url(), StrEq(kTestUrl));
  EXPECT_TRUE(website_closed_data.has_render_process_host_id());
  EXPECT_TRUE(website_closed_data.has_render_frame_routing_id());
  EXPECT_FALSE(missive_observer.HasNewEnqueuedRecord());
}

IN_PROC_BROWSER_TEST_F(WebsiteEventsObserverBrowserTest,
                       PRE_DisallowAllUrlClosed) {
  // Set up affiliated user.
  ::policy::AffiliationTestHelper::PreLoginUser(
      affiliation_mixin_.account_id());
}

IN_PROC_BROWSER_TEST_F(WebsiteEventsObserverBrowserTest, DisallowAllUrlClosed) {
  // Login as affiliated user and set policy to disallow all URLs.
  ::policy::AffiliationTestHelper::LoginUser(affiliation_mixin_.account_id());
  SetAllowlistedUrls({});

  // Simulate URL close.
  auto* const browser = website_metrics_browser_test_mixin_.CreateBrowser();
  website_metrics_browser_test_mixin_.NavigateActiveTab(browser, kTestUrl);
  ::chromeos::MissiveClientTestObserver missive_observer(
      base::BindRepeating(&IsMetricEventOfType, MetricEventType::URL_CLOSED));
  browser->tab_strip_model()->CloseAllTabs();

  // Verify data is not enqueued.
  EXPECT_FALSE(missive_observer.HasNewEnqueuedRecord());
}

}  // namespace
}  // namespace reporting
