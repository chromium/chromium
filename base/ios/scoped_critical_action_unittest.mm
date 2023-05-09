// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ios/scoped_critical_action.h"

#include <memory>

#include "base/strings/string_piece.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace base {
namespace ios {
namespace {

class ScopedCriticalActionTest : public PlatformTest {
 protected:
  ScopedCriticalActionTest() {
    ScopedCriticalAction::ClearNumActiveBackgroundTasksForTest();
  }

  ~ScopedCriticalActionTest() override = default;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(ScopedCriticalActionTest, ShouldStartBackgroundTaskWhenConstructed) {
  ASSERT_EQ(0, ScopedCriticalAction::GetNumActiveBackgroundTasksForTest());

  ScopedCriticalAction scoped_critical_action("name");
  EXPECT_EQ(1, ScopedCriticalAction::GetNumActiveBackgroundTasksForTest());
}

TEST_F(ScopedCriticalActionTest, ShouldEndBackgroundTaskWhenDestructed) {
  ScopedCriticalAction::ClearNumActiveBackgroundTasksForTest();
  ASSERT_EQ(0, ScopedCriticalAction::GetNumActiveBackgroundTasksForTest());

  auto scoped_critical_action = std::make_unique<ScopedCriticalAction>("name");
  ASSERT_EQ(1, ScopedCriticalAction::GetNumActiveBackgroundTasksForTest());

  scoped_critical_action.reset();
  EXPECT_EQ(0, ScopedCriticalAction::GetNumActiveBackgroundTasksForTest());
}

TEST_F(ScopedCriticalActionTest, ShouldUseMultipleBackgroundTasks) {
  ScopedCriticalAction::ClearNumActiveBackgroundTasksForTest();
  ASSERT_EQ(0, ScopedCriticalAction::GetNumActiveBackgroundTasksForTest());

  auto scoped_critical_action1 =
      std::make_unique<ScopedCriticalAction>("name1");
  ASSERT_EQ(1, ScopedCriticalAction::GetNumActiveBackgroundTasksForTest());

  auto scoped_critical_action2 =
      std::make_unique<ScopedCriticalAction>("name2");
  EXPECT_EQ(2, ScopedCriticalAction::GetNumActiveBackgroundTasksForTest());

  scoped_critical_action1.reset();
  EXPECT_EQ(1, ScopedCriticalAction::GetNumActiveBackgroundTasksForTest());

  scoped_critical_action2.reset();
  EXPECT_EQ(0, ScopedCriticalAction::GetNumActiveBackgroundTasksForTest());
}

TEST_F(ScopedCriticalActionTest, ShouldReuseBackgroundTasksForSameName) {
  ScopedCriticalAction::ClearNumActiveBackgroundTasksForTest();
  ASSERT_EQ(0, ScopedCriticalAction::GetNumActiveBackgroundTasksForTest());

  auto scoped_critical_action1 = std::make_unique<ScopedCriticalAction>("name");
  ASSERT_EQ(1, ScopedCriticalAction::GetNumActiveBackgroundTasksForTest());

  auto scoped_critical_action2 = std::make_unique<ScopedCriticalAction>("name");
  EXPECT_EQ(1, ScopedCriticalAction::GetNumActiveBackgroundTasksForTest());

  scoped_critical_action1.reset();
  EXPECT_EQ(1, ScopedCriticalAction::GetNumActiveBackgroundTasksForTest());

  scoped_critical_action2.reset();
  EXPECT_EQ(0, ScopedCriticalAction::GetNumActiveBackgroundTasksForTest());
}

TEST_F(ScopedCriticalActionTest,
       ShouldNotReuseBackgroundTasksForSameNameIfTimeDifferenceLarge) {
  ScopedCriticalAction::ClearNumActiveBackgroundTasksForTest();
  ASSERT_EQ(0, ScopedCriticalAction::GetNumActiveBackgroundTasksForTest());

  auto scoped_critical_action1 = std::make_unique<ScopedCriticalAction>("name");
  ASSERT_EQ(1, ScopedCriticalAction::GetNumActiveBackgroundTasksForTest());

  // Mimic advancing time more than 3 seconds (kMaxTaskReuseDelay).
  task_environment_.FastForwardBy(base::Seconds(4));

  auto scoped_critical_action2 = std::make_unique<ScopedCriticalAction>("name");
  EXPECT_EQ(2, ScopedCriticalAction::GetNumActiveBackgroundTasksForTest());

  scoped_critical_action1.reset();
  EXPECT_EQ(1, ScopedCriticalAction::GetNumActiveBackgroundTasksForTest());

  scoped_critical_action2.reset();
  EXPECT_EQ(0, ScopedCriticalAction::GetNumActiveBackgroundTasksForTest());
}

TEST_F(ScopedCriticalActionTest,
       ShouldReuseBackgroundTasksForSameNameIfTimeDifferenceSmall) {
  ScopedCriticalAction::ClearNumActiveBackgroundTasksForTest();
  ASSERT_EQ(0, ScopedCriticalAction::GetNumActiveBackgroundTasksForTest());

  auto scoped_critical_action1 = std::make_unique<ScopedCriticalAction>("name");
  ASSERT_EQ(1, ScopedCriticalAction::GetNumActiveBackgroundTasksForTest());

  // Mimic advancing time less than 3 seconds (kMaxTaskReuseDelay).
  task_environment_.FastForwardBy(base::Seconds(2));

  auto scoped_critical_action2 = std::make_unique<ScopedCriticalAction>("name");
  EXPECT_EQ(1, ScopedCriticalAction::GetNumActiveBackgroundTasksForTest());

  scoped_critical_action1.reset();
  EXPECT_EQ(1, ScopedCriticalAction::GetNumActiveBackgroundTasksForTest());

  scoped_critical_action2.reset();
  EXPECT_EQ(0, ScopedCriticalAction::GetNumActiveBackgroundTasksForTest());
}

}  // namespace
}  // namespace ios
}  // namespace base
