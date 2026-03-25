// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"

#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/metrics/metrics_features.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_reporting_level.h"
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

  void SetMetricsReportingLevel(metrics::MetricsReportingLevel level) {
    GetLocalState()->SetInteger(metrics::prefs::kMetricsReportingLevel,
                                static_cast<int>(level));
  }

  void TearDown() override {
    task_environment_.RunUntilIdle();
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

TEST_F(ChromeMetricsServiceAccessorTest,
       MetricsReportingEnabled_RestructureMetricsConsentSettings_FeatureOff) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      metrics::features::kRestructureMetricsConsentSettings);

  const char* kMigrationDonePref =
      metrics::prefs::kMetricsReportingMigrationDone;
  const char* kLegacyReportingPref = metrics::prefs::kMetricsReportingEnabled;

  // Initialize prefs to defaults.
  // Note: kLegacyReportingPref actually defaults to true, as metrics are
  // opt-out. (Though it can be set to false by the installer on Windows).
  GetLocalState()->SetBoolean(kMigrationDonePref, false);
  SetMetricsReportingLevel(metrics::MetricsReportingLevel::kNone);
  GetLocalState()->SetBoolean(kLegacyReportingPref, true);

  // If feature is OFF, it always falls back to the legacy pref.
  EXPECT_TRUE(
      ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled());

  GetLocalState()->SetBoolean(kLegacyReportingPref, false);
  SetMetricsReportingLevel(metrics::MetricsReportingLevel::kBasic);
  EXPECT_FALSE(
      ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled());

  SetMetricsReportingLevel(metrics::MetricsReportingLevel::kAdvanced);
  EXPECT_FALSE(
      ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled());

  // Even if migration IS done, it depends purely on the legacy pref.
  // The new reporting level pref should be ignored.
  GetLocalState()->SetBoolean(kMigrationDonePref, true);

  // Still have kLegacyReportingPref = false -> should be false.
  EXPECT_FALSE(
      ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled());

  // Set legacy to true -> should be true.
  GetLocalState()->SetBoolean(kLegacyReportingPref, true);
  EXPECT_TRUE(
      ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled());

  // Even if level is kNone, if legacy pref is true it's true.
  SetMetricsReportingLevel(metrics::MetricsReportingLevel::kNone);
  EXPECT_TRUE(
      ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled());

#else
  // Metrics Reporting is never enabled when GOOGLE_CHROME_BRANDING is
  // undefined.
  EXPECT_FALSE(
      ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled());
#endif
}

TEST_F(ChromeMetricsServiceAccessorTest,
       MetricsReportingEnabled_RestructureMetricsConsentSettings) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      metrics::features::kRestructureMetricsConsentSettings);

  const char* kMigrationDonePref =
      metrics::prefs::kMetricsReportingMigrationDone;
  const char* kLegacyReportingPref = metrics::prefs::kMetricsReportingEnabled;

  // Initialize prefs to defaults.
  // Note: kLegacyReportingPref actually defaults to true, as metrics are
  // opt-out. (Though it can be set to false by the installer on Windows).
  GetLocalState()->SetBoolean(kMigrationDonePref, false);
  SetMetricsReportingLevel(metrics::MetricsReportingLevel::kNone);
  GetLocalState()->SetBoolean(kLegacyReportingPref, true);

  // 1. If migration is NOT done, it falls back to the legacy pref.
  EXPECT_TRUE(
      ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled());

  // Even if the new pref is set to basic or advanced, it should be ignored if
  // migration is not done.
  GetLocalState()->SetBoolean(kLegacyReportingPref, false);
  SetMetricsReportingLevel(metrics::MetricsReportingLevel::kBasic);
  EXPECT_FALSE(
      ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled());

  SetMetricsReportingLevel(metrics::MetricsReportingLevel::kAdvanced);
  EXPECT_FALSE(
      ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled());

  // 2. If migration IS done, it depends purely on the new reporting level pref.
  // The legacy pref should be ignored.
  GetLocalState()->SetBoolean(kMigrationDonePref, true);
  SetMetricsReportingLevel(metrics::MetricsReportingLevel::kNone);
  EXPECT_FALSE(
      ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled());

  // Set level to kBasic -> should be true.
  SetMetricsReportingLevel(metrics::MetricsReportingLevel::kBasic);
  EXPECT_TRUE(
      ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled());

  // Set level to kAdvanced -> should be true.
  SetMetricsReportingLevel(metrics::MetricsReportingLevel::kAdvanced);
  EXPECT_TRUE(
      ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled());

  // Even if legacy pref is false, if level is advanced/basic it's true.
  GetLocalState()->SetBoolean(kLegacyReportingPref, false);
  EXPECT_TRUE(
      ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled());

#else
  // Metrics Reporting is never enabled when GOOGLE_CHROME_BRANDING is
  // undefined.
  EXPECT_FALSE(
      ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled());
#endif
}
