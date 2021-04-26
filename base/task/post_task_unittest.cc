// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/post_task.h"

#include "base/callback_helpers.h"
#include "base/run_loop.h"
#include "base/task/task_executor.h"
#include "base/task/test_task_traits_extension.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::Return;

namespace base {

namespace {

class MockTaskExecutor : public TaskExecutor {
 public:
  MockTaskExecutor() {
    ON_CALL(*this, PostDelayedTaskMock(_, _, _, _))
        .WillByDefault(Invoke([this](const Location& from_here,
                                     const TaskTraits& traits,
                                     OnceClosure& task, TimeDelta delay) {
          return runner_->PostDelayedTask(from_here, std::move(task), delay);
        }));
    ON_CALL(*this, CreateTaskRunner(_)).WillByDefault(Return(runner_));
    ON_CALL(*this, CreateSequencedTaskRunner(_)).WillByDefault(Return(runner_));
    ON_CALL(*this, CreateSingleThreadTaskRunner(_, _))
        .WillByDefault(Return(runner_));
#if defined(OS_WIN)
    ON_CALL(*this, CreateCOMSTATaskRunner(_, _)).WillByDefault(Return(runner_));
#endif  // defined(OS_WIN)
  }

  // TaskExecutor:
  // Helper because gmock doesn't support move-only types.
  bool PostDelayedTask(const Location& from_here,
                       const TaskTraits& traits,
                       OnceClosure task,
                       TimeDelta delay) override {
    return PostDelayedTaskMock(from_here, traits, task, delay);
  }
  MOCK_METHOD4(PostDelayedTaskMock,
               bool(const Location& from_here,
                    const TaskTraits& traits,
                    OnceClosure& task,
                    TimeDelta delay));
  MOCK_METHOD1(CreateTaskRunner,
               scoped_refptr<TaskRunner>(const TaskTraits& traits));
  MOCK_METHOD1(CreateSequencedTaskRunner,
               scoped_refptr<SequencedTaskRunner>(const TaskTraits& traits));
  MOCK_METHOD2(CreateSingleThreadTaskRunner,
               scoped_refptr<SingleThreadTaskRunner>(
                   const TaskTraits& traits,
                   SingleThreadTaskRunnerThreadMode thread_mode));
#if defined(OS_WIN)
  MOCK_METHOD2(CreateCOMSTATaskRunner,
               scoped_refptr<SingleThreadTaskRunner>(
                   const TaskTraits& traits,
                   SingleThreadTaskRunnerThreadMode thread_mode));
#endif  // defined(OS_WIN)

  TestSimpleTaskRunner* runner() const { return runner_.get(); }

 private:
  scoped_refptr<TestSimpleTaskRunner> runner_ =
      MakeRefCounted<TestSimpleTaskRunner>();

  DISALLOW_COPY_AND_ASSIGN(MockTaskExecutor);
};

}  // namespace

class PostTaskTestWithExecutor : public ::testing::Test {
 public:
  void SetUp() override {
    RegisterTaskExecutor(TestTaskTraitsExtension::kExtensionId, &executor_);
  }

  void TearDown() override {
    UnregisterTaskExecutorForTesting(TestTaskTraitsExtension::kExtensionId);
  }

 protected:
  testing::StrictMock<MockTaskExecutor> executor_;
  test::TaskEnvironment task_environment_;
};

TEST_F(PostTaskTestWithExecutor, PostTaskToTaskExecutor) {
  // Tasks with extension should go to the executor.
  {
    TaskTraits traits = {TestExtensionBoolTrait()};
    EXPECT_CALL(executor_, PostDelayedTaskMock(_, traits, _, _)).Times(1);
    EXPECT_TRUE(PostTask(FROM_HERE, traits, DoNothing()));
    EXPECT_TRUE(executor_.runner()->HasPendingTask());
    executor_.runner()->ClearPendingTasks();
  }

  {
    TaskTraits traits = {MayBlock(), TestExtensionBoolTrait()};
    EXPECT_CALL(executor_, PostDelayedTaskMock(_, traits, _, _)).Times(1);
    EXPECT_TRUE(PostTask(FROM_HERE, traits, DoNothing()));
    EXPECT_TRUE(executor_.runner()->HasPendingTask());
    executor_.runner()->ClearPendingTasks();
  }

  {
    TaskTraits traits = {TestExtensionEnumTrait::kB, TestExtensionBoolTrait()};
    EXPECT_CALL(executor_, PostDelayedTaskMock(_, traits, _, _)).Times(1);
    EXPECT_TRUE(PostTask(FROM_HERE, traits, DoNothing()));
    EXPECT_TRUE(executor_.runner()->HasPendingTask());
    executor_.runner()->ClearPendingTasks();
  }

  // Task runners with extension should be the executor's.
  {
    TaskTraits traits = {TestExtensionBoolTrait()};
    EXPECT_CALL(executor_, CreateTaskRunner(traits)).Times(1);
    auto task_runner = CreateTaskRunner(traits);
    EXPECT_EQ(executor_.runner(), task_runner);
    EXPECT_CALL(executor_, CreateSequencedTaskRunner(traits)).Times(1);
    auto sequenced_task_runner = CreateSequencedTaskRunner(traits);
    EXPECT_EQ(executor_.runner(), sequenced_task_runner);
    EXPECT_CALL(executor_, CreateSingleThreadTaskRunner(traits, _)).Times(1);
    auto single_thread_task_runner = CreateSingleThreadTaskRunner(traits);
    EXPECT_EQ(executor_.runner(), single_thread_task_runner);
#if defined(OS_WIN)
    EXPECT_CALL(executor_, CreateCOMSTATaskRunner(traits, _)).Times(1);
    auto comsta_task_runner = CreateCOMSTATaskRunner(traits);
    EXPECT_EQ(executor_.runner(), comsta_task_runner);
#endif  // defined(OS_WIN)
  }
}

namespace {

class FlagOnDelete {
 public:
  FlagOnDelete(bool* deleted) : deleted_(deleted) {}

  // Required for RefCountedData.
  FlagOnDelete(FlagOnDelete&& other) : deleted_(other.deleted_) {
    other.deleted_ = nullptr;
  }

  ~FlagOnDelete() {
    if (deleted_) {
      EXPECT_FALSE(*deleted_);
      *deleted_ = true;
    }
  }

 private:
  bool* deleted_;
  DISALLOW_COPY_AND_ASSIGN(FlagOnDelete);
};

}  // namespace

TEST_F(PostTaskTestWithExecutor, DeleteSoon) {
  constexpr TaskTraits traits = {TestExtensionBoolTrait(),
                                 TaskPriority::BEST_EFFORT};

  bool deleted = false;
  auto flag_on_delete = std::make_unique<FlagOnDelete>(&deleted);

  EXPECT_CALL(executor_, CreateSequencedTaskRunner(traits)).Times(1);
  base::DeleteSoon(FROM_HERE, traits, std::move(flag_on_delete));

  EXPECT_FALSE(deleted);

  EXPECT_TRUE(executor_.runner()->HasPendingTask());
  executor_.runner()->RunPendingTasks();

  EXPECT_TRUE(deleted);
}

TEST_F(PostTaskTestWithExecutor, ReleaseSoon) {
  constexpr TaskTraits traits = {TestExtensionBoolTrait(),
                                 TaskPriority::BEST_EFFORT};

  bool deleted = false;
  auto flag_on_delete = MakeRefCounted<RefCountedData<FlagOnDelete>>(&deleted);

  EXPECT_CALL(executor_, CreateSequencedTaskRunner(traits)).Times(1);
  base::ReleaseSoon(FROM_HERE, traits, std::move(flag_on_delete));

  EXPECT_FALSE(deleted);

  EXPECT_TRUE(executor_.runner()->HasPendingTask());
  executor_.runner()->RunPendingTasks();

  EXPECT_TRUE(deleted);
}

}  // namespace base
