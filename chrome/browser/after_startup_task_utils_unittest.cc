// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/after_startup_task_utils.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class WrappedTaskRunner : public base::SequencedTaskRunner {
 public:
  explicit WrappedTaskRunner(scoped_refptr<SequencedTaskRunner> real_runner)
      : real_task_runner_(std::move(real_runner)) {}

  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override {
    ++posted_task_count_;
    return real_task_runner_->PostDelayedTask(
        from_here,
        base::BindOnce(&WrappedTaskRunner::RunWrappedTask, this,
                       std::move(task)),
        base::TimeDelta());  // Squash all delays so our tests complete asap.
  }

  bool PostNonNestableDelayedTask(const base::Location& from_here,
                                  base::OnceClosure task,
                                  base::TimeDelta delay) override {
    // Not implemented.
    NOTREACHED_IN_MIGRATION();
    return false;
  }

  bool RunsTasksInCurrentSequence() const override {
    return real_task_runner_->RunsTasksInCurrentSequence();
  }

  base::SequencedTaskRunner* real_runner() const {
    return real_task_runner_.get();
  }

  int total_task_count() const { return posted_task_count_ + ran_task_count_; }
  int posted_task_count() const { return posted_task_count_; }
  int ran_task_count() const { return ran_task_count_; }

  void reset_task_counts() {
    posted_task_count_ = 0;
    ran_task_count_ = 0;
  }

 private:
  ~WrappedTaskRunner() override {}

  void RunWrappedTask(base::OnceClosure task) {
    ++ran_task_count_;
    std::move(task).Run();
  }

  scoped_refptr<base::SequencedTaskRunner> real_task_runner_;
  int posted_task_count_ = 0;
  int ran_task_count_ = 0;
};

}  // namespace

class AfterStartupTaskTest : public testing::Test {
 public:
  AfterStartupTaskTest() {
    ui_thread_ = base::MakeRefCounted<WrappedTaskRunner>(
        content::GetUIThreadTaskRunner({}));
    background_sequence_ = base::MakeRefCounted<WrappedTaskRunner>(
        base::ThreadPool::CreateSequencedTaskRunner({}));
    AfterStartupTaskUtils::UnsafeResetForTesting();
  }

  // Hop to the background sequence and call IsBrowserStartupComplete.
  bool GetIsBrowserStartupCompleteFromBackgroundSequence() {
    base::RunLoop run_loop;
    bool is_complete;
    background_sequence_->real_runner()->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&AfterStartupTaskUtils::IsBrowserStartupComplete),
        base::BindOnce(&AfterStartupTaskTest::GotIsOnBrowserStartupComplete,
                       &run_loop, &is_complete));
    run_loop.Run();
    return is_complete;
  }

  // Hop to the background sequence and call PostAfterStartupTask.
  void PostAfterStartupTaskFromBackgroundSequence(
      const base::Location& from_here,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      base::OnceClosure task) {
    base::RunLoop run_loop;
    background_sequence_->real_runner()->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&AfterStartupTaskUtils::PostTask, from_here,
                       std::move(task_runner), std::move(task)),
        base::BindLambdaForTesting([&]() { run_loop.Quit(); }));
    run_loop.Run();
  }

  // Make sure all tasks posted to the background sequence get run.
  void FlushBackgroundSequence() {
    base::RunLoop run_loop;
    background_sequence_->real_runner()->PostTaskAndReply(
        FROM_HERE, base::DoNothing(),
        base::BindLambdaForTesting([&]() { run_loop.Quit(); }));
    run_loop.Run();
  }

  static void VerifyExpectedSequence(base::SequencedTaskRunner* task_runner) {
    EXPECT_TRUE(task_runner->RunsTasksInCurrentSequence());
  }

 protected:
  scoped_refptr<WrappedTaskRunner> ui_thread_;
  scoped_refptr<WrappedTaskRunner> background_sequence_;

 private:
  static void GotIsOnBrowserStartupComplete(base::RunLoop* loop,
                                            bool* out,
                                            bool is_complete) {
    *out = is_complete;
    loop->Quit();
  }

  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(AfterStartupTaskTest, IsStartupComplete) {
  // Check IsBrowserStartupComplete on a background sequence first to
  // verify that it does not allocate the underlying flag on that sequence.
  // That allocation sequence correctness part of this test relies on
  // the DCHECK in CancellationFlag::Set().
  EXPECT_FALSE(GetIsBrowserStartupCompleteFromBackgroundSequence());
  EXPECT_FALSE(AfterStartupTaskUtils::IsBrowserStartupComplete());
  AfterStartupTaskUtils::SetBrowserStartupIsCompleteForTesting();
  EXPECT_TRUE(AfterStartupTaskUtils::IsBrowserStartupComplete());
  EXPECT_TRUE(GetIsBrowserStartupCompleteFromBackgroundSequence());
}

TEST_F(AfterStartupTaskTest, PostTask) {
  // Nothing should be posted prior to startup completion.
  EXPECT_FALSE(AfterStartupTaskUtils::IsBrowserStartupComplete());
  AfterStartupTaskUtils::PostTask(
      FROM_HERE, ui_thread_,
      base::BindOnce(&AfterStartupTaskTest::VerifyExpectedSequence,
                     base::RetainedRef(ui_thread_)));
  AfterStartupTaskUtils::PostTask(
      FROM_HERE, background_sequence_,
      base::BindOnce(&AfterStartupTaskTest::VerifyExpectedSequence,
                     base::RetainedRef(background_sequence_)));
  PostAfterStartupTaskFromBackgroundSequence(
      FROM_HERE, ui_thread_,
      base::BindOnce(&AfterStartupTaskTest::VerifyExpectedSequence,
                     base::RetainedRef(ui_thread_)));
  PostAfterStartupTaskFromBackgroundSequence(
      FROM_HERE, background_sequence_,
      base::BindOnce(&AfterStartupTaskTest::VerifyExpectedSequence,
                     base::RetainedRef(background_sequence_)));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, background_sequence_->total_task_count() +
                   ui_thread_->total_task_count());

  // Queued tasks should be posted upon setting the flag.
  AfterStartupTaskUtils::SetBrowserStartupIsCompleteForTesting();
  EXPECT_EQ(2, background_sequence_->posted_task_count());
  EXPECT_EQ(2, ui_thread_->posted_task_count());
  FlushBackgroundSequence();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, background_sequence_->ran_task_count());
  EXPECT_EQ(2, ui_thread_->ran_task_count());

  background_sequence_->reset_task_counts();
  ui_thread_->reset_task_counts();
  EXPECT_EQ(0, background_sequence_->total_task_count() +
                   ui_thread_->total_task_count());

  // Tasks posted after startup should get posted immediately.
  AfterStartupTaskUtils::PostTask(FROM_HERE, ui_thread_, base::DoNothing());
  AfterStartupTaskUtils::PostTask(FROM_HERE, background_sequence_,
                                  base::DoNothing());
  EXPECT_EQ(1, background_sequence_->posted_task_count());
  EXPECT_EQ(1, ui_thread_->posted_task_count());
  PostAfterStartupTaskFromBackgroundSequence(FROM_HERE, ui_thread_,
                                             base::DoNothing());
  PostAfterStartupTaskFromBackgroundSequence(FROM_HERE, background_sequence_,
                                             base::DoNothing());
  EXPECT_EQ(2, background_sequence_->posted_task_count());
  EXPECT_EQ(2, ui_thread_->posted_task_count());
  FlushBackgroundSequence();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, background_sequence_->ran_task_count());
  EXPECT_EQ(2, ui_thread_->ran_task_count());
}
