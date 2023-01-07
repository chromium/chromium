// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/reporting/network/network_bandwidth_sampler.h"

#include "base/test/task_environment.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace reporting {
namespace {

constexpr int64_t kInitDownloadSpeedKbps = 100;

class NetworkBandwidthSamplerTest : public ::testing::Test {
 protected:
  NetworkBandwidthSamplerTest() { EXPECT_TRUE(profile_manager_.SetUp()); }

  void SetUp() override {
    profile_ = profile_manager_.CreateTestingProfile("test-user");
  }

  void UpdateDownloadSpeedKbps(int64_t download_speed_kbps) {
    g_browser_process->network_quality_tracker()
        ->ReportRTTsAndThroughputForTesting(base::Milliseconds(100),
                                            download_speed_kbps);
    task_environment_.RunUntilIdle();
  }

  void SetPrefValue(bool value) {
    profile_->GetPrefs()->SetBoolean(::prefs::kInsightsExtensionEnabled, value);
    task_environment_.RunUntilIdle();
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  raw_ptr<Profile> profile_;
};

TEST_F(NetworkBandwidthSamplerTest, DoesNotReportDownloadSpeedByDefault) {
  UpdateDownloadSpeedKbps(kInitDownloadSpeedKbps);
  NetworkBandwidthSampler sampler(g_browser_process->network_quality_tracker(),
                                  profile_);

  ::reporting::test::TestEvent<absl::optional<MetricData>> test_event;
  sampler.MaybeCollect(test_event.cb());
  auto result = test_event.result();
  ASSERT_FALSE(result.has_value());
}

TEST_F(NetworkBandwidthSamplerTest, ReportsDownloadSpeedWhenPrefSet) {
  SetPrefValue(true);
  UpdateDownloadSpeedKbps(kInitDownloadSpeedKbps);
  NetworkBandwidthSampler sampler(g_browser_process->network_quality_tracker(),
                                  profile_);

  ::reporting::test::TestEvent<absl::optional<MetricData>> test_event;
  sampler.MaybeCollect(test_event.cb());
  auto result = test_event.result();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->mutable_telemetry_data()
                ->mutable_networks_telemetry()
                ->mutable_bandwidth_data()
                ->download_speed_kbps(),
            kInitDownloadSpeedKbps);
}

TEST_F(NetworkBandwidthSamplerTest, DoesNotReportDownloadSpeedWhenPrefUnset) {
  SetPrefValue(false);
  UpdateDownloadSpeedKbps(kInitDownloadSpeedKbps);
  NetworkBandwidthSampler sampler(g_browser_process->network_quality_tracker(),
                                  profile_);

  ::reporting::test::TestEvent<absl::optional<MetricData>> test_event;
  sampler.MaybeCollect(test_event.cb());
  auto result = test_event.result();
  ASSERT_FALSE(result.has_value());
}

TEST_F(NetworkBandwidthSamplerTest, DoesNotReportDownloadSpeedIfUnavailable) {
  SetPrefValue(true);
  NetworkBandwidthSampler sampler(g_browser_process->network_quality_tracker(),
                                  profile_);

  ::reporting::test::TestEvent<absl::optional<MetricData>> test_event;
  sampler.MaybeCollect(test_event.cb());
  auto result = test_event.result();
  ASSERT_FALSE(result.has_value());
}

TEST_F(NetworkBandwidthSamplerTest, ReportsUpdatedDownloadSpeed) {
  SetPrefValue(true);
  UpdateDownloadSpeedKbps(kInitDownloadSpeedKbps);
  NetworkBandwidthSampler sampler(g_browser_process->network_quality_tracker(),
                                  profile_);

  // Update download speed so we can verify that the sampler reports the new
  // value.
  const int64_t download_speed_kbps = 100000;
  UpdateDownloadSpeedKbps(download_speed_kbps);

  ::reporting::test::TestEvent<absl::optional<MetricData>> test_event;
  sampler.MaybeCollect(test_event.cb());
  auto result = test_event.result();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->mutable_telemetry_data()
                ->mutable_networks_telemetry()
                ->mutable_bandwidth_data()
                ->download_speed_kbps(),
            download_speed_kbps);
}

}  // namespace
}  // namespace reporting
