// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <vector>

#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
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
             nullptr,
             base::kNullProcessHandle,
             process_id),
        type_(type) {}

  FakeTask(const FakeTask&) = delete;
  FakeTask& operator=(const FakeTask&) = delete;

  Type GetType() const override { return type_; }

  int GetChildProcessUniqueID() const override { return 0; }

  const Task* GetParentTask() const override { return nullptr; }

  SessionID GetTabId() const override { return SessionID::InvalidValue(); }

 private:
  Type type_;
};

class FakeTaskProvider : public TaskProvider {
 public:
  FakeTaskProvider() = default;
  FakeTaskProvider(const FakeTaskProvider&) = delete;
  FakeTaskProvider& operator=(const FakeTaskProvider&) = delete;
  ~FakeTaskProvider() override = default;

  Task* GetTaskOfUrlRequest(int child_id, int route_id) override {
    return nullptr;
  }

  void TaskAdded(Task* task) {
    NotifyObserverTaskAdded(task);
    task_provider_tasks_.emplace_back(task);
  }

  void TaskRemoved(Task* task) {
    NotifyObserverTaskRemoved(task);
    std::erase(task_provider_tasks_, task);
  }

 private:
  void StartUpdating() override {
    for (Task* task : task_provider_tasks_) {
      NotifyObserverTaskAdded(task);
    }
  }

  void StopUpdating() override {}

  std::vector<raw_ptr<Task, VectorExperimental>> task_provider_tasks_;
};

// Defines a test for the child process task provider and the child process
// tasks themselves.
class FallbackTaskProviderTest : public testing::Test,
                                 public TaskProviderObserver {
 public:
  FallbackTaskProviderTest() {
    std::vector<std::unique_ptr<TaskProvider>> primary_subproviders;
    primary_subproviders.push_back(std::make_unique<FakeTaskProvider>());
    primary_subproviders.push_back(std::make_unique<FakeTaskProvider>());

    task_provider_ = std::make_unique<FallbackTaskProvider>(
        std::move(primary_subproviders), std::make_unique<FakeTaskProvider>());

    task_provider_->allow_fallback_for_testing_ = true;
  }

  FallbackTaskProviderTest(const FallbackTaskProviderTest&) = delete;
  FallbackTaskProviderTest& operator=(const FallbackTaskProviderTest&) = delete;
  ~FallbackTaskProviderTest() override = default;

  // task_manager::TaskProviderObserver:
  void TaskAdded(Task* task) override {
    EXPECT_FALSE(base::Contains(seen_tasks_, task));
    seen_tasks_.emplace_back(task);
  }

  void TaskRemoved(Task* task) override {
    EXPECT_TRUE(base::Contains(seen_tasks_, task));
    std::erase(seen_tasks_, task);
  }

  // This adds tasks to the first primary subprovider.
  void FirstPrimaryTaskAdded(Task* task) {
    DCHECK(task);
    static_cast<FakeTaskProvider*>(
        task_provider_->primary_sources_[0]->subprovider())
        ->TaskAdded(task);
  }

  // This removes tasks from the first primary subprovider.
  void FirstPrimaryTaskRemoved(Task* task) {
    DCHECK(task);
    static_cast<FakeTaskProvider*>(
        task_provider_->primary_sources_[0]->subprovider())
        ->TaskRemoved(task);
  }

  // This adds tasks to the second primary subprovider.
  void SecondPrimaryTaskAdded(Task* task) {
    DCHECK(task);
    static_cast<FakeTaskProvider*>(
        task_provider_->primary_sources_[1]->subprovider())
        ->TaskAdded(task);
  }

  // This removes tasks from the second primary subprovider.
  void SecondPrimaryTaskRemoved(Task* task) {
    DCHECK(task);
    static_cast<FakeTaskProvider*>(
        task_provider_->primary_sources_[1]->subprovider())
        ->TaskRemoved(task);
  }

  // This adds tasks to the secondary subprovider.
  void SecondaryTaskAdded(Task* task) {
    DCHECK(task);
    static_cast<FakeTaskProvider*>(
        task_provider_->secondary_source_->subprovider())
        ->TaskAdded(task);
  }

  // This removes tasks from the secondary subprovider.
  void SecondaryTaskRemoved(Task* task) {
    DCHECK(task);
    static_cast<FakeTaskProvider*>(
        task_provider_->secondary_source_->subprovider())
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

  void StartUpdating() { task_provider_->SetObserver(this); }

  void StopUpdating() {
    task_provider_->ClearObserver();
    seen_tasks_.clear();
  }

  // This is the vector of tasks the FallbackTaskProvider has told us about.
  std::vector<raw_ptr<Task, VectorExperimental>> seen_tasks() {
    return seen_tasks_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<FallbackTaskProvider> task_provider_;
  std::vector<raw_ptr<Task, VectorExperimental>> seen_tasks_;
};

TEST_F(FallbackTaskProviderTest, BasicTest) {
  // The delay for showing a secondary source is 750ms; delay 1000ms to ensure
  // we see them.
  base::TimeDelta delay = base::Milliseconds(1000);
  base::ScopedMockTimeMessageLoopTaskRunner mock_main_runner;
  StartUpdating();

  // There are two primary task providers and one secondary task provider. The
  // naming convention here is "P_x_y". P is either P or Q for the two primary
  // providers, and S for the secondary provider. The x is the PID of the
  // process, and the y is the task index hosted within the process. For
  // example, the first secondary task with process PID of 1 will be named
  // "S_1_1". Similarly, the third primary task from the first primary provider
  // with process PID of 7 would be "P_7_3".

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
  FirstPrimaryTaskAdded(&fake_primary_task_1_1);
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

  FakeTask fake_primary_task_3_1(3, Task::RENDERER, "Q_3_1");
  SecondPrimaryTaskAdded(&fake_primary_task_3_1);
  mock_main_runner->FastForwardBy(delay);
  EXPECT_EQ(
      "P_1_1\n"
      "S_2_1\n"
      "Q_3_1\n",
      DumpSeenTasks());

  FirstPrimaryTaskRemoved(&fake_primary_task_1_1);
  mock_main_runner->FastForwardBy(delay);
  EXPECT_EQ(
      "S_2_1\n"
      "Q_3_1\n"
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
      "Q_3_1\n"
      "S_1_1\n"
      "S_1_2\n"
      "S_1_3\n"
      "S_2_1\n",
      DumpSeenTasks());

  FirstPrimaryTaskAdded(&fake_primary_task_1_1);
  mock_main_runner->FastForwardBy(delay);
  EXPECT_EQ(
      "Q_3_1\n"
      "S_2_1\n"
      "P_1_1\n",
      DumpSeenTasks());

  FakeTask fake_primary_task_1_2(1, Task::RENDERER, "P_1_2");
  FirstPrimaryTaskAdded(&fake_primary_task_1_2);
  EXPECT_EQ(
      "Q_3_1\n"
      "S_2_1\n"
      "P_1_1\n"
      "P_1_2\n",
      DumpSeenTasks());

  FirstPrimaryTaskRemoved(&fake_primary_task_1_1);
  mock_main_runner->FastForwardBy(delay);
  EXPECT_EQ(
      "Q_3_1\n"
      "S_2_1\n"
      "P_1_2\n",
      DumpSeenTasks());

  SecondaryTaskRemoved(&fake_secondary_task_2_1);
  mock_main_runner->FastForwardBy(delay);
  EXPECT_EQ(
      "Q_3_1\n"
      "P_1_2\n",
      DumpSeenTasks());

  SecondaryTaskRemoved(&fake_secondary_task_1_1);
  mock_main_runner->FastForwardBy(delay);
  EXPECT_EQ(
      "Q_3_1\n"
      "P_1_2\n",
      DumpSeenTasks());

  FirstPrimaryTaskRemoved(&fake_primary_task_1_2);
  mock_main_runner->FastForwardBy(delay);
  EXPECT_EQ(
      "Q_3_1\n"
      "S_1_2\n"
      "S_1_3\n",
      DumpSeenTasks());

  SecondPrimaryTaskRemoved(&fake_primary_task_3_1);
  mock_main_runner->FastForwardBy(delay);
  EXPECT_EQ(
      "S_1_2\n"
      "S_1_3\n",
      DumpSeenTasks());
}

}  // namespace task_manager
