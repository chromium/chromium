// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/reporting/websites/website_usage_observer.h"

#include <memory>
#include <vector>

#include "base/json/values_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/metrics/website_metrics.h"
#include "chrome/browser/chromeos/reporting/metric_reporting_prefs.h"
#include "chrome/browser/chromeos/reporting/websites/website_metrics_retriever_interface.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/reporting/metrics/fakes/fake_reporting_settings.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::NotNull;
using ::testing::SizeIs;

namespace reporting {
namespace {

constexpr char kTestUserId[] = "TestUser";
constexpr char kTestUrl[] = "https://a.example.org/";

// Mock retriever for the `WebsiteMetrics` component.
class MockWebsiteMetricsRetriever : public WebsiteMetricsRetrieverInterface {
 public:
  MockWebsiteMetricsRetriever() = default;
  MockWebsiteMetricsRetriever(const MockWebsiteMetricsRetriever&) = delete;
  MockWebsiteMetricsRetriever& operator=(const MockWebsiteMetricsRetriever&) =
      delete;
  ~MockWebsiteMetricsRetriever() override = default;

  MOCK_METHOD(void,
              GetWebsiteMetrics,
              (WebsiteMetricsCallback callback),
              (override));
};

class WebsiteUsageObserverTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile(kTestUserId);
    website_metrics_ = std::make_unique<::apps::WebsiteMetrics>(
        profile_, /*user_type_by_device_type=*/0);
    auto mock_website_metrics_retriever =
        std::make_unique<MockWebsiteMetricsRetriever>();
    EXPECT_CALL(*mock_website_metrics_retriever, GetWebsiteMetrics(_))
        .WillOnce(
            [this](WebsiteMetricsRetrieverInterface::WebsiteMetricsCallback
                       callback) {
              std::move(callback).Run(website_metrics_.get());
            });

    website_usage_observer_ = std::make_unique<WebsiteUsageObserver>(
        profile_->GetWeakPtr(), &reporting_settings_,
        std::move(mock_website_metrics_retriever));
  }

  void SetAllowlistedUrls(const std::vector<std::string>& allowlisted_urls) {
    base::Value::List allowed_urls;
    for (const auto& url : allowlisted_urls) {
      allowed_urls.Append(url);
    }
    reporting_settings_.SetList(kReportWebsiteTelemetryAllowlist,
                                std::move(allowed_urls));
  }

  void SetAllowlistedTelemetryTypes(
      const std::vector<std::string>& allowlisted_telemetry_types) {
    base::Value::List allowed_telemetry_types;
    for (const auto& telemetry_type : allowlisted_telemetry_types) {
      allowed_telemetry_types.Append(telemetry_type);
    }
    reporting_settings_.SetList(kReportWebsiteTelemetry,
                                std::move(allowed_telemetry_types));
  }

  void AssertWebsiteUsageDataInPrefStore(const std::string& url,
                                         const base::TimeDelta& running_time) {
    const auto& usage_dict_pref = profile_->GetPrefs()->GetDict(kWebsiteUsage);
    ASSERT_THAT(usage_dict_pref.Find(url), NotNull());
    EXPECT_THAT(base::ValueToTimeDelta(usage_dict_pref.Find(url)),
                Eq(running_time));
  }

  ::content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  raw_ptr<::content::WebContents> web_contents_;

  test::FakeReportingSettings reporting_settings_;
  raw_ptr<Profile> profile_;
  std::unique_ptr<::apps::WebsiteMetrics> website_metrics_;
  std::unique_ptr<WebsiteUsageObserver> website_usage_observer_;
};

TEST_F(WebsiteUsageObserverTest, AllowAllUrlsAndTelemetryTypes) {
  SetAllowlistedUrls({ContentSettingsPattern::Wildcard().ToString()});
  SetAllowlistedTelemetryTypes({kWebsiteTelemetryUsageType});

  // Simulate website usage by calling observer callback directly. We will
  // generate real events in browser tests.
  static constexpr base::TimeDelta kRunningTime = base::Minutes(2);
  ASSERT_TRUE(profile_->GetPrefs()->GetDict(kWebsiteUsage).empty());
  website_usage_observer_->OnUrlUsage(GURL(kTestUrl), kRunningTime);

  ASSERT_THAT(profile_->GetPrefs()->GetDict(kWebsiteUsage), SizeIs(1UL));
  AssertWebsiteUsageDataInPrefStore(kTestUrl, kRunningTime);
}

TEST_F(WebsiteUsageObserverTest, AllowlistedUrlsUnset) {
  SetAllowlistedTelemetryTypes({kWebsiteTelemetryUsageType});

  // Simulate website usage by calling observer callback directly. We will
  // generate real events in browser tests.
  static constexpr base::TimeDelta kRunningTime = base::Minutes(2);
  ASSERT_TRUE(profile_->GetPrefs()->GetDict(kWebsiteUsage).empty());
  website_usage_observer_->OnUrlUsage(GURL(kTestUrl), kRunningTime);

  EXPECT_TRUE(profile_->GetPrefs()->GetDict(kWebsiteUsage).empty());
}

TEST_F(WebsiteUsageObserverTest, DisallowedUrl) {
  SetAllowlistedUrls({});
  SetAllowlistedTelemetryTypes({kWebsiteTelemetryUsageType});

  // Simulate website usage by calling observer callback directly. We will
  // generate real events in browser tests.
  static constexpr base::TimeDelta kRunningTime = base::Minutes(2);
  ASSERT_TRUE(profile_->GetPrefs()->GetDict(kWebsiteUsage).empty());
  website_usage_observer_->OnUrlUsage(GURL(kTestUrl), kRunningTime);

  EXPECT_TRUE(profile_->GetPrefs()->GetDict(kWebsiteUsage).empty());
}

TEST_F(WebsiteUsageObserverTest, AllowedTelemetryTypesUnset) {
  SetAllowlistedUrls({ContentSettingsPattern::Wildcard().ToString()});

  // Simulate website usage by calling observer callback directly. We will
  // generate real events in browser tests.
  static constexpr base::TimeDelta kRunningTime = base::Minutes(2);
  ASSERT_TRUE(profile_->GetPrefs()->GetDict(kWebsiteUsage).empty());
  website_usage_observer_->OnUrlUsage(GURL(kTestUrl), kRunningTime);

  EXPECT_TRUE(profile_->GetPrefs()->GetDict(kWebsiteUsage).empty());
}

TEST_F(WebsiteUsageObserverTest, DisallowedUsageTelemetryType) {
  SetAllowlistedUrls({ContentSettingsPattern::Wildcard().ToString()});
  SetAllowlistedTelemetryTypes({});

  // Simulate website usage by calling observer callback directly. We will
  // generate real events in browser tests.
  static constexpr base::TimeDelta kRunningTime = base::Minutes(2);
  ASSERT_TRUE(profile_->GetPrefs()->GetDict(kWebsiteUsage).empty());
  website_usage_observer_->OnUrlUsage(GURL(kTestUrl), kRunningTime);

  EXPECT_TRUE(profile_->GetPrefs()->GetDict(kWebsiteUsage).empty());
}

TEST_F(WebsiteUsageObserverTest, MicrosecondUsageData) {
  SetAllowlistedUrls({ContentSettingsPattern::Wildcard().ToString()});
  SetAllowlistedTelemetryTypes({kWebsiteTelemetryUsageType});

  // Simulate website usage by calling observer callback directly. We will
  // generate real events in browser tests.
  static constexpr base::TimeDelta kRunningTime = base::Microseconds(200);
  ASSERT_TRUE(profile_->GetPrefs()->GetDict(kWebsiteUsage).empty());
  website_usage_observer_->OnUrlUsage(GURL(kTestUrl), kRunningTime);

  EXPECT_TRUE(profile_->GetPrefs()->GetDict(kWebsiteUsage).empty());
}

TEST_F(WebsiteUsageObserverTest, SubsequentUsageData) {
  SetAllowlistedUrls({ContentSettingsPattern::Wildcard().ToString()});
  SetAllowlistedTelemetryTypes({kWebsiteTelemetryUsageType});

  // Simulate website usage by calling observer callback directly. We will
  // generate real events in browser tests.
  static constexpr base::TimeDelta kRunningTime = base::Minutes(2);
  ASSERT_TRUE(profile_->GetPrefs()->GetDict(kWebsiteUsage).empty());
  website_usage_observer_->OnUrlUsage(GURL(kTestUrl), kRunningTime);
  ASSERT_THAT(profile_->GetPrefs()->GetDict(kWebsiteUsage), SizeIs(1UL));
  AssertWebsiteUsageDataInPrefStore(kTestUrl, kRunningTime);

  // Simulate further usage and verify this usage data is aggregated with the
  // one in the pref store.
  website_usage_observer_->OnUrlUsage(GURL(kTestUrl), kRunningTime);
  ASSERT_THAT(profile_->GetPrefs()->GetDict(kWebsiteUsage), SizeIs(1UL));
  AssertWebsiteUsageDataInPrefStore(kTestUrl, kRunningTime + kRunningTime);
}

TEST_F(WebsiteUsageObserverTest, SubsequentUsageDataWithDifferentUrl) {
  SetAllowlistedUrls({ContentSettingsPattern::Wildcard().ToString()});
  SetAllowlistedTelemetryTypes({kWebsiteTelemetryUsageType});

  // Simulate website usage by calling observer callback directly. We will
  // generate real events in browser tests.
  static constexpr base::TimeDelta kRunningTime = base::Minutes(2);
  ASSERT_TRUE(profile_->GetPrefs()->GetDict(kWebsiteUsage).empty());
  website_usage_observer_->OnUrlUsage(GURL(kTestUrl), kRunningTime);
  ASSERT_THAT(profile_->GetPrefs()->GetDict(kWebsiteUsage), SizeIs(1UL));
  AssertWebsiteUsageDataInPrefStore(kTestUrl, kRunningTime);

  // Simulate usage for different URL and verify there is a new entry in the
  // pref store.
  static constexpr char kOtherUrl[] = "https://b.example.org/";
  website_usage_observer_->OnUrlUsage(GURL(kOtherUrl), kRunningTime);
  ASSERT_THAT(profile_->GetPrefs()->GetDict(kWebsiteUsage), SizeIs(2UL));
  AssertWebsiteUsageDataInPrefStore(kTestUrl, kRunningTime);
  AssertWebsiteUsageDataInPrefStore(kOtherUrl, kRunningTime);
}

}  // namespace
}  // namespace reporting
