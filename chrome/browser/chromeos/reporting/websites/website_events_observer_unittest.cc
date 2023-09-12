// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/reporting/websites/website_events_observer.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/metrics/website_metrics.h"
#include "chrome/browser/chromeos/reporting/metric_reporting_prefs.h"
#include "chrome/browser/chromeos/reporting/websites/website_metrics_retriever_interface.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/reporting/metrics/fakes/fake_reporting_settings.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::StrEq;

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

class WebsiteEventsObserverTest : public ::testing::Test {
 protected:
  WebsiteEventsObserverTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    auto* const profile = profile_manager_.CreateTestingProfile(kTestUserId);
    website_metrics_ = std::make_unique<::apps::WebsiteMetrics>(
        profile, /*user_type_by_device_type=*/0);
    auto mock_website_metrics_retriever =
        std::make_unique<MockWebsiteMetricsRetriever>();
    EXPECT_CALL(*mock_website_metrics_retriever, GetWebsiteMetrics(_))
        .WillOnce(
            [this](WebsiteMetricsRetrieverInterface::WebsiteMetricsCallback
                       callback) {
              std::move(callback).Run(website_metrics_.get());
            });

    website_events_observer_ = std::make_unique<WebsiteEventsObserver>(
        std::move(mock_website_metrics_retriever), &reporting_settings_);
    web_contents_ = test_web_contents_factory_.CreateWebContents(profile);
  }

  void TearDown() override {
    website_metrics_.reset();
    profile_manager_.DeleteAllTestingProfiles();
  }

  void SetAllowlistedUrls(const std::vector<std::string>& allowlisted_urls) {
    base::Value::List allowed_urls;
    for (const auto& url : allowlisted_urls) {
      allowed_urls.Append(url);
    }
    reporting_settings_.SetList(kReportWebsiteActivityAllowlist,
                                std::move(allowed_urls));
  }

  ::content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  ::content::TestWebContentsFactory test_web_contents_factory_;
  raw_ptr<::content::WebContents> web_contents_;

  test::FakeReportingSettings reporting_settings_;
  std::unique_ptr<::apps::WebsiteMetrics> website_metrics_;
  std::unique_ptr<WebsiteEventsObserver> website_events_observer_;
};

TEST_F(WebsiteEventsObserverTest, OnUrlOpened) {
  SetAllowlistedUrls({ContentSettingsPattern::Wildcard().ToString()});
  base::test::TestFuture<MetricData> test_future;
  website_events_observer_->SetOnEventObservedCallback(
      test_future.GetRepeatingCallback());

  // Simulate URL opened event by calling observer callback directly. We will
  // generate real events in browser tests.
  website_events_observer_->OnUrlOpened(GURL(kTestUrl), web_contents_);
  const MetricData& result = test_future.Take();
  ASSERT_TRUE(result.has_event_data());
  EXPECT_THAT(result.event_data().type(), Eq(MetricEventType::URL_OPENED));
  ASSERT_TRUE(result.has_telemetry_data());
  ASSERT_TRUE(result.telemetry_data().has_website_telemetry());
  ASSERT_TRUE(
      result.telemetry_data().website_telemetry().has_website_opened_data());

  const auto& website_opened_data =
      result.telemetry_data().website_telemetry().website_opened_data();
  EXPECT_THAT(website_opened_data.url(), StrEq(kTestUrl));
  EXPECT_TRUE(website_opened_data.has_render_frame_routing_id());
  EXPECT_TRUE(website_opened_data.has_render_process_host_id());
}

TEST_F(WebsiteEventsObserverTest, OnUrlOpened_UnsetPolicy) {
  base::test::TestFuture<MetricData> test_future;
  website_events_observer_->SetOnEventObservedCallback(
      test_future.GetRepeatingCallback());

  // Simulate URL opened event by calling observer callback directly and verify
  // it is not reported. We will generate real events in browser tests.
  website_events_observer_->OnUrlOpened(GURL(kTestUrl), web_contents_);
  EXPECT_FALSE(test_future.IsReady());
}

TEST_F(WebsiteEventsObserverTest, OnUrlOpened_DisallowedUrl) {
  SetAllowlistedUrls({});
  base::test::TestFuture<MetricData> test_future;
  website_events_observer_->SetOnEventObservedCallback(
      test_future.GetRepeatingCallback());

  // Simulate URL opened event by calling observer callback directly and verify
  // it is not reported. We will generate real events in browser tests.
  website_events_observer_->OnUrlOpened(GURL(kTestUrl), web_contents_);
  EXPECT_FALSE(test_future.IsReady());
}

TEST_F(WebsiteEventsObserverTest, OnUrlClosed) {
  SetAllowlistedUrls({ContentSettingsPattern::Wildcard().ToString()});
  base::test::TestFuture<MetricData> test_future;
  website_events_observer_->SetOnEventObservedCallback(
      test_future.GetRepeatingCallback());

  // Simulate URL closed event by calling observer callback directly. We will
  // generate real events in browser tests.
  website_events_observer_->OnUrlClosed(GURL(kTestUrl), web_contents_);
  const MetricData& result = test_future.Take();
  ASSERT_TRUE(result.has_event_data());
  EXPECT_THAT(result.event_data().type(), Eq(MetricEventType::URL_CLOSED));
  ASSERT_TRUE(result.has_telemetry_data());
  ASSERT_TRUE(result.telemetry_data().has_website_telemetry());
  ASSERT_TRUE(
      result.telemetry_data().website_telemetry().has_website_closed_data());

  const auto& website_closed_data =
      result.telemetry_data().website_telemetry().website_closed_data();
  EXPECT_THAT(website_closed_data.url(), StrEq(kTestUrl));
  EXPECT_TRUE(website_closed_data.has_render_frame_routing_id());
  EXPECT_TRUE(website_closed_data.has_render_process_host_id());
}

TEST_F(WebsiteEventsObserverTest, OnUrlClosed_UnsetPolicy) {
  base::test::TestFuture<MetricData> test_future;
  website_events_observer_->SetOnEventObservedCallback(
      test_future.GetRepeatingCallback());

  // Simulate URL closed event by calling observer callback directly and verify
  // it is not reported. We will generate real events in browser tests.
  website_events_observer_->OnUrlClosed(GURL(kTestUrl), web_contents_);
  EXPECT_FALSE(test_future.IsReady());
}

TEST_F(WebsiteEventsObserverTest, OnUrlClosed_DisallowedUrl) {
  SetAllowlistedUrls({});
  base::test::TestFuture<MetricData> test_future;
  website_events_observer_->SetOnEventObservedCallback(
      test_future.GetRepeatingCallback());

  // Simulate URL closed event by calling observer callback directly and verify
  // it is not reported. We will generate real events in browser tests.
  website_events_observer_->OnUrlClosed(GURL(kTestUrl), web_contents_);
  EXPECT_FALSE(test_future.IsReady());
}

}  // namespace
}  // namespace reporting
