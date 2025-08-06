// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"

#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class ChromeMetricsServiceAccessorTest : public testing::Test {
 public:
  ChromeMetricsServiceAccessorTest() = default;

  ChromeMetricsServiceAccessorTest(const ChromeMetricsServiceAccessorTest&) =
      delete;
  ChromeMetricsServiceAccessorTest& operator=(
      const ChromeMetricsServiceAccessorTest&) = delete;

  PrefService* GetLocalState() {
    return TestingBrowserProcess::GetGlobal()->local_state();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(ChromeMetricsServiceAccessorTest, MetricsReportingEnabled) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  const char* pref = metrics::prefs::kMetricsReportingEnabled;
  GetLocalState()->SetDefaultPrefValue(pref, base::Value(false));

  GetLocalState()->SetBoolean(pref, false);
  EXPECT_FALSE(
      ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled());
  GetLocalState()->SetBoolean(pref, true);
  EXPECT_TRUE(
      ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled());
  GetLocalState()->ClearPref(pref);
  EXPECT_FALSE(
      ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled());
#else
  // Metrics Reporting is never enabled when GOOGLE_CHROME_BRANDING is
  // undefined.
  EXPECT_FALSE(
      ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled());
#endif
}
