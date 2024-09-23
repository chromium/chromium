// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/check_op.h"
#include "base/process/process_handle.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "chrome/browser/task_manager/providers/crosapi/crosapi_task.h"
#include "chrome/browser/task_manager/providers/crosapi/crosapi_task_provider_ash.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace task_manager {

namespace {
crosapi::mojom::TaskPtr CreateMojoTask(const std::string& task_uuid,
                                       const base::ProcessId& pid,
                                       crosapi::mojom::TaskType type,
                                       const std::string& title) {
  auto mojo_task = crosapi::mojom::Task::New();
  mojo_task->task_uuid = task_uuid;
  mojo_task->process_id = pid;
  mojo_task->type = type;
  mojo_task->title = base::ASCIIToUTF16(title);
  return mojo_task;
}

crosapi::mojom::TaskGroupPtr CreateMojoTaskGroup(base::ProcessId pid) {
  auto mojo_task_group = crosapi::mojom::TaskGroup::New();
  mojo_task_group->process_id = pid;
  return mojo_task_group;
}

}  // namespace

class CrosapiTaskProviderAshTest : public testing::Test,
                                   public TaskProviderObserver {
 public:
  CrosapiTaskProviderAshTest()
      : task_provider_(std::make_unique<CrosapiTaskProviderAsh>()) {}

  CrosapiTaskProviderAshTest(const CrosapiTaskProviderAshTest&) = delete;
  CrosapiTaskProviderAshTest& operator=(const CrosapiTaskProviderAshTest&) =
      delete;
  ~CrosapiTaskProviderAshTest() override = default;

  // task_manager::TaskProviderObserver:
  void TaskAdded(Task* task) override {
    ++task_added_count_;
    task_ids_.push_back(task->task_id());

    // Verify the count of GetSortedTaskIds() is the total number of crosapi
    // tasks, which includes the new task just added, when notifying its
    // observer for TaskAdded.
    DCHECK_EQ(GetTaskCount(), GetSortedTaskIdsCount());
  }

  void TaskRemoved(Task* task) override {
    ++task_removed_count_;
    size_t count = GetTaskCount();
    std::erase(task_ids_, task->task_id());
    DCHECK_EQ(count - 1, GetTaskCount());
  }

  void ActiveTaskFetched(TaskId task_id) override {
    ++active_task_fetched_count_;
  }

 protected:
  CrosapiTaskProviderAsh* task_provider() { return task_provider_.get(); }

  size_t GetTaskCount() const { return task_ids_.size(); }

  size_t GetSortedTaskIdsCount() const {
    return task_provider_->GetSortedTaskIds().size();
  }

  int task_added_count() const { return task_added_count_; }
  int task_removed_count() const { return task_removed_count_; }
  int active_task_fetched_count() const { return active_task_fetched_count_; }

  void ResetInternalCount() {
    task_added_count_ = 0;
    task_removed_count_ = 0;
    active_task_fetched_count_ = 0;
  }

  bool DoSortedTaskIdsMatchMojoTasksOrder(
      std::vector<std::string> mojo_task_uuids) const {
    const TaskIdList& sorted_task_ids = task_provider_->GetSortedTaskIds();
    if (sorted_task_ids.size() != mojo_task_uuids.size())
      return false;

    for (size_t i = 0; i < mojo_task_uuids.size(); ++i) {
      CrosapiTask* task =
          task_provider_->uuid_to_task_[mojo_task_uuids[i]].get();
      DCHECK(task);
      if (task->task_id() != sorted_task_ids[i])
        return false;
    }
    return true;
  }

 private:
  std::unique_ptr<CrosapiTaskProviderAsh> task_provider_;
  std::vector<TaskId> task_ids_;
  int task_added_count_ = 0;
  int task_removed_count_ = 0;
  int active_task_fetched_count_ = 0;
};

// Tests CrosapiTaskProviderAsh for processing the mojo task data returned from
// crosapi.
TEST_F(CrosapiTaskProviderAshTest, OnGetTaskManagerTasks) {
  task_provider()->SetObserver(this);

  // Create mojo task data set with 3 mojo tasks.
  std::vector<crosapi::mojom::TaskPtr> mojo_tasks;

  std::string task_1_uuid = base::Uuid::GenerateRandomV4().AsLowercaseString();
  base::ProcessId task_1_pid = base::GetCurrentProcId();
  std::string task_1_title = std::string("Task 1");
  crosapi::mojom::TaskType task_1_type = crosapi::mojom::TaskType::kBrowser;

  std::string task_2_uuid = base::Uuid::GenerateRandomV4().AsLowercaseString();
  base::ProcessId task_2_pid = task_1_pid + 100;
  std::string task_2_title = std::string("Task 2");
  crosapi::mojom::TaskType task_2_type = crosapi::mojom::TaskType::kGpu;

  std::string task_3_uuid = base::Uuid::GenerateRandomV4().AsLowercaseString();
  base::ProcessId task_3_pid = task_1_pid + 300;
  std::string task_3_title = std::string("Task 3");
  crosapi::mojom::TaskType task_3_type = crosapi::mojom::TaskType::kUtility;

  mojo_tasks.push_back(
      CreateMojoTask(task_1_uuid, task_1_pid, task_1_type, task_1_title));
  mojo_tasks.push_back(
      CreateMojoTask(task_2_uuid, task_2_pid, task_2_type, task_2_title));
  mojo_tasks.push_back(
      CreateMojoTask(task_3_uuid, task_3_pid, task_3_type, task_3_title));

  // Cache the task uuid of the mojo tasks with their sequence in
  // |mojo_tasks| preserved.
  std::vector<std::string> mojo_task_uuids_;
  for (const auto& mojo_task : mojo_tasks)
    mojo_task_uuids_.push_back(mojo_task->task_uuid);

  std::vector<crosapi::mojom::TaskGroupPtr> mojo_task_groups;
  mojo_task_groups.push_back(CreateMojoTaskGroup(task_1_pid));
  mojo_task_groups.push_back(CreateMojoTaskGroup(task_2_pid));
  mojo_task_groups.push_back(CreateMojoTaskGroup(task_3_pid));

  task_provider()->OnGetTaskManagerTasks(
      std::move(mojo_tasks), std::move(mojo_task_groups), task_1_uuid);

  // Verify all mojo tasks have been added to the task providers,
  // and GetSortedTaskIds returns the correct number of the task count.
  DCHECK_EQ(3u, GetTaskCount());
  DCHECK_EQ(3u, GetSortedTaskIdsCount());
  DCHECK_EQ(3, task_added_count());
  DCHECK_EQ(0, task_removed_count());
  DCHECK_EQ(1, active_task_fetched_count());
  // Verify that task ids returned by GetSortedTaskIds() matches
  // their order from mojo tasks sent from the crosapi.
  DCHECK(DoSortedTaskIdsMatchMojoTasksOrder(std::move(mojo_task_uuids_)));

  // Simulate that crosapi task data is refreshed, one of the task is removed,
  // and also 2 existing tasks have changed the order.
  mojo_tasks.push_back(
      CreateMojoTask(task_2_uuid, task_2_pid, task_2_type, task_2_title));
  mojo_tasks.push_back(
      CreateMojoTask(task_1_uuid, task_1_pid, task_1_type, task_1_title));
  mojo_task_uuids_.clear();
  for (const auto& mojo_task : mojo_tasks)
    mojo_task_uuids_.push_back(mojo_task->task_uuid);

  mojo_task_groups.push_back(CreateMojoTaskGroup(task_2_pid));
  mojo_task_groups.push_back(CreateMojoTaskGroup(task_1_pid));

  ResetInternalCount();
  task_provider()->OnGetTaskManagerTasks(
      std::move(mojo_tasks), std::move(mojo_task_groups), task_1_uuid);

  // Verify that one of the tasks has been removed.
  DCHECK_EQ(2u, GetTaskCount());
  DCHECK_EQ(2u, GetSortedTaskIdsCount());
  DCHECK_EQ(0, task_added_count());
  DCHECK_EQ(1, task_removed_count());
  DCHECK_EQ(1, active_task_fetched_count());
  // Verify that task ids returned by GetSortedTaskIds() matches
  // the order of the mojo tasks.
  DCHECK(DoSortedTaskIdsMatchMojoTasksOrder(std::move(mojo_task_uuids_)));
}

}  // namespace task_manager
