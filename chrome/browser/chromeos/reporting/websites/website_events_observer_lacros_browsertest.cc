// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/metrics/website_metrics_browser_test_mixin.h"
#include "chrome/browser/chromeos/reporting/metric_reporting_prefs.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/crosapi/mojom/crosapi.mojom-forward.h"
#include "chromeos/dbus/missive/missive_client_test_observer.h"
#include "chromeos/startup/browser_init_params.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/core/common/policy_loader_lacros.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "content/public/browser/browser_main_parts.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Eq;
using ::testing::StrEq;

namespace reporting {
namespace {

constexpr char kAffiliationId[] = "affiliation-id";
constexpr char kDomain[] = "domain.com";
constexpr char kFakeProfileClientId[] = "fake-profile-client-id";
constexpr char kTestDMToken[] = "token";
constexpr char kTestUrl[] = "https://a.example.org/";

void SetupUserDeviceAffiliation() {
  ::enterprise_management::PolicyData profile_policy_data;
  profile_policy_data.add_user_affiliation_ids(kAffiliationId);
  profile_policy_data.set_managed_by(kDomain);
  profile_policy_data.set_device_id(kFakeProfileClientId);
  profile_policy_data.set_request_token(kTestDMToken);
  ::policy::PolicyLoaderLacros::set_main_user_policy_data_for_testing(
      std::move(profile_policy_data));

  ::crosapi::mojom::BrowserInitParamsPtr init_params =
      ::crosapi::mojom::BrowserInitParams::New();
  init_params->device_properties = crosapi::mojom::DeviceProperties::New();
  init_params->device_properties->device_dm_token = kTestDMToken;
  init_params->device_properties->device_affiliation_ids = {kAffiliationId};
  ::chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
}

void AssertRecordData(Priority priority, const Record& record) {
  EXPECT_THAT(priority, Eq(Priority::SLOW_BATCH));
  ASSERT_TRUE(record.has_destination());
  EXPECT_THAT(record.destination(), Eq(Destination::EVENT_METRIC));
  ASSERT_TRUE(record.has_dm_token());
  EXPECT_THAT(record.dm_token(), StrEq(kTestDMToken));
  ASSERT_TRUE(record.has_source_info());
  EXPECT_THAT(record.source_info().source(), Eq(SourceInfo::LACROS));
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

class WebsiteEventsObserverBrowserTest : public MixinBasedInProcessBrowserTest {
 protected:
  WebsiteEventsObserverBrowserTest() {
    ::policy::SetDMTokenForTesting(
        ::policy::DMToken::CreateValidToken(kTestDMToken));
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

  void SetAllowlistedUrls(const std::vector<std::string>& allowlisted_urls) {
    base::Value::List allowed_urls;
    for (const auto& url : allowlisted_urls) {
      allowed_urls.Append(url);
    }
    profile()->GetPrefs()->SetList(kReportWebsiteActivityAllowlist,
                                   std::move(allowed_urls));
  }

  Profile* profile() { return ProfileManager::GetPrimaryUserProfile(); }

  ::apps::WebsiteMetricsBrowserTestMixin website_metrics_browser_test_mixin_{
      &mixin_host_};
};

IN_PROC_BROWSER_TEST_F(WebsiteEventsObserverBrowserTest, ReportAllUrlOpened) {
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
                       ReportFilteredUrlOpened) {
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
  EXPECT_FALSE(missive_observer.HasNewEnqueuedRecords());
}

IN_PROC_BROWSER_TEST_F(WebsiteEventsObserverBrowserTest, DisallowAllUrlOpened) {
  SetAllowlistedUrls({});

  // Simulate URL open.
  auto* const browser = website_metrics_browser_test_mixin_.CreateBrowser();
  ::chromeos::MissiveClientTestObserver missive_observer(
      base::BindRepeating(&IsMetricEventOfType, MetricEventType::URL_OPENED));
  website_metrics_browser_test_mixin_.InsertForegroundTab(browser, kTestUrl);

  // Verify data is not enqueued.
  EXPECT_FALSE(missive_observer.HasNewEnqueuedRecords());
}

IN_PROC_BROWSER_TEST_F(WebsiteEventsObserverBrowserTest, ReportAllUrlClosed) {
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
                       ReportFilteredUrlClosed) {
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
  EXPECT_FALSE(missive_observer.HasNewEnqueuedRecords());
}

IN_PROC_BROWSER_TEST_F(WebsiteEventsObserverBrowserTest, DisallowAllUrlClosed) {
  SetAllowlistedUrls({});

  // Simulate URL close.
  auto* const browser = website_metrics_browser_test_mixin_.CreateBrowser();
  website_metrics_browser_test_mixin_.InsertForegroundTab(browser, kTestUrl);
  ::chromeos::MissiveClientTestObserver missive_observer(
      base::BindRepeating(&IsMetricEventOfType, MetricEventType::URL_CLOSED));
  browser->tab_strip_model()->CloseAllTabs();

  // Verify data is not enqueued.
  EXPECT_FALSE(missive_observer.HasNewEnqueuedRecords());
}

}  // namespace
}  // namespace reporting
