// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "chrome/browser/task_manager/test_task_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace task_manager {

namespace {

// Defines a concrete observer that will be used for testing.
class TestObserver : public TaskManagerObserver {
 public:
  TestObserver(base::TimeDelta refresh_time, int64_t resources_flags)
      : TaskManagerObserver(refresh_time, resources_flags) {}

  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;
  ~TestObserver() override = default;

  // task_manager::TaskManagerObserver:
  void OnTaskAdded(TaskId id) override {}
  void OnTaskToBeRemoved(TaskId id) override {}
  void OnTasksRefreshed(const TaskIdList& task_ids) override {}
};

// Defines a test to validate the behavior of the task manager in response to
// adding and removing different kind of observers.
class TaskManagerObserverTest : public testing::Test {
 public:
  TaskManagerObserverTest() = default;
  TaskManagerObserverTest(const TaskManagerObserverTest&) = delete;
  TaskManagerObserverTest& operator=(const TaskManagerObserverTest&) = delete;
  ~TaskManagerObserverTest() override = default;

  TestTaskManager& task_manager() { return task_manager_; }

 private:
  TestTaskManager task_manager_;
};

}  // namespace

// Validates that the minimum refresh time to be requested is one second. Also
// validates the desired resource flags.
TEST_F(TaskManagerObserverTest, Basic) {
  base::TimeDelta refresh_time1 = base::Seconds(2);
  base::TimeDelta refresh_time2 = base::Milliseconds(999);
  int64_t flags1 = RefreshType::REFRESH_TYPE_CPU |
                   RefreshType::REFRESH_TYPE_WEBCACHE_STATS |
                   RefreshType::REFRESH_TYPE_HANDLES;
  int64_t flags2 = RefreshType::REFRESH_TYPE_MEMORY_FOOTPRINT |
                   RefreshType::REFRESH_TYPE_NACL;

  TestObserver observer1(refresh_time1, flags1);
  TestObserver observer2(refresh_time2, flags2);

  EXPECT_EQ(refresh_time1, observer1.desired_refresh_time());
  EXPECT_EQ(base::Seconds(1), observer2.desired_refresh_time());
  EXPECT_EQ(flags1, observer1.desired_resources_flags());
  EXPECT_EQ(flags2, observer2.desired_resources_flags());
}

// Validates the behavior of the task manager in response to adding and
// removing observers.
TEST_F(TaskManagerObserverTest, TaskManagerResponseToObservers) {
  EXPECT_EQ(base::TimeDelta::Max(), task_manager().GetRefreshTime());
  EXPECT_EQ(0, task_manager().GetEnabledFlags());

  // Add a bunch of observers and make sure the task manager responds correctly.
  base::TimeDelta refresh_time1 = base::Seconds(3);
  base::TimeDelta refresh_time2 = base::Seconds(10);
  base::TimeDelta refresh_time3 = base::Seconds(3);
  base::TimeDelta refresh_time4 = base::Seconds(2);
  int64_t flags1 = RefreshType::REFRESH_TYPE_CPU |
                   RefreshType::REFRESH_TYPE_WEBCACHE_STATS |
                   RefreshType::REFRESH_TYPE_HANDLES;
  int64_t flags2 = RefreshType::REFRESH_TYPE_MEMORY_FOOTPRINT |
                   RefreshType::REFRESH_TYPE_NACL;
  int64_t flags3 = RefreshType::REFRESH_TYPE_MEMORY_FOOTPRINT |
                   RefreshType::REFRESH_TYPE_CPU;
  int64_t flags4 = RefreshType::REFRESH_TYPE_GPU_MEMORY;

  TestObserver observer1(refresh_time1, flags1);
  TestObserver observer2(refresh_time2, flags2);
  TestObserver observer3(refresh_time3, flags3);
  TestObserver observer4(refresh_time4, flags4);

  task_manager().AddObserver(&observer1);
  task_manager().AddObserver(&observer2);
  task_manager().AddObserver(&observer3);
  task_manager().AddObserver(&observer4);

  EXPECT_EQ(refresh_time4, task_manager().GetRefreshTime());
  EXPECT_EQ(flags1 | flags2 | flags3 | flags4,
            task_manager().GetEnabledFlags());

  // Removing observers should also reflect on the refresh time and resource
  // flags.
  task_manager().RemoveObserver(&observer4);
  EXPECT_EQ(refresh_time3, task_manager().GetRefreshTime());
  EXPECT_EQ(flags1 | flags2 | flags3, task_manager().GetEnabledFlags());
  task_manager().RemoveObserver(&observer3);
  EXPECT_EQ(refresh_time1, task_manager().GetRefreshTime());
  EXPECT_EQ(flags1 | flags2, task_manager().GetEnabledFlags());
  task_manager().RemoveObserver(&observer2);
  EXPECT_EQ(refresh_time1, task_manager().GetRefreshTime());
  EXPECT_EQ(flags1, task_manager().GetEnabledFlags());
  task_manager().RemoveObserver(&observer1);
  EXPECT_EQ(base::TimeDelta::Max(), task_manager().GetRefreshTime());
  EXPECT_EQ(0, task_manager().GetEnabledFlags());
}

}  // namespace task_manager
