// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/reporting/websites/website_usage_telemetry_sampler.h"

#include <memory>
#include <optional>

#include "base/json/values_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/reporting/metric_reporting_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::test::EqualsProto;
using ::testing::ElementsAre;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

namespace reporting {
namespace {

constexpr char kTestUserId[] = "TestUser";
constexpr char kTestUrl[] = "https://a.example.org/";

class WebsiteUsageTelemetrySamplerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile(kTestUserId);
    website_usage_telemetry_sampler_ =
        std::make_unique<WebsiteUsageTelemetrySampler>(profile_->GetWeakPtr());
  }

  // Simulates website usage tracking by persisting relevant usage information
  // for specified URL in the pref store.
  void TrackWebsiteUsageForUrl(const std::string& url,
                               const base::TimeDelta& usage_duration) {
    PrefService* const user_prefs = profile_->GetPrefs();
    if (!user_prefs->HasPrefPath(kWebsiteUsage)) {
      // Create empty dictionary if none exists in the pref store.
      user_prefs->SetDict(kWebsiteUsage, base::Value::Dict());
    }

    ScopedDictPrefUpdate usage_dict_pref(user_prefs, kWebsiteUsage);
    usage_dict_pref->Set(url, base::TimeDeltaToValue(usage_duration));
  }

  // Returns a `WebsiteUsageData::WebsiteUsage` proto message that tests can use
  // to test match with the actual one.
  WebsiteUsageData::WebsiteUsage WebsiteUsageProto(
      const std::string& url,
      const base::TimeDelta& running_time) {
    WebsiteUsageData::WebsiteUsage website_usage;
    website_usage.set_url(url);
    website_usage.set_running_time_ms(running_time.InMilliseconds());
    return website_usage;
  }

  ::content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  raw_ptr<Profile> profile_;
  std::unique_ptr<WebsiteUsageTelemetrySampler>
      website_usage_telemetry_sampler_;
};

TEST_F(WebsiteUsageTelemetrySamplerTest, NoWebsiteUsageData) {
  base::test::TestFuture<std::optional<MetricData>> collected_data_future;
  website_usage_telemetry_sampler_->MaybeCollect(
      collected_data_future.GetCallback());
  const std::optional<MetricData> metric_data_result =
      collected_data_future.Take();
  ASSERT_FALSE(metric_data_result.has_value());
}

TEST_F(WebsiteUsageTelemetrySamplerTest, CollectWebsiteUsageData) {
  static constexpr base::TimeDelta kWebsiteUsageDuration = base::Minutes(2);
  TrackWebsiteUsageForUrl(kTestUrl, kWebsiteUsageDuration);
  ASSERT_THAT(profile_->GetPrefs()->GetDict(kWebsiteUsage), SizeIs(1UL));

  // Attempt to collect usage data and verify data is deleted from the pref
  // store after it is reported.
  {
    base::test::TestFuture<std::optional<MetricData>> collected_data_future;
    website_usage_telemetry_sampler_->MaybeCollect(
        collected_data_future.GetCallback());
    const std::optional<MetricData> metric_data_result =
        collected_data_future.Take();
    ASSERT_TRUE(metric_data_result.has_value());
    const auto& metric_data = metric_data_result.value();
    ASSERT_TRUE(metric_data.has_telemetry_data());
    ASSERT_TRUE(metric_data.telemetry_data().has_website_telemetry());
    ASSERT_TRUE(metric_data.telemetry_data()
                    .website_telemetry()
                    .has_website_usage_data());
    EXPECT_THAT(metric_data.telemetry_data()
                    .website_telemetry()
                    .website_usage_data()
                    .website_usage(),
                ElementsAre(EqualsProto(
                    WebsiteUsageProto(kTestUrl, kWebsiteUsageDuration))));
    ASSERT_TRUE(profile_->GetPrefs()->GetDict(kWebsiteUsage).empty());
  }

  // Attempt to collect usage data again and verify there is none being
  // reported.
  {
    base::test::TestFuture<std::optional<MetricData>> collected_data_future;
    website_usage_telemetry_sampler_->MaybeCollect(
        collected_data_future.GetCallback());
    const std::optional<MetricData> metric_data_result =
        collected_data_future.Take();
    EXPECT_FALSE(metric_data_result.has_value());
  }
}

TEST_F(WebsiteUsageTelemetrySamplerTest, CollectMultipleWebsiteUsageData) {
  static constexpr char kOtherUrl[] = "https://b.example.org";
  static constexpr base::TimeDelta kWebsiteUsageDuration = base::Minutes(2);
  TrackWebsiteUsageForUrl(kTestUrl, kWebsiteUsageDuration);
  TrackWebsiteUsageForUrl(kOtherUrl, kWebsiteUsageDuration);
  ASSERT_THAT(profile_->GetPrefs()->GetDict(kWebsiteUsage), SizeIs(2UL));

  // Attempt to collect usage data and verify data is deleted from the pref
  // store after it is reported.
  {
    base::test::TestFuture<std::optional<MetricData>> collected_data_future;
    website_usage_telemetry_sampler_->MaybeCollect(
        collected_data_future.GetCallback());
    const std::optional<MetricData> metric_data_result =
        collected_data_future.Take();
    ASSERT_TRUE(metric_data_result.has_value());
    const auto& metric_data = metric_data_result.value();
    ASSERT_TRUE(metric_data.has_telemetry_data());
    ASSERT_TRUE(metric_data.telemetry_data().has_website_telemetry());
    ASSERT_TRUE(metric_data.telemetry_data()
                    .website_telemetry()
                    .has_website_usage_data());
    EXPECT_THAT(
        metric_data.telemetry_data()
            .website_telemetry()
            .website_usage_data()
            .website_usage(),
        UnorderedElementsAre(
            EqualsProto(WebsiteUsageProto(kTestUrl, kWebsiteUsageDuration)),
            EqualsProto(WebsiteUsageProto(kOtherUrl, kWebsiteUsageDuration))));
    ASSERT_TRUE(profile_->GetPrefs()->GetDict(kWebsiteUsage).empty());
  }

  // Attempt to collect usage data again and verify there is none being
  // reported.
  {
    base::test::TestFuture<std::optional<MetricData>> collected_data_future;
    website_usage_telemetry_sampler_->MaybeCollect(
        collected_data_future.GetCallback());
    const std::optional<MetricData> metric_data_result =
        collected_data_future.Take();
    EXPECT_FALSE(metric_data_result.has_value());
  }
}

TEST_F(WebsiteUsageTelemetrySamplerTest,
       CollectWebsiteUsageDataAfterProfileDestructed) {
  static constexpr base::TimeDelta kWebsiteUsageDuration = base::Minutes(2);
  TrackWebsiteUsageForUrl(kTestUrl, kWebsiteUsageDuration);
  ASSERT_THAT(profile_->GetPrefs()->GetDict(kWebsiteUsage), SizeIs(1UL));

  // Delete the test profile.
  profile_ = nullptr;
  profile_manager_.DeleteAllTestingProfiles();

  // Attempt to collect usage data and verify there is no data being reported.
  base::test::TestFuture<std::optional<MetricData>> collected_data_future;
  website_usage_telemetry_sampler_->MaybeCollect(
      collected_data_future.GetCallback());
  const std::optional<MetricData> metric_data_result =
      collected_data_future.Take();
  EXPECT_FALSE(metric_data_result.has_value());
}

}  // namespace
}  // namespace reporting
