// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/desktop_platform_features_metrics_provider.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/native_theme/mock_os_settings_provider.h"

class DesktopPlatformFeaturesMetricsProviderTest : public testing::Test {
 public:
  DesktopPlatformFeaturesMetricsProviderTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override { ASSERT_TRUE(profile_manager_.SetUp()); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
};

TEST_F(DesktopPlatformFeaturesMetricsProviderTest, OverlayScrollbarsEnabled) {
  base::HistogramTester histogram_tester;
  ui::MockOsSettingsProvider mock_provider;

  mock_provider.SetPrefersOverlayScrollbars(true);
  DesktopPlatformFeaturesMetricsProvider provider;
  provider.ProvideCurrentSessionData(nullptr);

  histogram_tester.ExpectUniqueSample("Browser.OverlayScrollbarsEnabled", true,
                                      1);

  mock_provider.SetPrefersOverlayScrollbars(false);
  provider.ProvideCurrentSessionData(nullptr);

  histogram_tester.ExpectBucketCount("Browser.OverlayScrollbarsEnabled", false,
                                     1);
}
