// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/metrics/aw_stability_metrics_provider.h"

#include <memory>

#include "base/macros.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/metrics/stability_metrics_helper.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace android_webview {

namespace {

class AwStabilityMetricsProviderTest : public testing::Test {
 protected:
  AwStabilityMetricsProviderTest()
      : prefs_(new TestingPrefServiceSimple),
        notification_service_(content::NotificationService::Create()) {
    metrics::StabilityMetricsHelper::RegisterPrefs(prefs()->registry());
  }

  TestingPrefServiceSimple* prefs() { return prefs_.get(); }

 private:
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
  content::BrowserTaskEnvironment task_environment_;

  // AwStabilityMetricsProvider::RegisterForNotifications() requires the
  // NotificationService to be up and running. Initialize it here, throw away
  // the value because we don't need it directly.
  std::unique_ptr<content::NotificationService> notification_service_;

  DISALLOW_COPY_AND_ASSIGN(AwStabilityMetricsProviderTest);
};

class TestMetricsProvider : public AwStabilityMetricsProvider {
 public:
  explicit TestMetricsProvider(PrefService* local_state)
      : AwStabilityMetricsProvider(local_state) {}
  ~TestMetricsProvider() override {}

  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    AwStabilityMetricsProvider::Observe(type, source, details);
  }
};

}  // namespace

TEST_F(AwStabilityMetricsProviderTest, PageLoadCount) {
  base::HistogramTester histogram_tester;
  TestMetricsProvider provider(prefs());
  metrics::SystemProfileProto system_profile;

  // Create a fake notification. Source and details aren't checked in the
  // implementation, so just use empty values.
  provider.Observe(content::NOTIFICATION_LOAD_START,
                   content::NotificationService::AllSources(),
                   content::NotificationService::NoDetails());

  provider.ProvideStabilityMetrics(&system_profile);
  EXPECT_EQ(1, system_profile.stability().page_load_count());
  histogram_tester.ExpectUniqueSample(
      "Stability.Counts2", metrics::StabilityEventType::kPageLoad, 1);
}

TEST_F(AwStabilityMetricsProviderTest, RendererHangCount) {
  base::HistogramTester histogram_tester;
  TestMetricsProvider provider(prefs());
  metrics::SystemProfileProto system_profile;

  // Create a fake notification. Source and details aren't checked in the
  // implementation, so just use empty values.
  provider.Observe(content::NOTIFICATION_RENDER_WIDGET_HOST_HANG,
                   content::NotificationService::AllSources(),
                   content::NotificationService::NoDetails());

  provider.ProvideStabilityMetrics(&system_profile);
  EXPECT_EQ(1, system_profile.stability().renderer_hang_count());
  histogram_tester.ExpectUniqueSample("ChildProcess.HungRendererInForeground",
                                      /* true */ 1, 1);
  histogram_tester.ExpectTotalCount(
      "ChildProcess.HungRendererAvailableMemoryMB", 1);
}

TEST_F(AwStabilityMetricsProviderTest, RendererLaunchCount) {
  TestMetricsProvider provider(prefs());
  metrics::SystemProfileProto system_profile;

  // Create a fake notification. Source and details aren't checked in the
  // implementation, so just use empty values.
  provider.Observe(content::NOTIFICATION_RENDERER_PROCESS_CREATED,
                   content::NotificationService::AllSources(),
                   content::NotificationService::NoDetails());

  provider.ProvideStabilityMetrics(&system_profile);
  EXPECT_EQ(1, system_profile.stability().renderer_launch_count());
}

}  // namespace android_webview
