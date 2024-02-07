// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/controls/contextual_tooltip.h"

#include "ash/constants/ash_features.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/json/values_util.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "ui/aura/window.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace contextual_tooltip {

class ContextualTooltipTest : public AshTestBase,
                              public testing::WithParamInterface<bool> {
 public:
  ContextualTooltipTest() {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kHideShelfControlsInTabletMode);

    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kHideShelfControlsInTabletMode);
    }
  }
  ~ContextualTooltipTest() override = default;

  base::SimpleTestClock* clock() { return &test_clock_; }

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    contextual_tooltip::OverrideClockForTesting(&test_clock_);
    test_clock_.Advance(base::Seconds(360));
  }
  void TearDown() override {
    contextual_tooltip::ClearClockOverrideForTesting();
    AshTestBase::TearDown();
  }

  PrefService* GetPrefService() {
    return Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::SimpleTestClock test_clock_;
};

using ContextualTooltipDisabledTest = ContextualTooltipTest;

INSTANTIATE_TEST_SUITE_P(All,
                         ContextualTooltipDisabledTest,
                         testing::Values(false));
INSTANTIATE_TEST_SUITE_P(All, ContextualTooltipTest, testing::Values(true));

// Checks that nudges are not shown when the feature flag is disabled.
TEST_P(ContextualTooltipDisabledTest, FeatureFlagDisabled) {
  EXPECT_FALSE(contextual_tooltip::ShouldShowNudge(
      GetPrefService(), TooltipType::kInAppToHome, nullptr));
}

TEST_P(ContextualTooltipTest, ShouldShowPersistentDragHandleNudge) {
  base::TimeDelta recheck_delay;
  EXPECT_TRUE(contextual_tooltip::ShouldShowNudge(
      GetPrefService(), TooltipType::kInAppToHome, &recheck_delay));
  EXPECT_TRUE(recheck_delay.is_zero());
  EXPECT_TRUE(contextual_tooltip::GetNudgeTimeout(GetPrefService(),
                                                  TooltipType::kInAppToHome)
                  .is_zero());
}

// Checks that drag handle nudge has a timeout if it is not the first time it is
// being shown.
TEST_P(ContextualTooltipTest, NonPersistentDragHandleNudgeTimeout) {
  for (int shown_count = 1;
       shown_count < contextual_tooltip::kNotificationLimit; shown_count++) {
    contextual_tooltip::HandleNudgeShown(GetPrefService(),
                                         TooltipType::kInAppToHome);
    clock()->Advance(contextual_tooltip::kMinInterval);
    EXPECT_TRUE(contextual_tooltip::ShouldShowNudge(
        GetPrefService(), TooltipType::kInAppToHome, nullptr));
    EXPECT_EQ(contextual_tooltip::GetNudgeTimeout(GetPrefService(),
                                                  TooltipType::kInAppToHome),
              contextual_tooltip::kNudgeShowDuration);
  }
}

// Checks that drag handle nudge should be shown after kMinInterval has passed
// since the last time it was shown but not before the time interval has passed.
TEST_P(ContextualTooltipTest, ShouldShowTimedDragHandleNudge) {
  contextual_tooltip::HandleNudgeShown(GetPrefService(),
                                       TooltipType::kInAppToHome);
  base::TimeDelta recheck_delay;
  for (int shown_count = 1;
       shown_count < contextual_tooltip::kNotificationLimit; shown_count++) {
    EXPECT_FALSE(contextual_tooltip::ShouldShowNudge(
        GetPrefService(), TooltipType::kInAppToHome, &recheck_delay));
    EXPECT_EQ(contextual_tooltip::kMinInterval, recheck_delay);
    clock()->Advance(contextual_tooltip::kMinInterval / 2);
    EXPECT_FALSE(contextual_tooltip::ShouldShowNudge(
        GetPrefService(), TooltipType::kInAppToHome, &recheck_delay));
    EXPECT_EQ(
        contextual_tooltip::kMinInterval - contextual_tooltip::kMinInterval / 2,
        recheck_delay);
    clock()->Advance(contextual_tooltip::kMinInterval / 2);
    EXPECT_TRUE(contextual_tooltip::ShouldShowNudge(
        GetPrefService(), TooltipType::kInAppToHome, nullptr));
    contextual_tooltip::HandleNudgeShown(GetPrefService(),
                                         TooltipType::kInAppToHome);
  }
  clock()->Advance(contextual_tooltip::kMinInterval);
  EXPECT_FALSE(contextual_tooltip::ShouldShowNudge(
      GetPrefService(), TooltipType::kInAppToHome, &recheck_delay));
  EXPECT_TRUE(recheck_delay.is_zero());

  EXPECT_EQ(contextual_tooltip::GetNudgeTimeout(GetPrefService(),
                                                TooltipType::kInAppToHome),
            contextual_tooltip::kNudgeShowDuration);
}

// Tests that if the user has successfully performed the gesture for at least
// |kSuccessLimit|, the corresponding nudge should not be shown.
TEST_P(ContextualTooltipTest, ShouldNotShowNudgeAfterSuccessLimit) {
  EXPECT_TRUE(contextual_tooltip::ShouldShowNudge(
      GetPrefService(), TooltipType::kInAppToHome, nullptr));
  for (int success_count = 0;
       success_count < contextual_tooltip::kSuccessLimitInAppToHome;
       success_count++) {
    contextual_tooltip::HandleGesturePerformed(GetPrefService(),
                                               TooltipType::kInAppToHome);
  }

  base::TimeDelta recheck_delay;
  EXPECT_FALSE(contextual_tooltip::ShouldShowNudge(
      GetPrefService(), TooltipType::kInAppToHome, &recheck_delay));
  EXPECT_TRUE(recheck_delay.is_zero());
}

// Should not show back gesture nudge if drag handle nudge is expected to be
// shown.
TEST_P(ContextualTooltipTest,
       DoNotShowBackGestureNudgeIfDragHandleNudgeIsExpected) {
  EXPECT_TRUE(contextual_tooltip::ShouldShowNudge(
      GetPrefService(), TooltipType::kInAppToHome, nullptr));

  // The drag handle nudge is expected to show, so back gesture nudge should not
  // be shown at the same time.
  base::TimeDelta recheck_delay;
  EXPECT_FALSE(contextual_tooltip::ShouldShowNudge(
      GetPrefService(), TooltipType::kBackGesture, &recheck_delay));
  EXPECT_EQ(contextual_tooltip::kMinIntervalBetweenBackAndDragHandleNudge,
            recheck_delay);

  // After the nudge is shown, back gesture should remain hidden until
  // sufficient amount of time passes.
  contextual_tooltip::HandleNudgeShown(GetPrefService(),
                                       TooltipType::kInAppToHome);
  EXPECT_FALSE(contextual_tooltip::ShouldShowNudge(
      GetPrefService(), TooltipType::kBackGesture, &recheck_delay));

  EXPECT_EQ(contextual_tooltip::kMinIntervalBetweenBackAndDragHandleNudge,
            recheck_delay);

  clock()->Advance(
      contextual_tooltip::kMinIntervalBetweenBackAndDragHandleNudge / 2);
  EXPECT_FALSE(contextual_tooltip::ShouldShowNudge(
      GetPrefService(), TooltipType::kBackGesture, &recheck_delay));
  EXPECT_EQ(
      contextual_tooltip::kMinIntervalBetweenBackAndDragHandleNudge -
          contextual_tooltip::kMinIntervalBetweenBackAndDragHandleNudge / 2,
      recheck_delay);

  clock()->Advance(recheck_delay);
  EXPECT_TRUE(contextual_tooltip::ShouldShowNudge(
      GetPrefService(), TooltipType::kBackGesture, nullptr));

  // After the drag handle becomes eligible to show again, the back gesture
  // should be disabled.
  clock()->Advance(contextual_tooltip::kMinInterval);
  EXPECT_FALSE(contextual_tooltip::ShouldShowNudge(
      GetPrefService(), TooltipType::kBackGesture, &recheck_delay));
  EXPECT_EQ(contextual_tooltip::kMinIntervalBetweenBackAndDragHandleNudge,
            recheck_delay);
}

// Tests that back gesture is allowed if the shelf is hidden, even if drag
// handle would normally be available.
TEST_P(ContextualTooltipTest, AllowBackGestureForHiddenShelf) {
  EXPECT_TRUE(contextual_tooltip::ShouldShowNudge(
      GetPrefService(), TooltipType::kInAppToHome, nullptr));

  // The drag handle nudge is expected to show, so back gesture nudge should not
  // be shown at the same time.
  EXPECT_FALSE(contextual_tooltip::ShouldShowNudge(
      GetPrefService(), TooltipType::kBackGesture, nullptr));

  // If drag handle nudge is disabled because the shelf is hidden, the back
  // gesture nudge should be allowed.
  contextual_tooltip::SetDragHandleNudgeDisabledForHiddenShelf(true);
  EXPECT_FALSE(contextual_tooltip::ShouldShowNudge(
      GetPrefService(), TooltipType::kInAppToHome, nullptr));
  EXPECT_TRUE(contextual_tooltip::ShouldShowNudge(
      GetPrefService(), TooltipType::kBackGesture, nullptr));

  // Disallow back gesture nudge if the shelf becomes visible.
  contextual_tooltip::SetDragHandleNudgeDisabledForHiddenShelf(false);
  EXPECT_TRUE(contextual_tooltip::ShouldShowNudge(
      GetPrefService(), TooltipType::kInAppToHome, nullptr));
  EXPECT_FALSE(contextual_tooltip::ShouldShowNudge(
      GetPrefService(), TooltipType::kBackGesture, nullptr));
}

// Tests that the drag handle nudge should not be shown while back gesture is
// showing, or soon after it's been shown.
TEST_P(ContextualTooltipTest,
       DoNotShowDragHandleNudgeIfBackGestureNudgeIsShown) {
  EXPECT_TRUE(contextual_tooltip::ShouldShowNudge(
      GetPrefService(), TooltipType::kInAppToHome, nullptr));

  // Drag handle nudge not allowed if back gesture is showing.
  contextual_tooltip::SetDragHandleNudgeDisabledForHiddenShelf(true);
  contextual_tooltip::SetBackGestureNudgeShowing(true);
  contextual_tooltip::SetDragHandleNudgeDisabledForHiddenShelf(false);

  base::TimeDelta recheck_delay;
  EXPECT_FALSE(contextual_tooltip::ShouldShowNudge(
      GetPrefService(), TooltipType::kInAppToHome, &recheck_delay));
  EXPECT_EQ(contextual_tooltip::kMinIntervalBetweenBackAndDragHandleNudge,
            recheck_delay);

  // Allow drag handle only if sufficient amount of time passes since showing
  // the back gesture nudge.
  contextual_tooltip::SetBackGestureNudgeShowing(false);
  contextual_tooltip::HandleNudgeShown(GetPrefService(),
                                       TooltipType::kBackGesture);

  recheck_delay = base::TimeDelta();
  EXPECT_FALSE(contextual_tooltip::ShouldShowNudge(
      GetPrefService(), TooltipType::kInAppToHome, &recheck_delay));

  EXPECT_EQ(contextual_tooltip::kMinIntervalBetweenBackAndDragHandleNudge,
            recheck_delay);

  clock()->Advance(
      contextual_tooltip::kMinIntervalBetweenBackAndDragHandleNudge / 2);
  EXPECT_FALSE(contextual_tooltip::ShouldShowNudge(
      GetPrefService(), TooltipType::kInAppToHome, &recheck_delay));

  clock()->Advance(recheck_delay);
  EXPECT_TRUE(contextual_tooltip::ShouldShowNudge(
      GetPrefService(), TooltipType::kInAppToHome, nullptr));
}

}  // namespace contextual_tooltip

}  // namespace ash
