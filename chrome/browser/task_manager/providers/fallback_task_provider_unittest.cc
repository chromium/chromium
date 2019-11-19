// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "chrome/browser/task_manager/providers/fallback_task_provider.h"
#include "chrome/browser/task_manager/providers/task.h"
#include "chrome/browser/task_manager/task_manager_observer.h"
#include "content/public/common/process_type.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace task_manager {

class FakeTask : public Task {
 public:
  FakeTask(base::ProcessId process_id, Type type, const std::string& title)
      : Task(base::ASCIIToUTF16(title),
             "FakeTask",
             nullptr,
             base::kNullProcessHandle,
             process_id),
        type_(type) {}

  Type GetType() const override { return type_; }

  int GetChildProcessUniqueID() const override { return 0; }

  const Task* GetParentTask() const override { return nullptr; }

  SessionID GetTabId() const override { return SessionID::InvalidValue(); }

 private:
  Type type_;

  DISALLOW_COPY_AND_ASSIGN(FakeTask);
};

class FakeTaskProvider : public TaskProvider {
 public:
  FakeTaskProvider() {}
  ~FakeTaskProvider() override {}
  Task* GetTaskOfUrlRequest(int child_id, int route_id) override {
    return nullptr;
  }

  void TaskAdded(Task* task) {
    NotifyObserverTaskAdded(task);
    task_provider_tasks_.emplace_back(task);
  }

  void TaskRemoved(Task* task) {
    NotifyObserverTaskRemoved(task);
    base::Erase(task_provider_tasks_, task);
  }

 private:
  void StartUpdating() override {
    for (Task* task : task_provider_tasks_) {
      NotifyObserverTaskAdded(task);
    }
  }

  void StopUpdating() override {}

  std::vector<Task*> task_provider_tasks_;

  DISALLOW_COPY_AND_ASSIGN(FakeTaskProvider);
};

// Defines a test for the child process task provider and the child process
// tasks themselves.
class FallbackTaskProviderTest : public testing::Test,
                                 public TaskProviderObserver {
 public:
  FallbackTaskProviderTest() {
    std::unique_ptr<TaskProvider> primary_subprovider(new FakeTaskProvider());
    std::unique_ptr<TaskProvider> secondary_subprovider(new FakeTaskProvider());
    task_provider_ =
        std::unique_ptr<FallbackTaskProvider>(new FallbackTaskProvider(
            std::move(primary_subprovider), std::move(secondary_subprovider)));
  }

  ~FallbackTaskProviderTest() override {}

  // task_manager::TaskProviderObserver:
  void TaskAdded(Task* task) override {
    EXPECT_FALSE(base::Contains(seen_tasks_, task));
    seen_tasks_.emplace_back(task);
  }

  void TaskRemoved(Task* task) override {
    EXPECT_TRUE(base::Contains(seen_tasks_, task));
    base::Erase(seen_tasks_, task);
  }

  // This adds tasks to the |primary_subprovider|.
  void PrimaryTaskAdded(Task* task) {
    DCHECK(task);
    static_cast<FakeTaskProvider*>(
        task_provider_.get()->primary_source()->subprovider())
        ->TaskAdded(task);
  }
  // This removes tasks from the |primary_subprovider|.
  void PrimaryTaskRemoved(Task* task) {
    DCHECK(task);
    static_cast<FakeTaskProvider*>(
        task_provider_.get()->primary_source()->subprovider())
        ->TaskRemoved(task);
  }

  // This adds tasks to the |secondary_subprovider|.
  void SecondaryTaskAdded(Task* task) {
    DCHECK(task);
    static_cast<FakeTaskProvider*>(
        task_provider_.get()->secondary_source()->subprovider())
        ->TaskAdded(task);
  }

  // This removes tasks from the |secondary_subprovider|.
  void SecondaryTaskRemoved(Task* task) {
    DCHECK(task);
    static_cast<FakeTaskProvider*>(
        task_provider_.get()->secondary_source()->subprovider())
        ->TaskRemoved(task);
  }

  std::string DumpSeenTasks() {
    std::string result;
    for (Task* task : seen_tasks_) {
      result += base::UTF16ToUTF8(task->title());
      result += "\n";
    }
    return result;
  }

  void StartUpdating() { task_provider_.get()->SetObserver(this); }

  void StopUpdating() {
    task_provider_.get()->ClearObserver();
    seen_tasks_.clear();
  }

  // This is the vector of tasks the FallbackTaskProvider has told us about.
  std::vector<Task*> seen_tasks() { return seen_tasks_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<FallbackTaskProvider> task_provider_;
  std::vector<Task*> seen_tasks_;

  DISALLOW_COPY_AND_ASSIGN(FallbackTaskProviderTest);
};

TEST_F(FallbackTaskProviderTest, BasicTest) {
  base::TimeDelta delay = base::TimeDelta::FromMilliseconds(750);
  base::ScopedMockTimeMessageLoopTaskRunner mock_main_runner;
  StartUpdating();
  // In this secondary tasks are named starting with "S" followed by a
  // underscore with the next number being the Pid followed by the a underscore
  // and then the number of processes with that pid that have been created. For
  // instance the first secondary task with Pid of 1 and will be named "S_1_1".
  // Similarly for the third primary process with a Pid of 7 would be "P_7_3".
  FakeTask fake_secondary_task_1_1(1, Task::RENDERER, "S_1_1");
  SecondaryTaskAdded(&fake_secondary_task_1_1);
  mock_main_runner->FastForwardBy(delay);
  EXPECT_EQ("S_1_1\n", DumpSeenTasks());

  FakeTask fake_secondary_task_1_2(1, Task::RENDERER, "S_1_2");
  SecondaryTaskAdded(&fake_secondary_task_1_2);
  mock_main_runner->FastForwardBy(delay);
  EXPECT_EQ(
      "S_1_1\n"
      "S_1_2\n",
      DumpSeenTasks());

  FakeTask fake_primary_task_1_1(1, Task::RENDERER, "P_1_1");
  PrimaryTaskAdded(&fake_primary_task_1_1);
  mock_main_runner->FastForwardBy(delay);
  EXPECT_EQ("P_1_1\n", DumpSeenTasks());

  FakeTask fake_secondary_task_1_3(1, Task::RENDERER, "S_1_3");
  SecondaryTaskAdded(&fake_secondary_task_1_3);
  mock_main_runner->FastForwardBy(delay);
  EXPECT_EQ("P_1_1\n", DumpSeenTasks());

  FakeTask fake_secondary_task_2_1(2, Task::RENDERER, "S_2_1");
  SecondaryTaskAdded(&fake_secondary_task_2_1);
  mock_main_runner->FastForwardBy(delay);
  EXPECT_EQ(
      "P_1_1\n"
      "S_2_1\n",
      DumpSeenTasks());

  FakeTask fake_primary_task_3_1(3, Task::RENDERER, "P_3_1");
  PrimaryTaskAdded(&fake_primary_task_3_1);
  mock_main_runner->FastForwardBy(delay);
  EXPECT_EQ(
      "P_1_1\n"
      "S_2_1\n"
      "P_3_1\n",
      DumpSeenTasks());

  PrimaryTaskRemoved(&fake_primary_task_1_1);
  mock_main_runner->FastForwardBy(delay);
  EXPECT_EQ(
      "S_2_1\n"
      "P_3_1\n"
      "S_1_1\n"
      "S_1_2\n"
      "S_1_3\n",
      DumpSeenTasks());

  StopUpdating();
  EXPECT_EQ("", DumpSeenTasks());

  // After updating the primary tasks (Ps) will be added before the secondary
  // tasks (Ss) so it is reordered.
  StartUpdating();
  mock_main_runner->FastForwardBy(delay);
  EXPECT_EQ(
      "P_3_1\n"
      "S_1_1\n"
      "S_1_2\n"
      "S_1_3\n"
      "S_2_1\n",
      DumpSeenTasks());

  PrimaryTaskAdded(&fake_primary_task_1_1);
  mock_main_runner->FastForwardBy(delay);
  EXPECT_EQ(
      "P_3_1\n"
      "S_2_1\n"
      "P_1_1\n",
      DumpSeenTasks());

  FakeTask fake_primary_task_1_2(1, Task::RENDERER, "P_1_2");
  PrimaryTaskAdded(&fake_primary_task_1_2);
  EXPECT_EQ(
      "P_3_1\n"
      "S_2_1\n"
      "P_1_1\n"
      "P_1_2\n",
      DumpSeenTasks());

  PrimaryTaskRemoved(&fake_primary_task_1_1);
  mock_main_runner->FastForwardBy(delay);
  EXPECT_EQ(
      "P_3_1\n"
      "S_2_1\n"
      "P_1_2\n",
      DumpSeenTasks());

  SecondaryTaskRemoved(&fake_secondary_task_2_1);
  mock_main_runner->FastForwardBy(delay);
  EXPECT_EQ(
      "P_3_1\n"
      "P_1_2\n",
      DumpSeenTasks());

  SecondaryTaskRemoved(&fake_secondary_task_1_1);
  mock_main_runner->FastForwardBy(delay);
  EXPECT_EQ(
      "P_3_1\n"
      "P_1_2\n",
      DumpSeenTasks());

  PrimaryTaskRemoved(&fake_primary_task_1_2);
  mock_main_runner->FastForwardBy(delay);
  EXPECT_EQ(
      "P_3_1\n"
      "S_1_2\n"
      "S_1_3\n",
      DumpSeenTasks());

  PrimaryTaskRemoved(&fake_primary_task_3_1);
  mock_main_runner->FastForwardBy(delay);
  EXPECT_EQ(
      "S_1_2\n"
      "S_1_3\n",
      DumpSeenTasks());
}

}  // namespace task_manager
