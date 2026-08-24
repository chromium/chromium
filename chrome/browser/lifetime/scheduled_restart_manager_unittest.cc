// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/scheduled_restart_manager.h"

#include "base/time/time.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace scheduled_restart {

TEST(ScheduledRestartManagerTest, RegisterLocalStatePrefs) {
  TestingPrefServiceSimple prefs;
  ScheduledRestartManager::RegisterLocalStatePrefs(prefs.registry());

  EXPECT_NE(prefs.FindPreference(prefs::kScheduledRestartLastNudgeTime),
            nullptr);

  // Verify default values.
  EXPECT_EQ(prefs.GetTime(prefs::kScheduledRestartLastNudgeTime), base::Time());
}

TEST(ScheduledRestartManagerTest,
     RegisterLocalStatePrefs_TestingBrowserProcess) {
  PrefService* local_state = TestingBrowserProcess::GetGlobal()->local_state();
  ASSERT_NE(local_state, nullptr);

  EXPECT_NE(local_state->FindPreference(prefs::kScheduledRestartLastNudgeTime),
            nullptr);
  EXPECT_EQ(local_state->GetTime(prefs::kScheduledRestartLastNudgeTime),
            base::Time());
}

TEST(ScheduledRestartManagerTest, FeatureParamDefaults) {
  // Verify feature param default values.
  EXPECT_EQ(features::kScheduledRestartFirstNudgeDelay.Get(), base::Days(14));
  EXPECT_EQ(features::kScheduledRestartNudgeCooldown.Get(), base::Days(14));
  EXPECT_EQ(features::kScheduledRestartIdleThreshold.Get(), base::Seconds(300));
  EXPECT_EQ(features::kScheduledRestartLullWindows.Get(),
            "11:30-12:30,15:00-17:00");
}

}  // namespace scheduled_restart
