// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_browser_main_extra_parts_metrics.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/variations_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/screen.h"
#include "ui/display/test/test_screen.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/events/devices/device_data_manager_test_api.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/dbus/u2f/u2f_client.h"  // nogncheck
#endif

namespace {

const char kTouchEventFeatureDetectionEnabledHistogramName[] =
    "Touchscreen.TouchEventsEnabled";
const char kSupportsHDRHistogramName[] = "Hardware.Display.SupportsHDR";
constexpr char kEnableBenchmarkingPrefId[] = "enable_benchmarking_countdown";

// This is a fake that causes HandleEnableBenchmarkingCountdownAsync() to do
// nothing. A full implementation of HandleEnableBenchmarkingCountdownAsync
// would require significant fakes which we do not want to implement in this
// test.
class ChromeBrowserMainExtraPartsMetricsFake
    : public ChromeBrowserMainExtraPartsMetrics {
 public:
  // Expose a protected member.
  static void HandleEnableBenchmarkingCountdownPublic(
      PrefService* pref_service,
      std::unique_ptr<flags_ui::FlagsStorage> storage,
      flags_ui::FlagAccess access) {
    HandleEnableBenchmarkingCountdown(pref_service, std::move(storage), access);
  }

  // Disable for tests.
  void HandleEnableBenchmarkingCountdownAsync() override {}
};

}  // namespace

class ChromeBrowserMainExtraPartsMetricsTest : public testing::Test {
 public:
  ChromeBrowserMainExtraPartsMetricsTest();

  ChromeBrowserMainExtraPartsMetricsTest(
      const ChromeBrowserMainExtraPartsMetricsTest&) = delete;
  ChromeBrowserMainExtraPartsMetricsTest& operator=(
      const ChromeBrowserMainExtraPartsMetricsTest&) = delete;

  ~ChromeBrowserMainExtraPartsMetricsTest() override;

 protected:
  void SetUp() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // ChromeBrowserMainExtraPartsMetrics::RecordMetrics() requires a U2FClient,
    // which would ordinarily have been set up by browser DBus initialization.
    chromeos::U2FClient::InitializeFake();
#endif
  }

  void TearDown() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    task_environment_.RunUntilIdle();
    chromeos::U2FClient::Shutdown();
#endif
  }

#if BUILDFLAG(IS_OZONE)
  ui::DeviceDataManagerTestApi device_data_manager_test_api_;
#endif

 private:
  // Provides a message loop and allows the use of the task scheduler
  content::BrowserTaskEnvironment task_environment_;

  // Dummy screen required by a ChromeBrowserMainExtraPartsMetrics test target.
  display::test::TestScreen test_screen_;
};

ChromeBrowserMainExtraPartsMetricsTest::
    ChromeBrowserMainExtraPartsMetricsTest() {
  display::Screen::SetScreenInstance(&test_screen_);
}

ChromeBrowserMainExtraPartsMetricsTest::
    ~ChromeBrowserMainExtraPartsMetricsTest() {
  display::Screen::SetScreenInstance(nullptr);
}

// Verify a TouchEventsEnabled value isn't recorded during construction.
TEST_F(ChromeBrowserMainExtraPartsMetricsTest,
       VerifyTouchEventsEnabledIsNotRecordedAfterConstruction) {
  base::HistogramTester histogram_tester;
  ChromeBrowserMainExtraPartsMetrics test_target;
  histogram_tester.ExpectTotalCount(
      kTouchEventFeatureDetectionEnabledHistogramName, 0);
}

#if BUILDFLAG(IS_OZONE)

// Verify a TouchEventsEnabled value isn't recorded during PostBrowserStart if
// the device scan hasn't completed yet.
// TODO(https://crbug.com/940076): Consistently flaky.
TEST_F(ChromeBrowserMainExtraPartsMetricsTest,
       DISABLED_VerifyTouchEventsEnabledIsNotRecordedAfterPostBrowserStart) {
  base::HistogramTester histogram_tester;

  ChromeBrowserMainExtraPartsMetricsFake test_target;

  test_target.PostBrowserStart();
  histogram_tester.ExpectTotalCount(
      kTouchEventFeatureDetectionEnabledHistogramName, 0);
}

// Verify a TouchEventsEnabled value is recorded during PostBrowserStart if the
// device scan has already completed.
TEST_F(ChromeBrowserMainExtraPartsMetricsTest,
       VerifyTouchEventsEnabledIsRecordedAfterPostBrowserStart) {
  base::HistogramTester histogram_tester;

  device_data_manager_test_api_.OnDeviceListsComplete();

  ChromeBrowserMainExtraPartsMetricsFake test_target;

  test_target.PostBrowserStart();
  histogram_tester.ExpectTotalCount(
      kTouchEventFeatureDetectionEnabledHistogramName, 1);
}

// Verify a TouchEventsEnabled value is recorded when an asynchronous device
// scan completes.
TEST_F(ChromeBrowserMainExtraPartsMetricsTest,
       VerifyTouchEventsEnabledIsRecordedWhenDeviceListsComplete) {
  base::HistogramTester histogram_tester;
  ChromeBrowserMainExtraPartsMetricsFake test_target;

  test_target.PostBrowserStart();
  device_data_manager_test_api_.NotifyObserversDeviceListsComplete();
  histogram_tester.ExpectTotalCount(
      kTouchEventFeatureDetectionEnabledHistogramName, 1);
}

// Verify a TouchEventsEnabled value is only recorded once if multiple
// asynchronous device scans happen.
TEST_F(ChromeBrowserMainExtraPartsMetricsTest,
       VerifyTouchEventsEnabledIsOnlyRecordedOnce) {
  base::HistogramTester histogram_tester;
  ChromeBrowserMainExtraPartsMetricsFake test_target;

  test_target.PostBrowserStart();
  device_data_manager_test_api_.NotifyObserversDeviceListsComplete();
  device_data_manager_test_api_.NotifyObserversDeviceListsComplete();
  histogram_tester.ExpectTotalCount(
      kTouchEventFeatureDetectionEnabledHistogramName, 1);
}

#else

// Verify a TouchEventsEnabled value is recorded during PostBrowserStart.
// Flaky on Win only.  http://crbug.com/1026946
#if BUILDFLAG(IS_WIN)
#define MAYBE_VerifyTouchEventsEnabledIsRecordedAfterPostBrowserStart \
  DISABLED_VerifyTouchEventsEnabledIsRecordedAfterPostBrowserStart
#else
#define MAYBE_VerifyTouchEventsEnabledIsRecordedAfterPostBrowserStart \
  VerifyTouchEventsEnabledIsRecordedAfterPostBrowserStart
#endif
TEST_F(ChromeBrowserMainExtraPartsMetricsTest,
       MAYBE_VerifyTouchEventsEnabledIsRecordedAfterPostBrowserStart) {
  base::HistogramTester histogram_tester;
  ChromeBrowserMainExtraPartsMetricsFake test_target;

  test_target.PostBrowserStart();
  histogram_tester.ExpectTotalCount(
      kTouchEventFeatureDetectionEnabledHistogramName, 1);
}

#endif  // BUILDFLAG(IS_OZONE)

// Verify a Hardware.Display.SupportsHDR value is recorded during
// PostBrowserStart.
TEST_F(ChromeBrowserMainExtraPartsMetricsTest,
       VerifySupportsHDRIsRecordedAfterPostBrowserStart) {
  base::HistogramTester histogram_tester;
  ChromeBrowserMainExtraPartsMetricsFake test_target;

  test_target.PostBrowserStart();
  histogram_tester.ExpectTotalCount(kSupportsHDRHistogramName, 1);
}

// Tests that if the countdown is called when there are no prefs, that the pref
// is created.
TEST_F(ChromeBrowserMainExtraPartsMetricsTest,
       EnableBenchmarkingCountdownNoFlag) {
  // Register prefs.
  TestingPrefServiceSimple pref_service;
  ChromeBrowserMainExtraPartsMetrics::RegisterPrefs(pref_service.registry());
  flags_ui::PrefServiceFlagsStorage::RegisterPrefs(pref_service.registry());

  flags_ui::PrefServiceFlagsStorage storage(&pref_service);

  // If there's nothing in flags storage then the countdown should have no
  // effect.
  ChromeBrowserMainExtraPartsMetricsFake::
      HandleEnableBenchmarkingCountdownPublic(
          &pref_service,
          std::make_unique<flags_ui::PrefServiceFlagsStorage>(&pref_service),
          flags_ui::kOwnerAccessToFlags);
  EXPECT_FALSE(pref_service.HasPrefPath(kEnableBenchmarkingPrefId));
}

TEST_F(ChromeBrowserMainExtraPartsMetricsTest,
       EnableBenchmarkingCountdownFromNoStorage) {
  // Register prefs.
  TestingPrefServiceSimple pref_service;
  ChromeBrowserMainExtraPartsMetrics::RegisterPrefs(pref_service.registry());
  flags_ui::PrefServiceFlagsStorage::RegisterPrefs(pref_service.registry());

  flags_ui::PrefServiceFlagsStorage storage(&pref_service);

  // Once a flag is set we should see an effect.
  std::set<std::string> flags = {variations::switches::kEnableBenchmarking};
  storage.SetFlags(flags);
  ChromeBrowserMainExtraPartsMetricsFake::
      HandleEnableBenchmarkingCountdownPublic(
          &pref_service,
          std::make_unique<flags_ui::PrefServiceFlagsStorage>(&pref_service),
          flags_ui::kOwnerAccessToFlags);
  EXPECT_EQ(2, pref_service.GetInteger(kEnableBenchmarkingPrefId));
  EXPECT_EQ(1u, storage.GetFlags().size());
}

// Checks that the countdown takes the pref from 2 to 1.
TEST_F(ChromeBrowserMainExtraPartsMetricsTest,
       EnableBenchmarkingCountdownFromStorage2) {
  // Register prefs.
  TestingPrefServiceSimple pref_service;
  ChromeBrowserMainExtraPartsMetrics::RegisterPrefs(pref_service.registry());
  flags_ui::PrefServiceFlagsStorage::RegisterPrefs(pref_service.registry());

  flags_ui::PrefServiceFlagsStorage storage(&pref_service);

  std::set<std::string> flags = {variations::switches::kEnableBenchmarking};
  storage.SetFlags(flags);

  // Set initial state:
  pref_service.SetInteger(kEnableBenchmarkingPrefId, 2);

  ChromeBrowserMainExtraPartsMetricsFake::
      HandleEnableBenchmarkingCountdownPublic(
          &pref_service,
          std::make_unique<flags_ui::PrefServiceFlagsStorage>(&pref_service),
          flags_ui::kOwnerAccessToFlags);
  EXPECT_EQ(1, pref_service.GetInteger(kEnableBenchmarkingPrefId));
  EXPECT_EQ(1u, storage.GetFlags().size());
}

// Checks that the countdown takes the pref from 1 to 0 and clears the pref.
TEST_F(ChromeBrowserMainExtraPartsMetricsTest,
       EnableBenchmarkingCountdownFromStorage1) {
  // Register prefs.
  TestingPrefServiceSimple pref_service;
  ChromeBrowserMainExtraPartsMetrics::RegisterPrefs(pref_service.registry());
  flags_ui::PrefServiceFlagsStorage::RegisterPrefs(pref_service.registry());

  flags_ui::PrefServiceFlagsStorage storage(&pref_service);

  std::set<std::string> flags = {variations::switches::kEnableBenchmarking};
  storage.SetFlags(flags);

  // Set initial state:
  pref_service.SetInteger(kEnableBenchmarkingPrefId, 1);

  ChromeBrowserMainExtraPartsMetricsFake::
      HandleEnableBenchmarkingCountdownPublic(
          &pref_service,
          std::make_unique<flags_ui::PrefServiceFlagsStorage>(&pref_service),
          flags_ui::kOwnerAccessToFlags);
  EXPECT_FALSE(pref_service.HasPrefPath(kEnableBenchmarkingPrefId));
  EXPECT_EQ(0u, storage.GetFlags().size());
}
