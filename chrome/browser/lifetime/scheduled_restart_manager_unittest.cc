// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/scheduled_restart_manager.h"

#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/config/linux/dbus/buildflags.h"
#include "chrome/browser/lifetime/restartability_monitor.h"
#include "chrome/browser/lifetime/scheduled_restart_test_utils.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_LINUX) && BUILDFLAG(USE_DBUS)
#include "components/dbus/thread_linux/dbus_thread_linux.h"
#endif

namespace scheduled_restart {

using smart_restart::ExtendedRestartabilityState;

class ScheduledRestartManagerTest : public testing::Test {
 public:
  ScheduledRestartManagerTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kScheduledRestart,
        {{"idle_threshold", "300s"},
         {"first_nudge_delay", "14d"},
         {"nudge_cooldown", "14d"},
         {"lull_windows", "11:30-12:30,15:00-17:00"}});
  }

  void TearDown() override {
#if BUILDFLAG(IS_LINUX) && BUILDFLAG(USE_DBUS)
    dbus_thread_linux::ShutdownOnDBusThreadAndBlock();
#endif
  }

  PrefService* local_state() {
    return TestingBrowserProcess::GetGlobal()->local_state();
  }

  // Advances the mock clock forward so that the local wall-clock time matches
  // `time_str` ("HH:MM" 24-hour format). Resets seconds and milliseconds to 0.
  // If the target time has already passed today in the local timezone, advances
  // to that time tomorrow.
  void AdvanceToLocalTime(std::string_view time_str) {
    std::vector<std::string_view> parts = base::SplitStringPiece(
        time_str, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    CHECK_EQ(2u, parts.size());
    int hour = 0;
    int minute = 0;
    CHECK(base::StringToInt(parts[0], &hour));
    CHECK(base::StringToInt(parts[1], &minute));
    CHECK_GE(hour, 0);
    CHECK_LE(hour, 23);
    CHECK_GE(minute, 0);
    CHECK_LE(minute, 59);

    base::Time now = base::Time::Now();
    base::Time::Exploded exploded = {};
    now.LocalExplode(&exploded);
    exploded.hour = hour;
    exploded.minute = minute;
    exploded.second = 0;
    exploded.millisecond = 0;
    base::Time target_time;
    CHECK(base::Time::FromLocalExploded(exploded, &target_time));
    if (target_time <= now) {
      target_time += base::Days(1);
    }
    task_environment_.AdvanceClock(target_time - now);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  FakeUpgradeDetector fake_upgrade_detector_;
};

TEST_F(ScheduledRestartManagerTest, RegisterLocalStatePrefs) {
  TestingPrefServiceSimple prefs;
  ScheduledRestartManager::RegisterLocalStatePrefs(prefs.registry());

  EXPECT_NE(prefs.FindPreference(prefs::kScheduledRestartLastNudgeTime),
            nullptr);

  // Verify default values.
  EXPECT_EQ(prefs.GetTime(prefs::kScheduledRestartLastNudgeTime), base::Time());
}

TEST_F(ScheduledRestartManagerTest,
       RegisterLocalStatePrefs_TestingBrowserProcess) {
  PrefService* local_state = TestingBrowserProcess::GetGlobal()->local_state();
  ASSERT_NE(local_state, nullptr);

  EXPECT_NE(local_state->FindPreference(prefs::kScheduledRestartLastNudgeTime),
            nullptr);
  EXPECT_EQ(local_state->GetTime(prefs::kScheduledRestartLastNudgeTime),
            base::Time());
}

TEST_F(ScheduledRestartManagerTest, FeatureParamDefaults) {
  // Verify feature param default values.
  EXPECT_EQ(features::kScheduledRestartFirstNudgeDelay.Get(), base::Days(14));
  EXPECT_EQ(features::kScheduledRestartNudgeCooldown.Get(), base::Days(14));
  EXPECT_EQ(features::kScheduledRestartIdleThreshold.Get(), base::Seconds(300));
  EXPECT_EQ(features::kScheduledRestartLullWindows.Get(),
            "11:30-12:30,15:00-17:00");
}

TEST_F(ScheduledRestartManagerTest, ScheduledRestartModeHelpers) {
  // Test member methods on a standalone manager instance.
  ScheduledRestartManager manager(*UpgradeDetector::GetInstance());
  EXPECT_FALSE(manager.is_scheduled());
  EXPECT_EQ(ScheduledRestartMode::kNone, manager.mode());

  // Setting the schedule sets mode to kOnIdle. Calling twice is a safe no-op.
  manager.ScheduleRestartOnIdle();
  EXPECT_TRUE(manager.is_scheduled());
  EXPECT_EQ(ScheduledRestartMode::kOnIdle, manager.mode());
  manager.ScheduleRestartOnIdle();
  EXPECT_TRUE(manager.is_scheduled());
  EXPECT_EQ(ScheduledRestartMode::kOnIdle, manager.mode());

  manager.CancelSchedule();
  EXPECT_FALSE(manager.is_scheduled());
  EXPECT_EQ(ScheduledRestartMode::kNone, manager.mode());

  // Cancelling when not scheduled is a safe no-op.
  manager.CancelSchedule();
  EXPECT_FALSE(manager.is_scheduled());
  EXPECT_EQ(ScheduledRestartMode::kNone, manager.mode());
}

TEST_F(ScheduledRestartManagerTest, AllowsScheduledRestart) {
  ExtendedRestartabilityState state;
  EXPECT_TRUE(ScheduledRestartManager::AllowsScheduledRestart(state));

  state.AddBlocker(ExtendedRestartabilityState::SmartRestartBlocker::kDownload);
  EXPECT_FALSE(ScheduledRestartManager::AllowsScheduledRestart(state));

  ExtendedRestartabilityState media_state;
  media_state.AddBlocker(
      ExtendedRestartabilityState::SmartRestartBlocker::kMedia);
  EXPECT_FALSE(ScheduledRestartManager::AllowsScheduledRestart(media_state));
}

TEST_F(ScheduledRestartManagerTest, IsInLullWindowMalformed) {
  const char* kMalformedInputs[] = {
      "invalid",   "12:00",     "25:00-12:00", "12:65-13:00", "-1:00-12:00",
      "",          ",,",        ":15",         "12:",         ":15-12:00",
      "12:00-:15", "12:-13:00", "12:00-",      "-12:00",
  };

  AdvanceToLocalTime("12:00");
  for (const char* input : kMalformedInputs) {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(features::kScheduledRestart,
                                                    {{"lull_windows", input}});
    ScheduledRestartManager manager(fake_upgrade_detector_);
    EXPECT_FALSE(manager.IsInLullWindow(base::Time::Now()))
        << "Expected false for malformed input: " << input;
  }
}

TEST_F(ScheduledRestartManagerTest, LullWindowMidnightWrap) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kScheduledRestart, {{"lull_windows", "22:00-06:00"}});
  ScheduledRestartManager manager(fake_upgrade_detector_);

  // 22:00 -> True
  AdvanceToLocalTime("22:00");
  EXPECT_TRUE(manager.IsInLullWindow(base::Time::Now()));
  // 23:59 -> True
  AdvanceToLocalTime("23:59");
  EXPECT_TRUE(manager.IsInLullWindow(base::Time::Now()));
  // 00:00 -> True
  AdvanceToLocalTime("00:00");
  EXPECT_TRUE(manager.IsInLullWindow(base::Time::Now()));
  // 05:59 -> True
  AdvanceToLocalTime("05:59");
  EXPECT_TRUE(manager.IsInLullWindow(base::Time::Now()));
  // 06:00 -> False
  AdvanceToLocalTime("06:00");
  EXPECT_FALSE(manager.IsInLullWindow(base::Time::Now()));
  // 12:00 -> False
  AdvanceToLocalTime("12:00");
  EXPECT_FALSE(manager.IsInLullWindow(base::Time::Now()));
}

TEST_F(ScheduledRestartManagerTest, IsInLullWindow) {
  ScheduledRestartManager manager(fake_upgrade_detector_);

  // Window 1: 11:30 - 12:30.
  AdvanceToLocalTime("11:30");
  EXPECT_TRUE(manager.IsInLullWindow(base::Time::Now()));

  AdvanceToLocalTime("12:00");
  EXPECT_TRUE(manager.IsInLullWindow(base::Time::Now()));

  AdvanceToLocalTime("12:30");
  EXPECT_FALSE(manager.IsInLullWindow(base::Time::Now()));

  // Outside lull windows: 14:00.
  AdvanceToLocalTime("14:00");
  EXPECT_FALSE(manager.IsInLullWindow(base::Time::Now()));

  // Window 2: 15:00 - 17:00.
  AdvanceToLocalTime("15:00");
  EXPECT_TRUE(manager.IsInLullWindow(base::Time::Now()));

  AdvanceToLocalTime("16:00");
  EXPECT_TRUE(manager.IsInLullWindow(base::Time::Now()));

  AdvanceToLocalTime("17:00");
  EXPECT_FALSE(manager.IsInLullWindow(base::Time::Now()));
}

TEST_F(ScheduledRestartManagerTest, ShouldShowNudge_UnmetFirstNudgeDelay) {
  AdvanceToLocalTime("12:00");
  ScheduledRestartManager manager(fake_upgrade_detector_);

  // No upgrade available returns false.
  EXPECT_FALSE(manager.ShouldShowNudge());

  // Upgrade available but upgrade detected time is null.
  fake_upgrade_detector_.SetUpgradeAvailable();
  fake_upgrade_detector_.set_upgrade_detected_time(base::Time());
  EXPECT_FALSE(manager.ShouldShowNudge());

  // Upgrade detected 5 days ago (less than 14d first nudge delay).
  fake_upgrade_detector_.set_upgrade_detected_time(base::Time::Now() -
                                                   base::Days(5));
  EXPECT_FALSE(manager.ShouldShowNudge());
}

TEST_F(ScheduledRestartManagerTest, ShouldShowNudge_BlockedByCooldown) {
  AdvanceToLocalTime("12:00");
  fake_upgrade_detector_.SetUpgradeAvailable();
  fake_upgrade_detector_.set_upgrade_detected_time(base::Time::Now() -
                                                   base::Days(20));

  ScheduledRestartManager manager(fake_upgrade_detector_);
  EXPECT_TRUE(manager.ShouldShowNudge());

  // Record nudge shown at current time.
  manager.RecordNudgeShown();
  EXPECT_FALSE(manager.ShouldShowNudge());

  // Advance 5 days (still within 14d cooldown).
  task_environment_.AdvanceClock(base::Days(5));
  EXPECT_FALSE(manager.ShouldShowNudge());

  // Advance another 10 days (15 days total since last nudge).
  task_environment_.AdvanceClock(base::Days(10));
  EXPECT_TRUE(manager.ShouldShowNudge());
}

TEST_F(ScheduledRestartManagerTest, ShouldShowNudge_AllowedToNudge) {
  AdvanceToLocalTime("12:00");
  fake_upgrade_detector_.SetUpgradeAvailable();
  fake_upgrade_detector_.set_upgrade_detected_time(base::Time::Now() -
                                                   base::Days(20));

  ScheduledRestartManager manager(fake_upgrade_detector_);
  // In lull window (12:00), past first nudge delay, no cooldown -> Allowed.
  EXPECT_TRUE(manager.ShouldShowNudge());

  // Advance 2 hours to 14:00 (outside lull window).
  task_environment_.AdvanceClock(base::Hours(2));
  EXPECT_FALSE(manager.ShouldShowNudge());
}

TEST_F(ScheduledRestartManagerTest,
       ShouldNotShowNudgeIfRestartAlreadyScheduled) {
  AdvanceToLocalTime("12:00");
  fake_upgrade_detector_.SetUpgradeAvailable();
  fake_upgrade_detector_.set_upgrade_detected_time(base::Time::Now() -
                                                   base::Days(20));

  ScheduledRestartManager manager(fake_upgrade_detector_);
  EXPECT_TRUE(manager.ShouldShowNudge());

  manager.ScheduleRestartOnIdle();
  EXPECT_TRUE(manager.is_scheduled());
  EXPECT_FALSE(manager.ShouldShowNudge());

  manager.CancelSchedule();
  EXPECT_FALSE(manager.is_scheduled());
  EXPECT_TRUE(manager.ShouldShowNudge());
}

TEST_F(ScheduledRestartManagerTest, RecordNudgeShown) {
  AdvanceToLocalTime("12:00");
  ScheduledRestartManager manager(fake_upgrade_detector_);

  base::Time expected_time = base::Time::Now();
  manager.RecordNudgeShown();

  EXPECT_EQ(expected_time,
            local_state()->GetTime(prefs::kScheduledRestartLastNudgeTime));
}

TEST_F(ScheduledRestartManagerTest, ScheduleChangedCallbacksNotified) {
  ScheduledRestartManager manager(fake_upgrade_detector_);
  int callback_count = 0;
  auto subscription = manager.AddScheduleChangedCallback(
      base::BindRepeating([](int* count) { (*count)++; }, &callback_count));

  manager.ScheduleRestartOnIdle();
  EXPECT_EQ(callback_count, 1);

  // Setting to same mode does not re-notify.
  manager.ScheduleRestartOnIdle();
  EXPECT_EQ(callback_count, 1);

  manager.CancelSchedule();
  EXPECT_EQ(callback_count, 2);

  // Setting again after cancel notifies.
  manager.ScheduleRestartOnIdle();
  EXPECT_EQ(callback_count, 3);

  // Cancelling when not scheduled does not re-notify.
  manager.CancelSchedule();
  EXPECT_EQ(callback_count, 4);
  manager.CancelSchedule();
  EXPECT_EQ(callback_count, 4);
}

}  // namespace scheduled_restart
