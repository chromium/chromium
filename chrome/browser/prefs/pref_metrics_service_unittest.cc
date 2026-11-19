// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_metrics_service.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/device_info.h"
#endif

namespace {

constexpr char kStartupPageLoadSettingsHistogram[] =
    "Settings.StartupPageLoadSettings2";
constexpr char kStartupPageEngineTypesHistogram[] =
    "Settings.StartupPageEngineTypes2";
constexpr char kStartupUrl[] = "https://www.google.com/";

class PrefMetricsServiceTest : public testing::Test {
 public:
#if BUILDFLAG(IS_ANDROID)
  void TearDown() override {
    base::android::device_info::reset_is_desktop_for_testing();
  }
#endif

 protected:
  // Configures `profile` to open `kStartupUrl` on startup.
  static void SetStartupUrlsPref(TestingProfile* profile) {
    profile->GetPrefs()->SetInteger(prefs::kRestoreOnStartup,
                                    SessionStartupPref::kPrefValueURLs);
    base::ListValue urls;
    urls.Append(kStartupUrl);
    profile->GetPrefs()->SetList(prefs::kURLsToRestoreOnStartup,
                                 std::move(urls));
  }

  content::BrowserTaskEnvironment task_environment_;
};

// Desktop platforms (including desktop Android) record the startup preference
// and startup page engine types on regular profile load.
TEST_F(PrefMetricsServiceTest, RecordsStartupPrefs) {
#if BUILDFLAG(IS_ANDROID)
  base::android::device_info::set_is_desktop_for_testing(true);
#endif
  TestingProfile profile;
  SetStartupUrlsPref(&profile);
  base::HistogramTester histogram_tester;

  PrefMetricsService service(&profile);

  histogram_tester.ExpectUniqueSample(kStartupPageLoadSettingsHistogram,
                                      SessionStartupPref::kPrefValueURLs, 1);
  histogram_tester.ExpectTotalCount(kStartupPageEngineTypesHistogram, 1);
}

#if BUILDFLAG(IS_ANDROID)
// Non-desktop Android devices always restore the last session and must not
// record startup preference metrics.
TEST_F(PrefMetricsServiceTest, SkipsStartupPrefsOnNonDesktopAndroid) {
  base::android::device_info::set_is_desktop_for_testing(false);
  TestingProfile profile;
  SetStartupUrlsPref(&profile);
  base::HistogramTester histogram_tester;

  PrefMetricsService service(&profile);

  histogram_tester.ExpectTotalCount(kStartupPageLoadSettingsHistogram, 0);
  histogram_tester.ExpectTotalCount(kStartupPageEngineTypesHistogram, 0);
}
#endif  // BUILDFLAG(IS_ANDROID)

// The service is created for regular profiles only; Incognito profiles inherit
// the parent's settings and must not record a second sample.
TEST_F(PrefMetricsServiceTest, NotCreatedForOffTheRecordProfile) {
  TestingProfile profile;

  EXPECT_NE(nullptr, PrefMetricsService::Factory::GetForProfile(&profile));
  EXPECT_EQ(nullptr,
            PrefMetricsService::Factory::GetForProfile(
                profile.GetPrimaryOTRProfile(/*create_if_needed=*/true)));
}

}  // namespace
