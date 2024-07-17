// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/birch/birch_privacy_nudge_controller.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/toast/anchored_nudge_manager_impl.h"
#include "ash/test/ash_test_base.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "components/prefs/pref_service.h"

namespace ash {

namespace {

constexpr char kNudgeId[] = "BirchPrivacyId";

bool IsNudgeShown() {
  return Shell::Get()->anchored_nudge_manager()->IsNudgeShown(kNudgeId);
}

class BirchPrivacyNudgeControllerTest : public AshTestBase {
 public:
  BirchPrivacyNudgeControllerTest()
      : nudge_controller_(std::make_unique<BirchPrivacyNudgeController>()) {}

  BirchPrivacyNudgeController* nudge_controller() {
    return nudge_controller_.get();
  }

  AnchoredNudgeManagerImpl* nudge_manager() {
    return Shell::Get()->anchored_nudge_manager();
  }

  static base::Time GetTestTime() { return test_time_; }
  static void SetTestTime(base::Time test_time) { test_time_ = test_time; }

 private:
  std::unique_ptr<BirchPrivacyNudgeController> nudge_controller_;
  static base::Time test_time_;
};

// static
base::Time BirchPrivacyNudgeControllerTest::test_time_;

// Tests that the nudge shows by default.
TEST_F(BirchPrivacyNudgeControllerTest, NudgeShows_ByDefault) {
  EXPECT_FALSE(IsNudgeShown());
  nudge_controller()->MaybeShowNudge(nullptr);
  EXPECT_TRUE(IsNudgeShown());
}

// Tests that the nudge does not show if the birch context menu has been
// opened.
TEST_F(BirchPrivacyNudgeControllerTest, NudgeDoesNotShow_WhenMenuWasOpened) {
  BirchPrivacyNudgeController::DidShowContextMenu();

  EXPECT_FALSE(IsNudgeShown());
  nudge_controller()->MaybeShowNudge(nullptr);
  EXPECT_FALSE(IsNudgeShown());
}

// Tests that the nudge won't show if the time between shown threshold hasn't
// passed since it was last shown.
TEST_F(BirchPrivacyNudgeControllerTest, NudgeDoesNotShow_IfRecentlyShown) {
  SetTestTime(base::Time::Now());
  base::subtle::ScopedTimeClockOverrides clock_override(
      /*time_override=*/&BirchPrivacyNudgeControllerTest::GetTestTime,
      /*time_ticks_override=*/nullptr, /*thread_ticks_override=*/nullptr);

  // Show the nudge once and close it.
  EXPECT_FALSE(IsNudgeShown());
  nudge_controller()->MaybeShowNudge(nullptr);
  EXPECT_TRUE(IsNudgeShown());
  nudge_manager()->Cancel(kNudgeId);

  // Attempt showing the nudge again immediately. It should not show.
  nudge_controller()->MaybeShowNudge(nullptr);
  EXPECT_FALSE(IsNudgeShown());

  // Attempt showing the nudge after some time but before its threshold time has
  // fully passed. It should not show.
  SetTestTime(GetTestTime() + base::Hours(23));
  nudge_controller()->MaybeShowNudge(nullptr);
  EXPECT_FALSE(IsNudgeShown());

  // Attempt showing the nudge after its "time between shown" threshold has
  // passed. It should show.
  SetTestTime(GetTestTime() + base::Hours(24));
  nudge_controller()->MaybeShowNudge(nullptr);
  EXPECT_TRUE(IsNudgeShown());
}

TEST_F(BirchPrivacyNudgeControllerTest, NudgeDoesNotShow_IfMaxTimesShown) {
  SetTestTime(base::Time::Now());
  base::subtle::ScopedTimeClockOverrides clock_override(
      /*time_override=*/&BirchPrivacyNudgeControllerTest::GetTestTime,
      /*time_ticks_override=*/nullptr, /*thread_ticks_override=*/nullptr);

  // Show the nudge its max number of times.
  for (int i = 0; i < 3; i++) {
    nudge_controller()->MaybeShowNudge(nullptr);
    EXPECT_TRUE(IsNudgeShown());
    nudge_manager()->Cancel(kNudgeId);
    SetTestTime(GetTestTime() + base::Hours(24) + base::Minutes(5));
  }

  // Attempt showing the nudge once more. It should not show.
  nudge_controller()->MaybeShowNudge(nullptr);
  EXPECT_FALSE(IsNudgeShown());
}

}  // namespace
}  // namespace ash
