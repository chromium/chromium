// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/post_task_and_reply_impl.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/test_mock_time_task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace base {
namespace internal {

namespace {

class PostTaskAndReplyTaskRunner : public internal::PostTaskAndReplyImpl {
 public:
  explicit PostTaskAndReplyTaskRunner(TaskRunner* destination)
      : destination_(destination) {}

 private:
  bool PostTask(const Location& from_here, OnceClosure task) override {
    return destination_->PostTask(from_here, std::move(task));
  }

  // Non-owning.
  const raw_ptr<TaskRunner> destination_;
};

class ObjectToDelete : public RefCounted<ObjectToDelete> {
 public:
  // |delete_flag| is set to true when this object is deleted
  ObjectToDelete(bool* delete_flag) : delete_flag_(delete_flag) {
    EXPECT_FALSE(*delete_flag_);
  }

  ObjectToDelete(const ObjectToDelete&) = delete;
  ObjectToDelete& operator=(const ObjectToDelete&) = delete;

 private:
  friend class RefCounted<ObjectToDelete>;
  ~ObjectToDelete() { *delete_flag_ = true; }

  const raw_ptr<bool> delete_flag_;
};

class MockObject {
 public:
  MockObject() = default;

  MockObject(const MockObject&) = delete;
  MockObject& operator=(const MockObject&) = delete;

  MOCK_METHOD1(Task, void(scoped_refptr<ObjectToDelete>));
  MOCK_METHOD1(Reply, void(scoped_refptr<ObjectToDelete>));
};

class MockRunsTasksInCurrentSequenceTaskRunner : public TestMockTimeTaskRunner {
 public:
  MockRunsTasksInCurrentSequenceTaskRunner(
      TestMockTimeTaskRunner::Type type =
          TestMockTimeTaskRunner::Type::kStandalone)
      : TestMockTimeTaskRunner(type) {}

  MockRunsTasksInCurrentSequenceTaskRunner(
      const MockRunsTasksInCurrentSequenceTaskRunner&) = delete;
  MockRunsTasksInCurrentSequenceTaskRunner& operator=(
      const MockRunsTasksInCurrentSequenceTaskRunner&) = delete;

  void StopAcceptingTasks() { accepts_tasks_ = false; }

  void RunUntilIdleWithRunsTasksInCurrentSequence() {
    AutoReset<bool> reset(&runs_tasks_in_current_sequence_, true);
    RunUntilIdle();
  }

  void ClearPendingTasksWithRunsTasksInCurrentSequence() {
    AutoReset<bool> reset(&runs_tasks_in_current_sequence_, true);
    ClearPendingTasks();
  }

  // TestMockTimeTaskRunner:
  bool RunsTasksInCurrentSequence() const override {
    return runs_tasks_in_current_sequence_;
  }

  bool PostDelayedTask(const Location& from_here,
                       OnceClosure task,
                       TimeDelta delay) override {
    if (!accepts_tasks_)
      return false;

    return TestMockTimeTaskRunner::PostDelayedTask(from_here, std::move(task),
                                                   delay);
  }

 private:
  ~MockRunsTasksInCurrentSequenceTaskRunner() override = default;

  bool accepts_tasks_ = true;
  bool runs_tasks_in_current_sequence_ = false;
};

class PostTaskAndReplyImplTest : public testing::Test {
 public:
  PostTaskAndReplyImplTest(const PostTaskAndReplyImplTest&) = delete;
  PostTaskAndReplyImplTest& operator=(const PostTaskAndReplyImplTest&) = delete;

 protected:
  PostTaskAndReplyImplTest() = default;

  bool PostTaskAndReplyToMockObject() {
    return PostTaskAndReplyTaskRunner(post_runner_.get())
        .PostTaskAndReply(
            FROM_HERE,
            BindOnce(&MockObject::Task, Unretained(&mock_object_),
                     MakeRefCounted<ObjectToDelete>(&delete_task_flag_)),
            BindOnce(&MockObject::Reply, Unretained(&mock_object_),
                     MakeRefCounted<ObjectToDelete>(&delete_reply_flag_)));
  }

  void ExpectPostTaskAndReplyToMockObjectSucceeds() {
    // Expect the post to succeed.
    EXPECT_TRUE(PostTaskAndReplyToMockObject());

    // Expect the first task to be posted to |post_runner_|.
    EXPECT_TRUE(post_runner_->HasPendingTask());
    EXPECT_FALSE(reply_runner_->HasPendingTask());
    EXPECT_FALSE(delete_task_flag_);
    EXPECT_FALSE(delete_reply_flag_);
  }

  scoped_refptr<MockRunsTasksInCurrentSequenceTaskRunner> post_runner_ =
      MakeRefCounted<MockRunsTasksInCurrentSequenceTaskRunner>();
  scoped_refptr<MockRunsTasksInCurrentSequenceTaskRunner> reply_runner_ =
      MakeRefCounted<MockRunsTasksInCurrentSequenceTaskRunner>(
          TestMockTimeTaskRunner::Type::kBoundToThread);
  testing::StrictMock<MockObject> mock_object_;
  bool delete_task_flag_ = false;
  bool delete_reply_flag_ = false;
};

}  // namespace

TEST_F(PostTaskAndReplyImplTest, PostTaskAndReply) {
  ExpectPostTaskAndReplyToMockObjectSucceeds();

  EXPECT_CALL(mock_object_, Task(_));
  post_runner_->RunUntilIdleWithRunsTasksInCurrentSequence();
  testing::Mock::VerifyAndClear(&mock_object_);
  // The task should have been deleted right after being run.
  EXPECT_TRUE(delete_task_flag_);
  EXPECT_FALSE(delete_reply_flag_);

  // Expect the reply to be posted to |reply_runner_|.
  EXPECT_FALSE(post_runner_->HasPendingTask());
  EXPECT_TRUE(reply_runner_->HasPendingTask());

  EXPECT_CALL(mock_object_, Reply(_));
  reply_runner_->RunUntilIdleWithRunsTasksInCurrentSequence();
  testing::Mock::VerifyAndClear(&mock_object_);
  EXPECT_TRUE(delete_task_flag_);
  // The reply should have been deleted right after being run.
  EXPECT_TRUE(delete_reply_flag_);

  // Expect no pending task in |post_runner_| and |reply_runner_|.
  EXPECT_FALSE(post_runner_->HasPendingTask());
  EXPECT_FALSE(reply_runner_->HasPendingTask());
}

TEST_F(PostTaskAndReplyImplTest, TaskDoesNotRun) {
  ExpectPostTaskAndReplyToMockObjectSucceeds();

  // Clear the |post_runner_|. Both callbacks should be scheduled for deletion
  // on the |reply_runner_|.
  post_runner_->ClearPendingTasksWithRunsTasksInCurrentSequence();
  EXPECT_FALSE(post_runner_->HasPendingTask());
  EXPECT_TRUE(reply_runner_->HasPendingTask());
  EXPECT_FALSE(delete_task_flag_);
  EXPECT_FALSE(delete_reply_flag_);

  // Run the |reply_runner_|. Both callbacks should be deleted.
  reply_runner_->RunUntilIdleWithRunsTasksInCurrentSequence();
  EXPECT_TRUE(delete_task_flag_);
  EXPECT_TRUE(delete_reply_flag_);
}

TEST_F(PostTaskAndReplyImplTest, ReplyDoesNotRun) {
  ExpectPostTaskAndReplyToMockObjectSucceeds();

  EXPECT_CALL(mock_object_, Task(_));
  post_runner_->RunUntilIdleWithRunsTasksInCurrentSequence();
  testing::Mock::VerifyAndClear(&mock_object_);
  // The task should have been deleted right after being run.
  EXPECT_TRUE(delete_task_flag_);
  EXPECT_FALSE(delete_reply_flag_);

  // Expect the reply to be posted to |reply_runner_|.
  EXPECT_FALSE(post_runner_->HasPendingTask());
  EXPECT_TRUE(reply_runner_->HasPendingTask());

  // Clear the |reply_runner_| queue without running tasks. The reply callback
  // should be deleted.
  reply_runner_->ClearPendingTasksWithRunsTasksInCurrentSequence();
  EXPECT_TRUE(delete_task_flag_);
  EXPECT_TRUE(delete_reply_flag_);
}

// This is a regression test for crbug.com/922938.
TEST_F(PostTaskAndReplyImplTest,
       PostTaskToStoppedTaskRunnerWithoutSequencedContext) {
  reply_runner_.reset();
  EXPECT_FALSE(SequencedTaskRunner::HasCurrentDefault());
  post_runner_->StopAcceptingTasks();

  // Expect the post to return false, but not to crash.
  EXPECT_FALSE(PostTaskAndReplyToMockObject());

  // Expect all tasks to be deleted.
  EXPECT_TRUE(delete_task_flag_);
  EXPECT_TRUE(delete_reply_flag_);
}

}  // namespace internal
}  // namespace base
