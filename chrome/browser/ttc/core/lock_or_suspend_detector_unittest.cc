// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/lock_or_suspend_detector.h"

#include <vector>

#include "base/functional/bind.h"
#include "base/test/power_monitor_test.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ttc {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

class LockOrSuspendDetectorTest : public testing::Test {
 protected:
  using Reason = LockOrSuspendDetector::Reason;

  base::RepeatingCallback<void(Reason)> RecordReasons() {
    return base::BindRepeating(
        [](std::vector<Reason>* reasons, Reason reason) {
          reasons->push_back(reason);
        },
        &reasons_);
  }

  base::test::TaskEnvironment task_environment_;
  base::test::ScopedPowerMonitorTestSource power_monitor_source_;
  std::vector<Reason> reasons_;
};

TEST_F(LockOrSuspendDetectorTest, SuspendRunsCallback) {
  LockOrSuspendDetector detector(RecordReasons());
  power_monitor_source_.GenerateSuspendEvent();
  EXPECT_THAT(reasons_, ElementsAre(Reason::kSuspend));
}

TEST_F(LockOrSuspendDetectorTest, ScreenLockRunsCallback) {
  LockOrSuspendDetector detector(RecordReasons());
  detector.OnScreenLockChanged(true);
  EXPECT_THAT(reasons_, ElementsAre(Reason::kScreenLock));
}

TEST_F(LockOrSuspendDetectorTest, ScreenUnlockDoesNotRunCallback) {
  LockOrSuspendDetector detector(RecordReasons());
  detector.OnScreenLockChanged(false);
  EXPECT_THAT(reasons_, IsEmpty());
}

TEST_F(LockOrSuspendDetectorTest, SuspendAfterDestructionIsIgnored) {
  {
    LockOrSuspendDetector detector(RecordReasons());
  }
  power_monitor_source_.GenerateSuspendEvent();
  EXPECT_THAT(reasons_, IsEmpty());
}

}  // namespace
}  // namespace ttc
