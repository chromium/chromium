// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sequenced_task_runner.h"

#include <utility>

#include "base/bind.h"
#include "base/gtest_prod_util.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

class FlagOnDelete {
 public:
  FlagOnDelete(bool* deleted,
               scoped_refptr<SequencedTaskRunner> expected_deletion_sequence)
      : deleted_(deleted),
        expected_deletion_sequence_(std::move(expected_deletion_sequence)) {}
  FlagOnDelete(const FlagOnDelete&) = delete;
  FlagOnDelete& operator=(const FlagOnDelete&) = delete;

 private:
  friend class DeleteHelper<FlagOnDelete>;
  FRIEND_TEST_ALL_PREFIXES(SequencedTaskRunnerTest,
                           OnTaskRunnerDeleterTargetStoppedEarly);

  ~FlagOnDelete() {
    EXPECT_FALSE(*deleted_);
    *deleted_ = true;
    if (expected_deletion_sequence_)
      EXPECT_TRUE(expected_deletion_sequence_->RunsTasksInCurrentSequence());
  }

  bool* deleted_;
  const scoped_refptr<SequencedTaskRunner> expected_deletion_sequence_;
};

class SequencedTaskRunnerTest : public testing::Test {
 public:
  SequencedTaskRunnerTest(const SequencedTaskRunnerTest&) = delete;
  SequencedTaskRunnerTest& operator=(const SequencedTaskRunnerTest&) = delete;

 protected:
  SequencedTaskRunnerTest() : foreign_thread_("foreign") {}

  void SetUp() override {
    foreign_thread_.Start();
    foreign_runner_ = foreign_thread_.task_runner();
  }

  scoped_refptr<SequencedTaskRunner> foreign_runner_;

  Thread foreign_thread_;

 private:
  test::TaskEnvironment task_environment_;
};

using SequenceBoundUniquePtr =
    std::unique_ptr<FlagOnDelete, OnTaskRunnerDeleter>;

TEST_F(SequencedTaskRunnerTest, OnTaskRunnerDeleterOnMainThread) {
  bool deleted_on_main_thread = false;
  SequenceBoundUniquePtr ptr(
      new FlagOnDelete(&deleted_on_main_thread, ThreadTaskRunnerHandle::Get()),
      OnTaskRunnerDeleter(ThreadTaskRunnerHandle::Get()));
  EXPECT_FALSE(deleted_on_main_thread);
  foreign_runner_->PostTask(
      FROM_HERE, BindOnce([](SequenceBoundUniquePtr) {}, std::move(ptr)));

  {
    RunLoop run_loop;
    foreign_runner_->PostTaskAndReply(FROM_HERE, BindOnce([] {}),
                                      run_loop.QuitClosure());
    run_loop.Run();
  }
  EXPECT_TRUE(deleted_on_main_thread);
}

TEST_F(SequencedTaskRunnerTest, OnTaskRunnerDeleterTargetStoppedEarly) {
  bool deleted_on_main_thread = false;
  FlagOnDelete* raw =
      new FlagOnDelete(&deleted_on_main_thread, ThreadTaskRunnerHandle::Get());
  SequenceBoundUniquePtr ptr(raw, OnTaskRunnerDeleter(foreign_runner_));
  EXPECT_FALSE(deleted_on_main_thread);

  // Stopping the target ahead of deleting |ptr| should make its
  // OnTaskRunnerDeleter no-op.
  foreign_thread_.Stop();
  ptr = nullptr;
  EXPECT_FALSE(deleted_on_main_thread);

  delete raw;
  EXPECT_TRUE(deleted_on_main_thread);
}

}  // namespace
}  // namespace base
