// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/smb_client/smb_task_queue.h"

#include <map>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace smb_client {
namespace {

constexpr size_t kTaskQueueCapacity = 3;
}
// SmbTaskQueue is used to test SmbTaskQueue. Tasks are added to the task queue
// with specified |task_id|'s. When a task is run by the task_queue_, it is
// added to the pending_ map and can be completed by invoking the
// CompleteTask method.
class SmbTaskQueueTest : public testing::Test {
 public:
  SmbTaskQueueTest() : task_queue_(kTaskQueueCapacity) {}
  SmbTaskQueueTest(const SmbTaskQueueTest&) = delete;
  SmbTaskQueueTest& operator=(const SmbTaskQueueTest&) = delete;
  ~SmbTaskQueueTest() override = default;

 protected:
  // Calls SmbTaskQueue::GetNextOperationId().
  OperationId GetOperationId() { return task_queue_.GetNextOperationId(); }

  // Creates and adds a task with |task_id| and a default operation
  // id. |task_id| is used for testing to manually control when tasks finish.
  void CreateAndAddTask(uint32_t task_id) {
    const OperationId operation_id = task_queue_.GetNextOperationId();
    CreateAndAddTask(task_id, operation_id);
  }

  // Creates and adds a task with |task_id| for the corresponding
  // |operation_id|.
  void CreateAndAddTask(uint32_t task_id, OperationId operation_id) {
    base::OnceClosure reply =
        base::BindOnce(&SmbTaskQueueTest::OnReply, base::Unretained(this));
    SmbTask task =
        base::BindOnce(&SmbTaskQueueTest::Start, base::Unretained(this),
                       task_id, std::move(reply));

    task_queue_.AddTask(std::move(task), operation_id);
  }

  // Checks whether the task |task_id| is pending.
  bool IsPending(uint32_t task_id) const { return pending_.count(task_id); }

  // Completes the pending task |task_id|, running its reply.
  void CompleteTask(uint32_t task_id) {
    DCHECK(IsPending(task_id));

    SmbTask to_run = std::move(pending_[task_id]);
    pending_.erase(task_id);

    std::move(to_run).Run();
  }

  // Returns the number of pending tasks.
  size_t PendingCount() const { return pending_.size(); }

  // Calls AbortOperation with |operation_id| on task_queue_.
  void Abort(OperationId operation_id) {
    task_queue_.AbortOperation(operation_id);
  }

 private:
  void OnReply() { task_queue_.TaskFinished(); }

  void Start(uint32_t task_id, base::OnceClosure reply) {
    pending_[task_id] = std::move(reply);
  }

  SmbTaskQueue task_queue_;
  std::map<uint32_t, base::OnceClosure> pending_;
};

// SmbTaskQueue immediately runs a task when less than max_pending are running.
TEST_F(SmbTaskQueueTest, TaskQueueRunsASingleTask) {
  const uint32_t task_id = 1;

  CreateAndAddTask(task_id);
  EXPECT_TRUE(IsPending(task_id));

  CompleteTask(task_id);
  EXPECT_FALSE(IsPending(task_id));
}

// SmbTaskQueue runs atleast max_pending_ tasks concurrently.
TEST_F(SmbTaskQueueTest, TaskQueueRunsMultipleTasks) {
  const uint32_t task_id_1 = 1;
  const uint32_t task_id_2 = 2;
  const uint32_t task_id_3 = 3;

  CreateAndAddTask(task_id_1);
  CreateAndAddTask(task_id_2);
  CreateAndAddTask(task_id_3);

  EXPECT_EQ(kTaskQueueCapacity, PendingCount());

  EXPECT_TRUE(IsPending(task_id_1));
  EXPECT_TRUE(IsPending(task_id_2));
  EXPECT_TRUE(IsPending(task_id_3));

  CompleteTask(task_id_1);
  CompleteTask(task_id_2);
  CompleteTask(task_id_3);

  EXPECT_FALSE(IsPending(task_id_1));
  EXPECT_FALSE(IsPending(task_id_2));
  EXPECT_FALSE(IsPending(task_id_3));
}

// SmbTaskQueue runs at most max_pending_ tasks concurrently.
TEST_F(SmbTaskQueueTest, TaskQueueDoesNotRunAdditionalTestsWhenFull) {
  const uint32_t task_id_1 = 1;
  const uint32_t task_id_2 = 2;
  const uint32_t task_id_3 = 3;
  const uint32_t task_id_4 = 4;

  CreateAndAddTask(task_id_1);
  CreateAndAddTask(task_id_2);
  CreateAndAddTask(task_id_3);
  CreateAndAddTask(task_id_4);

  // The first three tasks should run.
  EXPECT_EQ(kTaskQueueCapacity, PendingCount());
  EXPECT_TRUE(IsPending(task_id_1));
  EXPECT_TRUE(IsPending(task_id_2));
  EXPECT_TRUE(IsPending(task_id_3));

  // The fourth task should wait until another task finishes to run.
  EXPECT_FALSE(IsPending(task_id_4));

  CompleteTask(task_id_1);
  EXPECT_FALSE(IsPending(task_id_1));

  // After completing a task, the fourth task should be able to run.
  EXPECT_TRUE(IsPending(task_id_4));
}

// AbortOperation removes all the tasks corresponding to an operation that has
// not begin to run yet.
TEST_F(SmbTaskQueueTest, AbortOperationRemovesAllTasksOfUnrunOperation) {
  // Saturate the SmbTaskQueue with tasks.
  const uint32_t filler_task_1 = 1;
  const uint32_t filler_task_2 = 2;
  const uint32_t filler_task_3 = 3;
  CreateAndAddTask(filler_task_1);
  CreateAndAddTask(filler_task_2);
  CreateAndAddTask(filler_task_3);

  // Create some tasks coresponding to a specific operation_id.
  const OperationId operation_id = GetOperationId();
  const uint32_t task_to_cancel_1 = 101;
  const uint32_t task_to_cancel_2 = 102;
  const uint32_t task_to_cancel_3 = 103;

  CreateAndAddTask(task_to_cancel_1, operation_id);
  CreateAndAddTask(task_to_cancel_2, operation_id);
  CreateAndAddTask(task_to_cancel_3, operation_id);

  // The filler tasks should be pending, the tasks to cancel should not be.
  EXPECT_EQ(kTaskQueueCapacity, PendingCount());
  EXPECT_TRUE(IsPending(filler_task_1));
  EXPECT_TRUE(IsPending(filler_task_2));
  EXPECT_TRUE(IsPending(filler_task_3));

  EXPECT_FALSE(IsPending(task_to_cancel_1));
  EXPECT_FALSE(IsPending(task_to_cancel_2));
  EXPECT_FALSE(IsPending(task_to_cancel_3));

  // Aborting operation_id and completeing the filler tasks should not run
  // the task_to_cancel's.
  Abort(operation_id);

  EXPECT_EQ(kTaskQueueCapacity, PendingCount());

  CompleteTask(filler_task_1);
  EXPECT_EQ(2u, PendingCount());

  CompleteTask(filler_task_2);
  EXPECT_EQ(1u, PendingCount());

  CompleteTask(filler_task_3);
  EXPECT_EQ(0u, PendingCount());

  EXPECT_FALSE(IsPending(task_to_cancel_1));
  EXPECT_FALSE(IsPending(task_to_cancel_2));
  EXPECT_FALSE(IsPending(task_to_cancel_3));
}

// AbortOperation aborts all the tasks correspoding to an operation that has
// some running tasks.
TEST_F(SmbTaskQueueTest, AbortOperationRemovesUnrunTasksOfRunningOperation) {
  const uint32_t filler_task_1 = 1;
  const uint32_t filler_task_2 = 2;
  CreateAndAddTask(filler_task_1);
  CreateAndAddTask(filler_task_2);

  // Create some tasks coresponding to a specific operation_id.
  const OperationId operation_id = GetOperationId();
  const uint32_t task_to_cancel_1 = 101;
  const uint32_t task_to_cancel_2 = 102;
  const uint32_t task_to_cancel_3 = 103;

  CreateAndAddTask(task_to_cancel_1, operation_id);
  CreateAndAddTask(task_to_cancel_2, operation_id);
  CreateAndAddTask(task_to_cancel_3, operation_id);

  // The task queue should be running the maximum number of pending tasks,
  // including the first task for operation_id. The remaining tasks for
  // operation_id are not yet running.
  EXPECT_EQ(kTaskQueueCapacity, PendingCount());
  EXPECT_TRUE(IsPending(task_to_cancel_1));

  EXPECT_FALSE(IsPending(task_to_cancel_2));
  EXPECT_FALSE(IsPending(task_to_cancel_3));

  // Aborting operation_id should not effect the status of task_to_cancel_1
  // since it was already pending.
  Abort(operation_id);

  EXPECT_TRUE(IsPending(task_to_cancel_1));

  // task_to_cancel_2 and task_to_cancel_3 should not run when the task queue
  // has space capacity for them.
  CompleteTask(filler_task_1);
  CompleteTask(filler_task_2);
  CompleteTask(task_to_cancel_1);

  EXPECT_LT(PendingCount(), kTaskQueueCapacity);

  EXPECT_FALSE(IsPending(task_to_cancel_2));
  EXPECT_FALSE(IsPending(task_to_cancel_3));
}

}  // namespace smb_client
}  // namespace chromeos
