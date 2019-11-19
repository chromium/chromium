// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/task_manager/providers/task.h"
#include "chrome/browser/task_manager/sampling/task_manager_impl.h"
#include "chrome/browser/task_manager/task_manager_observer.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace task_manager {

namespace {

// A Task for unittests, not backed by a real process, that can report any given
// value.
class FakeTask : public Task {
 public:
  FakeTask(base::ProcessId process_id,
           Type type,
           const std::string& title,
           SessionID tab_id)
      : Task(base::ASCIIToUTF16(title),
             "FakeTask",
             nullptr,
             base::kNullProcessHandle,
             process_id),
        type_(type),
        parent_(nullptr),
        tab_id_(tab_id) {
    TaskManagerImpl::GetInstance()->TaskAdded(this);
  }

  ~FakeTask() override { TaskManagerImpl::GetInstance()->TaskRemoved(this); }

  Type GetType() const override { return type_; }

  int GetChildProcessUniqueID() const override { return 0; }

  const Task* GetParentTask() const override { return parent_; }

  SessionID GetTabId() const override { return tab_id_; }

  void SetParent(Task* parent) { parent_ = parent; }

 private:
  Type type_;
  Task* parent_;
  SessionID tab_id_;

  DISALLOW_COPY_AND_ASSIGN(FakeTask);
};

}  // namespace

class TaskManagerImplTest : public testing::Test, public TaskManagerObserver {
 public:
  TaskManagerImplTest()
      : TaskManagerObserver(base::TimeDelta::FromSeconds(1),
                            REFRESH_TYPE_NONE) {
    TaskManagerImpl::GetInstance()->AddObserver(this);
  }
  ~TaskManagerImplTest() override {
    tasks_.clear();
    observed_task_manager()->RemoveObserver(this);
  }

  FakeTask* AddTask(int pid_offset,
                    Task::Type type,
                    const std::string& title,
                    SessionID tab_id) {
    // Offset based on the current process id, to avoid collisions with the
    // browser process task.
    base::ProcessId process_id = base::GetCurrentProcId() + pid_offset;
    tasks_.emplace_back(new FakeTask(process_id, type, title, tab_id));
    return tasks_.back().get();
  }

  std::string DumpSortedTasks() {
    std::string result;
    for (TaskId task_id : observed_task_manager()->GetTaskIdsList()) {
      result += base::UTF16ToUTF8(observed_task_manager()->GetTitle(task_id));
      result += "\n";
    }
    return result;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::vector<std::unique_ptr<FakeTask>> tasks_;
  DISALLOW_COPY_AND_ASSIGN(TaskManagerImplTest);
};

TEST_F(TaskManagerImplTest, SortingTypes) {
  constexpr SessionID kTabId1 = SessionID::FromSerializedValue(10);
  constexpr SessionID kTabId2 = SessionID::FromSerializedValue(20);

  AddTask(100, Task::GPU, "Gpu Process", /*tab_id=*/SessionID::InvalidValue());

  Task* tab1 = AddTask(200, Task::RENDERER, "Tab One", kTabId1);
  AddTask(400, Task::EXTENSION, "Extension Subframe: Tab One", kTabId1)
      ->SetParent(tab1);
  AddTask(300, Task::RENDERER, "Subframe: Tab One", kTabId1)->SetParent(tab1);

  Task* tab2 = AddTask(200, Task::RENDERER,
                       "Tab Two: sharing process with Tab One", kTabId2);

  AddTask(301, Task::RENDERER, "Subframe: Tab Two", kTabId2)->SetParent(tab2);
  AddTask(400, Task::EXTENSION, "Extension Subframe: Tab Two", kTabId2)
      ->SetParent(tab2);

  AddTask(600, Task::ARC, "ARC", /*tab_id=*/SessionID::InvalidValue());
  AddTask(650, Task::CROSTINI, "Crostini",
          /*tab_id=*/SessionID::InvalidValue());
  AddTask(800, Task::UTILITY, "Utility One",
          /*tab_id=*/SessionID::InvalidValue());
  AddTask(700, Task::UTILITY, "Utility Two",
          /*tab_id=*/SessionID::InvalidValue());
  AddTask(1000, Task::GUEST, "Guest", kTabId2);
  AddTask(900, Task::WORKER, "Worker", /*tab_id=*/SessionID::InvalidValue());
  AddTask(500, Task::ZYGOTE, "Zygote", /*tab_id=*/SessionID::InvalidValue());

  AddTask(300, Task::RENDERER, "Subframe: Tab One (2)", kTabId1)
      ->SetParent(tab1);
  AddTask(300, Task::RENDERER, "Subframe: Tab One (third)", kTabId1)
      ->SetParent(tab1);
  AddTask(300, Task::RENDERER, "Subframe: Tab One (4)", kTabId1)
      ->SetParent(tab1);

  EXPECT_EQ(
      "Browser\n"
      "Gpu Process\n"
      "ARC\n"
      "Crostini\n"
      "Zygote\n"
      "Utility One\n"
      "Utility Two\n"
      "Tab One\n"
      "Tab Two: sharing process with Tab One\n"
      "Subframe: Tab One\n"
      "Subframe: Tab One (2)\n"
      "Subframe: Tab One (third)\n"
      "Subframe: Tab One (4)\n"
      "Extension Subframe: Tab One\n"
      "Extension Subframe: Tab Two\n"
      "Subframe: Tab Two\n"
      "Guest\n"
      "Worker\n",
      DumpSortedTasks());
}

TEST_F(TaskManagerImplTest, SortingCycles) {
  constexpr SessionID kTabId1 = SessionID::FromSerializedValue(10);
  constexpr SessionID kTabId2 = SessionID::FromSerializedValue(20);
  constexpr SessionID kTabId3 = SessionID::FromSerializedValue(5);
  constexpr SessionID kTabId4 = SessionID::FromSerializedValue(30);

  // Two tabs, with subframes in the other's process. This induces a cycle in
  // the TaskGroup dependencies, without being a cycle in the Tasks. This can
  // happen in practice.
  Task* tab1 = AddTask(200, Task::RENDERER, "Tab 1: Process 200", kTabId1);
  AddTask(300, Task::RENDERER, "Subframe in Tab 1: Process 300", kTabId1)
      ->SetParent(tab1);
  Task* tab2 = AddTask(300, Task::RENDERER, "Tab 2: Process 300", kTabId2);
  AddTask(200, Task::RENDERER, "Subframe in Tab 2: Process 200", kTabId2)
      ->SetParent(tab2);

  // Simulated GPU process.
  AddTask(100, Task::GPU, "Gpu Process", /*tab_id=*/SessionID::InvalidValue());

  // Two subframes that list each other as a parent (a true cycle). This
  // shouldn't happen in practice, but we want the sorting code to handle it
  // gracefully.
  FakeTask* cycle1 = AddTask(501, Task::SANDBOX_HELPER, "Cycle 1",
                             /*tab_id=*/SessionID::InvalidValue());
  FakeTask* cycle2 =
      AddTask(500, Task::ARC, "Cycle 2", /*tab_id=*/SessionID::InvalidValue());
  cycle1->SetParent(cycle2);
  cycle2->SetParent(cycle1);

  // A cycle where both elements are in the same group.
  FakeTask* cycle3 = AddTask(600, Task::SANDBOX_HELPER, "Cycle 3",
                             /*tab_id=*/SessionID::InvalidValue());
  FakeTask* cycle4 =
      AddTask(600, Task::ARC, "Cycle 4", /*tab_id=*/SessionID::InvalidValue());
  cycle3->SetParent(cycle4);
  cycle4->SetParent(cycle3);

  // Tasks listing a cycle as their parent.
  FakeTask* lollipop5 = AddTask(701, Task::EXTENSION, "Child of Cycle 3",
                                /*tab_id=*/SessionID::InvalidValue());
  lollipop5->SetParent(cycle3);
  FakeTask* lollipop6 = AddTask(700, Task::PLUGIN, "Child of Cycle 4",
                                /*tab_id=*/SessionID::InvalidValue());
  lollipop6->SetParent(cycle4);

  // A task listing itself as parent.
  FakeTask* self_cycle = AddTask(800, Task::RENDERER, "Self Cycle", kTabId3);
  self_cycle->SetParent(self_cycle);

  // Add a plugin child to tab1 and tab2.
  AddTask(900, Task::PLUGIN, "Plugin: Tab 2", kTabId2)->SetParent(tab1);
  AddTask(901, Task::PLUGIN, "Plugin: Tab 1", kTabId1)->SetParent(tab1);

  // Finish with a normal renderer task.
  AddTask(903, Task::RENDERER, "Tab: Normal Renderer", kTabId4);

  // Cycles should wind up on the bottom of the list.
  EXPECT_EQ(
      "Browser\n"
      "Gpu Process\n"
      "Tab 1: Process 200\n"
      "Subframe in Tab 2: Process 200\n"
      "Tab 2: Process 300\n"
      "Subframe in Tab 1: Process 300\n"
      "Plugin: Tab 1\n"
      "Plugin: Tab 2\n"
      "Tab: Normal Renderer\n"
      "Cycle 2\n"           // ARC
      "Cycle 1\n"           // Child of 2
      "Cycle 4\n"           // ARC; task_id > Cycle 2's
      "Cycle 3\n"           // Same-process child of 4 (SANDBOX_HELPER > ARC)
      "Child of Cycle 4\n"  // Child of 4
      "Child of Cycle 3\n"  // Child of 3
      "Self Cycle\n",       // RENDERER (> ARC)
      DumpSortedTasks());
}

}  // namespace task_manager
