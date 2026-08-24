// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/scheduled_restart_manager.h"

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/lifetime/restartability_monitor.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace scheduled_restart {

using smart_restart::ExtendedRestartabilityState;

class ScheduledRestartManagerTest : public testing::Test {
 public:
  ScheduledRestartManagerTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kScheduledRestart, {{"idle_threshold", "300s"}});
  }

 protected:
  void SetUp() override { ASSERT_TRUE(profile_manager_.SetUp()); }
  void TearDown() override { profile_manager_.DeleteAllTestingProfiles(); }

  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
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

}  // namespace scheduled_restart
