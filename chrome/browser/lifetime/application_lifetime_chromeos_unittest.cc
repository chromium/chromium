// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/application_lifetime_chromeos.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "components/crash/core/common/crash_key.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace chrome {

TEST(ApplicationLifetimeChromeosTest, NoUpdateIfDbusIsNotInitialized) {
  ASSERT_FALSE(ash::DBusThreadManager::IsInitialized());
  EXPECT_FALSE(UpdatePending());
}

TEST(ApplicationLifetimeChromeosTest,
     SetShutdownPhaseRecordsMetricsAndCrashKey) {
  crash_reporter::InitializeCrashKeysForTesting();
  browser_shutdown::ResetShutdownGlobalsForTesting();
  absl::Cleanup cleanup = [] {
    browser_shutdown::ResetShutdownGlobalsForTesting();
    crash_reporter::ResetCrashKeysForTesting();
  };
  base::HistogramTester histogram_tester;

  using browser_shutdown::ShutdownPhase;
  EXPECT_EQ(browser_shutdown::GetShutdownPhaseForTesting(),
            ShutdownPhase::kNotStarted);
  EXPECT_EQ(crash_reporter::GetCrashKeyValue("cros-shutdown-phase"), "");

  struct Step {
    ShutdownPhase phase;
    const char* crash_key;
    const char* completed_histogram;
  };
  constexpr Step kSteps[] = {
      {ShutdownPhase::kClosingBrowsers, "closing-browsers", nullptr},
      {ShutdownPhase::kAshPreShell, "ash-pre-shell",
       "Shutdown.ChromeOS.PhaseDuration.ClosingBrowsers"},
      {ShutdownPhase::kAshShell, "ash-shell",
       "Shutdown.ChromeOS.PhaseDuration.AshPreShell"},
      {ShutdownPhase::kBrowserProcess, "browser-process",
       "Shutdown.ChromeOS.PhaseDuration.AshShell"},
      {ShutdownPhase::kAshPostProfile, "ash-post-profile",
       "Shutdown.ChromeOS.PhaseDuration.BrowserProcess"},
      {ShutdownPhase::kStoppingThreads, "stopping-threads",
       "Shutdown.ChromeOS.PhaseDuration.AshPostProfile"},
      {ShutdownPhase::kPostDestroyThreads, "post-destroy-threads",
       "Shutdown.ChromeOS.PhaseDuration.StoppingThreads"},
      {ShutdownPhase::kDone, "done",
       "Shutdown.ChromeOS.PhaseDuration.PostDestroyThreads"},
  };

  for (const Step& step : kSteps) {
    browser_shutdown::SetShutdownPhase(step.phase);
    EXPECT_EQ(browser_shutdown::GetShutdownPhaseForTesting(), step.phase);
    EXPECT_EQ(crash_reporter::GetCrashKeyValue("cros-shutdown-phase"),
              step.crash_key);
    if (step.completed_histogram) {
      histogram_tester.ExpectTotalCount(step.completed_histogram, 1);
    }
  }

  browser_shutdown::ResetShutdownGlobalsForTesting();
  EXPECT_EQ(browser_shutdown::GetShutdownPhaseForTesting(),
            ShutdownPhase::kNotStarted);
  EXPECT_EQ(crash_reporter::GetCrashKeyValue("cros-shutdown-phase"), "");
}

}  // namespace chrome
