// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/nudge_cap_tracker.h"

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace contextual_cueing {

class ContextualCueingNudgeCapTracker : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(ContextualCueingNudgeCapTracker, NoRestriction) {
  NudgeCapTracker tracker(0, base::TimeDelta());
  EXPECT_TRUE(tracker.CanShowNudge());
  task_environment_.FastForwardBy(base::Minutes(1));

  tracker.CueingNudgeShown();
  EXPECT_TRUE(tracker.CanShowNudge());

  tracker.CueingNudgeShown();
  EXPECT_TRUE(tracker.CanShowNudge());
}

TEST_F(ContextualCueingNudgeCapTracker, Basic) {
  NudgeCapTracker tracker(3, base::Hours(24));

  for (int i = 0; i < 3; i++) {
    EXPECT_TRUE(tracker.CanShowNudge());
    task_environment_.FastForwardBy(base::Minutes(1));
    tracker.CueingNudgeShown();
  }
  EXPECT_FALSE(tracker.CanShowNudge());

  task_environment_.FastForwardBy(base::Hours(25));
  EXPECT_TRUE(tracker.CanShowNudge());
}

TEST_F(ContextualCueingNudgeCapTracker, GetMostRecentTime) {
  NudgeCapTracker tracker(3, base::Hours(24));
  EXPECT_EQ(tracker.GetMostRecentNudgeTime(), std::nullopt);

  tracker.CueingNudgeShown();
  task_environment_.FastForwardBy(base::Minutes(1));
  tracker.CueingNudgeShown();
  task_environment_.FastForwardBy(base::Minutes(1));
  tracker.CueingNudgeShown();
  EXPECT_EQ(tracker.GetMostRecentNudgeTime(), base::TimeTicks::Now());
}

}  // namespace contextual_cueing
