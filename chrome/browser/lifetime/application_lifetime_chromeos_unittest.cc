// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/application_lifetime_chromeos.h"

#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "components/crash/core/common/crash_key.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome {

class ApplicationLifetimeChromeosTest : public testing::Test {
 protected:
  void SetUp() override {
    crash_reporter::InitializeCrashKeysForTesting();
    browser_shutdown::ResetShutdownGlobalsForTesting();
  }

  void TearDown() override {
    browser_shutdown::ResetShutdownGlobalsForTesting();
    crash_reporter::ResetCrashKeysForTesting();
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(ApplicationLifetimeChromeosTest, NoUpdateIfDbusIsNotInitialized) {
  ASSERT_FALSE(ash::DBusThreadManager::IsInitialized());
  EXPECT_FALSE(UpdatePending());
}

TEST_F(ApplicationLifetimeChromeosTest,
       SetShutdownPhaseRecordsMetricsAndCrashKey) {
  base::HistogramTester histogram_tester;

  using browser_shutdown::ShutdownPhase;
  EXPECT_EQ(browser_shutdown::GetShutdownPhaseForTesting(),
            ShutdownPhase::kNotStarted);
  EXPECT_EQ(crash_reporter::GetCrashKeyValue("cros-shutdown-phase"), "");
  EXPECT_EQ(crash_reporter::GetCrashKeyValue("cros-shutdown-elapsed-ms"), "");

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
    const std::string elapsed_ms =
        crash_reporter::GetCrashKeyValue("cros-shutdown-elapsed-ms");
    EXPECT_FALSE(elapsed_ms.empty());
    if (step.phase == ShutdownPhase::kClosingBrowsers) {
      EXPECT_EQ(elapsed_ms, "0");
    } else {
      EXPECT_NE(elapsed_ms, "0");
    }
    if (step.completed_histogram) {
      histogram_tester.ExpectTotalCount(step.completed_histogram, 1);
    }
    task_environment_.FastForwardBy(base::Milliseconds(10));
  }

  browser_shutdown::ResetShutdownGlobalsForTesting();
  EXPECT_EQ(browser_shutdown::GetShutdownPhaseForTesting(),
            ShutdownPhase::kNotStarted);
  EXPECT_EQ(crash_reporter::GetCrashKeyValue("cros-shutdown-phase"), "");
  EXPECT_EQ(crash_reporter::GetCrashKeyValue("cros-shutdown-elapsed-ms"), "");
}

TEST_F(ApplicationLifetimeChromeosTest,
       SetShutdownTriggerPreservesFirstTrigger) {
  using browser_shutdown::ShutdownTrigger;
  EXPECT_EQ(browser_shutdown::GetShutdownTriggerForTesting(),
            ShutdownTrigger::kNotSet);
  EXPECT_EQ(crash_reporter::GetCrashKeyValue("cros-shutdown-trigger"), "");

  browser_shutdown::SetShutdownTrigger(ShutdownTrigger::kLogout);
  EXPECT_EQ(browser_shutdown::GetShutdownTriggerForTesting(),
            ShutdownTrigger::kLogout);
  EXPECT_EQ(crash_reporter::GetCrashKeyValue("cros-shutdown-trigger"),
            "logout");

  // Follow-up SIGTERM during logout must not overwrite the logout trigger.
  browser_shutdown::SetShutdownTrigger(ShutdownTrigger::kSigterm);
  EXPECT_EQ(browser_shutdown::GetShutdownTriggerForTesting(),
            ShutdownTrigger::kLogout);
  EXPECT_EQ(crash_reporter::GetCrashKeyValue("cros-shutdown-trigger"),
            "logout");

  browser_shutdown::ResetShutdownGlobalsForTesting();
  EXPECT_EQ(browser_shutdown::GetShutdownTriggerForTesting(),
            ShutdownTrigger::kNotSet);
  EXPECT_EQ(crash_reporter::GetCrashKeyValue("cros-shutdown-trigger"), "");

  browser_shutdown::SetShutdownTrigger(ShutdownTrigger::kRestartFlags);
  EXPECT_EQ(browser_shutdown::GetShutdownTriggerForTesting(),
            ShutdownTrigger::kRestartFlags);
  EXPECT_EQ(crash_reporter::GetCrashKeyValue("cros-shutdown-trigger"),
            "restart-flags");

  browser_shutdown::ResetShutdownGlobalsForTesting();
  browser_shutdown::SetShutdownTrigger(ShutdownTrigger::kSigterm);
  EXPECT_EQ(browser_shutdown::GetShutdownTriggerForTesting(),
            ShutdownTrigger::kSigterm);
  EXPECT_EQ(crash_reporter::GetCrashKeyValue("cros-shutdown-trigger"),
            "sigterm");
}

TEST_F(ApplicationLifetimeChromeosTest,
       OnShutdownStartingRecordsTriggerAndCounts) {
  base::HistogramTester histogram_tester;

  browser_shutdown::OnShutdownStarting(
      browser_shutdown::ShutdownType::kEndSession);
  EXPECT_EQ(browser_shutdown::GetShutdownTriggerForTesting(),
            browser_shutdown::ShutdownTrigger::kOther);
  EXPECT_EQ(crash_reporter::GetCrashKeyValue("cros-shutdown-trigger"), "other");
  histogram_tester.ExpectUniqueSample("Shutdown.ChromeOS.TabsAtShutdown", 0, 1);
  histogram_tester.ExpectUniqueSample("Shutdown.ChromeOS.RenderersAtShutdown",
                                      0, 1);
}

}  // namespace chrome
