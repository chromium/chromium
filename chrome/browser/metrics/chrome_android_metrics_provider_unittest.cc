// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_android_metrics_provider.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

class ChromeAndroidMetricsProviderTest : public testing::Test {
 public:
  ChromeAndroidMetricsProviderTest() : metrics_provider_(&pref_service_) {
    ChromeAndroidMetricsProvider::RegisterPrefs(pref_service_.registry());
  }
  ~ChromeAndroidMetricsProviderTest() override = default;

 protected:
  base::HistogramTester histogram_tester_;
  TestingPrefServiceSimple pref_service_;
  ChromeAndroidMetricsProvider metrics_provider_;
};

TEST_F(ChromeAndroidMetricsProviderTest, OnDidCreateMetricsLog_CustomTabs) {
  metrics_provider_.OnDidCreateMetricsLog();
  histogram_tester_.ExpectTotalCount("CustomTabs.Visible", 1);
  histogram_tester_.ExpectTotalCount("Android.ChromeActivity.Type", 1);
}

TEST_F(ChromeAndroidMetricsProviderTest,
       ProvideCurrentSessionData_MultiWindowMode) {
  metrics::ChromeUserMetricsExtension uma_proto;
  metrics_provider_.ProvideCurrentSessionData(&uma_proto);
  histogram_tester_.ExpectTotalCount("Android.MultiWindowMode.Active", 1);
}

TEST_F(ChromeAndroidMetricsProviderTest,
       ProvideCurrentSessionData_AppNotifications) {
  metrics::ChromeUserMetricsExtension uma_proto;
  metrics_provider_.ProvideCurrentSessionData(&uma_proto);
  histogram_tester_.ExpectTotalCount("Android.AppNotificationStatus", 1);
}

TEST_F(ChromeAndroidMetricsProviderTest,
       ProvideCurrentSessionData_DarkModeState) {
  metrics::ChromeUserMetricsExtension uma_proto;
  ASSERT_FALSE(uma_proto.system_profile().os().has_dark_mode_state());

  metrics_provider_.ProvideCurrentSessionData(&uma_proto);
  ASSERT_TRUE(uma_proto.system_profile().os().has_dark_mode_state());
}
