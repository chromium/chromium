// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/launchpad/tasks_store.h"

#include <cstddef>
#include <utility>
#include <vector>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/launchpad/features.h"
#include "chrome/browser/launchpad/launchpad_task.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace launchpad {
namespace {

class TasksStoreTest : public ::testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
  TasksStore store_;
};

TEST_F(TasksStoreTest, AddAllTasksAndGetTasks) {
  base::test::TestFuture<std::vector<LaunchpadTask>> get_future;
  store_.GetAllTasks(get_future.GetCallback());
  EXPECT_TRUE(get_future.Get().empty());

  LaunchpadTask task1;
  task1.id = "task_1";
  task1.title = "Task 1";
  task1.source_references = {GmailReference{
      .url = GURL("https://mail.google.com/123"), .subject = "Test Subject"}};
  task1.actionable_url = GURL("https://example.com/checkin");

  LaunchpadTask task2;
  task2.id = "task_2";
  task2.title = "Task 2";

  const std::vector<LaunchpadTask> tasks_to_add = {task1, task2};
  base::test::TestFuture<bool> add_future;
  store_.AddAllTasks(tasks_to_add, add_future.GetCallback());
  EXPECT_TRUE(add_future.Get());

  base::test::TestFuture<std::vector<LaunchpadTask>> get_future2;
  store_.GetAllTasks(get_future2.GetCallback());
  auto tasks = get_future2.Get();
  ASSERT_EQ(tasks.size(), 2u);
  EXPECT_EQ(tasks[0].id, "task_1");
  EXPECT_EQ(tasks[0].title, "Task 1");
  EXPECT_EQ(tasks[0].status, LaunchpadTask::Status::kActive);
  ASSERT_EQ(tasks[0].source_references.size(), 1u);
  EXPECT_EQ(tasks[0].actionable_url, GURL("https://example.com/checkin"));
  EXPECT_EQ(tasks[1].id, "task_2");
  EXPECT_EQ(tasks[1].title, "Task 2");
}

TEST_F(TasksStoreTest, AddAllTasksSkipsEmptyId) {
  LaunchpadTask empty_id_task;
  empty_id_task.title = "Missing ID";

  LaunchpadTask valid_task;
  valid_task.id = "task_1";
  valid_task.title = "Valid Task";

  const std::vector<LaunchpadTask> tasks_to_add = {empty_id_task, valid_task};
  base::test::TestFuture<bool> add_future;
  store_.AddAllTasks(tasks_to_add, add_future.GetCallback());
  EXPECT_TRUE(add_future.Get());

  base::test::TestFuture<std::vector<LaunchpadTask>> get_future;
  store_.GetAllTasks(get_future.GetCallback());
  auto tasks = get_future.Get();
  ASSERT_EQ(tasks.size(), 1u);
  EXPECT_EQ(tasks[0].id, "task_1");
  EXPECT_EQ(tasks[0].title, "Valid Task");
}

TEST_F(TasksStoreTest, AddAllTasksReplacesExistingTasks) {
  LaunchpadTask task1;
  task1.id = "1";
  task1.title = "Original Title";
  task1.description = "Original Description";
  task1.importance_score = 0.9f;

  const std::vector<LaunchpadTask> initial_tasks = {task1};
  base::test::TestFuture<bool> add_future;
  store_.AddAllTasks(initial_tasks, add_future.GetCallback());
  EXPECT_TRUE(add_future.Get());

  LaunchpadTask task1_new;
  task1_new.id = "1";
  task1_new.title = "Replaced Title";

  const std::vector<LaunchpadTask> replacement = {task1_new};
  base::test::TestFuture<bool> replace_future;
  store_.AddAllTasks(replacement, replace_future.GetCallback());
  EXPECT_TRUE(replace_future.Get());

  base::test::TestFuture<std::vector<LaunchpadTask>> get_future;
  store_.GetAllTasks(get_future.GetCallback());
  auto tasks = get_future.Get();
  ASSERT_EQ(tasks.size(), 1u);
  EXPECT_EQ(tasks[0].id, "1");
  EXPECT_EQ(tasks[0].title, "Replaced Title");
  EXPECT_EQ(tasks[0].description, "");
  EXPECT_FLOAT_EQ(tasks[0].importance_score, 0.0f);
}

TEST_F(TasksStoreTest, EvictsLeastRecentlyUsedWhenExceedingMaxCapacity) {
  const size_t max_capacity = features::kMaxTasksCacheSize.Get();
  std::vector<LaunchpadTask> tasks_to_add;
  tasks_to_add.reserve(max_capacity + 1);
  for (size_t i = 0; i < max_capacity + 1; ++i) {
    LaunchpadTask task;
    task.id = base::StrCat({"task_", base::NumberToString(i)});
    tasks_to_add.push_back(std::move(task));
  }

  base::test::TestFuture<bool> add_future;
  store_.AddAllTasks(tasks_to_add, add_future.GetCallback());
  EXPECT_TRUE(add_future.Get());

  base::test::TestFuture<std::vector<LaunchpadTask>> get_future;
  store_.GetAllTasks(get_future.GetCallback());
  auto tasks = get_future.Get();
  ASSERT_EQ(tasks.size(), max_capacity);
  EXPECT_EQ(tasks.front().id, "task_0");
  EXPECT_EQ(tasks.back().id,
            base::StrCat({"task_", base::NumberToString(max_capacity - 1)}));
}

TEST_F(TasksStoreTest, RespectsCustomMaxTasksCacheSizeFeatureParam) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kLaunchpad, {{features::kMaxTasksCacheSize.name, "2"}});
  TasksStore custom_store;

  LaunchpadTask task1;
  task1.id = "task_1";
  LaunchpadTask task2;
  task2.id = "task_2";
  LaunchpadTask task3;
  task3.id = "task_3";

  const std::vector<LaunchpadTask> tasks_to_add = {task1, task2, task3};
  base::test::TestFuture<bool> add_future;
  custom_store.AddAllTasks(tasks_to_add, add_future.GetCallback());
  EXPECT_TRUE(add_future.Get());

  base::test::TestFuture<std::vector<LaunchpadTask>> get_future;
  custom_store.GetAllTasks(get_future.GetCallback());
  auto tasks = get_future.Get();
  ASSERT_EQ(tasks.size(), 2u);
  EXPECT_EQ(tasks[0].id, "task_1");
  EXPECT_EQ(tasks[1].id, "task_2");
}

TEST_F(TasksStoreTest, ClearStore) {
  LaunchpadTask task1;
  task1.id = "1";
  LaunchpadTask task2;
  task2.id = "2";

  const std::vector<LaunchpadTask> tasks_to_add = {task1, task2};
  base::test::TestFuture<bool> add_future;
  store_.AddAllTasks(tasks_to_add, add_future.GetCallback());
  EXPECT_TRUE(add_future.Get());

  base::test::TestFuture<void> clear_future;
  store_.Clear(clear_future.GetCallback());
  EXPECT_TRUE(clear_future.Wait());

  base::test::TestFuture<std::vector<LaunchpadTask>> get_future;
  store_.GetAllTasks(get_future.GetCallback());
  EXPECT_TRUE(get_future.Get().empty());
}

}  // namespace
}  // namespace launchpad
